#include "Vtop.h"
#include "verilated.h"
#include "verilated_vcd_c.h"
#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <cstdlib>

struct RefPair {
    uint32_t n;
    uint32_t ref_mod;
    uint32_t ref_sat;
};

void reset_refs(RefPair refs[4]) {
    refs[0] = {1,   0, 0};
    refs[1] = {2,   0, 0};
    refs[2] = {10,  0, 0};
    refs[3] = {256, 0, 0};
}

int main(int argc, char** argv) {
    VerilatedContext* contextp = new VerilatedContext;
    contextp->commandArgs(argc, argv);
    contextp->traceEverOn(true);

    Vtop* top = new Vtop{contextp};
    VerilatedVcdC* vcd = new VerilatedVcdC;
    top->trace(vcd, 99);
    vcd->open("wave.vcd");

    RefPair refs[4];
    reset_refs(refs);
    bool error = false;
    srand(42);

    top->clk = 0;
    top->rst = 1;
    top->en  = 0;
    top->eval();
    vcd->dump(0);

    // ====================== 第一大段：原有全局测试 ======================
    constexpr uint64_t GLOBAL_END = 799;
    for(uint64_t cycle = 0; cycle <= GLOBAL_END; cycle++) {
        // ---------- 低电平 phase clk=0 ----------
        top->clk = 0;
        if(cycle < 3) {
            top->rst = 1;
            top->en  = 0;
        } else if (cycle >=3 && cycle <= 522) {
            top->rst = 0;
            top->en  = 1;
        } else if (cycle == 600) {
            top->rst = 1;
            top->en  = 1;
        } else if (cycle == 602) {
            top->rst = 0;
            top->en  = 1;
        } else if (cycle >= 700 && cycle <= 799) {
            top->rst = rand() & 1;
            top->en  = rand() & 1;
        } else {
            top->rst = 0;
            top->en  = 1;
        }
        contextp->timeInc(1);
        top->eval();
        vcd->dump(contextp->time());

        // 低电平：仅做高电平保持检查（修改rst/en后输出不变），不更新ref、不校验寄存器
        uint8_t old_rst = top->rst;
        uint8_t old_en  = top->en;
        uint32_t dut_mod_before[4], dut_sat_before[4];
        dut_mod_before[0] = top->mod1_out;  dut_sat_before[0] = top->sat1_out;
        dut_mod_before[1] = top->mod2_out;  dut_sat_before[1] = top->sat2_out;
        dut_mod_before[2] = top->mod10_out; dut_sat_before[2] = top->sat10_out;
        dut_mod_before[3] = top->mod256_out;dut_sat_before[3] = top->sat256_out;

        top->rst = rand() & 1;
        top->en  = rand() & 1;
        top->eval();

        uint32_t dut_mod_after[4], dut_sat_after[4];
        dut_mod_after[0] = top->mod1_out;  dut_sat_after[0] = top->sat1_out;
        dut_mod_after[1] = top->mod2_out;  dut_sat_after[1] = top->sat2_out;
        dut_mod_after[2] = top->mod10_out; dut_sat_after[2] = top->sat10_out;
        dut_mod_after[3] = top->mod256_out;dut_sat_after[3] = top->sat256_out;

        for(int i=0;i<4;i++){
            if(dut_mod_before[i] != dut_mod_after[i]){
                printf("[HIGH_CHG] time=%lu N=%u MOD changed in clk low!\n",
                       (unsigned long)contextp->time(), refs[i].n);
                error = true;
            }
            if(dut_sat_before[i] != dut_sat_after[i]){
                printf("[HIGH_CHG] time=%lu N=%u SAT changed in clk low!\n",
                       (unsigned long)contextp->time(), refs[i].n);
                error = true;
            }
        }
        top->rst = old_rst;
        top->en  = old_en;
        top->eval();

        // ---------- 上升沿 phase clk=1 ----------
        top->clk = 1;
        contextp->timeInc(1);
        top->eval();
        vcd->dump(contextp->time());

        // 上升沿：采样rst/en，更新参考模型
        bool rst_sample = (top->rst != 0);
        bool en_sample  = (top->en  != 0);
        for(int i=0;i<4;i++){
            auto& r = refs[i];
            if(rst_sample) {
                r.ref_mod = 0;
                r.ref_sat = 0;
            } else if(en_sample) {
                r.ref_mod = (r.ref_mod + 1u) % r.n;
                r.ref_sat = std::min(r.ref_sat + 1u, r.n -1u);
            }
        }

        // 上升沿：校验寄存器输出
        uint32_t dut_mod[4], dut_sat[4];
        dut_mod[0] = top->mod1_out;  dut_sat[0] = top->sat1_out;
        dut_mod[1] = top->mod2_out;  dut_sat[1] = top->sat2_out;
        dut_mod[2] = top->mod10_out; dut_sat[2] = top->sat10_out;
        dut_mod[3] = top->mod256_out;dut_sat[3] = top->sat256_out;
        for(int i=0;i<4;i++){
            auto& r = refs[i];
            if(dut_mod[i] != r.ref_mod){
                printf("[RISE] time=%lu N=%u MOD: ref=%u dut=%u\n",
                       (unsigned long)contextp->time(), r.n, r.ref_mod, dut_mod[i]);
                error = true;
            }
            if(dut_sat[i] != r.ref_sat){
                printf("[RISE] time=%lu N=%u SAT: ref=%u dut=%u\n",
                       (unsigned long)contextp->time(), r.n, r.ref_sat, dut_sat[i]);
                error = true;
            }
        }

        // ---------- 下降沿 phase clk=0 ----------
        top->clk = 0;
        contextp->timeInc(1);
        top->eval();
        vcd->dump(contextp->time());
    }

    // ====================== 场景A：独立测试 N=10 末值暂停 ======================
    printf("\n==== Start independent test N=10 terminal pause ====\n");
    reset_refs(refs);
    // 同步复位
    top->rst = 1; top->en = 0; top->clk = 0;
    contextp->timeInc(1); top->eval(); vcd->dump(contextp->time());
    top->clk = 1; contextp->timeInc(1); top->eval(); vcd->dump(contextp->time());
    top->rst = 0; top->en = 1;

    // 计数9拍，到末值9
    for(int i=0; i<9; i++) {
        top->clk = 0;
        contextp->timeInc(1); top->eval(); vcd->dump(contextp->time());
        top->clk = 1;
        contextp->timeInc(1); top->eval(); vcd->dump(contextp->time());
        bool rst_sample = (top->rst != 0);
        bool en_sample  = (top->en  != 0);
        for(int k=0;k<4;k++){
            auto& r = refs[k];
            if(rst_sample) { r.ref_mod = 0; r.ref_sat = 0; }
            else if(en_sample) {
                r.ref_mod = (r.ref_mod + 1u) % r.n;
                r.ref_sat = std::min(r.ref_sat + 1u, r.n -1u);
            }
        }
    }
    if(top->mod10_out != 9 || top->sat10_out !=9) {
        printf("[TEST_N10_FAIL] Before pause, expect mod10=9 sat10=9, got mod10=%u sat10=%u\n", top->mod10_out, top->sat10_out);
        error=true;
    }
    // 暂停3拍 en=0
    for(int i=0;i<3;i++) {
        top->en = 0;
        top->clk = 0;
        contextp->timeInc(1); top->eval(); vcd->dump(contextp->time());
        top->clk = 1;
        contextp->timeInc(1); top->eval(); vcd->dump(contextp->time());
        bool rst_sample = (top->rst != 0);
        bool en_sample  = (top->en  != 0);
        for(int k=0;k<4;k++){
            auto& r = refs[k];
            if(rst_sample) { r.ref_mod = 0; r.ref_sat = 0; }
            else if(en_sample) {
                r.ref_mod = (r.ref_mod + 1u) % r.n;
                r.ref_sat = std::min(r.ref_sat + 1u, r.n -1u);
            }
        }
    }
    // 恢复使能1拍
    top->en = 1;
    top->clk = 0;
    contextp->timeInc(1); top->eval(); vcd->dump(contextp->time());
    top->clk = 1;
    contextp->timeInc(1); top->eval(); vcd->dump(contextp->time());
    bool rst_sample = (top->rst != 0);
    bool en_sample  = (top->en  != 0);
    for(int k=0;k<4;k++){
        auto& r = refs[k];
        if(rst_sample) { r.ref_mod = 0; r.ref_sat = 0; }
        else if(en_sample) {
            r.ref_mod = (r.ref_mod + 1u) % r.n;
            r.ref_sat = std::min(r.ref_sat + 1u, r.n -1u);
        }
    }
    if(top->mod10_out != 0 || top->sat10_out !=9) {
        printf("[TEST_N10_FAIL] After pause, expect mod10=0 sat10=9, got mod10=%u sat10=%u\n", top->mod10_out, top->sat10_out);
        error=true;
    }

    // ====================== 场景B：独立测试 N=256 末值暂停 ======================
    printf("\n==== Start independent test N=256 terminal pause ====\n");
    reset_refs(refs);
    // 同步复位
    top->rst = 1; top->en = 0; top->clk = 0;
    contextp->timeInc(1); top->eval(); vcd->dump(contextp->time());
    top->clk = 1; contextp->timeInc(1); top->eval(); vcd->dump(contextp->time());
    top->rst = 0; top->en = 1;

    // 计数255拍，到末值255
    for(int i=0; i<255; i++) {
        top->clk = 0;
        contextp->timeInc(1); top->eval(); vcd->dump(contextp->time());
        top->clk = 1;
        contextp->timeInc(1); top->eval(); vcd->dump(contextp->time());
        bool rst_sample = (top->rst != 0);
        bool en_sample  = (top->en  != 0);
        for(int k=0;k<4;k++){
            auto& r = refs[k];
            if(rst_sample) { r.ref_mod = 0; r.ref_sat = 0; }
            else if(en_sample) {
                r.ref_mod = (r.ref_mod + 1u) % r.n;
                r.ref_sat = std::min(r.ref_sat + 1u, r.n -1u);
            }
        }
    }
    if(top->mod256_out != 255 || top->sat256_out !=255) {
        printf("[TEST_N256_FAIL] Before pause, expect mod256=255 sat256=255, got mod256=%u sat256=%u\n", top->mod256_out, top->sat256_out);
        error=true;
    }
    // 暂停3拍 en=0
    for(int i=0;i<3;i++) {
        top->en = 0;
        top->clk = 0;
        contextp->timeInc(1); top->eval(); vcd->dump(contextp->time());
        top->clk = 1;
        contextp->timeInc(1); top->eval(); vcd->dump(contextp->time());
        bool rst_sample = (top->rst != 0);
        bool en_sample  = (top->en  != 0);
        for(int k=0;k<4;k++){
            auto& r = refs[k];
            if(rst_sample) { r.ref_mod = 0; r.ref_sat = 0; }
            else if(en_sample) {
                r.ref_mod = (r.ref_mod + 1u) % r.n;
                r.ref_sat = std::min(r.ref_sat + 1u, r.n -1u);
            }
        }
    }
    // 恢复使能1拍
    top->en = 1;
    top->clk = 0;
    contextp->timeInc(1); top->eval(); vcd->dump(contextp->time());
    top->clk = 1;
    contextp->timeInc(1); top->eval(); vcd->dump(contextp->time());
    rst_sample = (top->rst != 0);
    en_sample  = (top->en  != 0);
    for(int k=0;k<4;k++){
        auto& r = refs[k];
        if(rst_sample) { r.ref_mod = 0; r.ref_sat = 0; }
        else if(en_sample) {
            r.ref_mod = (r.ref_mod + 1u) % r.n;
            r.ref_sat = std::min(r.ref_sat + 1u, r.n -1u);
        }
    }
    if(top->mod256_out != 0 || top->sat256_out !=255) {
        printf("[TEST_N256_FAIL] After pause, expect mod256=0 sat256=255, got mod256=%u sat256=%u\n", top->mod256_out, top->sat256_out);
        error=true;
    }

    // ====================== 收尾 ======================
    vcd->close();
    delete vcd;
    top->final();
    delete top;
    delete contextp;

    if(error) {
        printf("\n==== SIMULATION FAILED ====\n");
        return 1;
    } else {
        printf("\n==== ALL TEST PASSED ====\n");
        return 0;
    }
}
