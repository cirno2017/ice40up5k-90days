#include <iostream>
#include <cstdint>
#include <cstdlib>
#include "Vtb_wrap_Mux4v.h"
#include "verilated.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: ./Vtb_wrap_Mux4v <WIDTH>\n";
        return 1;
    }
    int WIDTH = std::atoi(argv[1]);
    uint32_t mask = ( (1U << WIDTH) -1U );

    VerilatedContext* ctxp = new VerilatedContext;
    ctxp->commandArgs(argc, argv);
    Vtb_wrap_Mux4v* dut = new Vtb_wrap_Mux4v{ctxp};

    uint32_t test_patterns[] = {0U, UINT32_MAX, 0x55555555U,0xaaaaaaaaU};
    int npat = sizeof(test_patterns)/sizeof(test_patterns[0]);

    int total=0, fail=0;
    std::cout << "==== Test Mux4v WIDTH=" << WIDTH << " ====\n";

    for(int sel=0;sel<=3;sel++){
        for(int p0=0;p0<npat;p0++){
            for(int p1=0;p1<npat;p1++){
                for(int p2=0;p2<npat;p2++){
                    for(int p3=0;p3<npat;p3++){
                        uint32_t d0_raw = test_patterns[p0];
                        uint32_t d1_raw = test_patterns[p1];
                        uint32_t d2_raw = test_patterns[p2];
                        uint32_t d3_raw = test_patterns[p3];

                        dut->sel = sel;
                        dut->d0 = d0_raw & mask;
                        dut->d1 = d1_raw & mask;
                        dut->d2 = d2_raw & mask;
                        dut->d3 = d3_raw & mask;
                        dut->eval();

                        uint32_t ref_y_raw;
                        if(sel==0) ref_y_raw = d0_raw;
                        else if(sel==1) ref_y_raw = d1_raw;
                        else if(sel==2) ref_y_raw = d2_raw;
                        else ref_y_raw = d3_raw;

                        uint32_t ref_y = ref_y_raw & mask;
                        uint32_t dut_y = static_cast<uint32_t>(dut->y) & mask;

                        total++;
                        if( dut_y != ref_y ){
                            fail++;
                            std::cerr << "[FAIL] sel="<<sel
                                <<" d0=0x"<<std::hex<<(d0_raw & mask)
                                <<" d1=0x"<<(d1_raw & mask)
                                <<" d2=0x"<<(d2_raw & mask)
                                <<" d3=0x"<<(d3_raw & mask)
                                <<" dut y=0x"<<dut_y
                                <<" ref y=0x"<<ref_y<<std::dec<<"\n";
                        }else{
                            std::cout << "[OK] sel="<<sel
                                <<" d0=0x"<<std::hex<<(d0_raw & mask)
                                <<" d1=0x"<<(d1_raw & mask)
                                <<" d2=0x"<<(d2_raw & mask)
                                <<" d3=0x"<<(d3_raw & mask)
                                <<" y=0x"<<dut_y<<std::dec<<"\n";
                        }
                    }
                }
            }
        }
    }
    std::cout << "Mux4v(WIDTH="<<WIDTH<<") summary: total="<<total<<" fail="<<fail;
    if(fail==0) std::cout << " >>> ALL PASS\n";
    else std::cout << " >>> SOME FAILED !!!\n";

    delete dut;
    delete ctxp;
    return fail>0?1:0;
}
