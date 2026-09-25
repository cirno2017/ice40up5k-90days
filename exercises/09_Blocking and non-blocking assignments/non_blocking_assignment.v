/**/
`timescale 1ns / 1ps
`default_nettype none

module non_blocking_assignment (
    input logic clk,
    input logic [7:0] d,
    output logic [7:0] q1,
    output logic [7:0] q2
);

  always_ff @(posedge clk) begin
    q1 <= d;
    q2 <= q1;
  end

endmodule

`default_nettype wire
