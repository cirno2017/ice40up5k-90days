#include <iostream>
#include <cstdlib>
#include <cstdint>
#include <random>
#include "verilated.h"
#include "verilated_vcd_c.h"
#include "Vtop.h"

vluint64_t sim_time = 0;
constexpr vluint64_t CLK_HALF_PERIOD = 10;

// 软件参考模型：独立期望值，初始化不复制DUT输出
uint8_t ref_dff8_q = 0U;
uint8_t ref_reg8_en_q = 0U;

int main(int argc, char **argv)
{
    Verilated::commandArgs(argc, argv);
    Vtop *dut = new Vtop;

    Verilated::traceEverOn(true);
    VerilatedVcdC *trace = new VerilatedVcdC;
    dut->trace(trace, 99);
    trace->open("wave.vcd");

    int error_cnt = 0;

    auto check_dut = [&]()
    {
        uint8_t dut_dff8 = static_cast<uint8_t>(dut->dff8_q);
        uint8_t dut_reg8 = static_cast<uint8_t>(dut->reg8_en_q);
        bool err1 = (dut_dff8 != ref_dff8_q);
        bool err2 = (dut_reg8 != ref_reg8_en_q);
        if (err1 || err2)
        {
            error_cnt++;
            std::cerr << "[MISMATCH] dut(dff8=0x" << std::hex << (int)dut_dff8
                      << ", reg8=0x" << (int)dut_reg8 << ")"
                      << " ref(dff8=0x" << (int)ref_dff8_q
                      << ", reg8=0x" << (int)ref_reg8_en_q << ")\n";
        }
    };

    // ======================================================
    // 预初始化：主动采样 d=0，en=1，产生上升沿；独立期望值硬编码为0，做检查
    // 【重点】ref变量人为设置0，不从DUT复制输出
    // ======================================================
    std::cout << "\n==== Pre‑test: explicit sample d=0 en=1, expected value hard‑set to 0 ====\n";
    dut->clk = 0;
    dut->en = 1;
    dut->d = 0x00;
    dut->eval();
    trace->dump(sim_time);

    // 上升沿：硬件采样 d=0 en=1
    sim_time += CLK_HALF_PERIOD;
    dut->clk = 1;
    dut->eval();
    trace->dump(sim_time);
    // 独立设置期望值，不读取DUT输出
    ref_dff8_q = 0x00;
    ref_reg8_en_q = 0x00;
    check_dut(); // 如果DUT输出不是0，直接报错

    // 下降沿，回到clk=0
    sim_time += CLK_HALF_PERIOD;
    dut->clk = 0;
    dut->eval();
    trace->dump(sim_time);
    check_dut();
    std::printf("[%4luns] After init sample: expected dff8=0x00 reg8=0x00\n", sim_time);

    // ==============================
    // Test1：时钟停在低电平 clk=0，多次修改d，输出必须保持
    // ==============================
    std::cout << "\n==== Test1: CLK stay LOW (clk=0), modify d many times ====\n";
    for (int i = 0; i < 5; i++)
    {
        sim_time += CLK_HALF_PERIOD;
        dut->d = static_cast<uint8_t>(0x10 + i * 0x11);
        dut->eval();
        trace->dump(sim_time);
        check_dut();
        std::printf("[%4luns] clk=%d d=0x%02X | dut dff8_q=0x%02X reg8_en_q=0x%02X\n",
                    sim_time, (int)dut->clk, (int)dut->d,
                    (int)dut->dff8_q, (int)dut->reg8_en_q);
    }

    // ======================================================
    // Test2：高电平停钟测试：先产生一次上升沿更新参考模型，之后保持clk=1不变
    // ======================================================
    std::cout << "\n==== Pre‑Test2: generate valid rising‑edge before holding clk HIGH ====\n";
    // 先拉低时钟，准备新激励
    sim_time += CLK_HALF_PERIOD;
    dut->clk = 0;
    dut->d = 0x55;
    dut->en = 1;
    dut->eval();
    trace->dump(sim_time);
    check_dut();

    // 产生上升沿：硬件采样，同步更新参考模型
    sim_time += CLK_HALF_PERIOD;
    dut->clk = 1;
    dut->eval();
    trace->dump(sim_time);
    ref_dff8_q = 0x55;
    ref_reg8_en_q = 0x55;
    check_dut();

    std::cout << "\n==== Test2: CLK stay HIGH (clk=1), modify d & en many times ====\n";
    // 此后不再翻转时钟，一直保持 clk=1
    for (int i = 0; i < 5; i++)
    {
        sim_time += CLK_HALF_PERIOD;
        dut->d = static_cast<uint8_t>(0x30 + i * 0x07);
        dut->en = (i & 1U) ? 1 : 0;
        dut->eval();
        trace->dump(sim_time);
        check_dut();
        std::printf("[%4luns] clk=%d en=%d d=0x%02X | dut dff8_q=0x%02X reg8_en_q=0x%02X\n",
                    sim_time, (int)dut->clk, (int)dut->en, (int)dut->d,
                    (int)dut->dff8_q, (int)dut->reg8_en_q);
    }

    // ==============================
    // 确定序列测试：连续两拍禁用使能，再重新使能
    // ==============================
    std::cout << "\n==== Deterministic sequence: two cycles en=0, then en=1 ====\n";
    // cycle A: en=1, d=0xAA → sample
    sim_time += CLK_HALF_PERIOD;
    dut->clk = 0;
    dut->d = 0xAA;
    dut->en = 1;
    dut->eval();
    trace->dump(sim_time);
    sim_time += CLK_HALF_PERIOD;
    dut->clk = 1;
    dut->eval();
    trace->dump(sim_time);
    ref_dff8_q = 0xAA;
    if (1)
        ref_reg8_en_q = 0xAA;
    check_dut();

    // cycle B: en=0, d=0xBB → reg8_en hold
    sim_time += CLK_HALF_PERIOD;
    dut->clk = 0;
    dut->d = 0xBB;
    dut->en = 0;
    dut->eval();
    trace->dump(sim_time);
    sim_time += CLK_HALF_PERIOD;
    dut->clk = 1;
    dut->eval();
    trace->dump(sim_time);
    ref_dff8_q = 0xBB;
    check_dut();

    // cycle C: en=0, d=0xCC → reg8_en hold
    sim_time += CLK_HALF_PERIOD;
    dut->clk = 0;
    dut->d = 0xCC;
    dut->en = 0;
    dut->eval();
    trace->dump(sim_time);
    sim_time += CLK_HALF_PERIOD;
    dut->clk = 1;
    dut->eval();
    trace->dump(sim_time);
    ref_dff8_q = 0xCC;
    check_dut();

    // cycle D: en=1, d=0xDD → reg8_en update
    sim_time += CLK_HALF_PERIOD;
    dut->clk = 0;
    dut->d = 0xDD;
    dut->en = 1;
    dut->eval();
    trace->dump(sim_time);
    sim_time += CLK_HALF_PERIOD;
    dut->clk = 1;
    dut->eval();
    trace->dump(sim_time);
    ref_dff8_q = 0xDD;
    if (1)
        ref_reg8_en_q = 0xDD;
    check_dut();
    std::cout << "Deterministic sequence done.\n";

    // ==============================
    // Test3：固定种子随机测试，120周期；低电平先建立数据，再上升沿
    // ==============================
    std::cout << "\n==== Test3: Random test, 120 clock cycles ====\n";
    std::mt19937 rng(0x12345678U);
    std::uniform_int_distribution<> dist_d(0, 255);
    std::uniform_int_distribution<> dist_en(0, 1);
    const int CYCLE_TOTAL = 120;

    for (int cycle = 0; cycle < CYCLE_TOTAL; cycle++)
    {
        // 低电平阶段：先设置激励、eval、dump、检查保持
        sim_time += CLK_HALF_PERIOD;
        dut->clk = 0;
        uint8_t rand_d = static_cast<uint8_t>(dist_d(rng));
        uint8_t rand_en = static_cast<uint8_t>(dist_en(rng));
        dut->d = rand_d;
        dut->en = rand_en;
        dut->eval();
        trace->dump(sim_time);
        check_dut();

        // 上升沿
        sim_time += CLK_HALF_PERIOD;
        dut->clk = 1;
        dut->eval();
        trace->dump(sim_time);

        // 更新参考模型
        ref_dff8_q = rand_d;
        if (rand_en)
        {
            ref_reg8_en_q = rand_d;
        }
        check_dut();

        if (cycle % 10 == 0)
        {
            std::printf("[%4luns] cycle=%3d en=%d d=0x%02X | dut dff8=0x%02X reg8=0x%02X ref dff8=0x%02X reg8=0x%02X\n",
                        sim_time, cycle, (int)dut->en, (int)dut->d,
                        (int)dut->dff8_q, (int)dut->reg8_en_q,
                        (int)ref_dff8_q, (int)ref_reg8_en_q);
        }
    }

    std::cout << "\n==== Random test finish. total cycles:" << CYCLE_TOTAL
              << " error count:" << error_cnt << " ====\n";

    trace->flush();
    trace->close();
    delete trace;
    dut->final();
    delete dut;

    if (error_cnt > 0)
    {
        std::cerr << "Simulation FAILED, total " << error_cnt << " mismatch(es)!\n";
        return EXIT_FAILURE;
    }
    else
    {
        std::cout << "All tests PASS! wave.vcd generated.\n";
        return EXIT_SUCCESS;
    }
}
