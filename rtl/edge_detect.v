`timescale 1ns / 1ps
`default_nettype none

// 单比特边沿检测器
// 功能：
//   对已同步到 clk 域的输入 sig_sync 做边沿检测。
//   历史寄存器 sig_last 保存上一拍输入（非阻塞赋值），
//   两个事件输出由当前值与历史值组合产生，脉冲宽度恒为 1 个 clk。
// 真值表：
//   sig_last  sig_sync | rise_pulse  fall_pulse
//      0         0     |     0          0
//      0         1     |     1          0
//      1         0     |     0          1
//      1         1     |     0          0
// 约定：
//   - sig_last 初值为 1，与 sync_bit 两级寄存器初值一致（按键未按下为高），
//     上电后不会凭空产生事件；
//     iCE40 触发器支持配置期初始化，该初值会编入 bitstream，仿真亦按此启动。
module edge_detect (
    input  logic clk,
    input  logic sig_sync,    // 已同步到 clk 时钟域的输入
    output logic rise_pulse,  // 上升沿事件脉冲，持续 1 个 clk
    output logic fall_pulse   // 下降沿事件脉冲，持续 1 个 clk
);

  // 历史寄存器：上一拍的输入值
  logic sig_last = 1'b1;

  always_ff @(posedge clk) begin
    sig_last <= sig_sync;
  end

  // 组合输出：当前值与历史值比较
  assign rise_pulse = (~sig_last) & sig_sync;  // 0 -> 1：上升沿事件
  assign fall_pulse = sig_last & (~sig_sync);  // 1 -> 0：下降沿事件

endmodule
