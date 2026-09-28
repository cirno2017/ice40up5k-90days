`timescale 1ns / 1ps
`default_nettype none
module counter_mod_n #(
    parameter int N = 10
) (
    input logic clk,
    input logic rst,
    input logic en,
    output logic [WIDTH-1:0] count
);
  localparam int WIDTH = (N == 1) ? 1 : $clog2(N);
  // 重点：使用 WIDTH'(N-1) 做位宽强制转换，不是 logic'()
  localparam logic [WIDTH-1:0] MAXVAL = WIDTH'(N - 1);

  always_ff @(posedge clk) begin
    if (rst) begin
      count <= 'd0;
    end else if (en) begin
      if (count == MAXVAL) begin
        count <= 'd0;
      end else begin
        count <= count + 'd1;
      end
    end
  end
endmodule
`default_nettype wire
