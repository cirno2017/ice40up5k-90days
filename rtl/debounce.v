`timescale 1ns / 1ps
`default_nettype none

// 通用按键消抖模块
// 功能：
//   1) 对异步按键输入打两拍同步到 clk 时钟域，降低亚稳态传播风险
//      （同步器只提供额外恢复时间，不能保证完全消除亚稳态）；
//   2) 只有当输入电平连续保持 CNT_MAX+1 个时钟周期不变时，才更新稳定电平；
//   3) 输出稳定电平 stable，以及下降沿脉冲 fall_pulse（按下瞬间拉高 1 个时钟）。
module debounce #(
    parameter integer CNT_MAX = 120_000 - 1  // 消抖时长(时钟数)，12MHz 下 10ms
) (
    input  logic clk,
    input  logic btn,        // 异步按键输入（低电平有效：按下为 0）
    output logic stable,     // 消抖后的稳定电平
    output logic fall_pulse  // 按下事件脉冲，持续 1 个 clk
);

  localparam integer CNTWIDTH = $clog2(CNT_MAX + 1);

  // ---------- 第1步：跨时钟域同步（打两拍，降低亚稳态传播风险）----------
  // 例化 sync_bit 同步器（其内部即为两级寄存器，此处不再额外串联）。
  // 按键未按下为高，sync_bit 两级初值均为 1，上电后不会凭空产生按下事件
  logic sync2;
  sync_bit u_sync (
      .clk      (clk),
      .din_async(btn),   // 第1级：先“接住”异步输入
      .dout_sync(sync2)  // 第2级：同步到 clk 时钟域，后续逻辑只用这一级
  );

  // ---------- 第2步：消抖计数 ----------
  // 与稳定电平一致   -> 计数器清零；
  // 与稳定电平不一致 -> 累加计时，新电平持续满 CNT_MAX+1 拍才采信。
  logic [CNTWIDTH-1:0] debounce_cnt = '0;

  // 上电启动状态：stable 直接取“未按下”（高电平），
  // 与 sync1/sync2/stable_last 的初值一致，上电不会产生伪按下事件。
  // iCE40 触发器支持配置期初始化，yosys/nextpnr 会把初值编入 bitstream，
  // 仿真模型同样按此初值启动。
  initial stable = 1'b1;
  always_ff @(posedge clk) begin
    if (sync2 != stable) begin
      if (debounce_cnt == CNT_MAX[CNTWIDTH-1:0]) begin
        stable <= sync2;  // 新电平已连续保持 CNT_MAX+1 个时钟，采信
        debounce_cnt <= '0;
      end else begin
        debounce_cnt <= debounce_cnt + 1'b1;  // 新电平持续中，继续累计
      end
    end else begin
      debounce_cnt <= '0;  // 电平与稳定值一致，无需计时
    end
  end

  // ---------- 第3步：边沿检测 ----------
  logic stable_last = 1'b1;  // 与 stable 初值一致，上电不会出现伪下降沿
  always_ff @(posedge clk) begin
    stable_last <= stable;
  end
  assign fall_pulse = stable_last & (~stable);  // 1 -> 0 下降沿 = 按下

endmodule
