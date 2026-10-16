1. LED灯采用PMOD板与开发板PMOD2接口连接，具体连接关系为：
led_stream[0] 45
led_stream[1] 43
led_stream[2] 38
led_stream[3] 36
led_stream[4] 46
led_stream[5] 44
led_stream[6] 42
led_stream[7] 37
按钮开关使用一个8按钮开关模组，K7是流水灯换向，连接到34端口，K8是流水灯暂停/恢复，连接到31端口.
按钮开关模组的VCC端口连接到开发板的3.3V端口，由开发板供电，GND端口与开发板的GND端口连接，共地.
未按下按钮时的高电平，是由开发板的3.3V提供的.

2. 当前构建命令是make all，实际执行的是
nextpnr-ice40 \
        --up5k \
        --package sg48 \
        --json build/top.json \
        --pcf constraints/icesugar.pcf \
        --asc build/top.asc \
        --freq 12 \
        --log build/nextpnr.log

仿真命令是make vsim，实际执行的是
verilator \
        --cc \
        --exe \
        --build \
        -j 0 \
        -Wall \
        --top-module top \
        --trace \
        --Mdir build/verilator \
        -GDIV_FREQ=6000000 \
        -o Vtop_6000000 \
        /home/cirno/code/ice40up5k-90days/rtl/debounce.v /home/cirno/code/ice40up5k-90days/rtl/top.v \
        /home/cirno/code/ice40up5k-90days/sim/verilator_main.cpp

烧录命令是make prog，实际执行的是icesprog build/top.bin.

3. 资源占用情况:
Printing statistics.
   Number of wires:                 60
   Number of wire bits:            312
   Number of public wires:          60
   Number of public wire bits:     312
   Number of memories:               0
   Number of memory bits:            0
   Number of processes:              0
   Number of cells:                229
     SB_CARRY                       51
     SB_DFF                          6
     SB_DFFE                        12
     SB_DFFESR                      23
     SB_DFFSR                       34
     SB_LUT4                       103

12MHz时序结果:
Info: Max frequency for clock 'clk$SB_IO_IN_$glb_clk': 52.70 MHz (PASS at 12.00 MHz)
Info: Max delay <async>                       -> posedge clk$SB_IO_IN_$glb_clk: 3.97 ns
Info: Max delay posedge clk$SB_IO_IN_$glb_clk -> <async>                      : 11.62 ns

4. FPGA实际运行情况，可清晰的看到每0.5秒LED灯位移一格，K7按键可正常换向，K8按键可正常暂停/恢复.