readme_content = """# SAURIA NPU Core - SystemC Model

Mô hình SystemC mô phỏng chính xác theo từng chu kỳ (cycle-accurate) cho lõi tăng tốc phần cứng **SAURIA NPU (Neural Processing Unit)**, được thiết kế dựa trên kiến trúc không gian mảng tâm thu 2D (2D Systolic Array) phục vụ tăng tốc tính toán ma trận và mạng thần kinh nhân tạo.

---

## 📌 Các Tính Năng Cốt Lõi

- **Kiến trúc Mảng Tâm thu Khả cấu hình:** Lưới tính toán $X \times Y$ hỗ trợ cả kiểu dữ liệu số nguyên (INT) và số thực dấu phẩy động (FP).
- **Cơ chế Tiết kiệm Năng lượng (Zero-Gating):** Tự động phát hiện và tắt các bộ nhân/bộ cộng khi dữ liệu đầu vào nằm dưới ngưỡng cấu hình (`threshold`), tối ưu hóa công suất động cho ma trận thưa (sparsity).
- **Chuyển đổi Ngữ cảnh Hai lớp (Double-Buffering & Context Switch):**
  - Hệ thống bộ nhớ SRAM (A, B, C) sử dụng Double-Buffering giúp cô lập và song song hóa quá trình truyền dữ liệu từ Host với quá trình tính toán của NPU.
  - Các phần tử xử lý (PE) tích hợp mạng thanh ghi bóng (shadow/scan register) hỗ trợ đổi ngữ cảnh (`cswitch`) tức thời mà không làm gián đoạn luồng tính toán.
- **Quản lý Năng lượng Nâng cao:** Mô phỏng các trạng thái tiêu thụ năng lượng thấp gồm _Deep Sleep_ (giữ lại trạng thái nhưng tắt đầu ra) và _Power Gating_ (mất trạng thái dữ liệu).

---

## 📂 Cấu Trúc Thư Mục Dự Án

- `npu_top.h`: Module bọc mức cao nhất (Top-level Wrapper), kết nối các khối chức năng và quản lý dồn kênh giao tiếp Host AXI.
- `sauria_types.h`: Định nghĩa các kiểu dữ liệu dùng chung, cấu trúc vector tín hiệu và hàm dò vết sóng (`sc_trace`).
- `control/`
  - `main_controller.h`: Khối FSM điều khiển trung tâm quản lý toàn bộ chu kỳ chuẩn bị, tính toán, lật ngữ cảnh và xả dữ liệu.
- `data_feeder/`
  - `ifmap_feeder.h`: Đọc bộ nhớ activations từ SRAM A, tạo trễ nấc thang (wavefront skew delay) để đẩy luồng dữ liệu vào hàng của mảng.
  - `wei_feeder.h`: Đọc bộ nhớ trọng số từ SRAM B, nạp song song vào các cột của mảng tâm thu.
- `systolic_array/`
  - `sa_array.h`: Ghép nối các phần tử xử lý thành mảng 2D hoàn chỉnh.
  - `sa_processing_element.h`: Kiến trúc chi tiết một phần tử xử lý (PE) gồm thanh ghi, bộ pipeline nhân và bộ tích lũy.
- `psm/`
  - `psm_top.h`: Khối Partial Sum Memory quản lý luồng quét chuỗi dữ liệu kết quả ra từ mảng tâm thu (`cscan`) hoặc nạp trước dữ liệu (`preload`).
- `sram/`
  - `sram_top.h`: Khối điều phối bộ nhớ SRAM kép (SRAM A, B, C) tích hợp logic kiểm soát năng lượng toàn cục.
- `config/`
  - `config_regs.h`: Các thanh ghi cấu hình tĩnh điều khiển giới hạn vòng lặp (`incntlim`), số lần lặp và mặt nạ hàng kích hoạt (`rows_active`).

---

## 🧪 Hệ Thống Kiểm Thử (Verification Suite)

Mã nguồn đi kèm hệ thống testbench phân cấp rất toàn diện:

1. `tb.cpp`: Kiểm tra luồng Host AXI cơ bản, ghi/đọc SRAM và chu trình chạy NPU cơ bản.
2. `tb_32x32.cpp`: Kiểm thử nhân ma trận lớn $32 \times 32$ với dữ liệu ngẫu nhiên, phân tích độ thưa (Sparsity) và đánh giá hiệu năng GFLOPS đạt được.
3. `tb_32x32_stress.cpp`: Bộ stress test đa kịch bản (Ma trận đơn vị, độ thưa cực hạn 95% zero, bão hòa hằng số toàn 1, và kiểm tra biên quyết định threshold).
4. `tb_sa_array.cpp`, `tb_sram.cpp`, `tb_psm.cpp`, `tb_data_feeder.cpp`: Các testbench độc lập (standalone) kiểm tra biên chức năng cho từng module rời rạc.

---

## 🚀 Hướng Dẫn Chạy Mô Phỏng (Run Flow Template)

_Vui lòng điền quy trình biên dịch và chạy mô phỏng cụ thể cho môi trường của bạn vào biểu mẫu dưới đây:_
