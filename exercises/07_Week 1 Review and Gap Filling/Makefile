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
# Unit Test targets for each module (Verilator exhaustive test)
# ============================================================
TEST_RTL := $(wildcard $(RTL_DIR)/*.v)
TEST_BUILD := $(BUILD_DIR)/unit_test

# -------- Fadd 全加器 test_fadd --------
TEST_FADD_DIR := $(TEST_BUILD)/fadd
TEST_FADD_BIN := $(TEST_FADD_DIR)/VFadd
$(TEST_FADD_BIN): $(TEST_RTL) sim/tb_Fadd.cpp | $(TEST_BUILD)
	verilator --cc --exe --build -O0 -Wall --trace \
		--top-module Fadd \
		--Mdir $(TEST_FADD_DIR) \
		-o VFadd \
		$(RTL_ABS) \
		$(abspath sim/tb_Fadd.cpp)
.PHONY: test_fadd
test_fadd: $(TEST_FADD_BIN)
	$(TEST_FADD_BIN)

# -------- Hadd 半加器 test_hadd --------
TEST_HADD_DIR := $(TEST_BUILD)/hadd
TEST_HADD_BIN := $(TEST_HADD_DIR)/VHadd
$(TEST_HADD_BIN): $(TEST_RTL) sim/tb_Hadd.cpp | $(TEST_BUILD)
	verilator --cc --exe --build -O0 -Wall --trace \
		--top-module Hadd \
		--Mdir $(TEST_HADD_DIR) \
		-o VHadd \
		$(RTL_ABS) \
		$(abspath sim/tb_Hadd.cpp)
.PHONY: test_hadd
test_hadd: $(TEST_HADD_BIN)
	$(TEST_HADD_BIN)

# -------- Add_8bit_carryin test_add8 --------
TEST_ADD8_DIR := $(TEST_BUILD)/add8
TEST_ADD8_BIN := $(TEST_ADD8_DIR)/VAdd_8bit_carryin
$(TEST_ADD8_BIN): $(TEST_RTL) sim/tb_Add8.cpp | $(TEST_BUILD)
	verilator --cc --exe --build -O0 -Wall --trace \
		--top-module Add_8bit_carryin \
		--Mdir $(TEST_ADD8_DIR) \
		-o VAdd_8bit_carryin \
		$(RTL_ABS) \
		$(abspath sim/tb_Add8.cpp)
.PHONY: test_add8
test_add8: $(TEST_ADD8_BIN)
	$(TEST_ADD8_BIN)

# -------- Mux2 多WIDTH: w1 w8 w12 --------
define build_mux2
TEST_MUX2_W$(1)_DIR := $(TEST_BUILD)/mux2_w$(1)
TEST_MUX2_W$(1)_BIN := $$(TEST_MUX2_W$(1)_DIR)/Vtb_wrap_Mux2
$$(TEST_MUX2_W$(1)_BIN): $(TEST_RTL) sim/tb_Mux2.cpp | $(TEST_BUILD)
	verilator --cc --exe --build -O0 -Wall --trace \
		--top-module tb_wrap_Mux2 \
		-GWIDTH=$(1) \
		--Mdir $$(TEST_MUX2_W$(1)_DIR) \
		-o Vtb_wrap_Mux2 \
		$(RTL_ABS) \
		$(abspath sim/tb_Mux2.cpp)
.PHONY: test_mux2_w$(1)
test_mux2_w$(1): $$(TEST_MUX2_W$(1)_BIN)
	$$(TEST_MUX2_W$(1)_BIN) $(1)
endef
$(eval $(call build_mux2,1))
$(eval $(call build_mux2,8))
$(eval $(call build_mux2,12))

.PHONY: test_mux2_all
test_mux2_all: test_mux2_w1 test_mux2_w8 test_mux2_w12

# -------- Mux4v 多WIDTH w1 w8 w12 --------
define build_mux4v
TEST_MUX4V_W$(1)_DIR := $(TEST_BUILD)/mux4v_w$(1)
TEST_MUX4V_W$(1)_BIN := $$(TEST_MUX4V_W$(1)_DIR)/Vtb_wrap_Mux4v
$$(TEST_MUX4V_W$(1)_BIN): $(TEST_RTL) sim/tb_Mux4v.cpp | $(TEST_BUILD)
	verilator --cc --exe --build -O0 -Wall --trace \
		--top-module tb_wrap_Mux4v \
		-GWIDTH=$(1) \
		--Mdir $$(TEST_MUX4V_W$(1)_DIR) \
		-o Vtb_wrap_Mux4v \
		$(RTL_ABS) \
		$(abspath sim/tb_Mux4v.cpp)
.PHONY: test_mux4v_w$(1)
test_mux4v_w$(1): $$(TEST_MUX4V_W$(1)_BIN)
	$$(TEST_MUX4V_W$(1)_BIN) $(1)
endef
$(eval $(call build_mux4v,1))
$(eval $(call build_mux4v,8))
$(eval $(call build_mux4v,12))

.PHONY: test_mux4v_all
test_mux4v_all: test_mux4v_w1 test_mux4v_w8 test_mux4v_w12

# -------- run all unit tests --------
.PHONY: test_all
test_all: test_fadd test_hadd test_add8 test_mux2_all test_mux4v_all

$(TEST_BUILD):
	mkdir -p $(TEST_BUILD)

# extend clean: remove unit test output
clean:
	rm -rf $(BUILD_DIR)/verilator/*
	rm -rf $(TEST_BUILD)
