#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <cassert>
#include <verilated.h>
#include <verilated_vcd_c.h>
#include "Vtop.h"

vluint64_t sim_time = 0;

// 每一步：设置输入 → eval → dump → time++
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

    // 开启VCD波形跟踪，Makefile已经带--trace
    Verilated::traceEverOn(true);
    VerilatedVcdC *trace = new VerilatedVcdC;
    dut->trace(trace, 99);
    trace->open("wave.vcd");

    // ========= 参考模型状态变量（完全独立，不从DUT读回） =========
    uint8_t ref_nba_q1 = 0; // 非阻塞参考 q1
    uint8_t ref_nba_q2 = 0; // 非阻塞参考 q2
    uint8_t ref_blk_q1 = 0; // 阻塞参考 q1
    uint8_t ref_blk_q2 = 0; // 阻塞参考 q2

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

    // ---------------- 初始化阶段：先置clk=0，d=0，跑两个完整周期建立已知全0状态 ----------------
    dut->clk = 0;
    dut->d = 0;
    step(dut, trace);

    // 执行2个完整时钟周期，把内部寄存器刷到0，正式测试从第3个上升沿开始
    for (int i = 0; i < 2; i++)
    {
        dut->clk = 1;
        step(dut, trace); // posedge
        // posedge更新参考模型
        uint8_t sampled_d = dut->d;
        // 非阻塞参考模型：先保存旧q1
        const uint8_t old_q1 = ref_nba_q1;
        ref_nba_q1 = sampled_d;
        ref_nba_q2 = old_q1;
        // 阻塞参考模型：q1 q2直接取采样d
        ref_blk_q1 = sampled_d;
        ref_blk_q2 = sampled_d;

        dut->clk = 0;
        step(dut, trace); // negedge
    }
    std::cout << "==== finish two warm‑up cycles, enter formal test ====\n";
    // 预热结束后，应当全部输出0
    if (!check(sim_time, ref_nba_q1, ref_nba_q2, ref_blk_q1, ref_blk_q2))
    {
        std::cerr << "WARM‑UP CHECK FAILED\n";
        goto cleanup_fail;
    }

    // 【关键修复】所有测试代码放进独立的大括号局部域，goto不会跨变量初始化
    {
        // ========== 测试1：确定序列测试，手算表格：12 A5 3C，追加00 FF 80 01 ==========
        const uint8_t seq[] = {0x12, 0xA5, 0x3C, 0x00, 0xFF, 0x80, 0x01};
        const int seq_len = sizeof(seq) / sizeof(seq[0]);
        std::cout << "\n---- Deterministic sequence test ----\n";
        for (int idx = 0; idx < seq_len; idx++)
        {
            // 【驱动顺序严格按照任务文档】
            // 1.低电平设置d
            dut->clk = 0;
            dut->d = seq[idx];
            step(dut, trace);

            // 2.拉高clk（上升沿，发生寄存器更新）
            dut->clk = 1;
            step(dut, trace);
            uint8_t sampled_d = dut->d;
            // 更新C++参考模型（上升沿时刻）
            const uint8_t old_q1 = ref_nba_q1;
            ref_nba_q1 = sampled_d;
            ref_nba_q2 = old_q1;

            ref_blk_q1 = sampled_d;
            ref_blk_q2 = sampled_d;

            // 上升沿之后检查输出
            if (!check(sim_time, ref_nba_q1, ref_nba_q2, ref_blk_q1, ref_blk_q2))
            {
                std::cerr << "Deterministic seq test fail idx=" << idx << "\n";
                goto cleanup_fail;
            }

            // 3.高电平期间改变d，检查输出保持不变（边沿之间不能变）
            dut->d = 0x55;
            step(dut, trace);
            if (!check(sim_time, ref_nba_q1, ref_nba_q2, ref_blk_q1, ref_blk_q2))
            {
                std::cerr << "Hold check fail during clk high\n";
                goto cleanup_fail;
            }

            // 4.拉低clk
            dut->clk = 0;
            step(dut, trace);
            // 下降沿之后，继续检查保持
            if (!check(sim_time, ref_nba_q1, ref_nba_q2, ref_blk_q1, ref_blk_q2))
            {
                std::cerr << "Hold check fail after negedge\n";
                goto cleanup_fail;
            }
        }
        std::cout << "Deterministic sequence test PASS\n";

        // ==========测试2：固定种子随机测试，1234固定种子，>=100周期 ==========
        std::cout << "\n---- Random test (seed=1234, 120 cycles) ----\n";
        srand(1234U);
        for (int cyc = 0; cyc < 120; cyc++)
        {
            dut->clk = 0;
            dut->d = static_cast<uint8_t>(rand() & 0xFFU);
            step(dut, trace);

            dut->clk = 1;
            step(dut, trace);
            uint8_t sampled_d = dut->d;
            const uint8_t old_q1 = ref_nba_q1;
            ref_nba_q1 = sampled_d;
            ref_nba_q2 = old_q1;
            ref_blk_q1 = sampled_d;
            ref_blk_q2 = sampled_d;

            if (!check(sim_time, ref_nba_q1, ref_nba_q2, ref_blk_q1, ref_blk_q2))
            {
                std::cerr << "Random test fail cycle=" << cyc << "\n";
                goto cleanup_fail;
            }

            dut->d = static_cast<uint8_t>(rand() & 0xFFU);
            step(dut, trace);

            dut->clk = 0;
            step(dut, trace);
        }
        std::cout << "Random 120‑cycle test PASS\n";
    } // 结束测试局部域

    std::cout << "\n==== ALL TESTS PASSED ====\n";

    // 全部测试通过，正常退出
    trace->close();
    delete trace;
    delete dut;
    return EXIT_SUCCESS;

cleanup_fail:
    // 测试失败跳转到此
    trace->close();
    delete trace;
    delete dut;
    return EXIT_FAILURE;
}
