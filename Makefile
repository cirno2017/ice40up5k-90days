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

SIM_TOP  := top_tb
TB       := sim/top_tb.v
VVP      := $(BUILD_DIR)/$(SIM_TOP).vvp
VCD      := $(BUILD_DIR)/top_tb.vcd

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
	yosys -p "read_verilog -sv $(RTL); synth_ice40 -top $(TOP) -json $(JSON)"


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
		--freq $(FREQ)


# ============================================================
# Bitstream: ASC -> BIN
# ============================================================

$(BIN): $(ASC)
	icepack $(ASC) $(BIN)


# ============================================================
# Program FPGA Flash
# ============================================================

.PHONY: prog
prog: $(BIN)
	icesprog $(BIN)


# ============================================================
# Clean build files
# ============================================================

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)/verilator/*


# ============================================================
# Rebuild everything
# ============================================================

.PHONY: rebuild
rebuild: clean all

# ============================================================
# RTL Simulation
# ============================================================

.PHONY: sim
sim: $(VVP)
	vvp $(VVP)


$(VVP): $(RTL) $(TB) | $(BUILD_DIR)
	iverilog \
		-g2012 \
		-s $(SIM_TOP) \
		-o $(VVP) \
		$(RTL) \
		$(TB)


# ============================================================
# Waveform viewer
# ============================================================

.PHONY: wave
wave: sim
	gtkwave $(VCD)

# ============================================================
# Verilator lint
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
# Verilator waveform
# ============================================================

.PHONY: vwave
vwave: vsim
	gtkwave $(VERILATOR_VCD)





