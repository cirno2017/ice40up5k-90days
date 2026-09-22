/**/
`timescale 1ns / 1ps
`default_nettype none

module mux4 #(
    parameter int WIDTH = 8
) (
    input wire [1:0] sel,
    input wire [WIDTH-1:0] d0,
    input wire [WIDTH-1:0] d1,
    input wire [WIDTH-1:0] d2,
    input wire [WIDTH-1:0] d3,
    output reg [WIDTH-1:0] y
);
  assign y = (sel == 2'b00) ? d0 : ((sel == 2'b01) ? d1 : ((sel == 2'b10) ? d2 : d3));

endmodule

`default_nettype wire
