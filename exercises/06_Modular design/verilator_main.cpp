#include <verilated.h>
#include <verilated_vcd_c.h>
#include "Vtop.h"

// 单组测试函数，传入顶层实例指针、VCD指针、当前WIDTH值
template<typename TOP_MODULE>
void run_test(TOP_MODULE* top, VerilatedVcdC* vcd, int width_val)
{
    printf("\n======= Test TOP_WIDTH = %d =======\n", width_val);
    // 遍历所有 sel: 00,01,10,11
    for(int sel = 0; sel < 4; sel++)
    {
        // 给四路输入赋值：dataN = sel + N，方便观察
        top->top_sel = sel;
        top->data0 = sel + 0;
        top->data1 = sel + 1;
        top->data2 = sel + 2;
        top->data3 = sel + 3;

        // 仿真时间戳
        vcd->dump(10 * sel);
        top->eval();
        vcd->dump(10 * sel + 5);

        printf("sel=%2d | data0=%4d data1=%4d data2=%4d data3=%4d | selected=0x%x | output_data=%4d\n",
               (int)top->top_sel,
               (int)top->data0,
               (int)top->data1,
               (int)top->data2,
               (int)top->data3,
               (int)top->top_select,
               (int)top->output_data);
    }
}

int main(int argc, char** argv)
{
    Verilated::commandArgs(argc, argv);
    Verilated::traceEverOn(true); // 开启波形追踪

    // ========== 实例化 TOP_WIDTH=1 ==========
    Vtop* top_1 = new Vtop;
    VerilatedVcdC* vcd_1 = new VerilatedVcdC;
    top_1->trace(vcd_1, 99);
    vcd_1->open("wave_width1.vcd");
    run_test(top_1, vcd_1, 1);
    vcd_1->close();
    delete vcd_1;
    delete top_1;

    // ========== 实例化 TOP_WIDTH=8 ==========
    Vtop* top_8 = new Vtop;
    VerilatedVcdC* vcd_8 = new VerilatedVcdC;
    top_8->trace(vcd_8, 99);
    vcd_8->open("wave_width8.vcd");
    run_test(top_8, vcd_8, 8);
    vcd_8->close();
    delete vcd_8;
    delete top_8;

    // ========== 实例化 TOP_WIDTH=12 ==========
    Vtop* top_12 = new Vtop;
    VerilatedVcdC* vcd_12 = new VerilatedVcdC;
    top_12->trace(vcd_12, 99);
    vcd_12->open("wave_width12.vcd");
    run_test(top_12, vcd_12, 12);
    vcd_12->close();
    delete vcd_12;
    delete top_12;

    printf("\nAll tests done! Generated wave_width1.vcd / wave_width8.vcd / wave_width12.vcd\n");
    return 0;
}
