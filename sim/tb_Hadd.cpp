#include <iostream>
#include "VHadd.h"
#include "verilated.h"

int main(int argc, char** argv) {
    VerilatedContext* ctxp = new VerilatedContext;
    ctxp->commandArgs(argc, argv);
    VHadd* dut = new VHadd{ctxp};

    int total=0, fail=0;
    std::cout << "==== Test Hadd (Half Adder,1bit) ====\n";
    for(int a=0;a<=1;a++){
        for(int b=0;b<=1;b++){
            dut->a = a;
            dut->b = b;
            dut->eval();
            int ref_sum = a ^ b;
            int ref_carry = a & b;
            total++;
            if( (int)dut->sum!=ref_sum || (int)dut->carry!=ref_carry ){
                fail++;
                std::cerr << "[FAIL] a="<<a<<" b="<<b
                    << " dut sum="<<(int)dut->sum<<" carry="<<(int)dut->carry
                    << " ref sum="<<ref_sum<<" carry="<<ref_carry<<"\n";
            }else{
                std::cout << "[OK] a="<<a<<" b="<<b
                    << " sum="<<(int)dut->sum<<" carry="<<(int)dut->carry<<"\n";
            }
        }
    }
    std::cout << "Hadd summary: total="<<total<<" fail="<<fail;
    if(fail==0) std::cout << " >>> ALL PASS\n";
    else std::cout << " >>> SOME FAILED !!!\n";

    delete dut;
    delete ctxp;
    return fail>0 ?1:0;
}
