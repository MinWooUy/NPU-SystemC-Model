# SAURIA Data Feeder Block Report

This report documents the architectural parameterization, pipeline delay alignment for SRAM read latency, and verification results for both the **Activation (IFmap) Feeder** and the **Weight Feeder** blocks of the SAURIA NPU core.

---

## 1. Architectural Parameterization & Refactoring

We updated both feeders in the `v4_model` to align with the parameter report and to support fully parameterized queue dimensions, data types, and buffer depth.

### A. Template Parameter Integration
The feeder classes are parameterized as follows:

- **Activation Feeder** (`data_feeder/ifmap_feeder.h`):
  ```cpp
  template <
      int Y_DIM = 32,
      typename T_ACT = float,
      int SRAMA_CAP = 1024,
      int FIFO_DEPTH = 16
  >
  class IfmapFeeder : public sc_module { ... }
  ```
- **Weight Feeder** (`data_feeder/wei_feeder.h`):
  ```cpp
  template <
      int X_DIM = 32,
      typename T_WEI = float,
      int SRAMB_CAP = 1024,
      int FIFO_DEPTH = 16
  >
  class WeightFeeder : public sc_module { ... }
  ```

### B. Parameters Description
- **`Y_DIM` / `X_DIM`**: Dynamic systolic grid dimensions.
- **`T_ACT` / `T_WEI`**: Activation and Weight operand data types.
- **`SRAMA_CAP` / `SRAMB_CAP`**: Storage line capacity of the connected SRAM A and SRAM B banks.
- **`FIFO_DEPTH`**: Capacity limit of the internal queues before signaling the `o_fifo_full` status backpressure flag.

---

## 2. Pipeline Delay Alignment (SRAM Read Latency)

Mock SRAMs A and B operate with a **1-cycle read latency**. During refactoring, we addressed the cycle alignment issue between asserting the read request (`o_sram_rden`) and pushing the fetched data into the internal queues. 

### Alignment Strategy
Instead of pushing immediately on `rden_q`, we introduced an additional 1-cycle pipeline register `rdata_valid`:
1. **Cycle N**: `i_cnt_en` is `true`. We assert `o_srama_rden` and drive `o_srama_addr`. `rden_q` becomes `true`. `rdata_valid` remains `false`.
2. **Cycle N+1**: The memory controller reads from SRAM. `rdata_valid` gets the value of `rden_q` (`true`).
3. **Cycle N+2**: Data is now valid on `i_srama_data`. Since `rdata_valid` is `true`, the feeder reads the data bus and pushes the valid wavefront vectors into the FIFOs.

This fixes the issue where the feeder would push garbage data (0.0) into the queue before the SRAM read results were placed on the bus.

---

## 3. Standalone Testbench Architecture

The testbench in `data_feeder/tb_data_feeder.cpp` instantiates both feeders under test configured with dimensions `X_DIM=4, Y_DIM=4, FIFO_DEPTH=8`.

### A. Mock SRAM A & B
Behavioral memory models simulate:
- **1-cycle read latency**: Registered internal data lookup.
- **Deterministic initialization**: SRAM A stores values based on `(addr + 1) * 10.0 + y` (for activations), and SRAM B stores `(addr + 1) * 100.0 + x` (for weights).

### B. Verification Scenarios
- **TC1: Activation Feeder Wavefront Skew Test**
  - Loads 4 entries from SRAM A.
  - Pops them out, capturing the wavefront outputs.
  - Verifies the staggered skew delay propagation pattern: Row `y` is delayed by exactly `y` clock cycles.
- **TC2: Weight Feeder Test (Non-skewed Parallel)**
  - Loads 4 weight entries from SRAM B.
  - Pops them out parallelly.
  - Verifies that weights are driven to the systolic array concurrently on all columns without any skewing.
- **TC3: FIFO Full & Empty Capacity bounds**
  - Loads 8 entries into the FIFO (matching `FIFO_DEPTH`).
  - Checks if the `o_fifo_full` flag is raised.
  - Pops all 8 entries and checks if the `o_fifo_empty` flag is successfully asserted.

---

## 4. Compilation & Verification Output

### A. Compilation Command
We added compilation targets directly into the main `Makefile`:
```makefile
tb_data_feeder: data_feeder/tb_data_feeder.cpp $(HEADERS)
	@echo "[BUILD] Compiling Data Feeder Standalone TB..."
	$(CXX) $(CXXFLAGS) data_feeder/tb_data_feeder.cpp $(LDFLAGS) -o $@
	@echo "[BUILD] Built tb_data_feeder successfully!"
```

To build and run:
```bash
make tb_data_feeder && ./tb_data_feeder
```

### B. Execution Log
```
=============================================================
          SAURIA Standalone Feeders Block Testbench          
=============================================================
[TB] Reset asserted.
[TB] Reset released.

>>> Starting TC 1: Activation Feeder Wavefront Skew Test...

Info: (I702) default timescale unit used for tracing: 1 ps (waves_feeders.vcd)
 FIFOs Loaded check: Empty=0 Full=0

--- TC 1 WAVEFRONT SKEW PREVIEW ---
 Step Index |   Row 0   |   Row 1   |   Row 2   |   Row 3   | Status
------------+-----------+-----------+-----------+-----------+--------
     0      |        10 |         0 |         0 |         0 | PASS
     1      |        20 |        11 |         0 |         0 | PASS
     2      |        30 |        21 |        12 |         0 | PASS
     3      |        40 |        31 |        22 |        13 | PASS
     4      |         0 |        41 |        32 |        23 | PASS
     5      |         0 |         0 |        42 |        33 | PASS
     6      |         0 |         0 |         0 |        43 | PASS
     7      |         0 |         0 |         0 |         0 | PASS
>>> TC 1 PASSED SUCCESSFULLY!

>>> Starting TC 2: Weight Feeder Test (Non-skewed Parallel)...

--- TC 2 WEIGHT PARALLEL PREVIEW ---
 Step Index |   Col 0   |   Col 1   |   Col 2   |   Col 3   | Status
------------+-----------+-----------+-----------+-----------+--------
     0      |       100 |       101 |       102 |       103 | PASS
     1      |       200 |       201 |       202 |       203 | PASS
     2      |       300 |       301 |       302 |       303 | PASS
     3      |       400 |       401 |       402 |       403 | PASS
     4      |         0 |         0 |         0 |         0 | PASS
     5      |         0 |         0 |         0 |         0 | PASS
>>> TC 2 PASSED SUCCESSFULLY!

>>> Starting TC 3: FIFO Full Capacity Test...
 FIFO Full check (expected true): Activation Full=YES Weight Full=YES -> Status: PASS
 FIFO Empty check (expected true): Activation Empty=YES Weight Empty=YES -> Status: PASS
>>> TC 3 PASSED SUCCESSFULLY!

=============================================================
          SAURIA Standalone Feeders ALL TESTS PASSED          
=============================================================

Info: /OSCI/SystemC: Simulation stopped by user.
```
