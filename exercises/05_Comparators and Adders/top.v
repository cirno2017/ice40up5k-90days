/**/
`timescale 1ns / 1ps
`default_nettype none

module top (
    input wire [7:0] a,
    input wire [7:0] b,
    output wire [7:0] sum,
    output wire carry,
    output wire lt_unsigned,
    output wire lt_signed,
    output wire signed_overflow
);
  wire [8:0] full_sum;
  assign full_sum = {1'b0, a} + {1'b0, b};
  assign sum = {full_sum[7:0]};
  assign carry = full_sum[8];
  assign lt_unsigned = a < b;
  assign lt_signed = $signed(a) < $signed(b);
  assign signed_overflow = (a[7] == b[7]) && (full_sum[7] != a[7]);

endmodule

`default_nettype wire
