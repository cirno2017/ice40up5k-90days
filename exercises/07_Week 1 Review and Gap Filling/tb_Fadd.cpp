#include <iostream>
#include <iomanip>
#include "VFadd.h"
#include "verilated.h"

int main(int argc, char** argv) {
    VerilatedContext* ctxp = new VerilatedContext;
    ctxp->commandArgs(argc, argv);
    VFadd* dut = new VFadd{ctxp};

    int total = 0;
    int fail = 0;

    std::cout << "==== Test Fadd (Full Adder, 1bit) ====\n";
    // 穷举 a,b,cin 全部8种组合
    for(int a=0;a<=1;a++){
        for(int b=0;b<=1;b++){
            for(int cin=0;cin<=1;cin++){
                dut->a = a;
                dut->b = b;
                dut->cin = cin;
                dut->eval();

                int ref_sum  = (a ^ b ^ cin);
                int ref_cout = ((a&b)|(a&cin)|(b&cin));

                total++;
                if( (int)dut->sum != ref_sum || (int)dut->cout != ref_cout ){
                    fail++;
                    std::cerr << "[FAIL] a="<<a<<" b="<<b<<" cin="<<cin
                        << " | dut sum="<<(int)dut->sum<<" cout="<<(int)dut->cout
                        << " | ref sum="<<ref_sum<<" cout="<<ref_cout<<"\n";
                }else{
                    std::cout << "[OK]  a="<<a<<" b="<<b<<" cin="<<cin
                        << " sum="<<(int)dut->sum<<" cout="<<(int)dut->cout<<"\n";
                }
            }
        }
    }
    std::cout << "Fadd summary: total="<<total<<" fail="<<fail;
    if(fail==0) std::cout << " >>> ALL PASS\n";
    else std::cout << " >>> SOME FAILED !!!\n";

    delete dut;
    delete ctxp;
    return fail>0 ? 1 : 0;
}
