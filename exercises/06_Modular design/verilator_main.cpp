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
//
// 测试分为 4 个部分:
//   A. 基础功能测试   : 若干典型向量 x 4 个 sel
//   B. 单比特边界测试 : 被选通道数据仅最低位为 1 / 仅最高位为 1
//   C. 隔离性测试     : 固定 sel 与被选数据，只改变其余三路，
//                        验证输出不受未选中通道影响
//   D. 伪随机测试     : 固定种子，>= 256 组随机输入，
//                        同时检查 mux4 数据输出与 decoder2to4
//                        译码输出
//
// 对于 WIDTH=1，最高位与最低位是同一位，B 部分的两个子测试会
// 重合，这是预期行为，不代表测试失效。
// ============================================================

#include <array>
#include <bitset>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
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
constexpr const char* kCyan = "\033[36m";
}  // namespace ansi

// 伪随机测试的固定参数：固定种子 + 至少 256 组
static constexpr uint32_t kRandomSeed = 20240923u;
static constexpr int kRandomCount = 256;

static vluint64_t g_main_time = 0;
// Verilator 需要这个函数来获取仿真时间（供 $time / 波形使用）
double sc_time_stamp() { return static_cast<double>(g_main_time); }

// 全局统计
static int g_total = 0;
static int g_failed = 0;

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
  const uint32_t mask = (width >= 32) ? 0xFFFFFFFFu : ((1u << width) - 1u);
  const int hex_digits = (width + 3) / 4;  // 打印用的十六进制宽度

  // ---- 实例化 DUT ----
  Vtop* dut = new Vtop;

  // ---- 波形 (GTKWave) ----
  VerilatedVcdC* tfp = new VerilatedVcdC;
  dut->trace(tfp, 99);
  const std::string vcd_path =
      vcd_dir + "/wave_w" + std::to_string(width) + ".vcd";
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

    // 组合逻辑再 eval 一次，确保稳定后再打一拍波形
    dut->eval();
    tfp->dump(g_main_time);
    g_main_time += 5;
  };

  // 执行一个测试用例并校验 mux4 数据输出 + decoder2to4 译码输出。
  // verbose=true 时无论成败都打印一行；否则只在失败时打印。
  auto run_case = [&](uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3,
                       uint32_t sel, const std::string& tag,
                       bool verbose) -> bool {
    step(d0, d1, d2, d3, sel);

    uint32_t expected_data;
    switch (sel & 0x3u) {
      case 0: expected_data = d0 & mask; break;
      case 1: expected_data = d1 & mask; break;
      case 2: expected_data = d2 & mask; break;
      default: expected_data = d3 & mask; break;
    }
    const uint32_t expected_select = (1u << (sel & 0x3u)) & 0xFu;

    const uint32_t got_data = static_cast<uint32_t>(dut->output_data) & mask;
    const uint32_t got_select = static_cast<uint32_t>(dut->top_select) & 0xFu;

    const bool ok =
        (got_data == expected_data) && (got_select == expected_select);

    ++g_total;
    if (!ok) ++g_failed;

    if (verbose || !ok) {
      std::ostringstream line;
      line << "  " << tag << "  sel=" << (sel & 0x3u)
           << "  top_select 期望=" << std::bitset<4>(expected_select)
           << " 实际=" << std::bitset<4>(got_select)
           << "  output_data 期望=0x" << std::hex << std::setw(hex_digits)
           << std::setfill('0') << expected_data << " 实际=0x"
           << std::setw(hex_digits) << std::setfill('0') << got_data
           << std::dec;
      std::cout << line.str() << "  ["
                << (ok ? std::string(ansi::kGreen) + "PASS" + ansi::kReset
                       : std::string(ansi::kRed) + "FAIL" + ansi::kReset)
                << "]" << std::endl;
    }
    return ok;
  };

  auto section_header = [](const std::string& title) {
    std::cout << ansi::kCyan << "---- " << title << " ----" << ansi::kReset
              << std::endl;
  };

  std::cout << ansi::kBold
            << "======================================================\n"
            << "  开始测试  TOP_WIDTH = " << width << "  (数据掩码 = 0x"
            << std::hex << mask << std::dec << ")\n"
            << "======================================================"
            << ansi::kReset << std::endl;

  // ============================================================
  // A. 基础功能测试：若干典型向量 x 4 个 sel
  // ============================================================
  section_header("A. 基础功能测试");
  {
    struct Vec {
      uint32_t d0, d1, d2, d3;
      const char* name;
    };
    const std::vector<Vec> vectors = {
        {0x0u, mask, 0x0u, mask, "全零/全一交替"},
        {mask, 0x0u, mask, 0x0u, "全一/全零交替"},
        {mask & 0xAAAAAAAAu, mask & 0x55555555u, mask & 0xAAAAAAAAu,
         mask & 0x55555555u, "0xAA/0x55交替"},
        {0x1u & mask, 0x2u & mask, 0x3u & mask, 0x0u & mask, "递增序列"},
    };
    for (const auto& v : vectors) {
      for (uint32_t sel = 0; sel < 4; ++sel) {
        run_case(v.d0, v.d1, v.d2, v.d3, sel,
                 std::string("[") + v.name + "]", /*verbose=*/true);
      }
    }
  }

  // ============================================================
  // B. 单比特边界测试：被选通道数据仅最低位为1 / 仅最高位为1
  //    对于 WIDTH=1，最高位与最低位是同一位，两个子测试会重合，
  //    属于预期行为。
  // ============================================================
  section_header("B. 单比特边界测试 (被选数据仅LSB=1 / 仅MSB=1)");
  {
    const uint32_t lsb_pattern = 0x1u & mask;
    const uint32_t msb_pattern = (1u << (width - 1)) & mask;
    const uint32_t filler = 0x0u;  // 未被选中的通道填充值

    struct Boundary {
      uint32_t pattern;
      const char* name;
    };
    const std::vector<Boundary> patterns = {
        {lsb_pattern, "仅LSB=1"},
        {msb_pattern, "仅MSB=1"},
    };

    for (const auto& bp : patterns) {
      for (uint32_t sel = 0; sel < 4; ++sel) {
        uint32_t d[4] = {filler, filler, filler, filler};
        d[sel] = bp.pattern;
        run_case(d[0], d[1], d[2], d[3], sel,
                 std::string("[") + bp.name + " -> data" +
                     std::to_string(sel) + "]",
                 /*verbose=*/true);
      }
    }
  }

  // ============================================================
  // C. 隔离性测试：固定 sel 和被选数据，只改变另外三路，
  //    输出必须不变
  // ============================================================
  section_header("C. 隔离性测试 (固定sel与被选数据，其余三路任意变化)");
  {
    // 每个 sel 下被选通道固定的数值（互不相同，便于区分）
    const uint32_t fixed_values[4] = {
        0x5Au & mask, 0x3Cu & mask, 0x69u & mask, 0x96u & mask,
    };
    // 未选中通道依次使用的若干种“干扰”填充组合
    const std::vector<std::array<uint32_t, 3>> fillers = {
        {0x00u, 0x00u, 0x00u},
        {mask, mask, mask},
        {mask & 0xAAAAAAAAu, mask & 0x55555555u, 0x00u},
        {0x00u, mask, mask & 0x55555555u},
        {mask & 0x33333333u, mask & 0xCCCCCCCCu, mask & 0x0F0F0F0Fu},
    };

    for (uint32_t sel = 0; sel < 4; ++sel) {
      const uint32_t fixed_val = fixed_values[sel] & mask;
      uint32_t first_output = 0;
      bool first_recorded = false;

      for (size_t fi = 0; fi < fillers.size(); ++fi) {
        uint32_t d[4];
        // 把固定值放在被选通道，另外三路依次填入干扰值
        int other_idx = 0;
        for (uint32_t ch = 0; ch < 4; ++ch) {
          if (ch == sel) {
            d[ch] = fixed_val;
          } else {
            d[ch] = fillers[fi][other_idx++] & mask;
          }
        }

        const bool ok =
            run_case(d[0], d[1], d[2], d[3], sel,
                     std::string("[sel=") + std::to_string(sel) + " 干扰组" +
                         std::to_string(fi) + "]",
                     /*verbose=*/true);

        if (ok) {
          const uint32_t cur = static_cast<uint32_t>(dut->output_data) & mask;
          if (!first_recorded) {
            first_output = cur;
            first_recorded = true;
          } else if (cur != first_output) {
            // run_case 内部的期望值校验理论上已能发现此类问题，
            // 这里再加一道显式的“输出未变化”校验，双重确认隔离性。
            std::cout << "  " << ansi::kRed << "[隔离性异常] sel=" << sel
                       << " 输出随无关通道变化而改变!" << ansi::kReset
                       << std::endl;
            ++g_failed;
          }
        }
      }
    }
  }

  // ============================================================
  // D. 伪随机测试：固定种子，>= 256 组随机输入，
  //    同时校验 mux4 数据输出与 decoder2to4 译码输出
  // ============================================================
  section_header("D. 伪随机测试 (固定种子, " + std::to_string(kRandomCount) +
                 " 组)");
  {
    std::mt19937 rng(kRandomSeed);
    std::uniform_int_distribution<uint32_t> data_dist(0u, mask);
    std::uniform_int_distribution<uint32_t> sel_dist(0u, 3u);

    const int random_failed_before = g_failed;

    for (int i = 0; i < kRandomCount; ++i) {
      const uint32_t d0 = data_dist(rng);
      const uint32_t d1 = data_dist(rng);
      const uint32_t d2 = data_dist(rng);
      const uint32_t d3 = data_dist(rng);
      const uint32_t sel = sel_dist(rng);

      // 伪随机用例数量多，默认只在失败时打印，避免刷屏；
      // 每 64 组打印一次“心跳”，便于确认测试仍在推进。
      const bool heartbeat = (i % 64 == 0);
      run_case(d0, d1, d2, d3, sel,
               std::string("[随机#") + std::to_string(i) + "]", heartbeat);
    }

    const int random_failed = g_failed - random_failed_before;
    std::cout << "  伪随机测试小结: " << (kRandomCount - random_failed) << "/"
              << kRandomCount << " 通过";
    if (random_failed == 0) {
      std::cout << "  " << ansi::kGreen << "[ALL PASS]" << ansi::kReset;
    } else {
      std::cout << "  " << ansi::kRed << "[" << random_failed << " 项失败]"
                 << ansi::kReset;
    }
    std::cout << std::endl;
  }

  // ============================================================
  // 汇总
  // ============================================================
  std::cout << ansi::kBold
            << "------------------------------------------------------\n"
            << "  TOP_WIDTH = " << width
            << "  全部测试结束: " << (g_total - g_failed) << "/" << g_total
            << " 通过";
  if (g_failed == 0) {
    std::cout << "  " << ansi::kGreen << "[ALL PASS]" << ansi::kReset;
  } else {
    std::cout << "  " << ansi::kRed << "[" << g_failed << " 项失败]"
               << ansi::kReset;
  }
  std::cout << "\n  波形文件已生成: " << vcd_path << "\n"
            << "------------------------------------------------------"
            << ansi::kReset << std::endl;

  tfp->close();
  dut->final();

  delete tfp;
  delete dut;

  return (g_failed == 0) ? 0 : 1;
}