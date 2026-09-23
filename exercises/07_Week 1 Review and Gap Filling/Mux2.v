/**/
`timescale 1ns / 1ps
`default_nettype none

module Mux2 #(
    parameter int WIDTH = 8
) (
    input logic sel,
    input logic [WIDTH-1:0] d0,
    input logic [WIDTH-1:0] d1,
    output logic [WIDTH-1:0] y
);

  assign y = sel ? d1 : d0;

endmodule

`default_nettype wire
