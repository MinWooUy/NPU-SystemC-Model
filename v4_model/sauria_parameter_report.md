# SAURIA NPU Hardware Parameter Inventory & SystemC Mapping Report

This report provides a complete, dedicated inventory of the parameterization scheme across the SAURIA NPU SystemVerilog hardware modules and defines their mapping strategy to the SystemC simulation model.

---

## 1. Block-by-Block Hardware Parameter Breakdown

Below is the count and detailed list of parameters for each hardware module block in the RTL design.

### A. Systolic Array Grid (`sa_array.sv` & `sa_processing_element.sv`)
* **Total Parameters**: 18
* **Detail**:
  * `ARITHMETIC`: Selects integer/fixed-point (`0`) or floating-point (`1`) arithmetic representation.
  * `MUL_TYPE` / `ADD_TYPE`: Chooses the specific multiplier/adder design (e.g., standard or approximate models).
  * `M_APPROX` / `MM_APPROX`: Parameters configuration for approximate multiplication.
  * `A_APPROX` / `AA_APPROX`: Parameters configuration for approximate addition.
  * `X` / `Y`: Columns and rows dimensions of the PE matrix.
  * `IA_W` / `IB_W`: Input bit widths for activations and weights.
  * `OC_W`: Accumulator / partial sum bit width.
  * `TH_W`: Sparsity negligence threshold width (for zero-gating).
  * `STAGES_MUL`: Pipeline stages within the multiplier unit.
  * `INTERMEDIATE_PIPELINE_STAGE`: Pipeline register stage between the multiplier and adder.
  * `ZERO_GATING_MULT` / `ZERO_GATING_ADD`: Gating control switches.
  * `ZD_LOOKAHEAD`: Lookahead control for zero detection.
  * `EXTRA_CSREG`: Extra pipeline register on context switch signals.

### B. Activation (IFmap) Feeder (`ifmap_feeder.sv`)
* **Total Parameters**: 9
* **Detail**:
  * `Y`: Row count matching array height.
  * `FIFO_POSITIONS`: Capacity of each row's activation queue.
  * `IA_W`: Width of activation element.
  * `SRAMA_W`: SRAM A read bus data width.
  * `IDX_W`: Index counters resolution width.
  * `ADRA_W`: SRAM A address width.
  * `DILP_W`: Dilation pattern width.
  * `PARAMS_W`: Bit-width for stride/offset configurations.
  * `M`: Feeder replication/parallelism factor.

### C. Weight Feeder (`wei_feeder.sv`)
* **Total Parameters**: 7
* **Detail**:
  * `X`: Column count matching array width.
  * `FIFO_POSITIONS`: Capacity of each column's weight queue.
  * `IB_W`: Width of weight element.
  * `SRAMB_W`: SRAM B read bus data data width.
  * `IDX_W`: Index counters resolution width.
  * `ADRB_W`: SRAM B address width.
  * `PARAMS_W`: Bit-width for offset configurations.

### D. Partial Sum Memory Manager (`psm_top.sv` & `psm_shift_fsm.sv`)
* **Total Parameters**: 7
* **Detail**:
  * `X` / `Y`: Columns and rows dimensions.
  * `PARAMS_W`: Control parameter bit width.
  * `OC_W`: Partial sum accumulator bit width.
  * `SRAMC_W`: SRAM C write bus data width.
  * `IDX_W`: Counter index width.
  * `ADRC_W`: SRAM C address width.

### E. Configuration Registers (`config_regs.sv`)
* **Total Parameters**: 11
* **Detail**:
  * `IF_W` / `IF_ADR_W`: Host configuration interface data and address widths.
  * `X` / `Y`: Systolic array sizing registers.
  * `TH_W`: Negligence threshold parameter width.
  * `ACT_IDX_W` / `WEI_IDX_W` / `OUT_IDX_W`: Widths of control counters.
  * `PARAMS_W`: Configuration registers control signals width.
  * `DILP_W`: Dilation pattern configuration width.
  * `SRAMB_N`: Element count in weights bus.

### F. Main Controller (`main_controller.sv`)
* **Total Parameters**: 8
* **Detail**:
  * `X` / `Y`: Systolic array columns and rows.
  * `ACT_IDX_W` / `OUT_IDX_W`: Counters widths.
  * `ACT_FIFO_POSITIONS` / `WEI_FIFO_POSITIONS`: Feeders queues bounds.
  * `PE_LAT`: Latency cycle offset of processing element.
  * `EXTRA_CSREG`: Staggered context switch line pipelining configuration.

### G. Core Logic & Top Subsystem (`sauria_logic.sv` & `sauria_subsystem.sv`)
* **Total Parameters**: 9 (Logic) / 5 (Subsystem AXI)
* **Detail**:
  * `CFG_AXI_DATA_WIDTH` / `CFG_AXI_ADDR_WIDTH`: Slave interface sizes.
  * `DATA_AXI_DATA_WIDTH` / `DATA_AXI_ADDR_WIDTH`: Master interface sizes.
  * `DATA_AXI_ID_WIDTH`: Transaction ID width.
  * `ADRA_W` / `SRAMA_W` / `ADRB_W` / `SRAMB_W` / `ADRC_W` / `SRAMC_W`: Memory interconnect port mappings.

---

## 2. Complete Parameter Inventory Checklist

This table categorizes all parameters from the hardware specifications, classifies whether they are retained in SystemC, and provides the architectural rationale.

### A. Systolic Array Configuration
| Parameter | Description | Keep for SystemC? | Rationale |
| :--- | :--- | :--- | :--- |
| `X` | Array Columns (width) | **Yes** | Crucial for template sizing (`grid[Y][X]`) |
| `Y` | Array Rows (height) | **Yes** | Crucial for template sizing |
| `IA_W` | Activation operand bit width | **Yes** | Data type simulation mapping (`float`/`int`/`sc_fixed`) |
| `IB_W` | Weight operand bit width | **Yes** | Data type simulation mapping |
| `OC_W` | Output/Psum bit width | **Yes** | Data type simulation mapping |
| `TH_W` | Gating threshold register width | **Maybe** | Needed only if checking bit-level threshold limits |
| `PARAMS_W` | Control parameters width | **No** | Handled natively by C++ type system |

### B. Arithmetic & PE Configuration
| Parameter | Description | Keep for SystemC? | Rationale |
| :--- | :--- | :--- | :--- |
| `ARITHMETIC` | representation (0=INT, 1=FP) | **Yes** | Routes execution to floating-point or integer math |
| `MUL_TYPE` / `ADD_TYPE` | Multiplier / Adder hardware types | **Yes** | Selects functional approximation model |
| `M_APPROX` / `MM_APPROX` | Mult. approximation factors | **Yes** | Parametrizes approximate math logic |
| `A_APPROX` / `AA_APPROX` | Adder approximation factors | **Yes** | Parametrizes approximate math logic |
| `STAGES_MUL` | Multiplier pipeline stages | **Maybe** | Models computation cycle latency |
| `INTERMEDIATE_PIPELINE_STAGE` | Pipeline register between Mult & Add | **Maybe** | Adds cycle delay to data propagation |
| `ZERO_GATING_MULT` | Multiplier zero gating switch | **Maybe** | Proxy metric for energy estimation |
| `ZERO_GATING_ADD` | Adder zero gating switch | **Maybe** | Proxy metric for energy estimation |
| `ZD_LOOKAHEAD` | Zero detection lookahead control | **Maybe** | Proxy metric for energy estimation |
| `EXTRA_CSREG` | Context switch register latency | **Maybe** | Cycles offset alignment |

### C. FP16 Arithmetic Definitions
| Parameter | Description | Keep for SystemC? | Rationale |
| :--- | :--- | :--- | :--- |
| `FP_W` | Total floating-point bit width | **Yes** | Configures word size for custom formats |
| `MANT_W` | Mantissa bit width | **Yes** | Configures precision limits |
| `EXP_W` | Exponent bit width | **Derived** | Calculated dynamically as `FP_W - MANT_W - 1` |

### D. Memory Configuration
| Parameter | Description | Keep for SystemC? | Rationale |
| :--- | :--- | :--- | :--- |
| `SRAMA_W` / `SRAMB_W` / `SRAMC_W` | Memory data bus widths | **Derived** | Scaled automatically to `element_width * array_dim` |
| `SRAMA_DEPTH` / `SRAMB_DEPTH` / `SRAMC_DEPTH` | Memory row capacities | **Yes** | Allocates behavioral arrays sizes |
| `RF_A` / `RF_B` / `RF_C` | Memory register-file partitioning | **Maybe** | Abstracted unless simulating physical bank collision |
| `ADRA_W` / `ADRB_W` / `ADRC_W` | Memory address bit widths | **Derived** | Computed dynamically as `Ceil(log2(depth))` |
| `SRAMA_N` / `SRAMB_N` / `SRAMC_N` | Elements per bus line | **Derived** | Determined by bus width divided by operand width |

### E. Counter Index Widths
| Parameter | Description | Keep for SystemC? | Rationale |
| :--- | :--- | :--- | :--- |
| `ACT_IDX_W` | Activation index counters width | **Derived** | `ADRA_W + Ceil(log2(SRAMA_N)) + 1` |
| `WEI_IDX_W` | Weight index counters width | **Derived** | `ADRB_W + Ceil(log2(SRAMB_N)) + 1` |
| `OUT_IDX_W` | Output index counters width | **Derived** | `ADRC_W + Ceil(log2(SRAMC_N)) + 1` |

### F. IFmap Feeders Configuration
| Parameter | Description | Keep for SystemC? | Rationale |
| :--- | :--- | :--- | :--- |
| `M` | Feeder replication factor | **Yes** | Sets parallelism of activation streaming |
| `ACT_FIFO_POSITIONS` | IFmap queue capacity | **Maybe** | Used to simulate feeder stall / buffer depth |
| `DILP_W` | Dilation pattern register size | **Maybe** | Abstracted if dilation pattern is hardcoded |

### G. Weight Fetcher Configuration
| Parameter | Description | Keep for SystemC? | Rationale |
| :--- | :--- | :--- | :--- |
| `WEI_FIFO_POSITIONS` | Weight queue capacity | **Maybe** | Used to simulate weight queue stalls |

### H. Clock CDC (Clock Domain Crossings)
| Parameter | Description | Keep for SystemC? | Rationale |
| :--- | :--- | :--- | :--- |
| `CFG_CDC_FIFO_BITS` / `DATA_CDC_FIFO_BITS` | CDC FIFO capacity powers | **No** | CDC interface is bypassed in transaction-level simulation |

### I. uDMA / AXI Configuration
| Parameter | Description | Keep for SystemC? | Rationale |
| :--- | :--- | :--- | :--- |
| `DMA_MAX_ARLEN` / `DMA_MAX_AWLEN` | Max AXI burst lengths | **Yes** | Simulates burst slicing performance constraints |
| `DMA_RFIFO_LEN` / `DMA_WFIFO_LEN` | DMA queue sizes | **Maybe** | Models memory interface throughput throttling |
| `DMA_RADDR_OFFSET` / `DMA_WADDR_OFFSET` | Base AXI offsets | **Maybe** | Models base memory mapping |
| `DATA_ELM_BITS` | AXI bus element size | **Maybe** | Models AXI packing |
| `DMA_SYNC_AW_W` | Channel synchronization control | **No** | Bypassed in transaction-level simulation |
| `DMA_MAX_OUTSTANDING_READS` | Outstanding reads limit | **Yes** | Models concurrent read limits |
| `DMA_MAX_OUTSTANDING_WRITES` | Outstanding writes limit | **Yes** | Models concurrent write limits |

---

## 3. SystemC Parameterization Implementation Plan

To refactor the SystemC model in `v4_model` to match this inventory and allow flexible, compile-time configurable scaling, the following strategies will be used:

### A. Template Parameter Sizing
For parameters that dictate physical sizes, port widths, and array bounds (such as geometry, data widths, and memory capacities), C++ class templates will be implemented:
```cpp
template <
    int X_DIM = 32,
    int Y_DIM = 32,
    typename T_ACT = float,
    typename T_WEI = float,
    typename T_PSUM = float,
    int SRAMA_CAP = 1024,
    int SRAMB_CAP = 1024,
    int SRAMC_CAP = 4096
>
class NpuTop : public sc_module {
    // Submodules instantiated using these template bounds
    SystolicArray<X_DIM, Y_DIM, T_ACT, T_WEI, T_PSUM> systolic_array;
    IfmapFeeder<Y_DIM, T_ACT, SRAMA_CAP> ifmap_feeder;
    WeightFeeder<X_DIM, T_WEI, SRAMB_CAP> weight_feeder;
    Psm<X_DIM, Y_DIM, T_PSUM, SRAMC_CAP> psm_block;
    Sram<X_DIM, Y_DIM, T_ACT, T_WEI, T_PSUM, SRAMA_CAP, SRAMB_CAP, SRAMC_CAP> sram_block;
};
```

### B. Functional Parameter Configuration (Constructor-Based)
For parameters that define computation features rather than hardware bounds (such as `MUL_TYPE`, `ARITHMETIC`, and approximation factors), configurations can be passed to module constructors or configured dynamically via registers:
```cpp
struct PeConfig {
    int arithmetic_type; // 0=INT, 1=FP
    int mul_type;
    int add_type;
    float m_approx;
    float a_approx;
};
```
This hybrid model allows the SystemC simulator to perform cycle-accurate evaluations of various design-space configurations without requiring full recompilation for minor arithmetic variations.
