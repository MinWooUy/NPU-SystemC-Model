# ======================================================================
# SAURIA NPU - SCALABLE SYSTEMC MODEL
# ======================================================================

HOST := $(shell hostname)

ifeq ($(HOST), eda-server-01)
    SYSTEMC_HOME ?= /opt/arm/fastmodels/SystemC/Accellera/SystemC
    SYS_LIB_PATH = $(SYSTEMC_HOME)/dynlib/Linux64_GCC-10.3
else
    SYSTEMC_HOME ?= /usr/local/systemc-2.3.3
    SYS_LIB_PATH = $(SYSTEMC_HOME)/lib-linux64
endif

CXX := g++
CXXFLAGS := -O3 -Wall -std=c++17 -I. -I$(SYSTEMC_HOME)/include

LDFLAGS  := -L$(SYS_LIB_PATH) -Wl,-rpath,$(SYS_LIB_PATH)
LIBS     := -lsystemc -lm -pthread

TARGET ?= sauria_sim

# Ví dụ: OBJS_COMMON = sauria_types.o
OBJS_COMMON := 

# LUẬT BIÊN DỊCH TĂNG DẦN (Biến .cpp thành .o)
# Luật này giúp Make tự động tìm file .cpp tương ứng để dịch ra .o mà không cần viết lại nhiều lần
%.o: %.cpp
	@echo "[COMPILE] Compiling $<..."
	$(CXX) $(CXXFLAGS) -c $< -o $@

# CÁC TARGET LIÊN KẾT (Linking)
.PHONY: all run clean tb_sa_array tb_psm tb_data_feeder tb_sram tb_control_cfg

all: $(TARGET)

# Build chương trình chính
$(TARGET): tb.o $(OBJS_COMMON)
	@echo "[LINK] Building SystemC Target: $@"
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) $(LIBS) -o $@
	@echo "[SUCCESS] Built $@ successfully!"

# Build các Testbench độc lập
tb_sa_array: systolic_array/tb_sa_array.o $(OBJS_COMMON)
	@echo "[LINK] Building Systolic Array Standalone TB..."
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) $(LIBS) -o $@
	@echo "[SUCCESS] Built $@ successfully!"

tb_psm: psm/tb_psm.o $(OBJS_COMMON)
	@echo "[LINK] Building PSM Standalone TB..."
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) $(LIBS) -o $@
	@echo "[SUCCESS] Built $@ successfully!"

tb_data_feeder: data_feeder/tb_data_feeder.o $(OBJS_COMMON)
	@echo "[LINK] Building Data Feeder Standalone TB..."
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) $(LIBS) -o $@
	@echo "[SUCCESS] Built $@ successfully!"

tb_sram: sram/tb_sram.o $(OBJS_COMMON)
	@echo "[LINK] Building SRAM Standalone TB..."
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) $(LIBS) -o $@
	@echo "[SUCCESS] Built $@ successfully!"

tb_control_cfg: control/tb_control_cfg.o $(OBJS_COMMON)
	@echo "[LINK] Building Control and ConfigRegs Standalone TB..."
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) $(LIBS) -o $@
	@echo "[SUCCESS] Built $@ successfully!"

tb_evaluate: tb_evaluate.o $(OBJS_COMMON)
	@echo "[LINK] Building testbench 1"
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) $(LIBS) -o $@
	@echo "[SUCCESS] Built $@ successfully!"
	
tb_evaluate_config2: tb_evaluate_config2.o $(OBJS_COMMON)
	@echo "[LINK] Building testbench 2"
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) $(LIBS) -o $@
	@echo "[SUCCESS] Built $@ succesfully"
	
tb_evaluate_config3: tb_evaluate_config3.o $(OBJS_COMMON)
	@echo "[LINK] Building testbench 3"
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) $(LIBS) -o $@
	@echo "[SUCCESS] Built $@ succesfully"

tb_evaluate_config4: tb_evaluate_config4.o $(OBJS_COMMON)
	@echo "[LINK] Building testbench 4"
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) $(LIBS) -o $@
	@echo "[SUCCESS] Built $@ succesfully"

# 6. Các lệnh tiện ích (Run & Clean)
run: $(TARGET)
	@echo "[RUN] Starting SystemC Simulation..."
	./$(TARGET)

clean:
	@echo "[CLEAN] Removing compilation outputs..."
	rm -f $(TARGET) waves.vcd tb_sa_array tb_psm tb_data_feeder tb_sram tb_control_cfg
	rm -f *.o */*.o