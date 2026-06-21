# Copyright 2026 Barcelona Supercomputing Center (BSC)
# SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
#
# Makefile for SAURIA SystemC NPU Core Simulation

# Define SystemC location (can be overridden via environment or make argument)
SYSTEMC_HOME ?= /data/XPU00000/users/vuong.nguyen/project/sauria/systemc_install

# Detect library directory (SystemC 3.0.0 uses lib, older versions use lib-linux64)
LIB_DIR  := $(shell [ -d $(SYSTEMC_HOME)/lib-linux64 ] && echo "lib-linux64" || echo "lib")

# Compiler and Linker configs
CXX      := g++
CXXFLAGS := -O3 -Wall -std=c++17 -I. -I$(SYSTEMC_HOME)/include
LDFLAGS  := -L$(SYSTEMC_HOME)/$(LIB_DIR) -lsystemc -Wl,-rpath,$(SYSTEMC_HOME)/$(LIB_DIR)

# Targets
TARGET   ?= sauria_sim
SRCS     ?= tb.cpp
HEADERS  := sauria_types.h npu_top.h \
            config/config_regs.h \
            control/main_controller.h \
            data_feeder/ifmap_feeder.h \
            data_feeder/wei_feeder.h \
            psm/psm_top.h \
            sram/sram_top.h \
            systolic_array/sa_processing_element.h \
            systolic_array/sa_array.h

.PHONY: all run clean tb_sa_array tb_psm tb_data_feeder tb_sram tb_control_cfg tb_evaluate tb_evaluate_config2 tb_evaluate_config3 tb_evaluate_config4 tb_evaluate_config5 tb_standalone run_standalone tb_strided run_strided tb_multitile run_multitile tb_spec_evaluation run_spec_evaluation

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

tb_evaluate: tb_evaluate.cpp $(HEADERS)
	@echo "[BUILD] Compiling NpuTop Evaluation TB..."
	$(CXX) $(CXXFLAGS) tb_evaluate.cpp $(LDFLAGS) -o $@
	@echo "[BUILD] Built tb_evaluate successfully!"

tb_evaluate_config2: tb_evaluate_config2.cpp $(HEADERS)
	@echo "[BUILD] Compiling NpuTop Config 2 Evaluation TB..."
	$(CXX) $(CXXFLAGS) tb_evaluate_config2.cpp $(LDFLAGS) -o $@
	@echo "[BUILD] Built tb_evaluate_config2 successfully!"

tb_evaluate_config3: tb_evaluate_config3.cpp $(HEADERS)
	@echo "[BUILD] Compiling NpuTop Config 3 Evaluation TB..."
	$(CXX) $(CXXFLAGS) tb_evaluate_config3.cpp $(LDFLAGS) -o $@
	@echo "[BUILD] Built tb_evaluate_config3 successfully!"

tb_evaluate_config4: tb_evaluate_config4.cpp $(HEADERS)
	@echo "[BUILD] Compiling NpuTop Config 4 Evaluation TB..."
	$(CXX) $(CXXFLAGS) tb_evaluate_config4.cpp $(LDFLAGS) -o $@
	@echo "[BUILD] Built tb_evaluate_config4 successfully!"

tb_evaluate_config5: tb_evaluate_config5.cpp $(HEADERS)
	@echo "[BUILD] Compiling NpuTop Config 5 Evaluation TB..."
	$(CXX) $(CXXFLAGS) tb_evaluate_config5.cpp $(LDFLAGS) -o $@
	@echo "[BUILD] Built tb_evaluate_config5 successfully!"

tb_standalone: tb_standalone.cpp $(HEADERS)
	@echo "[BUILD] Compiling Standalone Core TB..."
	$(CXX) $(CXXFLAGS) tb_standalone.cpp $(LDFLAGS) -o $@
	@echo "[BUILD] Built tb_standalone successfully!"

tb_strided: tb_strided.cpp $(HEADERS)
	@echo "[BUILD] Compiling Strided Standalone Core TB..."
	$(CXX) $(CXXFLAGS) tb_strided.cpp $(LDFLAGS) -o $@
	@echo "[BUILD] Built tb_strided successfully!"

tb_multitile: tb_multitile.cpp $(HEADERS)
	@echo "[BUILD] Compiling Multi-Tile Standalone Core TB..."
	$(CXX) $(CXXFLAGS) tb_multitile.cpp $(LDFLAGS) -o $@
	@echo "[BUILD] Built tb_multitile successfully!"

tb_spec_evaluation: tb_spec_evaluation.cpp $(HEADERS)
	@echo "[BUILD] Compiling INT8 Spec Evaluation Core TB..."
	$(CXX) $(CXXFLAGS) tb_spec_evaluation.cpp $(LDFLAGS) -o $@
	@echo "[BUILD] Built tb_spec_evaluation successfully!"

run_standalone: tb_standalone
	@echo "[RUN] Generating standalone testcases..."
	python3 generate_standalone_test.py
	@echo "[RUN] Starting SystemC Standalone Simulation..."
	./tb_standalone

run_strided: tb_strided
	@echo "[RUN] Generating strided standalone testcases..."
	python3 generate_strided_test.py --K 32 --act_incntstep 2 --wei_incntstep 2
	@echo "[RUN] Starting SystemC Strided Simulation..."
	./tb_strided

run_multitile: tb_multitile
	@echo "[RUN] Generating multi-tile standalone testcases..."
	python3 generate_multitile_test.py --K 24 --act_incntstep 3 --wei_incntstep 3
	@echo "[RUN] Starting SystemC Multi-Tile Simulation..."
	./tb_multitile

run_spec_evaluation: tb_spec_evaluation
	@echo "[RUN] Generating INT8 Spec-compliant testcase..."
	python3 generate_spec_test.py
	@echo "[RUN] Starting SystemC INT8 Spec Evaluation Simulation..."
	./tb_spec_evaluation

run: $(TARGET)
	@echo "[RUN] Starting SystemC Simulation..."
	./$(TARGET)

clean:
	@echo "[CLEAN] Removing compilation outputs..."
	rm -f $(TARGET) waves.vcd test_sc tb_sa_array tb_psm tb_data_feeder tb_sram tb_control_cfg tb_evaluate tb_evaluate_config2 tb_evaluate_config3 tb_evaluate_config4 tb_evaluate_config5 waves_psm.vcd waves_sa_array.vcd waves_feeders.vcd tb_standalone tb_strided tb_multitile tb_spec_evaluation




