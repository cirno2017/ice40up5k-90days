/**/
`timescale 1ns / 1ps
`default_nettype none

module or_xor_gate (
    input  wire a,
    input  wire b,
    output wire c,
    output wire d
);
  assign c = a | b;
  assign d = a ^ b;
endmodule

