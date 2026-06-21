# SAURIA Control & ConfigRegs Component Refactoring & Verification Report

This report documents the parameterization, design features, and verification of the FSM Controller (`main_controller.h`) and Configuration Registers (`config_regs.h`) blocks in the SAURIA NPU SystemC model.

---

## 1. Parameters & Configuration Integration

To reflect the exact architectural parameters in the RTL design, we updated the `Control` and `ConfigRegs` classes:

* **ConfigRegs Template Parameters**:
  * `IF_W_PARAM` / `IF_ADR_W_PARAM`: Host interface widths.
  * `X_DIM` / `Y_DIM`: Sizing parameters of the systolic array.
  * `TH_W_PARAM`: Bit negligence threshold width.
  * `ACT_IDX_W_PARAM` / `WEI_IDX_W_PARAM` / `OUT_IDX_W_PARAM`: Limits and steps counters bitwidths.
  * `PARAMS_W_PARAM`: Configuration parameter register widths.
  * `DILP_W_PARAM`: Dilation pattern bitwidth.
  * `SRAMB_N_PARAM`: Weight memories count.

* **Controller Template Parameters**:
  * `PE_LAT`: Staggered processing element latency. Defaults to `X_DIM + Y_DIM` cycles.
  * `EXTRA_CSREG`: Extra context registers.

* **FSM Latency Alignment**:
  * Updated FSM transition checking inside `WAIT_CSWITCH` and `LAST_WAIT` states to check against the parameterized `PE_LAT` instead of hardcoded additions.

---

## 2. Host-Side AXI Register Interface

We implemented a functional register interface inside `ConfigRegs` matching the behavior of `config_regs.sv`:

1. **Control Status Register (`0x00000000`)**:
   * **Bit 0**: Start NPU execution (auto-clears; pulses `o_start` to the controller).
   * **Bit 1**: COW (Clear-On-Write) Done flag. Indicates that NPU FSM computation has completed.
   * **Bit 2**: Idle status.
   * **Bit 3**: Ready status.
   * **Bit 16-23**: Soft reset (auto-clears).
2. **Control Configurations (`CFG_CON_OFFSET = 0x200`)**:
   * **Index 0x00**: `incntlim` (Input counter limit)
   * **Index 0x04**: `act_reps` (Activation repetitions)
   * **Index 0x08**: `wei_reps` (Weight repetitions)
3. **Activation Configurations (`CFG_ACT_OFFSET = 0x400`)**:
   * **Index 0x00**: `rows_active` mask (Element-wise active rows mapping)
   * **Index 0x28**: `dil_pat` (Dilation pattern)

---

## 3. Standalone Verification Testbench (`tb_control_cfg.cpp`)

We implemented a standalone closed-loop testbench for the `Control` and `ConfigRegs` subsystem, which executes the following verification stages:

1. **TC1: Host Register Write & Read**
   * Writes unique limits to `incntlim` and `act_reps` using host-side AXI-lite write transactions.
   * Reads back and verifies that the registers retain written values.
2. **TC2: Host-Triggered FSM Start and Execution**
   * Triggers NPU execution by writing `1` to the start bit.
   * Monitors the closed-loop FSM sequence from `IDLE` through `START_COMP`, `ARRAY_CSWITCH`, `WAIT_CSWITCH`, `LAST_WAIT`, and `DONE`.
   * Verifies that the done bit gets raised in the control register and cleared via COW write.
3. **TC3: Deadlock Flag Detection**
   * Drives conflicting empty/full feeder status flags.
   * Assures that the subsystem outputs the correct deadlock alert to the top level.

---

## 4. Verification Execution Evidence

The standalone Control & ConfigRegs testbench compiles and executes successfully:

```
=============================================================
      SAURIA Standalone Control & ConfigRegs Testbench
=============================================================

[TB] Reset released.

>>> Starting TC 1: Host Register Write & Read...
 Readback incntlim: 10 (Expected: 10)
 Readback act_reps: 12 (Expected: 12)
>>> TC 1 PASSED SUCCESSFULLY!

>>> Starting TC 2: Host-Triggered FSM Start and Execution...
 FSM triggered. Waiting for completion...
 FSM finished in 40 cycles.
 Control Register status: 2
 Control Register status after COW clear: 12
>>> TC 2 PASSED SUCCESSFULLY!

>>> Starting TC 3: Deadlock Flag Detection...
 Deadlock output: 1 (Expected: 1)
 Deadlock output: 0 (Expected: 0)
>>> TC 3 PASSED SUCCESSFULLY!

=============================================================
    SAURIA Standalone Control & ConfigRegs ALL TESTS PASSED   
=============================================================
```
