import torch
import numpy as np

print("Đang khởi tạo mạng Convolution mô phỏng SAURIA...")

# ---------------------------------------------------------
# CÁC THÔNG SỐ TỪ example_basic.ipynb
# ---------------------------------------------------------
C_in = 32       # Input Channels
C_out = 32      # Output Channels
Kh, Kw = 3, 3   # Kernel size
s = 1           # Strides
d = 1           # Dilation coefficient

Cw, Ch = 8, 8   # Output tensor shape
# Công thức tính kích thước IFMAP dựa trên Output và Kernel
Aw = (1 + s * (Cw - 1)) + (1 + d * (Kw - 1)) - 1
Ah = (1 + s * (Ch - 1)) + (1 + d * (Kh - 1)) - 1

# ---------------------------------------------------------
# KHỞI TẠO TENSOR & TÍNH TOÁN (DÙNG SỐ NGUYÊN)
# ---------------------------------------------------------
# Để VP SystemC dễ test, ta dùng số nguyên (từ 1 đến 5) thay vì số thực thập phân
tensor_A_torch = torch.randint(1, 5, (C_in, Ah, Aw), dtype=torch.float32)

# Khởi tạo mô hình Conv2d (Tắt bias để giống phép nhân ma trận thuần túy)
B_conv_torch = torch.nn.Conv2d(C_in, C_out, (Kh, Kw), stride=s, dilation=d, bias=False)
# Ghi đè trọng số của mô hình bằng số nguyên ngẫu nhiên
B_conv_torch.weight.data = torch.randint(1, 5, B_conv_torch.weight.shape, dtype=torch.float32)

# Chạy PyTorch để lấy kết quả chuẩn (Golden Output)
tensor_C_torch = B_conv_torch(tensor_A_torch)

# ---------------------------------------------------------
# ÉP KIỂU SANG NUMPY & FLATTEN
# ---------------------------------------------------------
# Chuyển đổi thành uint32 để khớp với SystemC (biến uint32_t)
vp_image = tensor_A_torch.detach().numpy().astype(np.uint32).flatten()
vp_weight = B_conv_torch.weight.detach().numpy().astype(np.uint32).flatten()
vp_golden = tensor_C_torch.detach().numpy().astype(np.uint32).flatten()

# ---------------------------------------------------------
# XUẤT FILE BINARY (.bin) TỐC ĐỘ CAO
# ---------------------------------------------------------
vp_image.tofile("image_data.bin")
vp_weight.tofile("weight_data.bin")
vp_golden.tofile("golden_data.bin")

print("\n>>> EXPORT FILE BINARY FOR VIRTUAL PLATFORM FX1 <<<")
print(f"- image_data.bin  : {len(vp_image)} phần tử ({len(vp_image)*4} bytes)")
print(f"- weight_data.bin : {len(vp_weight)} phần tử ({len(vp_weight)*4} bytes)")
print(f"- golden_data.bin : {len(vp_golden)} phần tử ({len(vp_golden)*4} bytes)")

print("\n--- Export result (4x4) ---")
print(golden_output[:4, :4])
print("====================================================\n")
