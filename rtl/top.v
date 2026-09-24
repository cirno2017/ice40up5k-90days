/**/
`timescale 1ns / 1ps
`default_nettype none

module top (
    input logic clk,
    input logic en,
    input logic [7:0] d,
    output logic [7:0] dff8_q,
    output logic [7:0] reg8_en_q
);

  dff8 u_dff8 (
      .clk(clk),
      .d  (d),
      .q  (dff8_q)
  );

  reg8_en u_reg8_en (
      .clk(clk),
      .en (en),
      .d  (d),
      .q  (reg8_en_q)
  );

endmodule

`default_nettype wire
