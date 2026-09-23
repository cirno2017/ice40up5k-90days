`timescale 1ns / 1ps
`default_nettype none

module tb_wrap_Mux4v #(
    parameter int WIDTH = 8
) (
    input logic [1:0] sel,
    input logic [WIDTH-1:0] d0,
    input logic [WIDTH-1:0] d1,
    input logic [WIDTH-1:0] d2,
    input logic [WIDTH-1:0] d3,
    output logic [WIDTH-1:0] y
);

  Mux4v #(.WIDTH(WIDTH)) inst (.*);

endmodule

`default_nettype wire
