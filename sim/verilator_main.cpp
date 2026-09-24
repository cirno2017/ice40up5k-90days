//

#include <iostream>
#include <cstdlib>
#include <cstdint>
#include <random>
#include "verilated.h"
#include "verilated_vcd_c.h"
#include "Vtop.h"

vluint64_t sim_time = 0;
constexpr vluint64_t CLK_HALF_PERIOD = 10;

// 软件参考模型变量，模拟dff8、reg8_en行为
uint8_t ref_dff8_q = 0U;
uint8_t ref_reg8_en_q = 0U;

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Vtop* dut = new Vtop;

    Verilated::traceEverOn(true);
    VerilatedVcdC* trace = new VerilatedVcdC;
    dut->trace(trace, 99);
    trace->open("wave.vcd");

    // 初始化DUT输入
    dut->clk = 0;
    dut->en  = 0;
    dut->d   = 0x00;
    dut->eval();
    trace->dump(sim_time);

    // ==============================
    // 测试1：时钟停在低电平 clk=0
    // ==============================
    std::cout << "\n==== Test1: CLK stay LOW (clk=0), modify d many times ====\n";
    dut->clk = 0;
    dut->eval();
    trace->dump(sim_time);

    // clk保持低，多次改写d，输出应当不变
    for(int i = 0; i < 5; i++){
        sim_time += CLK_HALF_PERIOD;
        dut->d = static_cast<uint8_t>(0x10 + i*0x11);
        dut->eval();
        trace->dump(sim_time);
        std::printf("[%4luns] clk=%d d=0x%02X | dut dff8_q=0x%02X reg8_en_q=0x%02X\n",
            sim_time, (int)dut->clk, (int)dut->d,
            (int)dut->dff8_q, (int)dut->reg8_en_q);
    }

    // ==============================
    // 测试2：时钟停在高电平 clk=1
    // ==============================
    std::cout << "\n==== Test2: CLK stay HIGH (clk=1), modify d & en many times ====\n";
    dut->clk = 1;
    dut->eval();
    trace->dump(sim_time);

    for(int i = 0; i < 5; i++){
        sim_time += CLK_HALF_PERIOD;
        dut->d  = static_cast<uint8_t>(0x30 + i*0x07);
        dut->en = (i & 1U) ? 1 : 0; // 来回切换en
        dut->eval();
        trace->dump(sim_time);
        std::printf("[%4luns] clk=%d en=%d d=0x%02X | dut dff8_q=0x%02X reg8_en_q=0x%02X\n",
            sim_time, (int)dut->clk, (int)dut->en, (int)dut->d,
            (int)dut->dff8_q, (int)dut->reg8_en_q);
    }

    // ==============================
    // 测试3：固定种子随机测试，至少100个完整时钟周期
    // ==============================
    std::cout << "\n==== Test3: Random test, 120 clock cycles, compare with ref model ====\n";
    // 固定随机种子，每次运行随机序列完全一致
    std::mt19937 rng(0x12345678U);
    std::uniform_int_distribution<> dist_d(0, 255);
    std::uniform_int_distribution<> dist_en(0, 1);

    const int CYCLE_TOTAL = 120;
    int error_cnt = 0;

    for(int cycle = 0; cycle < CYCLE_TOTAL; cycle++)
    {
        // ---------- 时钟下降沿 (clk=0) ----------
        sim_time += CLK_HALF_PERIOD;
        dut->clk = 0;
        dut->eval();
        trace->dump(sim_time);

        // ---------- 时钟上升沿 (clk=1)：寄存器采样时刻，更新参考模型 ----------
        sim_time += CLK_HALF_PERIOD;
        dut->clk = 1;

        // 在上升沿前生成随机激励 d / en
        uint8_t rand_d  = static_cast<uint8_t>(dist_d(rng));
        uint8_t rand_en = static_cast<uint8_t>(dist_en(rng));
        dut->d  = rand_d;
        dut->en = rand_en;

        dut->eval();
        trace->dump(sim_time);

        // ========== 软件参考模型：仅上升沿更新 ==========
        ref_dff8_q      = rand_d;                     // dff8：无条件采样d
        if(rand_en) ref_reg8_en_q = rand_d;           // reg8_en：en=1才采样d，否则保持

        // ========== DUT和参考模型比对 ==========
        bool err1 = (dut->dff8_q != ref_dff8_q);
        bool err2 = (dut->reg8_en_q != ref_reg8_en_q);

        if(err1 || err2)
        {
            error_cnt ++;
            std::cerr << "[ERROR] Cycle:" << cycle
                      << " dut(dff8=0x" << std::hex << (int)dut->dff8_q
                      << ", reg8=0x" << (int)dut->reg8_en_q << ")"
                      << " ref(dff8=0x" << (int)ref_dff8_q
                      << ", reg8=0x" << (int)ref_reg8_en_q << ")\n";
        }

        if(cycle % 10 == 0){
            std::printf("[%4luns] cycle=%3d en=%d d=0x%02X | dut dff8=0x%02X reg8=0x%02X ref dff8=0x%02X reg8=0x%02X\n",
                sim_time, cycle, (int)dut->en, (int)dut->d,
                (int)dut->dff8_q, (int)dut->reg8_en_q,
                (int)ref_dff8_q, (int)ref_reg8_en_q);
        }
    }

    std::cout << "\n==== Random test finish. total cycles:" << CYCLE_TOTAL
              << " error count:" << error_cnt << " ====\n";

    // 仿真收尾
    trace->flush();
    trace->close();
    delete trace;
    dut->final();
    delete dut;

    if(error_cnt > 0){
        std::cerr << "Simulation FAILED, there are " << error_cnt << " mismatch!\n";
        return EXIT_FAILURE;
    }else{
        std::cout << "All comparison PASS! wave.vcd generated.\n";
        return EXIT_SUCCESS;
    }
}






/*
#include <iostream>
#include <cstdlib>
#include "verilated.h"
#include "verilated_vcd_c.h"
#include "Vtop.h"

// 仿真时间单位，1ns一格
vluint64_t sim_time = 0;
// 时钟半周期，10ns半周期 => 20ns完整时钟周期
constexpr vluint64_t CLK_HALF_PERIOD = 10;

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);

    // 实例化DUT，顶层模块top生成C++类Vtop
    Vtop* dut = new Vtop;

    // ========== 开启VCD波形dump ==========
    Verilated::traceEverOn(true);
    VerilatedVcdC* trace = new VerilatedVcdC;
    dut->trace(trace, 99);   // 99代表追踪所有层级信号
    trace->open("wave.vcd"); // 和Makefile里VERILATOR_VCD=wave.vcd对应

    // 初始化输入
    dut->clk = 0;
    dut->en  = 0;
    dut->d   = 0x00;

    // 仿真循环，总仿真400ns
    while(sim_time <= 400)
    {
        // 翻转时钟
        dut->clk = !dut->clk;
        dut->eval();         // 评估硬件组合逻辑
        trace->dump(sim_time); // 记录当前时刻波形

        // 在时钟上升沿施加激励
        if(dut->clk == 1)
        {
            // 时间点设置输入激励
            switch(sim_time)
            {
                case  20: dut->d  = 0x11; dut->en = 0; break; // en=0，reg8_en不更新
                case  60: dut->d  = 0x22; dut->en = 1; break; // en=1，reg8_en更新
                case 100: dut->d  = 0x33; dut->en = 0; break; // en=0，保持
                case 140: dut->d  = 0x44; dut->en = 1; break;
                case 180: dut->d  = 0x55; dut->en = 0; break;
                case 220: dut->d  = 0x66; dut->en = 1; break;
                case 260: dut->d  = 0x77; dut->en = 0; break;
                case 300: dut->d  = 0x88; dut->en = 1; break;
            }
            // 打印调试信息
            std::printf("[%4luns] clk=%d en=%d d=0x%02X | dff8_q=0x%02X reg8_en_q=0x%02X\n",
                    sim_time,
                    (int)dut->clk,
                    (int)dut->en,
                    (int)dut->d,
                    (int)dut->dff8_q,
                    (int)dut->reg8_en_q);
        }

        sim_time += CLK_HALF_PERIOD;
    }

    // 收尾
    trace->flush();
    trace->close();
    delete trace;
    dut->final();
    delete dut;

    return EXIT_SUCCESS;
}
*/