/**/
`timescale 1ns / 1ps
`default_nettype none

module inverter (
    input  wire a,
    output wire c
);
  assign c = ~a;
endmodule
