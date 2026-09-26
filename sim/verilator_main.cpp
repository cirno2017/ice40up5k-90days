#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <verilated.h>
#include <verilated_vcd_c.h>
#include "Vtop.h"

vluint64_t sim_time = 0;

static void step(Vtop *dut, VerilatedVcdC *trace)
{
    dut->eval();
    trace->dump(sim_time);
    sim_time++;
}

int main(int argc, char **argv)
{
    Verilated::commandArgs(argc, argv);
    Vtop *dut = new Vtop;

    Verilated::traceEverOn(true);
    VerilatedVcdC *trace = new VerilatedVcdC;
    dut->trace(trace, 99);
    trace->open("wave.vcd");

    uint8_t ref_nba_q1 = 0;
    uint8_t ref_nba_q2 = 0;
    uint8_t ref_blk_q1 = 0;
    uint8_t ref_blk_q2 = 0;

    auto check = [&](vluint64_t t,
                     uint8_t exp_nba_q1, uint8_t exp_nba_q2,
                     uint8_t exp_blk_q1, uint8_t exp_blk_q2) -> bool
    {
        uint8_t act_nba_q1 = dut->non_blocking_assignment_q1;
        uint8_t act_nba_q2 = dut->non_blocking_assignment_q2;
        uint8_t act_blk_q1 = dut->blocking_assignment_q1;
        uint8_t act_blk_q2 = dut->blocking_assignment_q2;

        bool ok = true;
        if (act_nba_q1 != exp_nba_q1)
        {
            std::cerr << "TIME[" << t << "] FAIL nba_q1: exp 0x" << std::hex << (int)exp_nba_q1
                      << " act 0x" << (int)act_nba_q1 << std::dec << "\n";
            ok = false;
        }
        if (act_nba_q2 != exp_nba_q2)
        {
            std::cerr << "TIME[" << t << "] FAIL nba_q2: exp 0x" << std::hex << (int)exp_nba_q2
                      << " act 0x" << (int)act_nba_q2 << std::dec << "\n";
            ok = false;
        }
        if (act_blk_q1 != exp_blk_q1)
        {
            std::cerr << "TIME[" << t << "] FAIL blk_q1: exp 0x" << std::hex << (int)exp_blk_q1
                      << " act 0x" << (int)act_blk_q1 << std::dec << "\n";
            ok = false;
        }
        if (act_blk_q2 != exp_blk_q2)
        {
            std::cerr << "TIME[" << t << "] FAIL blk_q2: exp 0x" << std::hex << (int)exp_blk_q2
                      << " act 0x" << (int)act_blk_q2 << std::dec << "\n";
            ok = false;
        }
        return ok;
    };

    // 预热：先拉低时钟，d=0，跑两个完整时钟周期建立已知状态
    dut->clk = 0;
    dut->d = 0;
    step(dut, trace);

    for (int i = 0; i < 2; ++i)
    {
        dut->clk = 1;
        step(dut, trace);
        uint8_t sampled_d = dut->d;
        const uint8_t old_q1 = ref_nba_q1;
        ref_nba_q1 = sampled_d;
        ref_nba_q2 = old_q1;
        ref_blk_q1 = sampled_d;
        ref_blk_q2 = sampled_d;

        dut->clk = 0;
        step(dut, trace);
    }
    std::cout << "==== finish two warm‑up cycles, enter formal test ====\n";

    if (!check(sim_time - 1, ref_nba_q1, ref_nba_q2, ref_blk_q1, ref_blk_q2))
    {
        std::cerr << "WARM‑UP CHECK FAILED\n";
        goto cleanup_fail;
    }

    {
        // 确定序列测试：0x12,0xA5,0x3C,0x00,0xFF,0x80,0x01
        const uint8_t seq[] = {0x12, 0xA5, 0x3C, 0x00, 0xFF, 0x80, 0x01};
        const int seq_len = sizeof(seq) / sizeof(seq[0]);
        std::cout << "\n---- Deterministic sequence test ----\n";

        for (int idx = 0; idx < seq_len; idx++)
        {
            // 1. 时钟低电平设置d
            dut->clk = 0;
            dut->d = seq[idx];
            step(dut, trace);
            if (!check(sim_time - 1, ref_nba_q1, ref_nba_q2, ref_blk_q1, ref_blk_q2))
            {
                std::cerr << "Deterministic: check fail after set d(clk low), idx=" << idx << "\n";
                goto cleanup_fail;
            }

            // 2. 上升沿
            dut->clk = 1;
            step(dut, trace);
            uint8_t sampled_d = dut->d;
            const uint8_t old_q1 = ref_nba_q1;
            ref_nba_q1 = sampled_d;
            ref_nba_q2 = old_q1;
            ref_blk_q1 = sampled_d;
            ref_blk_q2 = sampled_d;

            if (!check(sim_time - 1, ref_nba_q1, ref_nba_q2, ref_blk_q1, ref_blk_q2))
            {
                std::cerr << "Deterministic: check fail at posedge, idx=" << idx << "\n";
                goto cleanup_fail;
            }

            // 3. 高电平期间修改d，输出应当保持不变
            dut->d = 0x55;
            step(dut, trace);
            if (!check(sim_time - 1, ref_nba_q1, ref_nba_q2, ref_blk_q1, ref_blk_q2))
            {
                std::cerr << "Deterministic: hold check fail during clk high, idx=" << idx << "\n";
                goto cleanup_fail;
            }

            // 4. 拉低时钟，下降沿之后输出保持
            dut->clk = 0;
            step(dut, trace);
            if (!check(sim_time - 1, ref_nba_q1, ref_nba_q2, ref_blk_q1, ref_blk_q2))
            {
                std::cerr << "Deterministic: hold check fail after negedge, idx=" << idx << "\n";
                goto cleanup_fail;
            }
        }
        std::cout << "Deterministic sequence test PASS\n";

        // 随机测试，固定种子，120周期
        std::cout << "\n---- Random test (seed=1234, 120 cycles) ----\n";
        srand(1234U);
        for (int cyc = 0; cyc < 120; cyc++)
        {
            // 1. clk low 设置随机d
            dut->clk = 0;
            dut->d = static_cast<uint8_t>(rand() & 0xFFU);
            step(dut, trace);
            if (!check(sim_time - 1, ref_nba_q1, ref_nba_q2, ref_blk_q1, ref_blk_q2))
            {
                std::cerr << "Random: check fail after set d(clk low), cycle=" << cyc << "\n";
                goto cleanup_fail;
            }

            // 2. 上升沿
            dut->clk = 1;
            step(dut, trace);
            uint8_t sampled_d = dut->d;
            const uint8_t old_q1 = ref_nba_q1;
            ref_nba_q1 = sampled_d;
            ref_nba_q2 = old_q1;
            ref_blk_q1 = sampled_d;
            ref_blk_q2 = sampled_d;

            if (!check(sim_time - 1, ref_nba_q1, ref_nba_q2, ref_blk_q1, ref_blk_q2))
            {
                std::cerr << "Random: check fail at posedge, cycle=" << cyc << "\n";
                goto cleanup_fail;
            }

            // 3. clk high 修改d，输出保持
            dut->d = static_cast<uint8_t>(rand() & 0xFFU);
            step(dut, trace);
            if (!check(sim_time - 1, ref_nba_q1, ref_nba_q2, ref_blk_q1, ref_blk_q2))
            {
                std::cerr << "Random: hold check fail during clk high, cycle=" << cyc << "\n";
                goto cleanup_fail;
            }

            // 4. 拉低时钟，下降沿后保持
            dut->clk = 0;
            step(dut, trace);
            if (!check(sim_time - 1, ref_nba_q1, ref_nba_q2, ref_blk_q1, ref_blk_q2))
            {
                std::cerr << "Random: hold check fail after negedge, cycle=" << cyc << "\n";
                goto cleanup_fail;
            }
        }
        std::cout << "Random 120‑cycle test PASS\n";
    }

    std::cout << "\n==== ALL TESTS PASSED ====\n";
    trace->close();
    delete trace;
    delete dut;
    return EXIT_SUCCESS;

cleanup_fail:
    trace->close();
    delete trace;
    delete dut;
    return EXIT_FAILURE;
}
