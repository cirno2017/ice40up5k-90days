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
    contextp->traceEverOn(true);

    Vtop *top = new Vtop{contextp};
    VerilatedVcdC *vcd = new VerilatedVcdC;
    top->trace(vcd, 99);
    vcd->open("wave.vcd");

    // 4组测试用例 N=1,2,10,256
    RefPair refs[4] = {
        {1, 0, 0},
        {2, 0, 0},
        {10, 0, 0},
        {256, 0, 0}};

    bool error = false;
    constexpr uint64_t MAX_CYCLES = 700;
    srand(42);

    // 初始状态
    top->clk = 0;
    top->rst = 1;
    top->en = 0;
    top->eval();
    vcd->dump(contextp->time());

    for (uint64_t cycle = 0; cycle < MAX_CYCLES; cycle++)
    {
        // ========== 阶段1：clk=0 低电平，更新激励 ==========
        top->clk = 0;
        if (cycle < 3)
        {
            top->rst = 1;
            top->en = 0;
        }
        else if (cycle >= 3 && cycle < 520)
        {
            top->rst = 0;
            top->en = 1;
        }
        else if (cycle >= 520 && cycle < 523)
        {
            top->rst = 0;
            top->en = 0;
        }
        else if (cycle == 523)
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
        else if (cycle >= 650)
        {
            top->rst = rand() & 1;
            top->en = rand() & 1;
        }
        contextp->timeInc(1);
        top->eval();
        vcd->dump(contextp->time()); // ✅ 时间推进后，仅一次dump

        // ========== 阶段2：clk上升沿 ==========
        top->clk = 1;
        contextp->timeInc(1);
        top->eval();
        vcd->dump(contextp->time()); // ✅ 时间推进后，仅一次dump

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
                printf("ERROR time=%lu N=%u MOD: ref=%u dut=%u\n",
                       (unsigned long)contextp->time(), r.n, r.ref_mod, dut_mod[i]);
                error = true;
            }
            if (dut_sat[i] != r.ref_sat)
            {
                printf("ERROR time=%lu N=%u SAT: ref=%u dut=%u\n",
                       (unsigned long)contextp->time(), r.n, r.ref_sat, dut_sat[i]);
                error = true;
            }
        }

        // ========== 阶段3：clk=1高电平 ==========
        top->eval();
        // ❗这里不再dump！没有timeInc，时间不变，避免重复dump警告

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
                printf("ERROR[HIGH] time=%lu N=%u MOD ref=%u dut=%u\n",
                       (unsigned long)contextp->time(), r.n, r.ref_mod, dut_mod[i]);
                error = true;
            }
            if (dut_sat[i] != r.ref_sat)
            {
                printf("ERROR[HIGH] time=%lu N=%u SAT ref=%u dut=%u\n",
                       (unsigned long)contextp->time(), r.n, r.ref_sat, dut_sat[i]);
                error = true;
            }
        }

        // ========== 阶段4：clk下降沿 ==========
        top->clk = 0;
        contextp->timeInc(1);
        top->eval();
        vcd->dump(contextp->time()); // ✅ 时间推进后，仅一次dump

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
                printf("ERROR[FALL] time=%lu N=%u MOD ref=%u dut=%u\n",
                       (unsigned long)contextp->time(), r.n, r.ref_mod, dut_mod[i]);
                error = true;
            }
            if (dut_sat[i] != r.ref_sat)
            {
                printf("ERROR[FALL] time=%lu N=%u SAT ref=%u dut=%u\n",
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
