/**/
`timescale 1ns / 1ps
`default_nettype none

module tick_gen #(
    parameter integer DIV_FREQ = 6_000_000,
    parameter integer DIV = DIV_FREQ - 1,
    parameter integer DIV_W = $clog2(DIV_FREQ)
) (
    input  logic clk,
    input  logic rst,
    output logic tick
);
  localparam logic [DIV_W-1:0] DivC = DIV[DIV_W-1:0];

  logic [DIV_W-1:0] cnt;

  always_ff @(posedge clk) begin
    if (rst) begin
      cnt  <= 'd0;
      tick <= 1'b0;
    end else begin
      if (cnt == DivC) begin
        cnt  <= 'd0;
        tick <= 1'b1;
      end else begin
        cnt  <= cnt + 'd1;
        tick <= 1'b0;
      end
    end

  end

endmodule
