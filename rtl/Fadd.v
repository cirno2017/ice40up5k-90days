/**/
`timescale 1ns / 1ps
`default_nettype none

module Fadd (
    input  logic a,
    input  logic b,
    input  logic cin,
    output logic sum,
    output logic cout
);
  assign sum  = a ^ b ^ cin;
  assign cout = (a & b) | (a & cin) | (b & cin);

endmodule

`default_nettype wire
