// ============================================================
// Verilator C++ testbench for top (LED toggle every 0.5s @ 12MHz)
// 参考模型仅在主时钟上升沿更新
// ============================================================
#include "Vtop.h"
#include "Vtop___024root.h"  // 访问内部信号 top__DOT__tick
#include "verilated.h"
#include "verilated_vcd_c.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

// 分频比，与构建时的 -GDIV_FREQ 保持一致（由命令行参数传入）
std::uint32_t ref_div = 6'000'000u;

vluint64_t sim_time = 0;
std::uint64_t error_count = 0;

// 参考模型状态（仿真专用）
std::uint32_t elapsed = 0;
bool ref_led = false;
bool ref_tick_r = false;  // tick 在 RTL 中是寄存器输出，期望值同样需要 1 bit 状态
bool rst_sample = false;
bool reset_seen = false;  // 第一次有效复位上升沿之前，不验收默认初值

Vtop* dut = nullptr;
VerilatedVcdC* tfp = nullptr;

// 波形窗口：12M 拍全量 dump 会产生过大的 VCD，
// 只录制开头一段和第一次 LED 翻转附近的一段
constexpr vluint64_t kHeadTraceUntil = 600;
vluint64_t toggle_window_begin = 0;
vluint64_t toggle_window_end = 0;

void dump() {
    if (!tfp) return;
    const bool in_head = sim_time < kHeadTraceUntil;
    const bool in_toggle =
        toggle_window_end != 0 && sim_time >= toggle_window_begin &&
        sim_time <= toggle_window_end;
    if (in_head || in_toggle) tfp->dump(sim_time);
}

// 比较 tick 和 led_state，失败时记录时间、期望值和实际值
void check_outputs() {
    if (!reset_seen) return;  // 第一次有效复位上升沿之前不验收

    const bool act_tick = dut->rootp->top__DOT__tick;
    const bool act_led = !dut->led_n;  // 引脚低电平点亮，换算回逻辑状态

    if (act_tick != ref_tick_r) {
        ++error_count;
        std::printf("[time=%lu] ERROR tick: expected=%u actual=%u\n",
                    static_cast<unsigned long>(sim_time),
                    static_cast<unsigned>(ref_tick_r),
                    static_cast<unsigned>(act_tick));
    }
    if (act_led != ref_led) {
        ++error_count;
        std::printf("[time=%lu] ERROR led_state: expected=%u actual=%u\n",
                    static_cast<unsigned long>(sim_time),
                    static_cast<unsigned>(ref_led),
                    static_cast<unsigned>(act_led));
    }
}

// 推进一个完整时钟周期，并在规定位置调用检查
void step(bool rst_value) {
    // 1. clk=0，设置 rst（端口为低电平有效的 rst_n），推进时间、eval()、dump()
    dut->clk = 0;
    dut->rst_n = !rst_value;
    ++sim_time;
    dut->eval();
    dump();

    // 2. 上升沿前保存本次复位输入
    rst_sample = rst_value;

    // 3. 推进时间，令 clk=1，eval()；更新参考模型，记录波形并比较
    ++sim_time;
    dut->clk = 1;
    dut->eval();

    // 仿真专用 C++：仅在主时钟上升沿更新
    if (rst_sample) {
        elapsed = 0;
        ref_led = false;
        ref_tick_r = false;
        reset_seen = true;
    } else {
        if (ref_tick_r) ref_led = !ref_led;  // top 在 tick 为高的沿翻转
        ref_tick_r = (elapsed == ref_div - 1u);  // tick 是寄存器输出
        elapsed = (elapsed == ref_div - 1u) ? 0 : elapsed + 1;
    }

    dump();
    check_outputs();

    // 4. 推进时间，令 clk=0，eval()、dump()，再次比较
    ++sim_time;
    dut->clk = 0;
    dut->eval();
    dump();
    check_outputs();
}

}  // namespace

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);

    // 用法：Vtop_<DIV> [div]，div 需与构建时的 -GDIV_FREQ 一致
    if (argc > 1) {
        ref_div = static_cast<std::uint32_t>(std::strtoul(argv[1], nullptr, 10));
    }
    if (ref_div < 2) {
        std::fprintf(stderr, "invalid div %u (must be >= 2)\n", ref_div);
        return 1;
    }
    std::printf("simulating with DIV_FREQ=%u\n", ref_div);

    dut = new Vtop;
    Verilated::traceEverOn(true);
    tfp = new VerilatedVcdC;
    dut->trace(tfp, 99);
    tfp->open("wave.vcd");

    // 上电默认初值阶段：复位尚未生效，此阶段不验收
    for (int i = 0; i < 4; ++i) step(false);

    // 有效复位若干拍
    for (int i = 0; i < 5; ++i) step(true);

    // 释放复位，运行两个完整的 LED 翻转周期
    // 每个 step 推进 3 个时间单位，第一次翻转约在释放后 ref_div 拍
    const vluint64_t toggle_at = sim_time + 3ull * ref_div;
    toggle_window_begin = toggle_at > 300 ? toggle_at - 300 : 0;
    toggle_window_end = toggle_at + 300;
    const std::uint64_t run_cycles = 2ull * ref_div + 100u;
    for (std::uint64_t i = 0; i < run_cycles; ++i) step(false);

    tfp->close();

    if (error_count == 0) {
        std::printf("TEST PASSED: tick and led_state matched reference model "
                    "for %llu cycles after reset\n",
                    static_cast<unsigned long long>(run_cycles));
    } else {
        std::printf("TEST FAILED: %llu mismatch(es)\n",
                    static_cast<unsigned long long>(error_count));
    }

    dut->final();
    delete tfp;
    delete dut;
    return error_count == 0 ? 0 : 1;
}
