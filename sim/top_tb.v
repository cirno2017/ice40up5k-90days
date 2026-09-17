/**/
`timescale 1ns / 1ps
`default_nettype none

/*
 * 模块名称：top_tb
 *
 * 这是 top 模块的 testbench（仿真测试平台）。testbench 不会被综合到
 * FPGA 中，它只负责在仿真器里产生输入信号、观察输出并自动判断结果。
 *
 * 本 testbench 验证以下颜色顺序：
 *
 *     红色 -> 绿色 -> 蓝色 -> 红色
 *
 * `timescale 1ns / 1ps 表示：
 *   - 仿真时间单位为 1 ns；
 *   - 仿真时间精度为 1 ps。
 * 因此下面的 #5 表示等待 5 ns。
 */
module top_tb;

  /*
     * clk 由 testbench 主动驱动，所以声明为 reg。
     * 三个 LED 信号由被测模块 dut 驱动，所以声明为 wire。
     */
  reg  clk;
  wire led_r;
  wire led_g;
  wire led_b;

  /*
     * 实例化被测模块 top，并把实例命名为 dut。
     * dut 是 Device Under Test（被测设计）的缩写。
     *
     * 实际硬件默认使用 23 位计数器，需要等待 2^23 个时钟才换色。
     * 仿真时把 COUNTER_BITS 改为 3，因此只需等待 2^3 = 8 个时钟
     * 就会切换颜色，可以大幅缩短仿真时间。
     */
  top #(
      .COUNTER_BITS(3)
  ) dut (
      .clk  (clk),
      .led_r(led_r),
      .led_g(led_g),
      .led_b(led_b)
  );

  /*
     * 生成 100 MHz 仿真时钟。
     *
     * clk 每等待 5 ns 翻转一次，因此完整周期为 10 ns：
     *
     *     1 / 10 ns = 100 MHz
     *
     * 这里不必模拟真实的 12 MHz，因为本测试只验证数字逻辑和颜色顺序，
     * 不验证现实时间。使用更快的仿真时钟可以更快完成测试。
     */
  initial begin
    // 在仿真开始时把时钟初始化为低电平，避免出现未知值 x。
    clk = 1'b0;

    // forever 会无限循环；每隔 5 ns 将 clk 取反一次。
    forever #5 clk = ~clk;
  end

  /*
     * 主测试过程。
     *
     * initial 块在仿真开始时执行一次。该过程依次检查初始红色状态、
     * 8 个时钟后的绿色状态、再过 8 个时钟后的蓝色状态，以及最后
     * 回到红色的状态。
     */
  initial begin
    /*
         * 生成 VCD 波形文件，可使用 GTKWave 查看。
         * $dumpvars(0, top_tb) 表示记录 top_tb 及其下层模块中的全部信号。
         */
    $dumpfile("build/top_tb.vcd");
    $dumpvars(0, top_tb);

    /*
         * 等待 1 ns，让寄存器初始值和组合逻辑输出稳定。
         *
         * {led_r, led_g, led_b} 将三个一位信号拼成三位向量。
         * LED 为低电平有效，所以：
         *   3'b011 表示红灯亮；
         *   3'b101 表示绿灯亮；
         *   3'b110 表示蓝灯亮。
         *
         * “!==”是四态不全等比较，同时检查 0、1、x 和 z。仿真输出
         * 如果出现未知值 x，也会被判定为错误，比普通“!=”更适合测试。
         */
    #1;
    if ({led_r, led_g, led_b} !== 3'b011) $fatal(1, "FAIL: expected red");

    /*
         * repeat (8) 表示重复等待 8 次 clk 上升沿。
         * 第 8 个上升沿到来时，3 位计数器溢出，颜色更新为绿色。
         * 随后的 #1 用来等待非阻塞赋值和组合逻辑完成更新后再检查。
         */
    repeat (8) @(posedge clk);
    #1;
    if ({led_r, led_g, led_b} !== 3'b101) $fatal(1, "FAIL: expected green");

    // 再等待 8 个时钟，检查绿色之后是否切换为蓝色。
    repeat (8) @(posedge clk);
    #1;
    if ({led_r, led_g, led_b} !== 3'b110) $fatal(1, "FAIL: expected blue");

    // 再等待 8 个时钟，检查蓝色之后是否回到红色。
    repeat (8) @(posedge clk);
    #1;
    if ({led_r, led_g, led_b} !== 3'b011) $fatal(1, "FAIL: expected red again");

    /*
         * 所有检查均通过时打印 PASS。
         * 如果前面的任意检查失败，$fatal 会立即结束仿真并返回非零状态，
         * 从而让 make sim 明确报告失败。
         */
    $display("PASS: red -> green -> blue -> red");

    // 主动结束仿真，否则产生时钟的 forever 循环会一直运行。
    $finish;
  end

endmodule

// 恢复默认网络类型，避免影响其他一起编译的 Verilog 文件。
`default_nettype wire
