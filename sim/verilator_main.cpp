// ============================================================
// verilator_main.cpp
//
// top 模块（decoder2to4 + mux4）的 Verilator C++ 测试平台
//
// 用法:
//   ./Vtop <WIDTH> [vcd输出目录]
//
//   WIDTH        : 与编译时 -GTOP_WIDTH=<N> 保持一致的位宽（用于
//                  生成/校验测试向量），由 Makefile 在运行时传入
//   vcd输出目录  : 可选，默认写到当前目录，波形文件名为
//                  wave_w<WIDTH>.vcd
//
// 同一份代码通过 Makefile 分别针对 WIDTH=1 / 8 / 12 构建三份
// 独立的可执行文件（各自独立的 --Mdir），运行时再把对应的 WIDTH
// 当作参数传入，用来计算数据掩码、生成测试向量、校验结果。
// ============================================================

#include <bitset>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "Vtop.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

// ---- ANSI 终端颜色（仅用于让 PASS/FAIL 更醒目） ----
namespace ansi {
constexpr const char* kReset = "\033[0m";
constexpr const char* kGreen = "\033[32m";
constexpr const char* kRed = "\033[31m";
constexpr const char* kBold = "\033[1m";
}  // namespace ansi

static vluint64_t g_main_time = 0;
// Verilator 需要这个函数来获取仿真时间（供 $time / 波形使用）
double sc_time_stamp() { return static_cast<double>(g_main_time); }

// 单个测试向量：4 路输入数据 + 描述文字
struct TestVector {
  uint32_t d0, d1, d2, d3;
  const char* name;
};

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Verilated::traceEverOn(true);

  // ---- 解析运行参数 ----
  if (argc < 2) {
    std::cerr << "用法: " << argv[0] << " <WIDTH> [vcd输出目录]\n";
    return 2;
  }
  const int width = std::atoi(argv[1]);
  const std::string vcd_dir = (argc >= 3) ? argv[2] : ".";

  if (width <= 0 || width > 32) {
    std::cerr << "错误: WIDTH 必须在 1~32 之间 (收到 " << width << ")\n";
    return 2;
  }

  // 数据位宽掩码，例如 WIDTH=8 -> 0xFF, WIDTH=12 -> 0xFFF
  const uint32_t mask =
      (width >= 32) ? 0xFFFFFFFFu : ((1u << width) - 1u);

  // ---- 实例化 DUT ----
  Vtop* dut = new Vtop;

  // ---- 波形 (GTKWave) ----
  VerilatedVcdC* tfp = new VerilatedVcdC;
  dut->trace(tfp, 99);
  const std::string vcd_path = vcd_dir + "/wave_w" + std::to_string(width) + ".vcd";
  tfp->open(vcd_path.c_str());

  // 每一步驱动输入 -> eval -> dump 波形
  auto step = [&](uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3,
                  uint32_t sel) {
    dut->data0 = d0 & mask;
    dut->data1 = d1 & mask;
    dut->data2 = d2 & mask;
    dut->data3 = d3 & mask;
    dut->top_sel = sel & 0x3u;

    dut->eval();
    tfp->dump(g_main_time);
    g_main_time += 5;

    // 组合逻辑再 eval 一次，确保稳定后再打一拍波形，方便在
    // GTKWave 里看清楚每个测试向量的边界
    dut->eval();
    tfp->dump(g_main_time);
    g_main_time += 5;
  };

  // ---- 测试向量 ----
  // 位宽较小时（例如 WIDTH=1）部分向量会退化重复，这是正常现象，
  // 因为 1 bit 只有 0/1 两种取值；测试逻辑本身按每路输入独立校验，
  // 不受向量是否重复的影响。
  const std::vector<TestVector> vectors = {
      {0x0u, mask, 0x0u, mask, "data0=0        data1=全1     data2=0        data3=全1"},
      {mask, 0x0u, mask, 0x0u, "data0=全1      data1=0       data2=全1      data3=0"},
      {mask & 0xAAAAAAAAu, mask & 0x55555555u, mask & 0xAAAAAAAAu,
       mask & 0x55555555u, "data0=0xAA...  data1=0x55...  data2=0xAA...  data3=0x55..."},
      {0x1u & mask, 0x2u & mask, 0x3u & mask, 0x0u & mask,
       "data0=1        data1=2       data2=3        data3=0 (递增序列)"},
  };

  const int hex_digits = (width + 3) / 4;  // 打印用的十六进制宽度

  std::cout << ansi::kBold << "======================================================\n"
            << "  开始测试  TOP_WIDTH = " << width << "  (数据掩码 = 0x" << std::hex
            << mask << std::dec << ")\n"
            << "======================================================" << ansi::kReset
            << std::endl;

  int total = 0;
  int failed = 0;

  for (const auto& v : vectors) {
    std::cout << "-- 测试向量: " << v.name << " --\n";

    for (uint32_t sel = 0; sel < 4; ++sel) {
      step(v.d0, v.d1, v.d2, v.d3, sel);

      // ---- 计算期望值 ----
      uint32_t expected_data;
      switch (sel) {
        case 0: expected_data = v.d0 & mask; break;
        case 1: expected_data = v.d1 & mask; break;
        case 2: expected_data = v.d2 & mask; break;
        default: expected_data = v.d3 & mask; break;
      }
      const uint32_t expected_select = (1u << sel) & 0xFu;

      // ---- 读取 DUT 输出 ----
      const uint32_t got_data = static_cast<uint32_t>(dut->output_data) & mask;
      const uint32_t got_select = static_cast<uint32_t>(dut->top_select) & 0xFu;

      const bool data_ok = (got_data == expected_data);
      const bool select_ok = (got_select == expected_select);
      const bool ok = data_ok && select_ok;

      ++total;
      if (!ok) ++failed;

      std::ostringstream line;
      line << "  sel=" << sel
           << "  top_select 期望=" << std::bitset<4>(expected_select)
           << " 实际=" << std::bitset<4>(got_select)
           << "  output_data 期望=0x" << std::hex << std::setw(hex_digits)
           << std::setfill('0') << expected_data
           << " 实际=0x" << std::setw(hex_digits) << std::setfill('0')
           << got_data << std::dec;

      std::cout << line.str() << "  ["
                << (ok ? std::string(ansi::kGreen) + "PASS" + ansi::kReset
                       : std::string(ansi::kRed) + "FAIL" + ansi::kReset)
                << "]" << std::endl;
    }
  }

  std::cout << ansi::kBold << "------------------------------------------------------\n"
            << "  TOP_WIDTH = " << width << "  测试结束: " << (total - failed) << "/"
            << total << " 通过";
  if (failed == 0) {
    std::cout << "  " << ansi::kGreen << "[ALL PASS]" << ansi::kReset;
  } else {
    std::cout << "  " << ansi::kRed << "[" << failed << " 项失败]" << ansi::kReset;
  }
  std::cout << "\n  波形文件已生成: " << vcd_path << "\n"
            << "------------------------------------------------------" << ansi::kReset
            << std::endl;

  tfp->close();
  dut->final();

  delete tfp;
  delete dut;

  return (failed == 0) ? 0 : 1;
}