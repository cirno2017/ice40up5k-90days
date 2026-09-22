/**/
`timescale 1ns / 1ps
`default_nettype none

module top #(
    parameter int TOP_WIDTH = 8
) (
    input wire [TOP_WIDTH-1:0] data0,
    input wire [TOP_WIDTH-1:0] data1,
    input wire [TOP_WIDTH-1:0] data2,
    input wire [TOP_WIDTH-1:0] data3,
    input wire [1:0] top_sel,
    output wire [3:0] top_select,
    output wire [TOP_WIDTH-1:0] output_data
);

  decoder2to4 u_decoder2t4 (
      .sel(top_sel),
      .selected(top_select)
  );

  mux4 #(
      .WIDTH(TOP_WIDTH)
  ) u_mux4 (
      .sel(top_sel),
      .d0 (data0),
      .d1 (data1),
      .d2 (data2),
      .d3 (data3),
      .y  (output_data)
  );

endmodule

`default_nettype wire
