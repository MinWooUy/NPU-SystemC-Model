# SAURIA SRAM Component Refactoring & Verification Report

This report documents the parameterization, design features, and verification of the double-buffered SRAM block (`sram_top.h`) in the SAURIA NPU SystemC model.

---

## 1. Parameters & Configuration Integration

To align with the hardware parameters and provide arbitrary power-of-two scalability without hardcoded constraints, we updated the `Sram` class:

* **Generic Sizing & Data Types**:
  * `X_DIM` / `Y_DIM`: Sizing of input and output vectors.
  * `T_ACT` / `T_WEI` / `T_PSUM`: Custom precision types for activations, weights, and partial sums.
  * `SRAMA_CAP` / `SRAMB_CAP` / `SRAMC_CAP`: Resizable physical buffer capacities.
* **Dynamic Shift Arithmetic (`log2_const`)**:
  We introduced a compile-time helper function to calculate the AXI sub-word address shift offsets dynamically based on array geometry:
  ```cpp
  static constexpr int log2_const(int n) {
      return (n <= 1) ? 0 : 1 + log2_const(n / 2);
  }
  ```
  This eliminates hardcoded switches and enables seamless scaling to any power-of-two row/column size.

---

## 2. Standalone Verification Testbench (`tb_sram.cpp`)

We implemented a dedicated standalone testbench at `sram/tb_sram.cpp` that configures `Sram` to a $4 \times 4$ size with small capacities to thoroughly validate all memory protocols:

1. **TC1: Host-Side Port Writes & Reads**
   * Verifies host-side write and read requests using sub-word offsets.
   * Asserts correct behavior for SRAM A, B, and C buffer arrays.
2. **TC2: Double Buffering Port Selection & Buffer Isolation**
   * Dynamically toggles buffer mappings using `i_select` configuration bits.
   * Validates that buffer ownership swaps cleanly between the host interface and NPU core, ensuring data isolation.
3. **TC3: Accelerator-Side Writes & Reads**
   * Tests accelerator-side writes into SRAM C with element-wise write masking (`i_sramc_wmask`), ensuring only targeted indices are overwritten.
   * Reads activation and weight vectors from SRAM A and B, validating NPU read port outputs.
4. **TC4: Deep Sleep & Power Gating States**
   * Asserts `i_deepsleep` to check that outputs read as zero while the internal states are preserved.
   * Asserts `i_powergate` to verify that memory arrays are completely cleared (data is wiped).

---

## 3. Verification Execution Evidence

The standalone SRAM testbench compiles and executes successfully:

```
=============================================================
          SAURIA Standalone SRAM block Testbench
=============================================================

[TB] Reset released.

>>> Starting TC 1: Host-Side Port Writes & Reads...
 SRAM A[0] Readback: 10, 11, 12, 13
 SRAM A[1] Readback: 20, 21, 22, 23
 SRAM B[0] Readback: 100, 101, 102, 103
 SRAM C[0] Readback: 1000, 1001, 1002, 1003
>>> TC 1 PASSED SUCCESSFULLY!

>>> Starting TC 2: Double Buffering Port Selection & Buffer Isolation...
 NPU SRAM A[0] (Reads Buffer 1): 10, 11, 12, 13
 NPU SRAM A[0] (Reads Buffer 0): 50, 51, 52, 53
>>> TC 2 PASSED SUCCESSFULLY!

>>> Starting TC 3: Accelerator-Side Writes & Reads...
 NPU SRAM C[0] Readback: 999, 0, 888, 0
>>> TC 3 PASSED SUCCESSFULLY!

>>> Starting TC 4: Deep Sleep & Power Gating States...
 NPU SRAM C[0] in Deep Sleep: 0, 0, 0, 0
 NPU SRAM C[0] after Deep Sleep exit: 999, 0, 888, 0
 NPU SRAM C[0] after Power Gating cycle: 0, 0, 0, 0
>>> TC 4 PASSED SUCCESSFULLY!

=============================================================
          SAURIA Standalone SRAM ALL TESTS PASSED            
=============================================================
```
