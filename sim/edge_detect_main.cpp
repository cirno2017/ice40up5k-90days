// ============================================================
// edge_detect 单元测试：sync_bit -> edge_detect 链
// （独立小型 Verilator C++ 仿真，顶层 sim/sync_edge_top.v）
//
// 由 Makefile 驱动：make esim
//
// 方法：
//   - C++ 驱动异步输入 din_async，只在非时钟边沿改变；
//   - 每次上升沿求值完成后，用参考模型逐拍比对
//     dout_sync / rise_pulse / fall_pulse；
//   - 参考模型寄存器 m_sync1/m_sync2/m_hist 初值均为 1，与 RTL 一致。
//
// 检查内容：
//   a. 初始状态没有事件；
//   b. 拉低保持稳定：只产生一个 fall_pulse，宽度 1 拍，并记录完整时序；
//   c. 拉高保持稳定：只产生一个 rise_pulse，宽度 1 拍；
//   d. 稳定高/低期间无重复事件；
//   e. rise_pulse 与 fall_pulse 不同时为高（每次求值后都监视）；
//   f. 固定随机种子改变输入相位，每次变化后保持 >= 4 拍，
//      事件数量与方向仍正确。
// ============================================================

#include "Vsync_edge_top.h"
#include "verilated.h"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

static Vsync_edge_top *dut = nullptr;
static vluint64_t t = 0; // 时间步（任意单位）
static int err_cnt = 0;

// 参考模型寄存器（初值与 RTL 一致：未按下为高）
static uint8_t m_sync1 = 1;
static uint8_t m_sync2 = 1;
static uint8_t m_hist = 1;

// 事件统计与互斥监视
static int rise_cnt = 0;
static int fall_cnt = 0;
static bool both_high_seen = false;

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

// 每次求值后调用：监视 rise/fall 同时为高的违例
static void watch()
{
    if (dut->rise_pulse && dut->fall_pulse)
        both_high_seen = true;
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
        watch();
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
        watch();
    }
}

// 上升沿：先记下本次采样值，求值完成后用参考模型比对全部输出
static void posedge(const char *tag)
{
    const uint8_t s_cur = dut->din_async; // 本次上升沿采样到的值
    dut->clk = 1;
    dut->eval();
    ++t;
    watch();

    // 模型推进：求值后的组合输出由“更新后的 sync2 与 hist”产生
    const uint8_t exp_sync2 = m_sync1;
    const uint8_t exp_hist = m_sync2;
    const uint8_t exp_rise = (~exp_hist) & exp_sync2 & 1;
    const uint8_t exp_fall = exp_hist & (~exp_sync2) & 1;

    check(dut->dout_sync == exp_sync2 && dut->rise_pulse == exp_rise && dut->fall_pulse == exp_fall,
          "t=%-3llu %s：dout=%d rise=%d fall=%d, 期望 dout=%d rise=%d fall=%d",
          (unsigned long long)t, tag, dut->dout_sync, dut->rise_pulse, dut->fall_pulse, exp_sync2,
          exp_rise, exp_fall);

    rise_cnt += dut->rise_pulse;
    fall_cnt += dut->fall_pulse;

    m_sync1 = s_cur;
    m_sync2 = exp_sync2;
    m_hist = exp_hist;
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
    dut = new Vsync_edge_top;
    printf("==== edge_detect 单元测试开始(sync_bit -> edge_detect 链) ====\n");

    // ---------- a. 初始状态没有事件 ----------
    dut->clk = 0;
    dut->din_async = 1; // 按键未按下
    dut->eval();
    watch();
    check(dut->dout_sync == 1 && dut->rise_pulse == 0 && dut->fall_pulse == 0,
          "a: 上电初始 dout=%d rise=%d fall=%d, 期望 1/0/0(无事件)", dut->dout_sync,
          dut->rise_pulse, dut->fall_pulse);
    for (int k = 0; k < 3; ++k)
        cycle("a: 稳定高电平(无事件)");

    // ---------- b. 拉低保持稳定：一个 fall 脉冲，并记录完整时序 ----------
    const int rise_before_b = rise_cnt;
    const int fall_before_b = fall_cnt;
    printf("---- 一次下降沿事件的时序(区分“脉冲产生”与“下游使用脉冲”) ----\n");
    cycle("b: 拉低后第1拍(同步链第一级接住,无事件)", 0, 0);

    // E：第二个上升沿，同步输出更新，组合边沿检测输出拉高
    low_phase(5, -1, 0);
    posedge("b: E 拍(脉冲应产生)");
    check(dut->dout_sync == 0 && dut->fall_pulse == 1,
          "b: E 上升沿更新后 dout=0 fall=%d(脉冲产生)", dut->fall_pulse);
    printf("  [时序] t=%-3llu E 上升沿更新后 : dout_sync 1->0, fall_pulse=1(脉冲产生)\n",
           (unsigned long long)t);

    // E ~ E+1 之间：事件脉冲保持高
    high_phase(5, -1, 0);
    low_phase(5, -1, 0);
    check(dut->fall_pulse == 1, "b: E~E+1 之间 fall_pulse 保持 1");
    printf("  [时序] t=%-3llu E ~ E+1 之间   : fall_pulse 保持 1(事件脉冲维持一拍)\n",
           (unsigned long long)t);

    // E+1 上升沿：下游时序逻辑读取该脉冲
    const uint8_t fall_at_e1 = dut->fall_pulse; // E+1 沿到来前，下游将采到的值
    posedge("b: E+1 拍(脉冲应恢复低)");
    check(fall_at_e1 == 1, "b: E+1 上升沿处下游采样到 fall_pulse=%d", fall_at_e1);
    printf("  [时序] t=%-3llu E+1 上升沿     : 下游时序逻辑此刻采样 fall_pulse=%d(下游使用脉冲)\n",
           (unsigned long long)t, fall_at_e1);

    // E+1 更新后：历史寄存器追上当前值，脉冲恢复低
    check(dut->fall_pulse == 0, "b: E+1 更新后 fall_pulse=%d(脉冲恢复低)", dut->fall_pulse);
    printf("  [时序] t=%-3llu E+1 更新后     : 历史寄存器追上当前值, fall_pulse=0\n",
           (unsigned long long)t);
    high_phase(5, -1, 0);

    // 继续保持低：无重复事件
    for (int k = 0; k < 3; ++k)
        cycle("b: 保持低电平(无重复事件)");
    check(fall_cnt - fall_before_b == 1 && rise_cnt == rise_before_b,
          "b: 一次稳定下降共产生 %d 个 fall / %d 个 rise, 期望 1/0", fall_cnt - fall_before_b,
          rise_cnt - rise_before_b);

    // ---------- c. 拉高保持稳定：一个 rise 脉冲，宽度 1 拍 ----------
    const int rise_before_c = rise_cnt;
    const int fall_before_c = fall_cnt;
    cycle("c: 拉高后第1拍(无事件)", 0, 1);
    cycle("c: 拉高后第2拍(rise 脉冲应产生)");
    check(dut->dout_sync == 1 && dut->rise_pulse == 1,
          "c: 上升沿事件拍 dout=1 rise=%d(脉冲产生)", dut->rise_pulse);
    cycle("c: 第3拍(脉冲应恢复低,宽度恰为1拍)");
    for (int k = 0; k < 2; ++k)
        cycle("c: 保持高电平(无重复事件)");
    check(rise_cnt - rise_before_c == 1 && fall_cnt == fall_before_c,
          "c: 一次稳定上升共产生 %d 个 rise / %d 个 fall, 期望 1/0", rise_cnt - rise_before_c,
          fall_cnt - fall_before_c);

    // ---------- f. 固定随机种子：随机相位，每次变化后保持 >= 4 拍 ----------
    // 保持拍数大于同步链+边沿检测的 3 拍延迟，每次变化必被采到且只产生一个事件
    srand(20251020); // 固定种子，结果可复现
    const int rise_before_r = rise_cnt;
    const int fall_before_r = fall_cnt;
    int exp_rise = 0;
    int exp_fall = 0;
    uint8_t din = 1; // 当前输入电平(场景 c 结束后为高)
    for (int i = 0; i < 30; ++i)
    {
        const uint8_t v = rand() & 1;    // 目标电平
        const int where = rand() % 10;   // 0-4: 低电平第 where 步; 5-9: 高电平第 where-5 步
        const int hold = 4 + rand() % 3; // 变化后保持 4~6 拍
        if (v != din)
        {
            if (v)
                ++exp_rise;
            else
                ++exp_fall;
        }
        if (where < 5)
            cycle("f: 随机相位", where, v, -1, 0);
        else
            cycle("f: 随机相位", -1, 0, where - 5, v);
        din = v;
        for (int k = 1; k < hold; ++k)
            cycle("f: 随机相位保持");
    }
    check(rise_cnt - rise_before_r == exp_rise && fall_cnt - fall_before_r == exp_fall,
          "f: 随机相位 30 次变化, 事件数 rise=%d(期望%d) fall=%d(期望%d)",
          rise_cnt - rise_before_r, exp_rise, fall_cnt - fall_before_r, exp_fall);

    // ---------- e. 全程互斥检查 ----------
    check(!both_high_seen, "e: 全程 rise_pulse 与 fall_pulse 未同时为高");

    dut->final();
    delete dut;

    if (err_cnt == 0)
    {
        printf("==== edge_detect 单元测试结束：共 %llu 时间步, 全部通过 ====\n",
               (unsigned long long)t);
        return 0;
    }
    printf("==== edge_detect 单元测试结束：%d 项检查失败 ====\n", err_cnt);
    return 1;
}
