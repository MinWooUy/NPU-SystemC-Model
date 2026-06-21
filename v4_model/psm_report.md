# SAURIA Partial Sum Memory (PSM) Block Report

This report documents the architectural parameters, dual-mode FSM implementation details, and verification results for the Partial Sum Memory (PSM) block of the SAURIA NPU core.

---

## 1. Architectural Parameter Integration

We updated the PSM block in [psm_top.h](file:///data/XPU00000/users/vuong.nguyen/project/sauria/RTL/src/v4_model/psm/psm_top.h) to align with hardware parameters and support parameterized dimensions:

```cpp
template <
    int X_DIM = 32,
    int Y_DIM = 32,
    typename T_PSUM = float,
    int SRAMC_CAP = 2048
>
class Psm : public sc_module { ... }
```

### A. Parameter Description
- **`X_DIM` / `Y_DIM`**: Grid width/height dimensions. Controls the flat array sizing and wavefront loops.
- **`T_PSUM`**: Truncated type of the partial sum accumulators.
- **`SRAMC_CAP`**: Storage line capacity of the connected SRAM C bank.

### B. Dual-Mode operations
We refactored the sequential FSM to run in two distinct modes configured by `i_preload_en`:

1. **Scan-out (Store) Mode (`i_preload_en = false`)**
   - Accumulator values shift out from the leftmost column of the Systolic Array.
   - We introduced `delay_cnt` to insert a 1-cycle pipeline delay, waiting for the first valid wavefront vector to arrive.
   - Drives `o_sramc_wren`, `o_sramc_addr`, and `o_sramc_wmask` to write data column-by-column to SRAM C.

2. **Preload (Load) Mode (`i_preload_en = true`)**
   - Reads preloaded bias/weight vectors from SRAM C and shifts them right-to-left into the Systolic Array.
   - Since SRAM C has a 1-cycle read latency, the first read command is issued on Cycle 0 (at `fsm_start`), and address incrementing is staggered to synchronize with data arriving on `i_sramc_rdata` starting from Cycle 2.

---

## 2. Standalone Testbench Architecture

The verification suite in [tb_psm.cpp](file:///data/XPU00000/users/vuong.nguyen/project/sauria/RTL/src/v4_model/psm/tb_psm.cpp) instantiates the PSM under test with dimension parameters `X_DIM=4, Y_DIM=4, SRAMC_CAP=128`.

### A. Mock SRAM C
A behavioral `MockSramC` model is implemented to capture:
- **Write-masking**: Only writes elements of the vector corresponding to active bits in `i_wmask`.
- **1-cycle read latency**: Simulates real SRAM synchronous read timing behavior using a registered internal output buffer.

### B. Verification Scenarios
- **TC1: Scan-out / Store Mode**
  - Asserts `fsm_start` and feeds staggered float vectors to the PSM.
  - Verifies that values written to Mock SRAM C match the expected inputs at each corresponding address.
- **TC2: Preload / Load Mode**
  - Populates the Mock SRAM C with predetermined float vectors.
  - Asserts `i_preload_en` and `fsm_start`.
  - Captures shifted out output values from the PSM and verifies they match the loaded memory contents.

---

## 3. Compilation & Verification Output

### A. Compilation Command
```bash
g++ -O3 -Wall -std=c++17 -I. \
  -I/data/XPU00000/users/vuong.nguyen/project/sauria/systemc_install/include \
  psm/tb_psm.cpp \
  -L/data/XPU00000/users/vuong.nguyen/project/sauria/systemc_install/lib \
  -lsystemc -Wl,-rpath,/data/XPU00000/users/vuong.nguyen/project/sauria/systemc_install/lib \
  -o tb_psm
```

### B. Execution Log
```
=============================================================
          SAURIA Standalone PSM block Testbench
=============================================================

[TB] Reset released.

>>> Starting TC 1: Scan-out Mode (Writing Accumulators to SRAM C)...
[TB] Scan-out finished.

--- TC 1 SRAM C WRITE CHECKS ---
 SRAM Address |  SRAM Written Data  |  Expected Wavefront  | Status
--------------+---------------------+----------------------+--------
      0       |  [1.1, 2.2, 3.3, 4.4]  |  [1.1, 2.2, 3.3, 4.4]  | PASS
      1       |  [5.5, 6.6, 7.7, 8.8]  |  [5.5, 6.6, 7.7, 8.8]  | PASS
      2       |  [9.9, 10.1, 11.2, 12.3]  |  [9.9, 10.1, 11.2, 12.3]  | PASS
      3       |  [13.4, 14.5, 15.6, 16.7]  |  [13.4, 14.5, 15.6, 16.7]  | PASS
>>> TC 1 PASSED SUCCESSFULLY!

>>> Starting TC 2: Preload Mode (Reading from SRAM C into Array)...
[TB] Preload finished.

--- TC 2 READBACK PREVIEW ---
 Step Index |  Shifted Array Out  |   Mock SRAM Data    | Status
------------+---------------------+---------------------+--------
     0      |  [100, 101, 102, 103]  |  [100, 101, 102, 103]  | PASS
     1      |  [200, 201, 202, 203]  |  [200, 201, 202, 203]  | PASS
     2      |  [300, 301, 302, 303]  |  [300, 301, 302, 303]  | PASS
     3      |  [400, 401, 402, 403]  |  [400, 401, 402, 403]  | PASS
>>> TC 2 PASSED SUCCESSFULLY!

=============================================================
           SAURIA Standalone PSM Simulation Ended            
=============================================================
```
