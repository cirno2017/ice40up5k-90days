/**/
`timescale 1ns / 1ps
`default_nettype none

module and_gate (
    input  wire a,
    input  wire b,
    output wire c
);
  assign c = a & b;
endmodule
