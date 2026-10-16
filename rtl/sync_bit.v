`timescale 1ns / 1ps
`default_nettype none

// 单比特两级同步器
// 功能：
//   把异步输入 din_async 打两拍同步到 clk 时钟域，
//   降低亚稳态传播风险（同步器只提供额外恢复时间，不能保证完全消除亚稳态）。
// 约定：
//   - 第一级 sync1 只连接第二级，功能逻辑一律使用第二级输出 dout_sync；
//   - 两级初值均为 1，对应本项目按键“未按下为高”的状态，
//     上电后不会凭空产生按下事件；
//     iCE40 触发器支持配置期初始化，该初值会编入 bitstream，仿真亦按此启动。
module sync_bit (
    input  logic clk,
    input  logic din_async,  // 异步输入（本项目为按键：未按下 = 1）
    output logic dout_sync   // 同步到 clk 时钟域后的输出
);

  logic sync1 = 1'b1;  // 第1级：先“接住”异步输入，只馈给第2级
  logic sync2 = 1'b1;  // 第2级：同步到 clk 时钟域

  always_ff @(posedge clk) begin
    sync1 <= din_async;
    sync2 <= sync1;
  end

  assign dout_sync = sync2;

endmodule
