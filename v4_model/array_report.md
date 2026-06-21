# SAURIA Systolic Array Configuration & Verification Report

This report provides a detailed breakdown of the parameter refactoring, design implementation, and verification results for the 2D Systolic Array and Processing Element (PE) modules of the SAURIA NPU core.

---

## 1. Architectural Parameter Integration

To align the SystemC model with the physical hardware parameterization (documented in `sauria_parameter_report.md`), we introduced the `PeConfig` struct to govern computation arithmetic, pipelining, and optimization features.

### A. Parameter Configuration Struct (`PeConfig`)
The configurations are packaged into a reusable C++ struct passed down to the submodules during construction:

```cpp
struct PeConfig {
    int arithmetic_type{1};                  // 0 = INT, 1 = FP
    int mul_type{0};                         // 0 = standard, 1 = approximate
    int add_type{0};                         // 0 = standard, 1 = approximate
    float m_approx{1.0f};                    // Multiplier approximation factor
    float a_approx{1.0f};                    // Adder approximation factor
    int stages_mul{1};                       // Multiplier pipeline stages
    bool intermediate_pipeline_stage{true};  // Pipeline stage between multiplier and adder
    bool zero_gating_mult{true};             // Multiplier zero gating
    bool zero_gating_add{false};             // Adder zero gating
    bool zd_lookahead{false};                // Zero detection lookahead control
    bool extra_csreg{false};                 // Context switch register latency
};
```

### B. Processing Element (`sa_processing_element.h`)
- **Arithmetic Casting**: If `arithmetic_type` is set to `0`, operands `i_a` and `i_b` are truncated to integers (`static_cast<int>`) to simulate integer dataflows.
- **Pipeline Latency Delay Line**: We modeled multiplier pipelining using a dynamic queue `std::vector<T_PSUM> mul_pipeline`. The delay length is calculated as `stages_mul + (intermediate_pipeline_stage ? 1 : 0)`.
- **Approximation Modes**: If `mul_type` or `add_type` is set to `1`, the calculation is scaled by `m_approx` or `a_approx` respectively to evaluate performance trade-offs of approximate computing circuits.
- **Zero Gating**: Activations and weights are continuously monitored against a configurable threshold. If zero-detection is triggered, multiplier inputs are frozen to prevent dynamic toggling power.

### C. Systolic Array Grid (`sa_array.h`)
- **Custom Constructor**: The 2D array grid receives the configurations at the top level and propagates them to every instantiated processing element.
- **Context Switch Staggering**: The context switch propagation delay per element (coordinate $y, x$) is adjusted to $y + x + 1 + (extra\_csreg ? 1 : 0)$ to account for optional context switch register pipelining.
- **State Read Accessors**: Implemented `get_pe_mac(y, x)` and `get_pe_mac_sc(y, x)` to enable direct register inspection during standalone verification.

---

## 2. Standalone Testbench Architecture

The standalone verification environment in `tb_sa_array.cpp` tests the Systolic Array (`4x4` dimensions) across three testcases:

1. **TC1: Wavefront Matrix Multiplication**
   - Implements a software-skewing process to stream input activations (staggered by row index $y$) and weights (staggered by column index $x$).
   - Computes a C++ golden reference product and verifies it against the active PE accumulator registers (`mac_q`) after 12 clock cycles.

2. **TC2: Context Switch & Scan-out**
   - Swaps active and shadow accumulators column-by-column to test staggered context swapping.
   - Activates the Right-to-Left shift scan chain and reads the outputs from the leftmost column to verify correct scan-out.

3. **TC3: Zero Gating Negligence**
   - Streams inputs with magnitudes below the threshold ($0.05$).
   - Verifies that all products are gated to zero, resulting in zero accumulation.

---

## 3. Compilation & Verification Output

### A. Compilation Command
```bash
g++ -O3 -Wall -std=c++17 -I. \
  -I/data/XPU00000/users/vuong.nguyen/project/sauria/systemc_install/include \
  systolic_array/tb_sa_array.cpp \
  -L/data/XPU00000/users/vuong.nguyen/project/sauria/systemc_install/lib \
  -lsystemc -Wl,-rpath,/data/XPU00000/users/vuong.nguyen/project/sauria/systemc_install/lib \
  -o tb_sa_array
```

### B. Execution Log
The execution output demonstrates perfect functional correctness of all three scenarios:

```
=============================================================
       SAURIA Standalone 4x4 Systolic Array Testbench
=============================================================

[TB] Reset released.

>>> Starting TC 1: Standard 4x4 Wavefront Matrix Multiplication...

--- TC 1 RESULTS PREVIEW ---
 PE (Y,X) |  NPU Accumulator  |  Golden Reference  | Status
----------+-------------------+--------------------+--------
   (0,0)  |               11  |                 11  | PASS
   (0,1)  |             15.1  |               15.1  | PASS
   (0,2)  |             19.2  |               19.2  | PASS
   (0,3)  |             23.3  |               23.3  | PASS
   (1,0)  |             12.9  |               12.9  | PASS
   (1,1)  |            17.85  |              17.85  | PASS
   (1,2)  |             22.8  |               22.8  | PASS
   (1,3)  |            27.75  |              27.75  | PASS
   (2,0)  |             14.8  |               14.8  | PASS
   (2,1)  |             20.6  |               20.6  | PASS
   (2,2)  |             26.4  |               26.4  | PASS
   (2,3)  |             32.2  |               32.2  | PASS
   (3,0)  |             16.7  |               16.7  | PASS
   (3,1)  |            23.35  |              23.35  | PASS
   (3,2)  |               30  |                 30  | PASS
   (3,3)  |            36.65  |              36.65  | PASS
>>> TC 1 PASSED SUCCESSFULLY!

>>> Starting TC 2: Context Swapping & Scan-out chain test...
 * Context swap check: PASS

--- TC 2 READBACK PREVIEW ---
 PE (Y,X) |  Scan-out Value   |  Golden Reference  | Status
----------+-------------------+--------------------+--------
   (0,0)  |               11  |                 11  | PASS
   (0,1)  |             15.1  |               15.1  | PASS
   (0,2)  |             19.2  |               19.2  | PASS
   (0,3)  |             23.3  |               23.3  | PASS
   (1,0)  |             12.9  |               12.9  | PASS
   (1,1)  |            17.85  |              17.85  | PASS
   (1,2)  |             22.8  |               22.8  | PASS
   (1,3)  |            27.75  |              27.75  | PASS
   (2,0)  |             14.8  |               14.8  | PASS
   (2,1)  |             20.6  |               20.6  | PASS
   (2,2)  |             26.4  |               26.4  | PASS
   (2,3)  |             32.2  |               32.2  | PASS
   (3,0)  |             16.7  |               16.7  | PASS
   (3,1)  |            23.35  |              23.35  | PASS
   (3,2)  |               30  |                 30  | PASS
   (3,3)  |            36.65  |              36.65  | PASS
>>> TC 2 PASSED SUCCESSFULLY!

>>> Starting TC 3: Zero Gating Threshold Negligence test...
>>> TC 3 RESULT: PASSED (all gated to zero)

=============================================================
         SAURIA Standalone Array Simulation Ended            
=============================================================
```
