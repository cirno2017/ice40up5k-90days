/**/
`timescale 1ns / 1ps
`default_nettype none

module decoder2to4 (
    input  wire [1:0] sel,
    output reg  [3:0] selected
);
  always_comb begin
    selected = 4'b0000;
    case (sel)
      2'b00:   selected = 4'b0001;
      2'b01:   selected = 4'b0010;
      2'b10:   selected = 4'b0100;
      2'b11:   selected = 4'b1000;
      default: selected = 4'b0000;
    endcase
  end

endmodule

`default_nettype wire
