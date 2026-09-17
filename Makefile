SHELL := cmd.exe

.DEFAULT_GOAL := all

TOP        := top

RTL_DIR    := rtl
SIM_DIR    := sim
CONSTR_DIR := constraints
BUILD_DIR  := build

RTL        := $(RTL_DIR)/$(TOP).v
TB         := $(SIM_DIR)/$(TOP)_tb.v
PCF        := $(CONSTR_DIR)/icesugar.pcf

JSON       := $(BUILD_DIR)/$(TOP).json
ASC        := $(BUILD_DIR)/$(TOP).asc
BIN        := $(BUILD_DIR)/$(TOP).bin

SIMV       := $(BUILD_DIR)/$(TOP)_tb.vvp
VCD        := $(BUILD_DIR)/$(TOP)_tb.vcd

DEVICE     := up5k
PACKAGE    := sg48
FREQ       := 12

.PHONY: all synth pnr pack prog sim wave clean help

all: $(BIN)

$(BUILD_DIR):
	@if not exist "$(BUILD_DIR)" mkdir "$(BUILD_DIR)"

$(JSON): $(RTL) | $(BUILD_DIR)
	yosys -p "synth_ice40 -top $(TOP) -json $(JSON)" $(RTL)

$(ASC): $(JSON) $(PCF)
	nextpnr-ice40 --$(DEVICE) --package $(PACKAGE) --json $(JSON) --pcf $(PCF) --asc $(ASC) --freq $(FREQ)

$(BIN): $(ASC)
	icepack $(ASC) $(BIN)

synth: $(JSON)

pnr: $(ASC)

pack: $(BIN)

prog: $(BIN)
	copy /Y build\top.bin E:\

$(SIMV): $(RTL) $(TB) | $(BUILD_DIR)
	iverilog -g2012 -Wall -s $(TOP)_tb -o $(SIMV) $(TB) $(RTL)

sim: $(SIMV)
	vvp $(SIMV)

wave: sim
	gtkwave $(VCD)

clean:
	@if exist "$(BUILD_DIR)" rmdir /S /Q "$(BUILD_DIR)"

help:
	@echo make all       - Synthesize, place and route, generate BIN
	@echo make           - Same as make all
	@echo make synth     - Run Yosys synthesis
	@echo make pnr       - Run nextpnr
	@echo make pack      - Generate bitstream
	@echo make prog      - Program the iCESugar SPI flash
	@echo make sim       - Run Icarus Verilog simulation
	@echo make wave      - Simulate and open GTKWave
	@echo make clean     - Remove build directory
