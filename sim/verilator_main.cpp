// ============================================================
// Verilator C++ 测试平台：iCESugar 流水灯（top + debounce）
//
// 由 Makefile 驱动：
//   make vsim DIV=4  ->  build/verilator/Vtop_4 4
//   make vsim DIV=7  ->  build/verilator/Vtop_7 7
// argv[1] = DIV_FREQ，必须与 verilator -G 的参数一致。
//
// 完整测试流程：
//   0) 上电初始状态检查；等待消抖稳定电平收敛到“未按下”
//   1) 右移流水 x8：逐周期校验移位周期恰好等于 DIV_FREQ
//   2) 暂停键：LED 必须完全冻结
//   3) 再按暂停键：恢复右移流水
//   4) 换向键：改左移；再按一次：恢复右移
// 全部通过返回 0，任一检查失败返回 1。
// ============================================================

#include "Vtop.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// 与 rtl/debounce.v 默认 CNT_MAX=120000-1 对应：
// 按键电平需连续保持 120000 个时钟才会被采信
static const uint64_t DEBOUNCE = 120000;
// 消抖之后的额外裕量，保证脉冲已发出 / 稳定电平已收敛
static const uint64_t SETTLE = 50;

static Vtop *dut = nullptr;
static VerilatedVcdC *tfp = nullptr; // DIV 很大时置空，关闭波形避免 VCD 过大
static vluint64_t sim_time = 0;
static int err_cnt = 0;

// 推进 n 个时钟周期（每周期含一个上升沿）
static void tick(uint64_t n = 1)
{
    for (uint64_t i = 0; i < n; ++i)
    {
        dut->clk = 0;
        dut->eval();
        if (tfp)
            tfp->dump(sim_time);
        ++sim_time;
        dut->clk = 1;
        dut->eval();
        if (tfp)
            tfp->dump(sim_time);
        ++sim_time;
    }
}

static void check(bool ok, const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (ok)
    {
        printf("PASS: %s\n", buf);
    }
    else
    {
        printf("FAIL: %s\n", buf);
        ++err_cnt;
    }
}

static uint8_t rot_r(uint8_t b) { return (b >> 1) | (b << 7); } // 循环右移
static uint8_t rot_l(uint8_t b) { return (b << 1) | (b >> 7); } // 循环左移
static uint8_t led_of(uint8_t bit) { return (uint8_t)~bit; }    // led_stream = ~bit_stream

// 完整按一次键（低电平有效）：
// 按住 DEBOUNCE+SETTLE 拍让消抖采信“按下”，松开后同等时长让稳定电平回到
// “未按下”，保证下一次按键还能产生新的下降沿脉冲。
static void press(bool pause_key)
{
    if (pause_key)
        dut->pause_switch = 0;
    else
        dut->stream_switch = 0;
    tick(DEBOUNCE + SETTLE);
    if (pause_key)
        dut->pause_switch = 1;
    else
        dut->stream_switch = 1;
    tick(DEBOUNCE + SETTLE);
}

// 校验连续 count 次移位：
//   第 1 次：cnt 相位未知，逐周期等待，div 拍内必翻转一次（否则方向/运行状态错误）；
//   之后  ：周期已知，先走 div-1 拍确认不提前翻转，再走 1 拍确认按时翻转。
static void verify_shift(uint8_t &bit, bool left, uint64_t div, int count, const char *tag)
{
    const char *dir = left ? "左移" : "右移";
    for (int k = 0; k < count; ++k)
    {
        const uint8_t expect = left ? rot_l(bit) : rot_r(bit);
        if (k == 0)
        {
            bool found = false;
            for (uint64_t i = 0; i < div; ++i)
            {
                tick();
                if (dut->led_stream == led_of(expect))
                {
                    found = true;
                    break;
                }
            }
            check(found, "%s：%s 第 1 次翻转应在 %llu 拍内出现且值为 0x%02X (实测 led=0x%02X)", tag,
                  dir, (unsigned long long)div, led_of(expect), dut->led_stream);
            if (!found)
            {
                bit = (uint8_t)~dut->led_stream; // 重新对齐后继续后续阶段
                return;
            }
        }
        else
        {
            tick(div - 1);
            check(dut->led_stream == led_of(bit),
                  "%s：%s 第 %d 次翻转前应维持 0x%02X 共 %llu 拍 (实测 led=0x%02X)", tag, dir, k + 1,
                  led_of(bit), (unsigned long long)(div - 1), dut->led_stream);
            tick();
            check(dut->led_stream == led_of(expect),
                  "%s：%s 第 %d 次翻转应准时变为 0x%02X (实测 led=0x%02X)", tag, dir, k + 1,
                  led_of(expect), dut->led_stream);
        }
        bit = expect;
    }
    printf("PASS: %s（%s x%d 完成）\n", tag, dir, count);
}

int main(int argc, char **argv)
{
    Verilated::commandArgs(argc, argv);
    const uint64_t div = (argc > 1) ? strtoull(argv[1], nullptr, 10) : 6000000;
    printf("==== 流水灯仿真开始：DIV_FREQ=%llu ====\n", (unsigned long long)div);

    dut = new Vtop;

    // DIV 很大时仿真周期数巨大，关闭 VCD 以免生成超大波形文件
    if (div <= 100000)
    {
        Verilated::traceEverOn(true);
        tfp = new VerilatedVcdC;
        dut->trace(tfp, 99);
        tfp->open("wave.vcd");
    }

    dut->clk = 0;
    dut->stream_switch = 1; // 按键低电平有效，未按下为高
    dut->pause_switch = 1;
    dut->eval();
    if (tfp)
        tfp->dump(sim_time);
    ++sim_time;

    // ---------- 0. 初始状态 ----------
    check(dut->led_stream == 0xFE, "初始 led_stream=0x%02X, 应为 0xFE (bit_stream=0x01, 低电平点亮)",
          dut->led_stream);

    // 等消抖稳定电平收敛到“未按下”(stable=1)，此后按键才能产生下降沿脉冲
    tick(DEBOUNCE + SETTLE);

    // ---------- 1. 右移流水 x8（绕一整圈回到起点） ----------
    uint8_t bit = (uint8_t)~dut->led_stream; // 与 DUT 当前位置对齐
    const uint8_t start = bit;
    verify_shift(bit, false, div, 8, "阶段1右移流水");
    check(bit == start, "右移 8 次后回到起点 0x%02X (当前 0x%02X)", start, bit);

    // ---------- 2. 暂停：LED 必须完全冻结 ----------
    press(true);
    {
        const uint8_t frozen = dut->led_stream;
        bool still = true;
        for (uint64_t i = 0; i < 8 * div; ++i)
        {
            tick();
            if (dut->led_stream != frozen)
                still = false;
        }
        check(still, "暂停后 LED 冻结在 0x%02X (持续 %llu 拍无变化)", frozen,
              (unsigned long long)(8 * div));
    }

    // ---------- 3. 再按暂停：恢复右移 ----------
    press(true);
    bit = (uint8_t)~dut->led_stream;
    verify_shift(bit, false, div, 4, "阶段3恢复后右移");

    // ---------- 4. 换向：改左移 ----------
    press(false);
    bit = (uint8_t)~dut->led_stream;
    verify_shift(bit, true, div, 4, "阶段4换向后左移");

    // ---------- 5. 再换向：恢复右移 ----------
    press(false);
    bit = (uint8_t)~dut->led_stream;
    verify_shift(bit, false, div, 4, "阶段5再次换向后右移");

    dut->final();
    if (tfp)
    {
        tfp->close();
        delete tfp;
    }
    delete dut;

    if (err_cnt == 0)
    {
        printf("==== 仿真结束：共 %llu 拍，全部测试通过 (DIV_FREQ=%llu) ====\n",
               (unsigned long long)(sim_time / 2), (unsigned long long)div);
        return 0;
    }
    printf("==== 仿真结束：%d 项检查失败 (DIV_FREQ=%llu) ====\n", err_cnt, (unsigned long long)div);
    return 1;
}
