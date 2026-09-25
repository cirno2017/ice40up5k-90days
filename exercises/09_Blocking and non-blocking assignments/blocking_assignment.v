/**/
`timescale 1ns / 1ps
`default_nettype none

module blocking_assignment (
    input logic clk,
    input logic [7:0] d,
    output logic [7:0] q1,
    output logic [7:0] q2
);

  always @(posedge clk) begin
    /* verilator lint_off BLKSEQ */
    q1 = d;
    q2 = q1;
    /* verilator lint_on BLKSEQ */
  end

endmodule

`default_nettype wire
