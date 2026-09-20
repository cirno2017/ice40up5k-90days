#include <verilated.h>
#include "Vtop.h"
#include <cstdint>
#include <iostream>

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Vtop* dut = new Vtop;

    int error_cnt = 0;
    const int total_tests = 1024;

    for(int i = 0; i < total_tests; i++) {
        // 将测试序号拆分成sel和输入数据
        uint8_t data_val = i & 0xFF;
        uint8_t sel_val  = (i >> 8) & 0x03;

        // 设置四路输入：只把当前data_val放到sel选中的那一路，其余固定0
        dut->d0 = (sel_val == 0) ? data_val : 0;
        dut->d1 = (sel_val == 1) ? data_val : 0;
        dut->d2 = (sel_val == 2) ? data_val : 0;
        dut->d3 = (sel_val == 3) ? data_val : 0;
        dut->sel = sel_val;

        dut->eval();

        uint8_t expect = data_val;
        uint8_t actual = dut->y;

        if(actual != expect) {
            std::printf("[FAIL] i=%4d sel=%d expect=0x%02X actual=0x%02X\n",
                        i, sel_val, expect, actual);
            error_cnt++;
        } else {
            std::printf("[OK]   i=%4d sel=%d y=0x%02X\n", i, sel_val, actual);
        }
    }

    std::cout << "\n=====================================" << std::endl;
    std::cout << "Test finished. Total: " << total_tests
              << "  Errors: " << error_cnt << std::endl;

    delete dut;
    return (error_cnt > 0) ? 1 : 0;
}
