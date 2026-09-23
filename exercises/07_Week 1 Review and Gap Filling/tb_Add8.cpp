#include <iostream>
#include <cstdint>
#include "VAdd_8bit_carryin.h"
#include "verilated.h"

int main(int argc, char** argv) {
    VerilatedContext* ctxp = new VerilatedContext;
    ctxp->commandArgs(argc, argv);
    VAdd_8bit_carryin* dut = new VAdd_8bit_carryin{ctxp};

    int total=0, fail=0;
    std::cout << "==== Test Add_8bit_carryin 8bit adder ====\n";
    // d0[7:0], d1[7:0], carry_input(1bit)
    for(uint16_t d0=0;d0<=0xff;d0++){
        for(uint16_t d1=0;d1<=0xff;d1++){
            for(int cin=0;cin<=1;cin++){
                dut->d0 = d0;
                dut->d1 = d1;
                dut->carry_input = cin;
                dut->eval();

                uint16_t ref_total = d0 + d1 + cin;
                uint8_t ref_sum = ref_total & 0xff;
                int ref_cout = (ref_total >> 8) & 1;

                total++;
                if( (uint8_t)dut->sum != ref_sum || (int)dut->carry_output != ref_cout ){
                    fail++;
                    std::cerr << "[FAIL] d0=0x"<<std::hex<<(int)d0
                        <<" d1=0x"<<(int)d1<<" cin="<<cin
                        <<" dut sum=0x"<<(int)dut->sum<<" cout="<<(int)dut->carry_output
                        <<" ref sum=0x"<<(int)ref_sum<<" cout="<<ref_cout<<std::dec<<"\n";
                    // 遇到错误可以break;
                }
            }
        }
    }
    std::cout << "Add8 summary: total="<<total<<" fail="<<fail;
    if(fail==0) std::cout << " >>> ALL PASS\n";
    else std::cout << " >>> SOME FAILED !!!\n";

    delete dut;
    delete ctxp;
    return fail>0?1:0;
}
