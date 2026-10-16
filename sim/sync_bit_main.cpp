// ============================================================
// sync_bit 单元测试：单比特两级同步器（独立小型 Verilator C++ 仿真）
//
// 由 Makefile 驱动：make ssim
//
// 方法：
//   - 输入 din_async 只在非时钟边沿（低电平或高电平期间）改变；
//   - 每次上升沿求值完成后检查输出；
//   - 参考模型：dout_sync 在上升沿后应等于“上一个上升沿采样到的输入值”
//     （两级采样关系：新输入最迟在第 2 个上升沿后反映到输出）。
//
// 覆盖场景：
//   a. 初始输入为高                -> 输出保持高
//   b. 输入拉低并保持多个周期       -> 输出按两级采样关系变低
//   c. 输入拉高并保持多个周期       -> 输出按两级采样关系变高
//   d. 改变输入相对于时钟的相位     -> 逐拍结果仍符合采样关系
//   e. 短脉冲完全落在两个采样沿之间 -> 被遗漏
//   f. 同宽度短脉冲跨过一个采样沿   -> 被捕获
// ============================================================

#include "Vsync_bit.h"
#include "verilated.h"

#include <cstdarg>
#include <cstdint>
#include <cstdio>

static Vsync_bit *dut = nullptr;
static vluint64_t t = 0; // 时间步（任意单位）
static int err_cnt = 0;

// 参考模型：s_prev = 上一个上升沿采样到的输入值。
// 上电时两级寄存器均为 1，等效于“第 0 拍采到 1”。
static uint8_t s_prev = 1;

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

// 时钟低电平阶段推进 steps 个时间步；change_at >= 0 时在该步把输入改为 v
static void low_phase(int steps, int change_at, uint8_t v)
{
    for (int i = 0; i < steps; ++i)
    {
        if (i == change_at)
            dut->din_async = v;
        dut->clk = 0;
        dut->eval();
        ++t;
    }
}

// 时钟高电平阶段，同上
static void high_phase(int steps, int change_at, uint8_t v)
{
    for (int i = 0; i < steps; ++i)
    {
        if (i == change_at)
            dut->din_async = v;
        dut->clk = 1;
        dut->eval();
        ++t;
    }
}

// 上升沿：先记下本次将采到的值，求值完成后检查输出 == 上一拍采样值
static void posedge(const char *tag)
{
    const uint8_t s_cur = dut->din_async; // 本次上升沿采样到的值
    dut->clk = 1;
    dut->eval();
    ++t;
    check(dut->dout_sync == s_prev, "t=%-3llu %s：上升沿后 dout_sync=%d, 期望(上一拍采样值)=%d",
          (unsigned long long)t, tag, dut->dout_sync, s_prev);
    s_prev = s_cur;
}

// 一个完整周期：低电平 5 步 -> 上升沿 -> 高电平 5 步
static void cycle(const char *tag, int low_at = -1, uint8_t low_v = 0, int high_at = -1,
                  uint8_t high_v = 0)
{
    low_phase(5, low_at, low_v);
    posedge(tag);
    high_phase(5, high_at, high_v);
}

int main(int argc, char **argv)
{
    Verilated::commandArgs(argc, argv);
    dut = new Vsync_bit;
    printf("==== sync_bit 单元测试开始 ====\n");

    // ---------- a. 初始输入为高，输出保持高 ----------
    dut->clk = 0;
    dut->din_async = 1; // 按键未按下
    dut->eval();
    check(dut->dout_sync == 1, "a: 上电初始 dout_sync=%d, 期望 1(未按下)", dut->dout_sync);
    for (int k = 0; k < 3; ++k)
        cycle("a: 输入持续为高");

    // ---------- b. 输入拉低并保持，输出按两级采样关系变低 ----------
    // 第 1 个上升沿后输出应仍为 1(第一级刚接住)，第 2 个上升沿后应变 0
    cycle("b: 拉低后第1拍(输出应仍为1)", 0, 0);
    cycle("b: 拉低后第2拍(输出应变0)");
    for (int k = 0; k < 2; ++k)
        cycle("b: 保持低电平(输出保持0)");

    // ---------- c. 输入拉高并保持，输出按两级采样关系变高 ----------
    cycle("c: 拉高后第1拍(输出应仍为0)", 0, 1);
    cycle("c: 拉高后第2拍(输出应变1)");
    for (int k = 0; k < 2; ++k)
        cycle("c: 保持高电平(输出保持1)");

    // ---------- d. 改变输入相对时钟的相位，逐拍仍符合采样关系 ----------
    // 相同的电平变化，分别落在：高电平期间 / 低电平早期 / 低电平中期 / 紧挨上升沿
    cycle("d: 高电平期间拉低", -1, 0, 1, 0); // 上一拍高电平阶段拉低，下一采样沿采到 0
    cycle("d: 保持低(输出仍为1)");           // 本拍才采到 0，输出下一拍才反映
    cycle("d: 低电平早期拉高(输出变0)", 0, 1);
    cycle("d: 保持高(输出变1)");
    cycle("d: 低电平中期拉低", 2, 0);
    cycle("d: 保持低(输出变0)");
    cycle("d: 紧挨上升沿拉高", 4, 1);
    cycle("d: 保持高(输出变1)");

    // ---------- e. 短脉冲完全落在两个采样沿之间 -> 被遗漏 ----------
    // 低电平阶段制造宽度 2 个时间步的低脉冲(step1 拉低, step3 拉回)，
    // 上升沿到来时输入已恢复为高，脉冲不被采到，输出应一直为 1
    for (int i = 0; i < 5; ++i)
    {
        if (i == 1)
            dut->din_async = 0;
        if (i == 3)
            dut->din_async = 1;
        dut->clk = 0;
        dut->eval();
        ++t;
    }
    posedge("e: 脉冲落在两沿之间(输出应保持1)");
    high_phase(5, -1, 0);
    cycle("e: 脉冲后第2拍(输出仍应1)");
    cycle("e: 脉冲后第3拍(输出仍应1)");

    // ---------- f. 同宽度短脉冲跨过一个采样沿 -> 被捕获 ----------
    // 宽度同为 2 个时间步：低电平最后一步拉低，高电平第 1 步拉回
    // （低电平末步 + 采样沿步，与 e 同为 2 步，只是相位不同），
    // 跨过中间那个上升沿被采到一次，输出应低一个周期(延迟两拍出现)
    low_phase(5, 4, 0);
    posedge("f: 脉冲跨过采样沿(本拍输出仍1)");
    for (int i = 0; i < 5; ++i)
    {
        if (i == 0)
            dut->din_async = 1;
        dut->clk = 1;
        dut->eval();
        ++t;
    }
    cycle("f: 脉冲出现在输出(应变0)");
    cycle("f: 脉冲离开输出(应回1)");
    cycle("f: 输出保持高");

    dut->final();
    delete dut;

    if (err_cnt == 0)
    {
        printf("==== sync_bit 单元测试结束：共 %llu 时间步，全部通过 ====\n", (unsigned long long)t);
        return 0;
    }
    printf("==== sync_bit 单元测试结束：%d 项检查失败 ====\n", err_cnt);
    return 1;
}
