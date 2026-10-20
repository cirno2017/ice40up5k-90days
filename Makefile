# ============================================================
# iCESugar iCE40UP5K Makefile
# ============================================================

TOP      := top
DEVICE   := up5k
PACKAGE  := sg48
FREQ     := 12

RTL_DIR  := rtl
CONSTR_DIR := constraints
BUILD_DIR := build

PCF      := $(CONSTR_DIR)/icesugar.pcf

# 自动收集 rtl/ 目录下所有 .v 文件
RTL      := $(wildcard $(RTL_DIR)/*.v)

JSON     := $(BUILD_DIR)/$(TOP).json
ASC      := $(BUILD_DIR)/$(TOP).asc
BIN      := $(BUILD_DIR)/$(TOP).bin

# 综合/布局布线日志与报告（时序结果 + 资源摘要）
YOSYS_LOG   := $(BUILD_DIR)/yosys.log
NEXTPNR_LOG := $(BUILD_DIR)/nextpnr.log
TIMING_RPT  := $(BUILD_DIR)/timing.txt
REPORT      := $(BUILD_DIR)/report.txt

# ============================================================
# Verilator
# ============================================================

VERILATOR_DIR := $(BUILD_DIR)/verilator
VERILATOR_CPP := $(abspath sim/verilator_main.cpp)

# 仿真分频比：make vsim DIV=7 可用小分频快速仿真
# 二进制名带 DIV，切换 DIV 时会自动重新生成模型
DIV          ?= 6000000
VERILATOR_BIN := $(VERILATOR_DIR)/V$(TOP)_$(DIV)
VERILATOR_VCD := wave.vcd

RTL_ABS := $(abspath $(RTL))


# ============================================================
# Default target
# ============================================================

.PHONY: all
all: $(BIN)


# ============================================================
# Create build directory
# ============================================================

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)


# ============================================================
# Synthesis: Verilog -> JSON
# ============================================================

$(JSON): $(RTL) | $(BUILD_DIR)
	yosys -l $(YOSYS_LOG) -p "read_verilog -sv $(RTL); synth_ice40 -top $(TOP) -json $(JSON)"


# ============================================================
# Place & Route: JSON -> ASC
# ============================================================

$(ASC): $(JSON) $(PCF)
	nextpnr-ice40 \
		--$(DEVICE) \
		--package $(PACKAGE) \
		--json $(JSON) \
		--pcf $(PCF) \
		--asc $(ASC) \
		--freq $(FREQ) \
		--log $(NEXTPNR_LOG)


# ============================================================
# Bitstream: ASC -> BIN
# ============================================================

$(BIN): $(ASC)
	icepack $(ASC) $(BIN)


# ============================================================
# Static timing + resource summary
#   report.txt：nextpnr 资源摘要 + nextpnr/icetime 时序结果
# ============================================================

$(TIMING_RPT): $(ASC)
	icetime -d $(DEVICE) -P $(PACKAGE) -p $(PCF) -m -r $@ $<


.PHONY: report
report: $(BIN) $(TIMING_RPT)
	@echo "==== 资源摘要（nextpnr 布局布线后）====" | tee $(REPORT)
	@sed -n '/Device utilisation/,/^$$/p' $(NEXTPNR_LOG) | tee -a $(REPORT)
	@echo "" | tee -a $(REPORT)
	@echo "==== 时序结果（目标 $(FREQ)MHz）====" | tee -a $(REPORT)
	@grep "Max frequency" $(NEXTPNR_LOG) | tee -a $(REPORT)
	@grep -E "Max frequency|Total path delay" $(TIMING_RPT) | tee -a $(REPORT)
	@echo "报告已保存到 $(REPORT)"


# ============================================================
# Program FPGA Flash
# ============================================================

.PHONY: prog
prog: $(BIN)
	icesprog $(BIN)


# ============================================================
# Clean: 覆盖全部生成目录与文件
#   build/   —— 综合、布局布线、bitstream、日志、报告、Verilator 模型
#   obj_dir/ —— 未指定 --Mdir 时 Verilator 的默认输出目录（历史遗留）
#   wave.vcd —— vsim 生成的波形
# ============================================================

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR) obj_dir
	rm -f $(VERILATOR_VCD)


# ============================================================
# Rebuild everything
# ============================================================

.PHONY: rebuild
rebuild: clean all


# ============================================================
# 仿真入口统一为 Verilator C++ 流程（sim/verilator_main.cpp）：
#   make vlint        —— Verilator lint
#   make vsim DIV=4   —— 构建并运行 C++ 仿真（DIV 即 DIV_FREQ）
#   make vwave DIV=4  —— 仿真后用 GTKWave 查看 wave.vcd
# 旧的 Icarus/iverilog 入口（sim/wave，基于 sim/top_tb.v）已删除。
# ============================================================

.PHONY: vlint
vlint:
	verilator \
		--lint-only \
		-Wall \
		--top-module $(TOP) \
		$(RTL)


# ============================================================
# Verilator C++ simulation
# ============================================================

$(VERILATOR_BIN): $(RTL) sim/verilator_main.cpp | $(BUILD_DIR)
	verilator \
		--cc \
		--exe \
		--build \
		-j 0 \
		-Wall \
		--top-module $(TOP) \
		--trace \
		--Mdir $(VERILATOR_DIR) \
		-GDIV_FREQ=$(DIV) \
		-o V$(TOP)_$(DIV) \
		$(RTL_ABS) \
		$(VERILATOR_CPP)


.PHONY: vsim
vsim: $(VERILATOR_BIN)
	$(VERILATOR_BIN) $(DIV)


# ============================================================
# sync_bit 单元测试（独立小型 Verilator C++ 仿真）
#   独立 --Mdir，避免与 top 的模型文件互相覆盖
# ============================================================

SYNC_DIR := $(BUILD_DIR)/verilator_sync
SYNC_BIN := $(SYNC_DIR)/Vsync_bit
SYNC_CPP := $(abspath sim/sync_bit_main.cpp)

$(SYNC_BIN): $(RTL_DIR)/sync_bit.v sim/sync_bit_main.cpp | $(BUILD_DIR)
	verilator \
		--cc \
		--exe \
		--build \
		-j 0 \
		-Wall \
		--top-module sync_bit \
		--Mdir $(SYNC_DIR) \
		$(abspath $(RTL_DIR)/sync_bit.v) \
		$(SYNC_CPP)


.PHONY: ssim
ssim: $(SYNC_BIN)
	$(SYNC_BIN)


# ============================================================
# edge_detect 单元测试（sync_bit -> edge_detect 链）
#   仿真顶层 sim/sync_edge_top.v 只在仿真中使用，不参与 FPGA 综合；
#   独立 --Mdir，避免与其他模型文件互相覆盖
# ============================================================

EDGE_DIR := $(BUILD_DIR)/verilator_edge
EDGE_BIN := $(EDGE_DIR)/Vsync_edge_top
EDGE_SIM_TOP := $(abspath sim/sync_edge_top.v)
EDGE_CPP := $(abspath sim/edge_detect_main.cpp)

$(EDGE_BIN): $(RTL_DIR)/sync_bit.v $(RTL_DIR)/edge_detect.v sim/sync_edge_top.v sim/edge_detect_main.cpp | $(BUILD_DIR)
	verilator \
		--cc \
		--exe \
		--build \
		-j 0 \
		-Wall \
		--top-module sync_edge_top \
		--Mdir $(EDGE_DIR) \
		$(abspath $(RTL_DIR)/sync_bit.v) \
		$(abspath $(RTL_DIR)/edge_detect.v) \
		$(EDGE_SIM_TOP) \
		$(EDGE_CPP)


.PHONY: esim
esim: $(EDGE_BIN)
	$(EDGE_BIN)


# ============================================================
# Verilator waveform
# ============================================================

.PHONY: vwave
vwave: vsim
	gtkwave $(VERILATOR_VCD)
