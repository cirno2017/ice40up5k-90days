`timescale 1ns / 1ps
`default_nettype none

// 仿真顶层（仅用于 edge_detect 单元测试，不参与 FPGA 综合）：
//   sync_bit -> edge_detect 链。
// C++ 直接驱动异步输入 din_async；edge_detect 接收的 sig_sync 是
// sync_bit 第二级寄存器输出，即同一 clk 域的寄存器信号。
module sync_edge_top (
    input  logic clk,
    input  logic din_async,   // 异步输入（按键：未按下 = 1）
    output logic dout_sync,   // sync_bit 同步输出
    output logic rise_pulse,  // edge_detect 上升沿事件脉冲
    output logic fall_pulse   // edge_detect 下降沿事件脉冲
);

  sync_bit u_sync (
      .clk      (clk),
      .din_async(din_async),
      .dout_sync(dout_sync)
  );

  edge_detect u_edge (
      .clk       (clk),
      .sig_sync  (dout_sync),
      .rise_pulse(rise_pulse),
      .fall_pulse(fall_pulse)
  );

endmodule
