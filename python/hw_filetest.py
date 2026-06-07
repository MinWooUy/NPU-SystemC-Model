import numpy as np

# Tạo 2 ma trận 16x16 chứa các số nguyên ngẫu nhiên từ 1 đến 5
input_matrix = np.random.randint(1, 5, size=(16, 16), dtype=np.uint32)
weight_matrix = np.random.randint(1, 5, size=(16, 16), dtype=np.uint32)

# Golden Model: Thực hiện phép nhân ma trận chuẩn thay vì Conv2d
# NPU: Psums = Input * Weight
golden_result = np.matmul(input_matrix, weight_matrix).astype(np.uint32)


input_matrix.flatten().tofile("image_data.bin")
weight_matrix.flatten().tofile("weight_data.bin")
golden_result.flatten().tofile("golden_data.bin")

print("Đã tạo file Test Hardware (16x16 MatMul) thành công!")
