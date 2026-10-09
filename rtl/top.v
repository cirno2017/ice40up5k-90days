/**/
`timescale 1ns / 1ps
`default_nettype none

module top #(
    parameter integer DIV_FREQ = 6_000_000
) (
    input  logic clk,
    input  logic rst_n,  // 板载开关 BTN_N，低电平复位
    output logic led_n   // RGB 绿色通道，低电平点亮
);

  logic tick;
  logic led_state;

  tick_gen #(
      .DIV_FREQ(DIV_FREQ)
  ) u_tick_gen (
      .clk (clk),
      .rst (~rst_n),
      .tick(tick)
  );

  always_ff @(posedge clk) begin
    if (!rst_n) begin
      led_state <= 1'b0;
    end else if (tick) begin
      led_state <= ~led_state;
    end
  end

  assign led_n = ~led_state;

endmodule
