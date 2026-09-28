`timescale 1ns / 1ps
`default_nettype none
module counter_sat_n #(
    parameter int N = 10
) (
    input logic clk,
    input logic rst,
    input logic en,
    output logic [WIDTH-1:0] count
);
  localparam int WIDTH = (N == 1) ? 1 : $clog2(N);
  // WIDTH'() 位宽转换，不是 logic'()
  localparam logic [WIDTH-1:0] MAXVAL = WIDTH'(N - 1);

  always_ff @(posedge clk) begin
    if (rst) begin
      count <= 'd0;
    end else if (en) begin
      /* verilator lint_off UNSIGNED */
      if (count < MAXVAL) begin
        /* verilator lint_on UNSIGNED */
        count <= count + 'd1;
      end else begin
        count <= count;
      end
    end
  end
endmodule
`default_nettype wire
