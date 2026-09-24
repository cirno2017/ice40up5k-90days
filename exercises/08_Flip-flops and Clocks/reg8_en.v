/**/
`timescale 1ns / 1ps
`default_nettype none

module reg8_en (
    input logic clk,
    input logic en,
    input logic [7:0] d,
    output logic [7:0] q
);

  always_ff @(posedge clk) begin
    if (en) q <= d;
  end

endmodule

`default_nettype wire
