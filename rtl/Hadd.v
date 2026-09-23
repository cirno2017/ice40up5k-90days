/**/
`timescale 1ns / 1ps
`default_nettype none

module Hadd (
    input  logic a,
    input  logic b,
    output logic sum,
    output logic carry
);

  assign sum   = a ^ b;
  assign carry = a & b;

endmodule

`default_nettype wire
