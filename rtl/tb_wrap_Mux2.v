`timescale 1ns / 1ps
`default_nettype none

module tb_wrap_Mux2 #(
    parameter int WIDTH = 8
) (
    input logic sel,
    input logic [WIDTH-1:0] d0,
    input logic [WIDTH-1:0] d1,
    output logic [WIDTH-1:0] y
);

  Mux2 #(.WIDTH(WIDTH)) inst (.*);

endmodule

`default_nettype wire
