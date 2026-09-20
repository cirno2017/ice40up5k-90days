/**/
`timescale 1ns / 1ps
`default_nettype none


// 实际运行版本，d1 - d3写成固定的
module top (
    input  wire [1:0] switches,
    output reg  [7:0] leds2
);

  always_comb begin
    leds2 = 8'h00;
    case (switches)
      2'b00:   leds2 = 8'b10101010;
      2'b01:   leds2 = 8'b01010101;
      2'b10:   leds2 = 8'b11001100;
      2'b11:   leds2 = 8'b00110011;
      default: leds2 = 8'h00;
    endcase
  end

endmodule

/* if-else version
module top (
    input  wire [7:0] d0,
    input  wire [7:0] d1,
    input  wire [7:0] d2,
    input  wire [7:0] d3,
    input  wire [1:0] sel,
    output reg  [7:0] y
);

  always_comb begin
    y = 8'h00;
    if (sel == 2'b00) y = d0;
    else if (sel == 2'b01) y = d1;
    else if (sel == 2'b10) y = d2;
    else if (sel == 2'b11) y = d3;
    else y = 8'h00;
  end

endmodule
*/

/* case version
module top (
    input  wire [7:0] d0,
    input  wire [7:0] d1,
    input  wire [7:0] d2,
    input  wire [7:0] d3,
    input  wire [1:0] sel,
    output reg  [7:0] y
);

  always_comb begin
    y = 8'h00;
    case (sel)
      2'b00:   y = d0;
      2'b01:   y = d1;
      2'b10:   y = d2;
      2'b11:   y = d3;
      default: y = 8'h00;
    endcase
  end

endmodule
*/



/* Ternary operator version
module top (
    input  wire [7:0] d0,
    input  wire [7:0] d1,
    input  wire [7:0] d2,
    input  wire [7:0] d3,
    input  wire [1:0] sel,
    output wire [7:0] y
);

  assign y = sel[1] ? (sel[0] ? d3 : d2) : (sel[0] ? d1 : d0);

endmodule
*/
`default_nettype wire
