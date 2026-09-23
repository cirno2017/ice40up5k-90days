#include <iostream>
#include <cstdint>
#include <cstdlib>
#include "Vtb_wrap_Mux2.h"
#include "verilated.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: ./Vtb_wrap_Mux2 <WIDTH>\n";
        return 1;
    }
    int WIDTH = std::atoi(argv[1]);
    uint32_t mask = ( (1U << WIDTH) - 1U ); // 根据位宽生成掩码

    VerilatedContext* ctxp = new VerilatedContext;
    ctxp->commandArgs(argc, argv);
    Vtb_wrap_Mux2* dut = new Vtb_wrap_Mux2{ctxp};

    uint32_t test_patterns[] = {0U, UINT32_MAX, 0x55555555U,0xaaaaaaaaU};
    int npat = sizeof(test_patterns)/sizeof(test_patterns[0]);

    int total=0, fail=0;
    std::cout << "==== Test Mux2 WIDTH=" << WIDTH << " ====\n";

    for(int sel=0;sel<=1;sel++){
        for(int pi0=0;pi0<npat;pi0++){
            for(int pi1=0;pi1<npat;pi1++){
                uint32_t d0_raw = test_patterns[pi0];
                uint32_t d1_raw = test_patterns[pi1];

                dut->sel = sel;
                dut->d0 = d0_raw & mask;
                dut->d1 = d1_raw & mask;
                dut->eval();

                uint32_t ref_y_raw = sel ? d1_raw : d0_raw;
                uint32_t ref_y = ref_y_raw & mask;
                uint32_t dut_y = static_cast<uint32_t>(dut->y) & mask;

                total++;
                if( dut_y != ref_y ){
                    fail++;
                    std::cerr << "[FAIL] sel="<<sel
                        <<" d0=0x"<<std::hex<<(d0_raw & mask)
                        <<" d1=0x"<<(d1_raw & mask)
                        <<" dut y=0x"<<dut_y
                        <<" ref y=0x"<<ref_y<<std::dec<<"\n";
                }else{
                    std::cout << "[OK] sel="<<sel
                        <<" d0=0x"<<std::hex<<(d0_raw & mask)
                        <<" d1=0x"<<(d1_raw & mask)
                        <<" y=0x"<<dut_y<<std::dec<<"\n";
                }
            }
        }
    }
    std::cout << "Mux2(WIDTH="<<WIDTH<<") summary: total="<<total<<" fail="<<fail;
    if(fail==0) std::cout << " >>> ALL PASS\n";
    else std::cout << " >>> SOME FAILED !!!\n";

    delete dut;
    delete ctxp;
    return fail>0?1:0;
}
