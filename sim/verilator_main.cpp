#include "Vtop.h"
#include "verilated.h"
#include "verilated_vcd_c.h"
#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <cstdlib>

// 故障注入开关，打开后mod10会在count=8提前回0，用于验证自检能抓到错误
// #define FAULT_INJECT

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

    // 4组待测参数 N=1,2,10,256
    RefPair refs[4] = {
        {1, 0, 0},
        {2, 0, 0},
        {10, 0, 0},
        {256, 0, 0}};

    top->clk = 0;
    top->rst = 1;
    top->en = 0;
    top->eval();
    vcd->dump(contextp->time());

    bool error = false;
    constexpr uint64_t MAX_CYCLES = 520 + 100; // 确定性520 + 随机100周期
    srand(42);                                 // 固定种子，可复现随机测试

    for (uint64_t cycle = 0; cycle < MAX_CYCLES; cycle++)
    {
        // ========== 施加激励 ==========
        if (cycle < 3)
        {
            // 初始同步复位阶段
            top->rst = 1;
            top->en = 0;
        }
        else if (cycle < 520)
        {
            // 确定性测试阶段
            if (cycle == 100)
            {
                // 到达末值后 en=0，保持3拍
                top->en = 0;
            }
            else if (cycle == 103)
            {
                // 恢复使能
                top->en = 1;
            }
            else if (cycle == 200)
            {
                // 复位与使能冲突：rst=en同时高
                top->rst = 1;
                top->en = 1;
            }
            else if (cycle == 202)
            {
                top->rst = 0;
                top->en = 1;
            }
            else
            {
                top->rst = 0;
                top->en = 1;
            }
        }
        else
        {
            // 固定种子随机测试：cycle >=520
            top->rst = rand() & 1;
            top->en = rand() & 1;
        }

        // ========== 时钟上升沿 ==========
        top->clk = 1;
        contextp->timeInc(1);
        top->eval();
        vcd->dump(contextp->time());

        // ----只有上升沿更新C++参考模型----
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
#ifdef FAULT_INJECT
                // 故障注入：N=10，ref不变，但DUT RTL人为bug，C++参考依然正确
                if (r.n == 10 && r.ref_mod == 8)
                {
                    r.ref_mod = (r.ref_mod + 1u) % r.n;
                }
                else
                {
                    r.ref_mod = (r.ref_mod + 1u) % r.n;
                }
#else
                r.ref_mod = (r.ref_mod + 1u) % r.n;
#endif
                r.ref_sat = std::min(r.ref_sat + 1u, r.n - 1u);
            }
        }

        // ===========读取DUT输出，和参考模型比对===========
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

        // ==========时钟下降沿，组合逻辑更新，再次检查保持=========
        top->clk = 0;
        contextp->timeInc(1);
        top->eval();
        vcd->dump(contextp->time());

        // 下降沿再次读取输出，必须和参考模型保持不变
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
            // 修复：去掉 r.ref_sat[i] 的 [i]，r是单个结构体
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
        return 1; // 非零退出码，Make检测失败
    }
    else
    {
        printf("\n==== ALL TEST PASSED ====\n");
        return 0;
    }
}
