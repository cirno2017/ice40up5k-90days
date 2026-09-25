/**/
`timescale 1ns / 1ps
`default_nettype none

module top (
    input logic clk,
    input logic [7:0] d,
    output logic [7:0] blocking_assignment_q1,
    output logic [7:0] blocking_assignment_q2,
    output logic [7:0] non_blocking_assignment_q1,
    output logic [7:0] non_blocking_assignment_q2
);

  blocking_assignment u_blocking_assignment (
      .clk(clk),
      .d  (d),
      .q1 (blocking_assignment_q1),
      .q2 (blocking_assignment_q2)
  );

  non_blocking_assignment u_non_blocking_assignment (
      .clk(clk),
      .d  (d),
      .q1 (non_blocking_assignment_q1),
      .q2 (non_blocking_assignment_q2)
  );



endmodule

`default_nettype wire
