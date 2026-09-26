#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <verilated.h>
#include <verilated_vcd_c.h>
#include "Vtop.h"

vluint64_t sim_time = 0;
const vluint64_t CLK_PERIOD = 84; // 12MHz 时钟周期 ≈83.33ns

int main(int argc, char **argv)
{
    Verilated::commandArgs(argc, argv);
    Vtop *dut = new Vtop;

    VerilatedVcdC *vcd = new VerilatedVcdC;
    Verilated::traceEverOn(true);
    dut->trace(vcd, 99);
    vcd->open("wave.vcd");

    uint8_t expected = 0;

    //========== 初始化流程 ==========
    dut->clk = 0;
    dut->rst = 1;
    dut->clear = 0;
    dut->en = 0;
    dut->eval();
    vcd->dump(sim_time);

    // 第一个上升沿
    sim_time += CLK_PERIOD / 2;
    dut->clk = 1;
    dut->eval();
    vcd->dump(sim_time);

    if (static_cast<uint8_t>(dut->count) != 0U)
    {
        std::cerr << "ERROR t=" << sim_time
                  << ": After first reset posedge, expect 0, got "
                  << static_cast<int>(dut->count) << std::endl;
        return 1;
    }

    // 下降沿，在低电平阶段释放 rst
    sim_time += CLK_PERIOD / 2;
    dut->clk = 0;
    dut->rst = 0;
    dut->eval();
    vcd->dump(sim_time);

    //=====================================================
    // clock_tick：完整一个时钟周期
    // 输入在clk低电平设置；上升沿更新DUT与参考模型
    //=====================================================
    auto clock_tick = [&](uint8_t rst_samp, uint8_t clear_samp, uint8_t en_samp)
    {
        // 低电平更新输入
        dut->clk = 0;
        dut->rst = rst_samp;
        dut->clear = clear_samp;
        dut->en = en_samp;
        dut->eval();
        sim_time += CLK_PERIOD / 2;
        vcd->dump(sim_time);

        // 上升沿
        dut->clk = 1;
        dut->eval();
        sim_time += CLK_PERIOD / 2;
        vcd->dump(sim_time);

        // C++参考模型：仅上升沿更新
        if (rst_samp || clear_samp)
        {
            expected = 0U;
        }
        else if (en_samp)
        {
            expected = static_cast<uint8_t>(expected + 1U);
        }
        // else:保持不变

        // 比对
        uint8_t dut_cnt = static_cast<uint8_t>(dut->count);
        if (dut_cnt != expected)
        {
            std::cerr << "FAIL t=" << sim_time
                      << " RTL=" << static_cast<int>(dut_cnt)
                      << " EXP=" << static_cast<int>(expected)
                      << " rst=" << static_cast<int>(rst_samp)
                      << " clear=" << static_cast<int>(clear_samp)
                      << " en=" << static_cast<int>(en_samp)
                      << std::endl;
            exit(EXIT_FAILURE);
        }
    };

    std::cout << "\n==== Test1: Normal count ====\n";
    expected = 0U;
    for (int i = 0; i < 5; ++i)
    {
        clock_tick(0, 0, 1);
    }

    std::cout << "\n==== Test2: Pause, hold value ====\n";
    uint8_t hold_val = expected;
    for (int i = 0; i < 3; ++i)
    {
        clock_tick(0, 0, 0);
        if (static_cast<uint8_t>(dut->count) != hold_val)
        {
            std::cerr << "ERROR: Pause hold failed\n";
            exit(EXIT_FAILURE);
        }
    }

    std::cout << "\n==== Test3: clear & en simultaneous (non‑zero) ====\n";
    clock_tick(0, 0, 1);
    clock_tick(0, 1, 1);

    std::cout << "\n==== Test4: rst & en simultaneous ====\n";
    clock_tick(0, 0, 1);
    clock_tick(1, 0, 1);

    std::cout << "\n==== Test5: rst & clear & en all 1 ====\n";
    clock_tick(0, 0, 1);
    clock_tick(1, 1, 1);

    std::cout << "\n==== Test6: Run‑time reset (assert between edges) ====\n";
    for (int i = 0; i < 5; ++i)
    {
        clock_tick(0, 0, 1);
    }
    // clk低电平，边沿之间拉高rst，不产生上升沿
    dut->rst = 1;
    dut->eval();
    sim_time += CLK_PERIOD / 4;
    vcd->dump(sim_time);
    if (static_cast<uint8_t>(dut->count) != expected)
    {
        std::cerr << "ERROR: sync reset took effect without posedge!\n";
        exit(EXIT_FAILURE);
    }
    clock_tick(1, 0, 0);

    std::cout << "\n==== Test7: Short reset pulse between edges (no clear) ====\n";
    for (int i = 0; i < 7; ++i)
    {
        clock_tick(0, 0, 1);
    }
    dut->clk = 0;
    dut->rst = 1;
    dut->eval();
    sim_time += CLK_PERIOD / 8;
    vcd->dump(sim_time);
    dut->rst = 0;
    dut->eval();
    sim_time += CLK_PERIOD / 8;
    vcd->dump(sim_time);
    if (static_cast<uint8_t>(dut->count) != expected)
    {
        std::cerr << "FAIL: inter‑edge rst pulse incorrectly cleared counter\n";
        exit(EXIT_FAILURE);
    }
    clock_tick(0, 0, 1);

    std::cout << "\n==== Test8: Persistent reset multi‑cycle ====\n";
    for (int i = 0; i < 4; ++i)
    {
        clock_tick(1, 0, 1);
        if (static_cast<uint8_t>(dut->count) != 0U)
        {
            std::cerr << "ERROR: persistent reset fail\n";
            exit(EXIT_FAILURE);
        }
    }

    std::cout << "\n==== Test9: Reset release then resume count ====\n";
    clock_tick(1, 0, 0);
    clock_tick(0, 0, 1);
    clock_tick(0, 0, 1);

    std::cout << "\n==== Test10: 0xFF wrap‑around to 0x00 ====\n";
    while (expected != 0xFFU)
    {
        clock_tick(0, 0, 1);
    }
    clock_tick(0, 0, 1);

    std::cout << "\n==== Test11: Random test 100 cycles (seed=42) ====\n";
    srand(42);
    for (int i = 0; i < 100; i++)
    {
        uint8_t r_rst = static_cast<uint8_t>(rand() & 1);
        uint8_t r_clear = static_cast<uint8_t>(rand() & 1);
        uint8_t r_en = static_cast<uint8_t>(rand() & 1);
        clock_tick(r_rst, r_clear, r_en);
    }

    std::cout << "\n>>> ALL TEST PASSED <<<\n";

    vcd->close();
    delete vcd;
    delete dut;
    return EXIT_SUCCESS;
}
