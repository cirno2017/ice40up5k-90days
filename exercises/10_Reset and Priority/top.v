`timescale 1ns / 1ps
`default_nettype none

module top (
    input logic clk,
    input logic rst,
    input logic clear,
    input logic en,
    output logic [7:0] count
);

  always_ff @(posedge clk) begin
    if (rst) begin
      count <= 8'h00;
    end else if (clear) begin
      count <= 8'h00;
    end else if (en) begin
      count <= count + 8'h01;
    end
  end

endmodule

`default_nettype wire
