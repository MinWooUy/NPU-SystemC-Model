import pandas as pd

# Dữ liệu chỉ bao gồm 4 cột: Nhóm, Parameter, Ý nghĩa, Ghi chú
data = [
    ["CONTROL", "INCNTLIM", "Giới hạn vòng lặp đếm tổng (phụ thuộc vào kích thước ma trận/chập)", ""],
    ["CONTROL", "ACT_REPS", "Số lần lặp lại dữ liệu Activation (dùng cho cơ chế Tiling/Reuse)", ""],
    ["CONTROL", "WEI_REPS", "Số lần lặp lại dữ liệu Weight", ""],
    ["CONTROL", "THRES", "Ngưỡng hoạt động/Threshold (chưa active)", ""],
    
    ["ACTIVATION", "XLIM", "Giới hạn X của ifmap", ""],
    ["ACTIVATION", "XSTEP", "Bước nhảy X của ifmap", ""],
    ["ACTIVATION", "YLIM", "Giới hạn Y của ifmap", ""],
    ["ACTIVATION", "YSTEP", "Bước nhảy Y của ifmap", ""],
    ["ACTIVATION", "CHLIM", "Giới hạn Channel của ifmap", ""],
    ["ACTIVATION", "CHSTEP", "Bước nhảy Channel của ifmap", ""],
    ["ACTIVATION", "TIL_XLIM", "Giới hạn X của Tile ifmap", ""],
    ["ACTIVATION", "TIL_XSTEP", "Bước nhảy X của Tile ifmap", ""],
    ["ACTIVATION", "TIL_YLIM", "Giới hạn Y của Tile ifmap", ""],
    ["ACTIVATION", "TIL_YSTEP", "Bước nhảy Y của Tile ifmap", ""],
    ["ACTIVATION", "DIL_PAT", "Bitmask định cấu hình Dilation (khoảng giãn) cho convolution", ""],
    ["ACTIVATION", "ROWS_ACTIVE", "Bitmask xác định các hàng PE đang hoạt động", "Mảng cứng 16x16, nếu chạy 8x16 thì 0xff"],
    ["ACTIVATION", "LOC_WOFFS", "Offset cục bộ cho weight", ""],
    
    ["WEIGHT", "WLIM", "Tổng số weight bytes/elements stream", ""],
    ["WEIGHT", "WSTEP", "Bước nhảy weight stream", ""],
    ["WEIGHT", "KLIM", "Inner k loop limit", ""],
    ["WEIGHT", "KSTEP", "K loop step", ""],
    ["WEIGHT", "TIL_KLIM", "Tile k limit", ""],
    ["WEIGHT", "TIL_KSTEP", "Tile k step", ""],
    ["WEIGHT", "COLS_ACTIVE", "Active PE columns", ""],
    ["WEIGHT", "WALIGNED", "Weight aligned optimization", ""],
    
    ["PSM / OUTPUT", "NCONTEXTS", "Số context/ output accumulator context", ""],
    ["PSM / OUTPUT", "CXLIM", "Output x/context limit", ""],
    ["PSM / OUTPUT", "CXSTEP", "Output x step", ""],
    ["PSM / OUTPUT", "CKLIM", "Output k limit", ""],
    ["PSM / OUTPUT", "CKSTEP", "Output k step", ""],
    ["PSM / OUTPUT", "TIL_CYLIM", "Tile ouput y/x limit", ""],
    ["PSM / OUTPUT", "TIL_CYSTEP", "Tile output y/x step", ""],
    ["PSM / OUTPUT", "TIL_CKLIM", "Total output elems/tile", ""],
    ["PSM / OUTPUT", "TIL_CKSTEP", "Output tile K step", ""],
    ["PSM / OUTPUT", "INACTIVE_COLS", "Inactive PE columns", ""],
    ["PSM / OUTPUT", "PRELOAD_EN", "Preload SRAMC/Psum enable", ""],
    
    ["MEMORY / BASE", "ACT_BASE_ADDR", "Base nội bộ SRAMA", "Trỏ vào vị trí tensor của IFMAP"],
    ["MEMORY / BASE", "WEI_BASE_ADDR", "Base nội bộ SRAMB", "Trỏ vào vị trí tensor của WEIGHT"],
    ["MEMORY / BASE", "OUT_BASE_ADDR", "Base nội bộ SRAMC", "Trỏ vào vị trí tensor của PSM/OUT"],
    ["MEMORY / BASE", "ACT_DRAM_BASE", "Base trong initial_dram.txt (Activation)", ""],
    ["MEMORY / BASE", "WEI_DRAM_BASE", "Base trong initital_dram.txt (Weight)", ""],
    ["MEMORY / BASE", "OUT_DRAM_BASE", "Base trong initial/golden_dram.txt (Output)", ""],
    ["MEMORY / BASE", "select", "Tín hiệu chọn buffer", "Ví dụ Swap double-buffer select 000 / 111"]
]

# Đóng gói dữ liệu thành DataFrame
columns = ["Nhóm", "Parameter", "Ý nghĩa", "Ghi chú"]
df = pd.DataFrame(data, columns=columns)

# Tên file xuất ra
excel_filename = "Sauria_Runtime_Parameters_Filtered.xlsx"

# Xuất ra file Excel
df.to_excel(excel_filename, index=False)

print(f"Bảng dữ liệu đã được tạo: {excel_filename}")