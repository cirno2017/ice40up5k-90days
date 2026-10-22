// ============================================================
// debounce 单元测试：直接验证消抖模块(内部含 sync_bit 同步链)
//
// 由 Makefile 驱动：make dsim
//   构建两个模型：CNT_MAX=3 (N=4) 与 CNT_MAX=0 (N=1)，
//   argv[1] = N，必须与 verilator -G 参数一致。
//
// 计数规格：
//   N = CNT_MAX + 1。
//   消抖逻辑需连续 N 个上升沿采到与当前 stable 相反的同步输入，
//   才更新 stable；中途只要采到一次与 stable 相同的值，
//   就清零重新计数。
//   默认 CNT_MAX=119999，对应 12 MHz 下标称 10 ms；
//   同步链(sync_bit 两级寄存器)延迟 2 拍，另外计算。
//
// 参考模型：纯规格描述(“连续不一致样本数”)，不读取 DUT 内部计数器。
//   输入路径：btn 经两级同步，第 k 拍的样本 = 第 k-2 拍采到的 btn；
//   逐拍比对 DUT 的 stable / fall_pulse(失配才打印)，
//   并统计 DUT 实际产生的按下事件数。
// ============================================================

#include "Vdebounce.h"
#include "verilated.h"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

static Vdebounce *dut = nullptr;
static vluint64_t t = 0; // 时间步（任意单位）
static int err_cnt = 0;
static long N = 4; // 规格门限 N = CNT_MAX + 1，由 argv[1] 传入

// ---- 参考模型(纯规格，不读 DUT 内部信号) ----
static uint8_t m_s1 = 1;         // 同步链第 1 级
static uint8_t m_s2 = 1;         // 同步链第 2 级(消抖逻辑看到的样本)
static uint8_t m_stable = 1;     // 稳定电平(未按下为高)
static uint8_t m_stable_old = 1; // 上一拍稳定电平(用于 fall_pulse)
static long m_run = 0;           // 连续与 stable 不一致的样本数
static long fall_cnt = 0;        // 按下事件计数

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

// 时钟低电平阶段推进 5 步；change_at >= 0 时在该步把 btn 改为 v
static void low_phase(int change_at, uint8_t v)
{
    for (int i = 0; i < 5; ++i)
    {
        if (i == change_at)
            dut->btn = v;
        dut->clk = 0;
        dut->eval();
        ++t;
    }
}

// 时钟高电平阶段推进 5 步
static void high_phase()
{
    for (int i = 0; i < 5; ++i)
    {
        dut->clk = 1;
        dut->eval();
        ++t;
    }
}

// 上升沿：采样 -> 规格推进 -> 逐拍比对 stable / fall_pulse(仅失配时打印)
static void posedge(const char *tag)
{
    const uint8_t s = dut->btn; // 本拍采到的 btn
    dut->clk = 1;
    dut->eval();
    ++t;

    // 规格推进(全部使用沿前的旧值)
    const uint8_t sample = m_s2; // 本拍消抖逻辑看到的同步样本
    uint8_t new_stable = m_stable;
    if (sample != m_stable)
    {
        ++m_run; // 不一致样本数 +1
        if (m_run >= N)
        {
            new_stable = sample; // 连续 N 个 -> 采信新电平
            m_run = 0;
        }
    }
    else
    {
        m_run = 0; // 与 stable 一致 -> 清零重新计数
    }
    const uint8_t exp_fall = m_stable & (~new_stable) & 1;

    if (dut->stable != new_stable || dut->fall_pulse != exp_fall)
    {
        check(false, "t=%-4llu %s：stable=%d fall=%d, 期望 stable=%d fall=%d",
              (unsigned long long)t, tag, dut->stable, dut->fall_pulse, new_stable, exp_fall);
    }
    if (exp_fall)
    {
        ++fall_cnt;
        printf("  [事件] t=%llu 按下事件 fall_pulse=1 (stable 1->0)\n", (unsigned long long)t);
    }

    m_stable_old = m_stable;
    m_stable = new_stable;
    m_s2 = m_s1;
    m_s1 = s;
}

// 一个完整周期：低电平 5 步 -> 上升沿 -> 高电平 5 步
static void cycle(const char *tag, int change_at = -1, uint8_t v = 1)
{
    low_phase(change_at, v);
    posedge(tag);
    high_phase();
}

// 上电初始化与初始检查
static void power_on()
{
    dut->clk = 0;
    dut->btn = 1; // 按键未按下为高
    dut->eval();
    ++t;
    check(dut->stable == 1 && dut->fall_pulse == 0,
          "上电初始 stable=%d fall=%d, 期望 1/0(无伪事件)", dut->stable, dut->fall_pulse);
}

int main(int argc, char **argv)
{
    Verilated::commandArgs(argc, argv);
    N = (argc > 1) ? atol(argv[1]) : 4;
    if (N != 4 && N != 1)
    {
        printf("仅支持 N=4 (CNT_MAX=3) 或 N=1 (CNT_MAX=0)\n");
        return 2;
    }
    printf("==== debounce 单元测试开始：CNT_MAX=%ld, N=%ld ====\n", N - 1, N);

    dut = new Vdebounce;
    power_on();

    if (N == 4)
    {
        // ---------- 场景1: 初始未按下，持续为高 ----------
        for (int k = 0; k < 5; ++k)
            cycle("场景1: 持续高");
        check(dut->stable == 1 && fall_cnt == 0, "场景1: 初始持续高, stable=1 且无事件");

        // ---------- 场景2: 低电平仅持续 3 个样本后恢复高 -> 不接受 ----------
        cycle("场景2: 低第1拍", 0, 0);
        cycle("场景2: 低第2拍");
        cycle("场景2: 低第3拍");
        cycle("场景2: 恢复高", 0, 1);
        for (int k = 0; k < 4; ++k)
            cycle("场景2: 保持高");
        check(dut->stable == 1 && fall_cnt == 0,
              "场景2: 低电平仅 3(<N=4) 个样本, 不接受按下(stable=1, 无事件)");

        // ---------- 场景3: 低电平连续达到 4 个样本 -> 第 4 个样本处更新 ----------
        // (样本经 2 拍同步链延迟到达，事件应落在按低后第 6 个上升沿；
        //  逐拍比对保证不多不少正好在第 4 个样本处更新)
        const long ev3 = fall_cnt;
        cycle("场景3: 按下", 0, 0);
        for (int k = 0; k < 7; ++k)
            cycle("场景3: 持续按住");
        check(fall_cnt - ev3 == 1 && dut->stable == 0,
              "场景3: 连续 N=4 个低样本, 第 4 个样本处更新, 恰好 1 次按下事件");

        // ---------- 场景5: 持续按下很多拍 -> 不重复产生事件 ----------
        const long ev5 = fall_cnt;
        for (int k = 0; k < 6; ++k)
            cycle("场景5: 持续按住");
        check(fall_cnt == ev5, "场景5: 持续按住 6 拍, 不重复产生事件");

        // ---------- 干净释放，为场景4准备未按下状态 ----------
        for (int k = 0; k < 8; ++k)
            cycle("释放: 持续高", k == 0 ? 0 : -1, 1);
        check(dut->stable == 1, "释放: 持续高达到门限, stable 回到 1");

        // ---------- 场景4: 低 3 拍、高 1 拍、再低 4 拍 ----------
        const long ev4 = fall_cnt;
        cycle("场景4: 低第1拍", 0, 0);
        cycle("场景4: 低第2拍");
        cycle("场景4: 低第3拍");
        cycle("场景4: 高1拍(应打断计数)", 0, 1);
        check(fall_cnt == ev4 && dut->stable == 1,
              "场景4: 前一段 3 个低样本被高样本打断, 计数取消, 无事件");
        cycle("场景4: 再低第1拍", 0, 0);
        cycle("场景4: 再低第2拍");
        cycle("场景4: 再低第3拍");
        cycle("场景4: 再低第4拍");
        for (int k = 0; k < 2; ++k)
            cycle("场景4: 保持按住");
        check(fall_cnt - ev4 == 1 && dut->stable == 0,
              "场景4: 后一段重新计满 N=4 个样本, 产生 1 次按下事件");
        const long ev4b = fall_cnt;
        for (int k = 0; k < 3; ++k)
            cycle("场景4: 继续按住");
        check(fall_cnt == ev4b, "场景4: 按住期间不重复产生事件");

        // ---------- 场景6: 释放时反复抖动，最终持续高 ----------
        // 高电平段最长 2 个样本(<N=4)，抖动期间不应提前释放；
        // 最终持续高达到门限后释放，且释放不产生按下事件
        const long ev6 = fall_cnt;
        cycle("场景6: 抖动 高1", 0, 1);
        cycle("场景6: 抖动 低1", 0, 0);
        cycle("场景6: 抖动 高1", 0, 1);
        cycle("场景6: 抖动 低1", 0, 0);
        cycle("场景6: 抖动 高1", 0, 1);
        cycle("场景6: 抖动 高2");
        cycle("场景6: 抖动 低1", 0, 0);
        check(dut->stable == 0, "场景6: 抖动期间 stable 保持 0(未提前释放)");
        for (int k = 0; k < 8; ++k)
            cycle("场景6: 最终持续高", k == 0 ? 0 : -1, 1);
        check(dut->stable == 1, "场景6: 抖动结束持续高达到门限, stable 回到 1(释放)");
        check(fall_cnt == ev6, "场景6: 释放过程不产生按下事件");

        // ---------- 场景7: 完整释放后再次按下 -> 再产生一次按下事件 ----------
        const long ev7 = fall_cnt;
        cycle("场景7: 再次按下", 0, 0);
        for (int k = 0; k < 7; ++k)
            cycle("场景7: 按住");
        check(fall_cnt - ev7 == 1 && dut->stable == 0,
              "场景7: 完整释放后再次按下, 再产生 1 次按下事件");

        check(fall_cnt == 3, "汇总: 全程共 %ld 次按下事件, 期望 3(场景3/4/7 各 1 次)", fall_cnt);
    }
    else // N == 1 (CNT_MAX=0)：单个不一致样本即采信
    {
        // ---------- 初始稳定高，无事件 ----------
        for (int k = 0; k < 3; ++k)
            cycle("N1: 持续高");
        check(dut->stable == 1 && fall_cnt == 0, "N1: 初始持续高, stable=1 且无事件");

        // ---------- 低电平仅 1 个样本也会被采信(与 N=4 的场景2 形成对照) ----------
        const long ev1 = fall_cnt;
        cycle("N1: 低 1 拍", 0, 0);
        cycle("N1: 恢复高", 0, 1);
        for (int k = 0; k < 4; ++k)
            cycle("N1: 保持高");
        check(fall_cnt - ev1 == 1, "N1: 仅 1 个低样本即采信, 产生 1 次按下事件");
        check(dut->stable == 1, "N1: 随后 1 个高样本即释放, stable 回到 1");

        // ---------- 再次按下 ----------
        const long ev2 = fall_cnt;
        cycle("N1: 再次按下", 0, 0);
        for (int k = 0; k < 5; ++k)
            cycle("N1: 按住");
        check(fall_cnt - ev2 == 1 && dut->stable == 0, "N1: 再次按下再产生 1 次事件");
        check(fall_cnt == 2, "N1 汇总: 全程共 %ld 次按下事件, 期望 2", fall_cnt);
    }

    dut->final();
    delete dut;

    if (err_cnt == 0)
    {
        printf("==== debounce 单元测试结束：N=%ld, 共 %llu 时间步, 全部通过 ====\n", N,
               (unsigned long long)t);
        return 0;
    }
    printf("==== debounce 单元测试结束：N=%ld, %d 项检查失败 ====\n", N, err_cnt);
    return 1;
}
