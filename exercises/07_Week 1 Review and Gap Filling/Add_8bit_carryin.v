/**/
`timescale 1ns / 1ps
`default_nettype none

module Add_8bit_carryin (
    input logic [7:0] d0,
    input logic [7:0] d1,
    input logic carry_input,
    output logic [7:0] sum,
    output logic carry_output
);
  logic [8:0] add_temp;
  assign add_temp = d0 + d1 + {8'b0, carry_input};
  assign sum = add_temp[7:0];
  assign carry_output = add_temp[8];

endmodule

`default_nettype wire
