#include "Vtop.h"
#include "verilated.h"
#include "verilated_vcd_c.h"
#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <cstdlib>

struct RefPair
{
    uint32_t n;
    uint32_t ref_mod;
    uint32_t ref_sat;
};

int main(int argc, char **argv)
{
    VerilatedContext *contextp = new VerilatedContext;
    contextp->commandArgs(argc, argv);
    // ✅ 必须在实例化DUT、VCD之前开启traceEverOn
    contextp->traceEverOn(true);

    Vtop *top = new Vtop{contextp};
    VerilatedVcdC *vcd = new VerilatedVcdC;
    top->trace(vcd, 99);
    vcd->open("wave.vcd");

    RefPair refs[4] = {
        {1, 0, 0},
        {2, 0, 0},
        {10, 0, 0},
        {256, 0, 0}};
    bool error = false;
    constexpr uint64_t MAX_CYCLES = 850;
    srand(42);

    top->clk = 0;
    top->rst = 1;
    top->en = 0;
    top->eval();
    vcd->dump(0);

    for (uint64_t cycle = 0; cycle < MAX_CYCLES; cycle++)
    {
        // ========== 阶段1：clk=0 低电平，更新激励 ==========
        top->clk = 0;
        if (cycle < 3)
        {
            top->rst = 1;
            top->en = 0;
        }
        else if (cycle >= 3 && cycle < 523)
        {
            top->rst = 0;
            top->en = 1;
        }
        else if (cycle >= 523 && cycle < 526)
        {
            // N=10 末值暂停3拍
            top->rst = 0;
            top->en = 0;
        }
        else if (cycle == 526)
        {
            top->rst = 0;
            top->en = 1;
        }
        else if (cycle >= 550 && cycle < 553)
        {
            // N=256 末值暂停3拍
            top->rst = 0;
            top->en = 0;
        }
        else if (cycle == 553)
        {
            top->rst = 0;
            top->en = 1;
        }
        else if (cycle == 600)
        {
            top->rst = 1;
            top->en = 1;
        }
        else if (cycle == 602)
        {
            top->rst = 0;
            top->en = 1;
        }
        else if (cycle >= 700 && cycle < 800)
        {
            // 随机测试100周期
            top->rst = rand() & 1;
            top->en = rand() & 1;
        }
        else
        {
            top->rst = 0;
            top->en = 1;
        }

        contextp->timeInc(1);
        top->eval();
        vcd->dump(contextp->time());

        // ========== 低电平比对，cycle>0，跳过初始复位 ==========
        if (cycle > 0)
        {
            uint32_t dut_mod[4], dut_sat[4];
            dut_mod[0] = top->mod1_out;
            dut_sat[0] = top->sat1_out;
            dut_mod[1] = top->mod2_out;
            dut_sat[1] = top->sat2_out;
            dut_mod[2] = top->mod10_out;
            dut_sat[2] = top->sat10_out;
            dut_mod[3] = top->mod256_out;
            dut_sat[3] = top->sat256_out;
            for (int i = 0; i < 4; i++)
            {
                auto &r = refs[i];
                if (dut_mod[i] != r.ref_mod)
                {
                    printf("[LOW] time=%lu N=%u MOD: ref=%u dut=%u\n",
                           (unsigned long)contextp->time(), r.n, r.ref_mod, dut_mod[i]);
                    error = true;
                }
                if (dut_sat[i] != r.ref_sat)
                {
                    printf("[LOW] time=%lu N=%u SAT: ref=%u dut=%u\n",
                           (unsigned long)contextp->time(), r.n, r.ref_sat, dut_sat[i]);
                    error = true;
                }
            }
        }

        // ========== 阶段2：上升沿 ==========
        top->clk = 1;
        contextp->timeInc(1);
        top->eval();
        vcd->dump(contextp->time());

        bool rst_sample = (top->rst != 0);
        bool en_sample = (top->en != 0);
        for (int i = 0; i < 4; i++)
        {
            auto &r = refs[i];
            if (rst_sample)
            {
                r.ref_mod = 0;
                r.ref_sat = 0;
            }
            else if (en_sample)
            {
                r.ref_mod = (r.ref_mod + 1u) % r.n;
                r.ref_sat = std::min(r.ref_sat + 1u, r.n - 1u);
            }
        }

        uint32_t dut_mod[4], dut_sat[4];
        dut_mod[0] = top->mod1_out;
        dut_sat[0] = top->sat1_out;
        dut_mod[1] = top->mod2_out;
        dut_sat[1] = top->sat2_out;
        dut_mod[2] = top->mod10_out;
        dut_sat[2] = top->sat10_out;
        dut_mod[3] = top->mod256_out;
        dut_sat[3] = top->sat256_out;
        for (int i = 0; i < 4; i++)
        {
            auto &r = refs[i];
            if (dut_mod[i] != r.ref_mod)
            {
                printf("[RISE] time=%lu N=%u MOD: ref=%u dut=%u\n",
                       (unsigned long)contextp->time(), r.n, r.ref_mod, dut_mod[i]);
                error = true;
            }
            if (dut_sat[i] != r.ref_sat)
            {
                printf("[RISE] time=%lu N=%u SAT: ref=%u dut=%u\n",
                       (unsigned long)contextp->time(), r.n, r.ref_sat, dut_sat[i]);
                error = true;
            }
        }

        // ========== 阶段3：高电平修改rst/en，检查输出保持 ==========
        uint8_t old_rst = top->rst;
        uint8_t old_en = top->en;
        top->rst = rand() & 1;
        top->en = rand() & 1;
        top->eval();
        dut_mod[0] = top->mod1_out;
        dut_sat[0] = top->sat1_out;
        dut_mod[1] = top->mod2_out;
        dut_sat[1] = top->sat2_out;
        dut_mod[2] = top->mod10_out;
        dut_sat[2] = top->sat10_out;
        dut_mod[3] = top->mod256_out;
        dut_sat[3] = top->sat256_out;
        for (int i = 0; i < 4; i++)
        {
            auto &r = refs[i];
            if (dut_mod[i] != r.ref_mod)
            {
                printf("[HIGH_CHG] time=%lu N=%u MOD: ref=%u dut=%u\n",
                       (unsigned long)contextp->time(), r.n, r.ref_mod, dut_mod[i]);
                error = true;
            }
            if (dut_sat[i] != r.ref_sat)
            {
                printf("[HIGH_CHG] time=%lu N=%u SAT: ref=%u dut=%u\n",
                       (unsigned long)contextp->time(), r.n, r.ref_sat, dut_sat[i]);
                error = true;
            }
        }
        // 恢复原来控制信号
        top->rst = old_rst;
        top->en = old_en;
        top->eval();

        // ========== 阶段4：下降沿 ==========
        top->clk = 0;
        contextp->timeInc(1);
        top->eval();
        vcd->dump(contextp->time());
        dut_mod[0] = top->mod1_out;
        dut_sat[0] = top->sat1_out;
        dut_mod[1] = top->mod2_out;
        dut_sat[1] = top->sat2_out;
        dut_mod[2] = top->mod10_out;
        dut_sat[2] = top->sat10_out;
        dut_mod[3] = top->mod256_out;
        dut_sat[3] = top->sat256_out;
        for (int i = 0; i < 4; i++)
        {
            auto &r = refs[i];
            if (dut_mod[i] != r.ref_mod)
            {
                printf("[FALL] time=%lu N=%u MOD: ref=%u dut=%u\n",
                       (unsigned long)contextp->time(), r.n, r.ref_mod, dut_mod[i]);
                error = true;
            }
            if (dut_sat[i] != r.ref_sat)
            {
                printf("[FALL] time=%lu N=%u SAT: ref=%u dut=%u\n",
                       (unsigned long)contextp->time(), r.n, r.ref_sat, dut_sat[i]);
                error = true;
            }
        }
    }

    vcd->close();
    delete vcd;
    top->final();
    delete top;
    delete contextp;

    if (error)
    {
        printf("\n==== SIMULATION FAILED ====\n");
        return 1;
    }
    else
    {
        printf("\n==== ALL TEST PASSED ====\n");
        return 0;
    }
}
