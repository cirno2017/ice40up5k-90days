/**/
`timescale 1ns / 1ps
`default_nettype none

/*
 * 模块名称：top
 *
 * 功能：
 *   使用开发板上的 12 MHz 时钟驱动计数器，并按照
 *   “红色 -> 绿色 -> 蓝色 -> 红色”的顺序循环点亮板载 RGB LED。
 *
 * 说明：
 *   iCESugar 板载 RGB LED 是低电平有效：
 *     输出 1：该颜色熄灭；
 *     输出 0：该颜色点亮。
 *
 * COUNTER_BITS 用来设置计数器位宽。
 * N 位二进制计数器每经过 2^N 个时钟周期溢出一次。
 * 默认值为 23，在 12 MHz 时钟下，每种颜色持续时间约为：
 *
 *     2^23 / 12,000,000 ≈ 0.699 秒
 *
 * 将计数器位宽做成参数，可以让硬件综合与仿真使用不同的位宽：
 *   - 实际硬件使用 23 位，肉眼可以看到颜色切换；
 *   - testbench 使用较小位宽，避免等待数百万个仿真时钟。
 */
module top #(
    parameter integer COUNTER_BITS = 23
) (
    input  clk,    // 12 MHz 板载时钟输入
    output led_r,  // RGB LED 红色通道，低电平点亮
    output led_g,  // RGB LED 绿色通道，低电平点亮
    output led_b   // RGB LED 蓝色通道，低电平点亮
);

  /*
     * 分频计数器。
     *
     * {COUNTER_BITS{1'b0}} 是 Verilog 的重复拼接写法，表示生成
     * COUNTER_BITS 个 0，用来把整个计数器初始化为 0。
     */
  reg [COUNTER_BITS-1:0] counter = {COUNTER_BITS{1'b0}};

  /*
     * 当前颜色状态：
     *   2'd0：红色；
     *   2'd1：绿色；
     *   2'd2：蓝色。
     *
     * 两位寄存器还可以表示 2'd3，但下面的状态切换逻辑不会进入该状态。
     */
  reg [1:0] color = 2'd0;

  /*
     * 时序逻辑：只在 clk 的上升沿执行。
     *
     * 时序逻辑通常使用非阻塞赋值“<=”。所有非阻塞赋值会在当前
     * 时钟事件结束时统一更新，符合多个触发器并行工作的硬件行为。
     */
  always @(posedge clk) begin
    // 每个时钟上升沿加 1；达到最大值后自然溢出并回到 0。
    counter <= counter + 1;

    /*
         * &counter 是“归约与”运算：只有 counter 的每一位都为 1 时，
         * 结果才为 1。因此这里检测的是计数器即将从最大值溢出到 0
         * 的那个时钟周期。
         */
    if (&counter) begin
      // 蓝色结束后回到红色，否则进入下一个颜色状态。
      if (color == 2'd2) color <= 2'd0;
      else color <= color + 1'b1;
    end
  end

  /*
     * 组合逻辑输出。
     *
     * 条件运算符格式为：条件 ? 条件成立时的值 : 条件不成立时的值。
     * 因为 LED 低电平点亮，所以当前颜色对应的输出为 0，其他通道为 1。
     * 任意时刻只会点亮一个颜色通道。
     */
  assign led_r = (color == 2'd0) ? 1'b0 : 1'b1;
  assign led_g = (color == 2'd1) ? 1'b0 : 1'b1;
  assign led_b = (color == 2'd2) ? 1'b0 : 1'b1;

endmodule

// 恢复 Verilog 默认的隐式网络行为，避免影响同一次编译中的其他文件。
`default_nettype wire
