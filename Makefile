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
VERILATOR_BIN := $(VERILATOR_DIR)/V$(TOP)
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
		-o V$(TOP) \
		$(RTL_ABS) \
		$(VERILATOR_CPP)


.PHONY: vsim
vsim: $(VERILATOR_BIN)
	$(VERILATOR_BIN)


# ============================================================
# Verilator waveform
# ============================================================

.PHONY: vwave
vwave: vsim
	gtkwave $(VERILATOR_VCD)





# ============================================================
# Verilator 多位宽仿真 (WIDTH = 1 / 8 / 12)
#
#   测试配置    Verilator 参数         独立构建目录
#   1  位      -GTOP_WIDTH=1         build/verilator_w1
#   8  位      -GTOP_WIDTH=8         build/verilator_w8
#   12 位      -GTOP_WIDTH=12        build/verilator_w12
#
#   make sim1 / sim8 / sim12   分别构建并运行对应位宽的仿真
#   make simall                依次运行全部三个位宽
#   make vwave1 / vwave8 / vwave12   用 gtkwave 查看对应波形
# ============================================================

VERILATOR_DIR_W1  := $(BUILD_DIR)/verilator_w1
VERILATOR_DIR_W8  := $(BUILD_DIR)/verilator_w8
VERILATOR_DIR_W12 := $(BUILD_DIR)/verilator_w12

VERILATOR_BIN_W1  := $(VERILATOR_DIR_W1)/V$(TOP)
VERILATOR_BIN_W8  := $(VERILATOR_DIR_W8)/V$(TOP)
VERILATOR_BIN_W12 := $(VERILATOR_DIR_W12)/V$(TOP)

VERILATOR_VCD_W1  := $(VERILATOR_DIR_W1)/wave_w1.vcd
VERILATOR_VCD_W8  := $(VERILATOR_DIR_W8)/wave_w8.vcd
VERILATOR_VCD_W12 := $(VERILATOR_DIR_W12)/wave_w12.vcd

$(VERILATOR_BIN_W1): $(RTL) $(VERILATOR_CPP) | $(BUILD_DIR)
	verilator \
		--cc \
		--exe \
		--build \
		-j 0 \
		-Wall \
		--top-module $(TOP) \
		--trace \
		-GTOP_WIDTH=1 \
		--Mdir $(VERILATOR_DIR_W1) \
		-o V$(TOP) \
		$(RTL_ABS) \
		$(VERILATOR_CPP)

$(VERILATOR_BIN_W8): $(RTL) $(VERILATOR_CPP) | $(BUILD_DIR)
	verilator \
		--cc \
		--exe \
		--build \
		-j 0 \
		-Wall \
		--top-module $(TOP) \
		--trace \
		-GTOP_WIDTH=8 \
		--Mdir $(VERILATOR_DIR_W8) \
		-o V$(TOP) \
		$(RTL_ABS) \
		$(VERILATOR_CPP)

$(VERILATOR_BIN_W12): $(RTL) $(VERILATOR_CPP) | $(BUILD_DIR)
	verilator \
		--cc \
		--exe \
		--build \
		-j 0 \
		-Wall \
		--top-module $(TOP) \
		--trace \
		-GTOP_WIDTH=12 \
		--Mdir $(VERILATOR_DIR_W12) \
		-o V$(TOP) \
		$(RTL_ABS) \
		$(VERILATOR_CPP)

.PHONY: sim1 sim8 sim12
sim1: $(VERILATOR_BIN_W1)
	$(VERILATOR_BIN_W1) 1 $(VERILATOR_DIR_W1)

sim8: $(VERILATOR_BIN_W8)
	$(VERILATOR_BIN_W8) 8 $(VERILATOR_DIR_W8)

sim12: $(VERILATOR_BIN_W12)
	$(VERILATOR_BIN_W12) 12 $(VERILATOR_DIR_W12)

.PHONY: simall
simall: sim1 sim8 sim12

.PHONY: vwave1 vwave8 vwave12
vwave1: sim1
	gtkwave $(VERILATOR_VCD_W1)

vwave8: sim8
	gtkwave $(VERILATOR_VCD_W8)

vwave12: sim12
	gtkwave $(VERILATOR_VCD_W12)