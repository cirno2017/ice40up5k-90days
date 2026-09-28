`timescale 1ns / 1ps
`default_nettype none
module top (
    input logic clk,
    input logic rst,
    input logic en,

    output logic [0:0] mod1_out,
    sat1_out,  // N=1 W=1
    output logic [0:0] mod2_out,
    sat2_out,  // N=2 W=1
    output logic [3:0] mod10_out,
    sat10_out,  // N=10 W=4
    output logic [7:0] mod256_out,
    sat256_out  // N=256 W=8
);

  counter_mod_n #(
      .N(1)
  ) u_mod1 (
      .clk,
      .rst,
      .en,
      .count(mod1_out)
  );
  counter_sat_n #(
      .N(1)
  ) u_sat1 (
      .clk,
      .rst,
      .en,
      .count(sat1_out)
  );

  counter_mod_n #(
      .N(2)
  ) u_mod2 (
      .clk,
      .rst,
      .en,
      .count(mod2_out)
  );
  counter_sat_n #(
      .N(2)
  ) u_sat2 (
      .clk,
      .rst,
      .en,
      .count(sat2_out)
  );

  counter_mod_n #(
      .N(10)
  ) u_mod10 (
      .clk,
      .rst,
      .en,
      .count(mod10_out)
  );
  counter_sat_n #(
      .N(10)
  ) u_sat10 (
      .clk,
      .rst,
      .en,
      .count(sat10_out)
  );

  counter_mod_n #(
      .N(256)
  ) u_mod256 (
      .clk,
      .rst,
      .en,
      .count(mod256_out)
  );
  counter_sat_n #(
      .N(256)
  ) u_sat256 (
      .clk,
      .rst,
      .en,
      .count(sat256_out)
  );

endmodule
`default_nettype wire
