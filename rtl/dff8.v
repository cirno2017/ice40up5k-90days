/**/
`timescale 1ns / 1ps
`default_nettype none

module dff8 (
    input logic clk,
    input logic [7:0] d,
    output logic [7:0] q
);

  always_ff @(posedge clk) begin
    q <= d;
  end

endmodule

`default_nettype wire
