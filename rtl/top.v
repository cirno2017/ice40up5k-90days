//
//
`timescale 1ns / 1ps
`default_nettype none
module top #(
    parameter integer DIV_FREQ         = 6_000_000,
    parameter integer CNT_MAX          = DIV_FREQ - 1,     // 1s移位
    parameter integer DEBOUNCE_CNT_MAX = 120_000 - 1,      // 10ms消抖@12MHz
    parameter integer DIV_W            = $clog2(DIV_FREQ)
) (
    input logic clk,
    input logic stream_switch,  // 34脚：换向按键
    input logic pause_switch,  // 31脚：暂停/恢复按键
    output logic [7:0] led_stream
);

  logic [DIV_W-1:0] cnt;
  logic [7:0] bit_stream = 8'b00000001;
  logic direction = 1'b0;
  logic run_en    = 1'b1; // 1:运行，0:暂停

  //==================== 按键消抖（复用 debounce 模块）====================
  // 两路按键各自例化一个 debounce，输出“按下”脉冲。
  logic stream_fall;
  logic pause_fall;
  /* verilator lint_off UNUSEDSIGNAL */
  logic stream_stable;  // debounce 的 stable 输出，本设计未使用
  logic pause_stable;
  /* verilator lint_on UNUSEDSIGNAL */

  // 按键1：stream_switch 换向
  debounce #(
      .CNT_MAX(DEBOUNCE_CNT_MAX)
  ) u_debounce_stream (
      .clk       (clk),
      .btn       (stream_switch),
      .stable    (stream_stable),  // 未使用，见上方 lint 注释
      .fall_pulse(stream_fall)
  );

  // 按键2：pause_switch 暂停/恢复
  debounce #(
      .CNT_MAX(DEBOUNCE_CNT_MAX)
  ) u_debounce_pause (
      .clk       (clk),
      .btn       (pause_switch),
      .stable    (pause_stable),
      .fall_pulse(pause_fall)
  );

  //==================== 按键动作 ====================
  // 每检测到一次“按下”，换向标志翻转一次
  always_ff @(posedge clk) begin
    if (stream_fall) begin
      direction <= ~direction;
    end
  end

  // 每按一次，暂停/运行状态 run_en 翻转一次
  always_ff @(posedge clk) begin
    if (pause_fall) begin
      run_en <= ~run_en;
    end
  end

  //==================== 流水灯主逻辑 ====================
  always_ff @(posedge clk) begin
    if (run_en) begin  // 只有运行状态才计数移位
      if (cnt == DIV_W'(CNT_MAX)) begin
        cnt <= '0;
        if (direction) begin
          bit_stream <= {bit_stream[6:0], bit_stream[7]};  //左移
        end else begin
          bit_stream <= {bit_stream[0], bit_stream[7:1]};  //右移
        end
      end else begin
        cnt <= cnt + 1'b1;
      end
    end
    // run_en=0 时：cnt、bit_stream全部保持不变，LED冻结
  end

  assign led_stream = ~bit_stream;

endmodule
