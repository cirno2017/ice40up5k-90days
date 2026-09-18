#include <iostream>

#include "Vtop.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

int main(int argc, char **argv)
{
    // 初始化 Verilator 运行环境
    Verilated::commandArgs(argc, argv);

    // 实例化被测模块
    Vtop *top = new Vtop;

    // 开启波形追踪
    Verilated::traceEverOn(true);
    VerilatedVcdC *tfp = new VerilatedVcdC;
    top->trace(tfp, 99); // 99 = 追踪层级深度，足够覆盖所有子模块信号
    tfp->open("wave.vcd");

    vluint64_t main_time = 0; // 仿真时间戳，单位与 `timescale 1ns/1ps 对应，即 1 = 1ns

    // 施加一组输入并保持 10ns，期间每 1ns 采样一次波形
    auto apply_and_hold = [&](int a, int b, int hold_ns)
    {
        top->wire_a = a;
        top->wire_b = b;

        for (int i = 0; i < hold_ns; i++)
        {
            top->eval();          // 组合逻辑求值（无时钟，纯组合电路）
            tfp->dump(main_time); // 在当前时刻写入波形
            main_time++;
        }
    };

    // 00 -> 01 -> 10 -> 11，每组保持 10ns
    apply_and_hold(0, 0, 10);
    apply_and_hold(0, 1, 10);
    apply_and_hold(1, 0, 10);
    apply_and_hold(1, 1, 10);

    // 最后再 eval + dump 一次，确保末尾状态被完整记录
    top->eval();
    tfp->dump(main_time);

    // 收尾
    tfp->close();
    delete tfp;
    delete top;

    return 0;
}