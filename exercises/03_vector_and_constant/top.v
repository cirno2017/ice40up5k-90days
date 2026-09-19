/**/
`timescale 1ns / 1ps
`default_nettype none


module top (
    input  wire [7:0] switches,
    output wire [7:0] leds,
    output wire [7:0] leds2
);
  assign leds  = ~switches;  // PMOD LED板是1灭0亮，和拨码开关板逻辑以及外接LED灯逻辑相反，所以做反相把逻辑纠正过来
  genvar i;
  generate
    for (i = 0; i < 8; i = i + 1) begin : gen_reverse
      assign leds2[i] = switches[7-i];
    end
  endgenerate


endmodule

`default_nettype wire
