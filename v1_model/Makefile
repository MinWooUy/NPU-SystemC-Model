# Copyright 2026 Barcelona Supercomputing Center (BSC)
# SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
#
# Makefile for SAURIA SystemC NPU Core Simulation

# Define SystemC location (can be overridden via environment or make argument)
SYSTEMC_HOME ?= /data/XPU00000/users/vuong.nguyen/project/sauria/systemc_install

# Detect library directory (SystemC 3.0.0 uses lib, older versions use lib-linux64)
LIB_DIR  := $(shell [ -d $(SYSTEMC_HOME)/lib-linux64 ] && echo "lib-linux64" || echo "lib")

# Compiler and Linker configs
CXX      := g++
CXXFLAGS := -O3 -Wall -std=c++17 -I. -I$(SYSTEMC_HOME)/include
LDFLAGS  := -L$(SYSTEMC_HOME)/$(LIB_DIR) -lsystemc -Wl,-rpath,$(SYSTEMC_HOME)/$(LIB_DIR)

# Targets
TARGET   ?= sauria_sim
SRCS     ?= tb.cpp
HEADERS  := sauria_types.h npu_top.h \
            config/config_regs.h \
            control/main_controller.h \
            data_feeder/ifmap_feeder.h \
            data_feeder/wei_feeder.h \
            psm/psm_top.h \
            sram/sram_top.h \
            systolic_array/sa_processing_element.h \
            systolic_array/sa_array.h

.PHONY: all run clean tb_sa_array tb_psm tb_data_feeder tb_sram tb_control_cfg

all: $(TARGET)

$(TARGET): $(SRCS) $(HEADERS)
	@echo "[BUILD] Compiling SystemC targets..."
	$(CXX) $(CXXFLAGS) $(SRCS) $(LDFLAGS) -o $@
	@echo "[BUILD] Built $(TARGET) successfully!"

tb_sa_array: systolic_array/tb_sa_array.cpp $(HEADERS)
	@echo "[BUILD] Compiling Systolic Array Standalone TB..."
	$(CXX) $(CXXFLAGS) systolic_array/tb_sa_array.cpp $(LDFLAGS) -o $@
	@echo "[BUILD] Built tb_sa_array successfully!"

tb_psm: psm/tb_psm.cpp $(HEADERS)
	@echo "[BUILD] Compiling PSM Standalone TB..."
	$(CXX) $(CXXFLAGS) psm/tb_psm.cpp $(LDFLAGS) -o $@
	@echo "[BUILD] Built tb_psm successfully!"

tb_data_feeder: data_feeder/tb_data_feeder.cpp $(HEADERS)
	@echo "[BUILD] Compiling Data Feeder Standalone TB..."
	$(CXX) $(CXXFLAGS) data_feeder/tb_data_feeder.cpp $(LDFLAGS) -o $@
	@echo "[BUILD] Built tb_data_feeder successfully!"

tb_sram: sram/tb_sram.cpp $(HEADERS)
	@echo "[BUILD] Compiling SRAM Standalone TB..."
	$(CXX) $(CXXFLAGS) sram/tb_sram.cpp $(LDFLAGS) -o $@
	@echo "[BUILD] Built tb_sram successfully!"

tb_control_cfg: control/tb_control_cfg.cpp $(HEADERS)
	@echo "[BUILD] Compiling Control and ConfigRegs Standalone TB..."
	$(CXX) $(CXXFLAGS) control/tb_control_cfg.cpp $(LDFLAGS) -o $@
	@echo "[BUILD] Built tb_control_cfg successfully!"

run: $(TARGET)
	@echo "[RUN] Starting SystemC Simulation..."
	./$(TARGET)

clean:
	@echo "[CLEAN] Removing compilation outputs..."
	rm -f $(TARGET) waves.vcd test_sc tb_sa_array tb_psm tb_data_feeder tb_sram tb_control_cfg waves_psm.vcd waves_sa_array.vcd waves_feeders.vcd