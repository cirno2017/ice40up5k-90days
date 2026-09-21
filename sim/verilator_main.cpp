#include <iostream>
#include <cstdint>
#include "Vtop.h"
#include "verilated.h"

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Vtop* dut = new Vtop;

    uint64_t total = 0;
    uint64_t error_cnt = 0;

    // 遍历 a:0~255, b:0~255
    for(uint32_t a = 0; a <= 255; a++) {
        for(uint32_t b = 0; b <= 255; b++) {
            dut->a = a;
            dut->b = b;
            dut->eval();
            total++;

            // ========== 参考模型计算 ==========
            uint16_t full_sum_ref = a + b;
            uint8_t sum_ref       = full_sum_ref & 0xFF;
            bool carry_ref        = (full_sum_ref >> 8) & 1;

            bool lt_unsigned_ref  = (a < b);

            // 有符号8位：转换为 int8_t
            int8_t sa = static_cast<int8_t>(a);
            int8_t sb = static_cast<int8_t>(b);
            bool lt_signed_ref    = (sa < sb);

            // 有符号溢出判断：同号相加，结果符号翻转
            int16_t ssum = sa + sb;
            bool signed_overflow_ref;
            if( (sa >=0 && sb >=0) || (sa <0 && sb <0) ) {
                int8_t res8 = static_cast<int8_t>(ssum);
                signed_overflow_ref = (res8 <0) ? (sa >=0) : (sa <0);
            } else {
                signed_overflow_ref = false;
            }

            // ========== DUT输出对比 ==========
            bool ok = true;
            if( dut->sum != sum_ref ) ok = false;
            if( dut->carry != carry_ref ) ok = false;
            if( dut->lt_unsigned != lt_unsigned_ref ) ok = false;
            if( dut->lt_signed != lt_signed_ref ) ok = false;
            if( dut->signed_overflow != signed_overflow_ref ) ok = false;

            if(!ok) {
                error_cnt++;
                std::cerr << "ERROR: a=" << (uint32_t)a
                          << " b=" << (uint32_t)b
                          << " | DUT sum:" << (uint32_t)dut->sum
                          << " carry:" << (int)dut->carry
                          << " lt_u:" << (int)dut->lt_unsigned
                          << " lt_s:" << (int)dut->lt_signed
                          << " ovf:" << (int)dut->signed_overflow
                          << " | REF sum:" << (uint32_t)sum_ref
                          << " carry:" << (int)carry_ref
                          << " lt_u:" << (int)lt_unsigned_ref
                          << " lt_s:" << (int)lt_signed_ref
                          << " ovf:" << (int)signed_overflow_ref
                          << std::endl;
            }
        }
    }

    std::cout << "Test finished. Total cases: " << total
              << " Errors: " << error_cnt << std::endl;

    delete dut;
    return error_cnt > 0 ? 1 : 0;
}
