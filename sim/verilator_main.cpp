#include <verilated.h>
#include <verilated_vcd_c.h>
#include "Vtop.h" // verilator编译后自动生成的头文件

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);

    // 实例化DUT
    Vtop* dut = new Vtop;

    // 开启VCD波形记录
    VerilatedVcdC* tfp = new VerilatedVcdC;
    Verilated::traceEverOn(true);
    dut->trace(tfp, 99); // 深度99，抓取所有子模块信号
    tfp->open("wave.vcd"); // 输出文件名 wave.vcd

    vluint64_t sim_time = 0; // 仿真时间，单位ns

    // 遍历8位输入：0 ~ 255，一共256种组合
    for(int sw_val = 0; sw_val <= 255; sw_val++){
        dut->switches = sw_val;    // 设置输入switches
        dut->eval();               // 评估组合逻辑（你的代码没有时钟，只需要eval）

        tfp->dump(sim_time);       // 把当前时刻信号存入vcd
        sim_time += 10;            // 每次间隔10ns，方便在波形里区分

        // 打印到终端，方便核对结果
        printf("switches = 0x%02X | leds = 0x%02X | leds2 = 0x%02X\n",
               dut->switches, dut->leds, dut->leds2);
    }

    // 收尾
    tfp->close();
    delete dut;
    delete tfp;
    return 0;
}
