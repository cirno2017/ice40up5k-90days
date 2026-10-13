// ============================================================
// tb_blink: top(LED 0.5s 翻转) SystemVerilog 自检测试平台
// 时钟 always #5 产生，initial 编排激励，$dumpfile/$dumpvars 录波
// 输入在下降沿改变，上升沿后 #1 检查；首次有效复位前不验收
// ============================================================
`timescale 1ns / 1ps
`default_nettype none

module tb_blink;

  logic clk = 1'b0;
  logic rst_n4 = 1'b1;
  logic rst_n7 = 1'b1;
  logic led_n4;
  logic led_n7;

  top #(.DIV_FREQ(4)) dut4 (
      .clk  (clk),
      .rst_n(rst_n4),
      .led_n(led_n4)
  );

  top #(.DIV_FREQ(7)) dut7 (
      .clk  (clk),
      .rst_n(rst_n7),
      .led_n(led_n7)
  );

  /* verilator lint_off BLKSEQ */  // 时钟发生器是 testbench 惯用法
  always #5 clk = ~clk;
  /* verilator lint_on BLKSEQ */

  int errors = 0;
  bit en4 = 1'b0;  // 首次有效复位前不验收
  bit en7 = 1'b0;

  // 参考模型：仅在上升沿更新（tick 是寄存器输出，需要 1 bit 状态）
  /* verilator lint_off DECLFILENAME */  // 类定义在 tb 文件内，属 testbench 惯例
  class RefModel;
    int unsigned div;
    int unsigned elapsed = 0;
    bit tick_r = 0;
    bit led = 0;
    function new(int unsigned d);
      div = d;
    endfunction
    function void update(bit rst);
      if (rst) begin
        elapsed = 0;
        tick_r  = 0;
        led     = 0;
      end else begin
        if (tick_r) led = ~led;              // top 在 tick 为高的沿翻转
        tick_r  = (elapsed == div - 1);      // tick 是寄存器输出
        elapsed = (elapsed == div - 1) ? 0 : elapsed + 1;
      end
    endfunction
  endclass
  /* verilator lint_on DECLFILENAME */

  RefModel m4 = new(4);
  RefModel m7 = new(7);

  // 检查任务：比较 dut.tick 和 ~led_n，输出阶段、期望、实际值并累计错误
  task automatic check(string phase, bit exp_tick, bit exp_led, bit act_tick, bit act_led);
    bit ok = (act_tick === exp_tick) && (act_led === exp_led);
    $display("[%0t] %-24s tick exp=%b act=%b | led exp=%b act=%b : %s", $time, phase,
             exp_tick, act_tick, exp_led, act_led, ok ? "PASS" : "ERROR");
    if (!ok) errors++;
  endtask

  // 推进一个完整周期：下降沿改变输入，上升沿更新参考模型，#1 后检查
  task automatic cycle(input bit rn4, input bit rn7, string tag);
    @(negedge clk);
    rst_n4 = rn4;
    rst_n7 = rn7;
    @(posedge clk);
    #1;
    if (en4) begin
      m4.update(!rn4);
      check({tag, " [DIV=4]"}, m4.tick_r, m4.led, dut4.tick, ~led_n4);
    end
    if (en7) begin
      m7.update(!rn7);
      check({tag, " [DIV=7]"}, m7.tick_r, m7.led, dut7.tick, ~led_n7);
    end
  endtask

  initial begin
    $dumpfile("tb_blink.vcd");
    $dumpvars(0, tb_blink);

    // 上电自由跑 3 拍：首次有效复位前不验收
    repeat (3) @(posedge clk);

    // ---------------- 场景1：复位后连续运行（DIV=4） ----------------
    // 预期：第4拍 tick 高、第5拍 LED 翻转；第8拍 tick 高、第9拍 LED 再次翻转
    $display("=== Scenario 1: continuous run after reset (DIV=4) ===");
    en4 = 1;
    cycle(0, 1, "s1 reset");
    cycle(0, 1, "s1 reset");
    for (int i = 1; i <= 16; i++) cycle(1, 1, $sformatf("s1 run %0d", i));

    // ---------------- 场景2：LED=1 时复位（DIV=4） ----------------
    // 预期：下降沿拉低 rst_n 后 LED 保持；下一个上升沿清零；
    //       释放后重新等待5拍首次翻转
    $display("=== Scenario 2: reset while LED=1 (DIV=4) ===");
    for (int i = 1; i <= 5; i++) cycle(1, 1, $sformatf("s2 run %0d", i));  // 进入 led=1、tick=0 的拍
    @(negedge clk);
    rst_n4 = 0;
    #1;
    check("s2 hold @negedge", 0, 1, dut4.tick, ~led_n4);  // 下降沿后 LED 保持
    @(posedge clk);
    #1;
    m4.update(1);
    check("s2 clear @posedge", 0, 0, dut4.tick, ~led_n4);  // 上升沿清零
    cycle(0, 1, "s2 reset");
    for (int i = 1; i <= 12; i++) cycle(1, 1, $sformatf("s2 rerun %0d", i));

    // ---------------- 场景3：tick=1 时复位（DIV=4） ----------------
    // 预期：下降沿拉低 rst_n 后 tick 仍为1、LED 保持；
    //       下一个上升沿两者清零，取消待执行的翻转
    // 场景2结束时恰处于 tick=1、led=0 的拍
    $display("=== Scenario 3: reset while tick=1 (DIV=4) ===");
    @(negedge clk);
    rst_n4 = 0;
    #1;
    check("s3 hold @negedge", 1, 0, dut4.tick, ~led_n4);  // tick 仍为1、LED 保持
    @(posedge clk);
    #1;
    m4.update(1);
    check("s3 clear @posedge", 0, 0, dut4.tick, ~led_n4);  // 上升沿两者清零
    cycle(0, 1, "s3 reset");
    for (int i = 1; i <= 8; i++) cycle(1, 1, $sformatf("s3 rerun %0d", i));

    // ---------------- DIV=7：复位后连续运行 ----------------
    // 预期：第7拍 tick 高、第8拍 LED 首次翻转；运行 4*7=28 个有效计数拍
    $display("=== DIV=7: continuous run after reset ===");
    en7 = 1;
    cycle(1, 0, "d7 reset");
    cycle(1, 0, "d7 reset");
    for (int i = 1; i <= 28; i++) cycle(1, 1, $sformatf("d7 run %0d", i));

    if (errors == 0) begin
      $display("TEST PASSED: all scenarios matched reference model");
    end else begin
      $fatal(1, "TEST FAILED: %0d error(s)", errors);
    end
    $finish;
  end

endmodule
