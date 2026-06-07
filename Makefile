# ==============================================================================
# SAURIA NPU - VIRTUAL PLATFORM MAKEFILE
# ==============================================================================

# Cấu hình Compiler
CXX = g++
# Cờ biên dịch: bật tối ưu hóa -O3 (để chạy nhanh), tắt cảnh báo lỗi SystemC cũ
CXXFLAGS = -O3 -Wall -Wno-deprecated

SYSTEMC_HOME ?= /usr/local/systemc-2.3.3

# Khai báo thư mục Include và Library
INCLUDES = -I./include -I$(SYSTEMC_HOME)/include
LIBDIR   = -L$(SYSTEMC_HOME)/lib-linux64
LIBS     = -lsystemc -lm

# Danh sách các file mã nguồn (Source files)
SRCS = src/main.cpp \
       src/axi_router.cpp\
       src/SAURIA_dma.cpp\
       src/SAURIA_sram.cpp \
       src/SAURIA_pe.cpp \
       src/SAURIA_feed_lane.cpp \
       src/SAURIA_weight_fetcher.cpp\
       src/SAURIA_partial_sums_manager.cpp \
       src/SAURIA_systolic_array.cpp \
       src/SAURIA_npu.cpp

# Tự động suy ra tên các file object (.o) từ danh sách .cpp
OBJS = $(SRCS:.cpp=.o)

# Tên file thực thi đầu ra
TARGET = vp_sim

# ==============================================================================
# BUILD RULES
# ==============================================================================

.PHONY: all clean run

# Lệnh mặc định khi gõ `make`
all: $(TARGET)

# Quy tắc liên kết (Linking) các file .o thành file thực thi
$(TARGET): $(OBJS)
	@echo "===================================================="
	@echo "[Linking] Linking file $(TARGET)..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(LIBDIR) -o $(TARGET) $(OBJS) $(LIBS)
	@echo "[Successfully] Virtual Platfrom is ready!"
	@echo "===================================================="

# Quy tắc biên dịch từng file .cpp thành file .o
src/%.o: src/%.cpp
	@echo "[Compiling] Compile $<..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Lệnh dọn dẹp (Xóa file .o và file thực thi)
clean:
	@echo "[Cleaning] Clear forder..."
	rm -f $(OBJS) $(TARGET)

# Lệnh chạy tự động kèm set biến môi trường
run: $(TARGET)
	@echo "[Running] Python - Golden Model"
	python3 python/generate_golden.py
	@echo "[Running] SystemC - FX1 Virtual Platform..."
	export LD_LIBRARY_PATH=$(SYSTEMC_HOME)/lib-linux64:$$LD_LIBRARY_PATH && ./$(TARGET)
