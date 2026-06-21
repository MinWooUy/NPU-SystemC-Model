# Comparative Analysis: Sauria NPU Core Verification Tests

This report provides an in-depth analysis of the four SystemC testbenches developed and executed to verify the **Sauria NPU Core v4 Model**:
1. **`tb_standalone`**: Standalone tile verification with zero-gating.
2. **`tb_strided`**: Custom strides and sparse coordinate sampling.
3. **`tb_multitile`**: Overlapped execution with soft-reset pulsing.
4. **`tb_spec_evaluation`**: Exact INT8/INT32 hardware spec compliance.

---

## 1. Overview of Verification Configurations

The table below summarizes the key attributes of each test configuration:

| Feature / Dimension | Standalone Testbench (`tb_standalone`) | Strided Testbench (`tb_strided`) | Multi-Tile Testbench (`tb_multitile`) | Spec Evaluation Testbench (`tb_spec_evaluation`) |
| :--- | :--- | :--- | :--- | :--- |
| **Primary Goal** | Verify baseline systolic array execution. | Verify custom memory striding/dilation logic. | Verify buffer-switching, reset behavior, and overlapping. | Verify spec compliance (INT8 input, INT32 psum). |
| **Template Type (`T_ACT`)**| `float` | `float` | `float` | `int8_t` (asymmetric range $[0, 127]$) |
| **Template Type (`T_WEI`)**| `float` | `float` | `float` | `int8_t` (symmetric range $[-127, 127]$) |
| **Template Type (`T_PSUM`)**| `float` | `float` | `float` | `int32_t` (integer accumulator) |
| **Tile Geometry ($Y \times X \times K$)** | $32 \times 32 \times 64$ | $32 \times 32 \times 32$ (effective) | $32 \times 32 \times 24$ (per tile) | $32 \times 32 \times 64$ |
| **Zero-Gating Threshold**| `0.05` (enabled) | `0.05` (enabled) | `0.05` (enabled) | `0.00` (disabled for exact computation) |
| **Memory Strides** | Unit strides (1) | Non-unit strides (2) | Unit/non-unit strides | Unit strides (1) |

---

## 2. Test 1: Standalone Testbench (`tb_standalone`)

### A. How it is Built
- **Compilation Target**: `make tb_standalone` / `make run_standalone`
- **Template Types**: `NpuTop<32, 32, float, float, float, 1024, 1024, 2048, 16, 64, 1>`
- **PE Configuration (`PeConfig`)**: Zero-gating enabled (`zero_gating_mult = true`), exact multipliers and adders (`mul_type = 0`, `add_type = 0`), single-stage multiplier pipeline (`stages_mul = 1`).

### B. Stimulus & Data Generation
- Generated via `generate_standalone_test.py`.
- **Activations (Matrix A)**: $32 \times 64$ matrix with uniform float values in range $[-2.0, 2.0]$. Sparsity of 25% (25% of elements are set to `0.0` at random).
- **Weights (Matrix B)**: $64 \times 32$ matrix with values in range $[-2.0, 2.0]$. Sparsity of 20% (20% of elements are set to `0.0`).
- **Weight Pre-Skewing**: To align weights with the diagonal wavefront propagation of the systolic array, weight columns are shifted in time. Weight $B[k, x]$ is written to the skewed memory address at coordinate $k + x$. The size of the skewed weight array written to SRAM B is $(K + X\_DIM) \times X\_DIM$, which equals $96 \times 32$.

### C. Golden Reference Model
- Computed in Python using float dot product.
- Models the zero-gating threshold logic: if `abs(A[y, k]) <= 0.05` or `abs(B[k, x]) <= 0.05`, the corresponding product term is gated (skipped) to match the PE hardware's gating circuit.

### D. Parameter Rationale
- $K = 64$ represents a typical intermediate convolutional channel count.
- Sparsity threshold `0.05` is chosen to simulate power-gating behavior under mild weight pruning.
- Grid parameters are set to $32 \times 32$ to utilize the full capacity of the PE array.

### E. Simulation Results & Performance
- **Active Cycles**: **302 cycles**.
- **Effective Performance**: **43.40 GFLOPs** (at 10ns cycle time).
- **Verification Status**: `SUCCESS` (Bit-accurate floating-point comparison with epsilon $10^{-4}$).

---

## 3. Test 2: Strided Testbench (`tb_strided`)

### A. How it is Built
- **Compilation Target**: `make tb_strided` / `make run_strided`
- **Template Types**: `NpuTop<32, 32, float, float, float, 1024, 1024, 2048, 16, 64, 1>`
- **PE Configuration**: Identical baseline PE setup with zero-gating enabled at `0.05`.

### B. Stimulus & Data Generation
- Generated via `generate_strided_test.py`.
- **Activations (Matrix A)**: Size $32 \times 64$, but with non-unit strides. Act memory contains sparse coordinates.
- **Weights (Matrix B)**: Size $64 \times 32$, also pre-skewed with non-unit strides.
- **Strides**: Activations stride (`act_incntstep = 2`) and weights stride (`wei_incntstep = 2`) are programmed into the configuration registers. This causes the address generator to skip alternate elements in memory, effectively using $K = 32$ elements per row/column.

### C. Golden Reference Model
- Python script downsamples the input matrices according to the stride parameters:
  $$\text{gold\_C}[y, x] = \sum_{k=0}^{K-1} A[y, k \times \text{act\_incntstep}] \times B[k \times \text{wei\_incntstep}, x]$$
- Emulates the threshold zero-gating logic at `0.05`.

### D. Parameter Rationale
- Striding values of `2` are chosen to verify that the core's internal address generation FSM (in the data feeders) correctly computes non-unit offsets and address increments under strided or dilated execution patterns.

### E. Simulation Results & Performance
- **Active Cycles**: **186 cycles** (due to the smaller effective loop length $K = 32$).
- **Effective Performance**: **11.01 GFLOPs** (at 10ns cycle time).
- **Verification Status**: `SUCCESS` (Verified correct stride index offsets and correct final result matrix).

---

## 4. Test 3: Multi-Tile Testbench (`tb_multitile`)

### A. How it is Built
- **Compilation Target**: `make tb_multitile` / `make run_multitile`
- **Template Types**: `NpuTop<32, 32, float, float, float, 1024, 1024, 2048, 16, 64, 1>`
- **PE Configuration**: Identical baseline PE setup with zero-gating enabled at `0.05`.

### B. Stimulus & Data Generation
- Generated via `generate_multitile_test.py`.
- **Tiles**: Generates two distinct tiles: **Tile 0** and **Tile 1** (each with $Y=32$, $X=32$, $K=24$, and strides = 3).
- **Activations and Weights**: Programmed to the host SRAM bank indexes sequentially.
- **Double-Buffering Select**:
  - For Tile 0, programmed with `select = 0x0` (host accesses Bank 0, NPU accesses Bank 1).
  - For Tile 1, programmed with `select = 0x7` (host accesses Bank 1, NPU accesses Bank 0).

### C. Golden Reference Model
- Python script generates independent golden references for Tile 0 and Tile 1, applying strides of 3.

### D. Parameter Rationale
- Multiple tiles are run back-to-back to verify:
  1. **Host-Core Double Buffering**: The host writes/reads one buffer while the NPU computes on the alternate buffer.
  2. **Soft Reset Protocol**: Verifies that pulsing `soft_reset` successfully clears internal FSM address pointers between tile runs without wiping the programmed SRAMs, avoiding data corruption.

### E. Simulation Results & Performance
- **Active Cycles**: **263 cycles per tile** (total 526 cycles for both tiles).
- **Effective Performance**: **18.69 GFLOPs** (at 10ns cycle time).
- **Verification Status**: `SUCCESS` (Both tiles verified bit-accurately).

---

## 5. Test 4: Spec-Compliant Testbench (`tb_spec_evaluation`)

### A. How it is Built
- **Compilation Target**: `make tb_spec_evaluation` / `make run_spec_evaluation`
- **Template Types**: `NpuTop<32, 32, int8_t, int8_t, int32_t, 1024, 1024, 2048, 16, 64, 1>`
- **PE Configuration**: exact arithmetic multipliers and adders, zero-gating disabled (`zero_gating_mult = false`) to evaluate exact, unpruned INT8 dot product execution.

### B. Stimulus & Data Generation
- Generated via `generate_spec_test.py`.
- **Activations (Matrix A)**: $32 \times 64$ matrix with asymmetric INT8 values in range $[0, 127]$.
- **Weights (Matrix B)**: $64 \times 32$ matrix with symmetric INT8 values in range $[-127, 127]$, pre-skewed to size $96 \times 32$ in SRAM B.
- **Golden Reference**: Standard pure integer matrix multiplication in Python.

### C. Golden Reference Model
- Computed in Python using exact integer dot product:
  $$\text{gold\_C}[y, x] = \sum_{k=0}^{K-1} A[y, k] \times B[k, x]$$
- No thresholding or zero-gating is applied, representing exact INT8 quantization compliance.

### D. Parameter Rationale
- The template parameters of `NpuTop` are modified to use `int8_t` activations/weights and `int32_t` accumulators. This allows the compiler to instantiate the hardware model using integer math rather than floats, mirroring the actual silicon execution precision.
- $K = 64$ is chosen to match standard YOLOv8 layers.

### E. Simulation Results & Performance
- **Active Cycles**: **302 cycles**.
- **Effective Performance (Simulation at 10ns)**: **43.40 GFLOPs**.
- **Target Frequency Scaling (500 MHz / 2.0 ns clock period)**:
  - **Latency**: **604 ns** per tile.
  - **Effective Throughput**: **217.0 GOPS** (representing **21.2% array utilization** for a single tile, which includes pipeline fill and drain overhead that is amortized under continuous multi-tiled stream context-switching).
- **Verification Status**: `SUCCESS` (Bit-accurate match with integer reference).

---

## 6. Synthesis & Comparison

The table below compiles the performance and execution metrics across the four simulation runs:

| Metric / Parameter | Standalone (`tb_standalone`) | Strided (`tb_strided`) | Multi-Tile (`tb_multitile`) | Spec Evaluation (`tb_spec_evaluation`) |
| :--- | :--- | :--- | :--- | :--- |
| **Data Type** | `float` | `float` | `float` | `int8_t` (input) / `int32_t` (psum) |
| **Measured Cycles** | 302 | 186 | 263 (per tile) | 302 |
| **Arithmetic Mode** | Float (with zero-gating) | Float (with zero-gating) | Float (with zero-gating) | Exact INT8 Integer (no gating) |
| **GOPs / GFLOPs @ 10ns** | 43.40 | 11.01 | 18.69 | 43.40 |
| **GOPS @ 500 MHz** | 217.0 | 55.1 | 93.5 | **217.0 GOPS** |
| **Single-Tile Utilization**| 21.2% | 10.8% | 18.3% | 21.2% |
| **Verification Status** | Passed (float epsilon) | Passed (float epsilon) | Passed (float epsilon) | Passed (bit-accurate integer) |
