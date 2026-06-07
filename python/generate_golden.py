import numpy as np

print("====================================================")
print("[Python Golden Model]: Generate data for NPU...")

# 1. Định nghĩa cấu hình phần cứng (Khớp với file sauria_config.h)
ROWS = 16
COLS = 16

# 2. Sinh dữ liệu ngẫu nhiên (Constrained Random)
# Chúng ta giới hạn random từ 1 đến 10 để số nhỏ, dễ tính nhẩm bằng tay.
# Kiểu dữ liệu uint32_t (4 bytes) khớp với C++
ifmap = np.random.randint(1, 10, size=(ROWS, COLS), dtype=np.uint32)
weight = np.random.randint(1, 10, size=(ROWS, COLS), dtype=np.uint32)

# 3. MÔ PHỎNG THUẬT TOÁN CỦA LÕI NPU (Mô hình Vàng)
# Tùy thuộc vào hàm compute() trong lõi PE của bạn đang làm phép toán gì.
# Giả sử PE hiện tại đang nhân từng phần tử (Element-wise MAC):
# golden_output = np.multiply(ifmap, weight)

golden_output = np.dot(ifmap, weight) # Matrix Multiplier

# Nếu NPU của bạn nhân ma trận chuẩn (Matrix Multiplication):
# golden_output = np.dot(ifmap, weight) 

# 4. Lưu dữ liệu ra file Nhị phân (.bin) để C++ (DMA) bốc vào
ifmap.tofile("image_data.bin")
weight.tofile("weight_data.bin")
golden_output.tofile("golden_data.bin")

print(f"[Python Golden Model]: Generated ifmap ({ROWS}x{COLS}) and weight ({ROWS}x{COLS}).")
print("[Python Golden Model]: Export file .bin successfully!")

# 5. (Tùy chọn) In thử 1 góc 4x4 của ma trận ra màn hình để con người tự nghiệm thu
print("\n--- PYTHON OUTPUT (4x4 left corner) ---")
print(golden_output[:4, :4])
print("====================================================\n")
