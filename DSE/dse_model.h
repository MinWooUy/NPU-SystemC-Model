// Copyright 2026.
// SPDX-License-Identifier: Apache-2.0
//
// FX1 NPU — Analytical Design-Space-Exploration (DSE) model
// ----------------------------------------------------------
// Purpose: answer "why 32x32 and not 64x64?" quantitatively, for camera CNNs,
// on an Output-Stationary GeMM systolic array of the SAURIA family.
//
// This is a fast, closed-form model (no SystemC needed) meant to PRODUCE THE
// CURVE that justifies the array-size decision. It is calibrated against the
// SAURIA silicon datapoints (22nm, FP16, 16x16) so the absolute numbers are
// anchored, and it cross-checks against the cycle-accurate SystemC core.
//
// Mapping convention (matches the SystemC core and the paper):
//   - array ROWS  (Y) <- output spatial X (image width tile)
//   - array COLS  (X) <- output channels (K)
//   - reduction over input channels (C) * kernel (Kh*Kw) is the GeMM depth.
//
// Everything here is header-only and dependency-free (std only).

#ifndef FX1_DSE_MODEL_H
#define FX1_DSE_MODEL_H

#include <cstdint>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

namespace fx1_dse {

// ---------------------------------------------------------------------------
// Data type / precision
// ---------------------------------------------------------------------------
enum class Dtype { FP16, INT8, INT16 };

inline int bytes_of(Dtype d) {
    switch (d) {
        case Dtype::INT8:  return 1;
        case Dtype::INT16: return 2;
        case Dtype::FP16:  return 2;
    }
    return 2;
}

// MAC density multiplier vs. the FP16 baseline (per PE, per cycle).
// INT8 MACs are far cheaper; with packing a PE can do ~2 INT8 MACs in the
// area/energy of one FP16 MAC. This is the lever for DSE axis #2 (precision).
inline double mac_density(Dtype d) {
    switch (d) {
        case Dtype::INT8:  return 2.0;   // 2x throughput per PE area
        case Dtype::INT16: return 1.0;
        case Dtype::FP16:  return 1.0;
    }
    return 1.0;
}

// ---------------------------------------------------------------------------
// Layer description (one conv / matmul layer of a camera CNN)
// ---------------------------------------------------------------------------
enum class LayerKind { CONV, DEPTHWISE, POINTWISE, FC };

struct Layer {
    std::string name;
    LayerKind   kind{LayerKind::CONV};
    int H{0}, W{0};      // output spatial height/width
    int C{0};            // input channels
    int K{0};            // output channels
    int Kh{1}, Kw{1};    // kernel size
    int stride{1};
    int dilation{1};

    // MACs for this layer (dense). Depthwise breaks channel reduction.
    long long macs() const {
        if (kind == LayerKind::DEPTHWISE)
            return (long long)H * W * C * Kh * Kw;          // one filter per channel
        return (long long)H * W * K * C * Kh * Kw;
    }
};

// ---------------------------------------------------------------------------
// Hardware configuration point in the design space
// ---------------------------------------------------------------------------
struct HwConfig {
    int    X{32};               // array columns (output channels)
    int    Y{32};               // array rows    (output spatial X)
    double freq_hz{500e6};      // clock
    Dtype  dtype{Dtype::FP16};
    double dram_bw_GBs{6.4};    // off-chip bandwidth (LPDDR3 like SAURIA)
    int    sram_bus_bytes{32};  // per-SRAM data bus width in bytes (256-bit = 32B)
    bool   onchip_im2col{true}; // SAURIA data feeder (no inflated DRAM traffic)
};

// ---------------------------------------------------------------------------
// Calibration constants — anchored to SAURIA silicon (22nm, FP16, 16x16)
// Table I / Fig. 5 of the paper:
//   16x16 = 256 PEs, 0.897 mm2 total area, 258 mW @ peak (dense), 284 GFLOP/s.
//   PEs ~83% of logic area; data feeder ~7% logic / 4% total; PEs ~75% power.
//   DRAM energy ~120 pJ/byte (LPDDR3, DRAMPower).
// We split area/power into a PER-PE term (scales with N^2) and a roughly
// FIXED term (control, feeders, SRAM macros) that does NOT scale with N^2.
// ---------------------------------------------------------------------------
struct Calib {
    // Reference design
    int    ref_pes        = 256;       // 16x16
    double ref_area_mm2   = 0.897;
    double ref_power_mW   = 258.0;

    // Fraction of reference attributable to the PE array (scales ~N^2)
    double pe_area_frac   = 0.45;      // ~45% of 0.897 mm2 is the array logic
    double pe_power_frac  = 0.75;      // PEs are ~75% of power

    // DRAM energy
    double dram_pJ_per_byte = 120.0;

    // Per-PE area/power derived from reference (FP16 baseline)
    double area_per_pe_mm2() const { return ref_area_mm2 * pe_area_frac / ref_pes; }
    double fixed_area_mm2()  const { return ref_area_mm2 * (1.0 - pe_area_frac); }
    double power_per_pe_mW() const { return ref_power_mW * pe_power_frac / ref_pes; }
    double fixed_power_mW()  const { return ref_power_mW * (1.0 - pe_power_frac); }
};

// ---------------------------------------------------------------------------
// Per-layer evaluation result
// ---------------------------------------------------------------------------
struct LayerResult {
    long long cycles{0};
    double    pe_util{0};        // fraction of PEs doing useful MACs (avg)
    long long bytes_dram{0};
    double    dram_stall_frac{0};
    long long macs{0};
};

// ---------------------------------------------------------------------------
// Whole-network evaluation result
// ---------------------------------------------------------------------------
struct EvalResult {
    long long total_cycles{0};
    long long total_macs{0};
    long long total_bytes_dram{0};
    double    avg_util{0};
    double    eff_gops{0};         // effective (real) GOP/s
    double    peak_gops{0};        // peak (paper-style) GOP/s
    double    area_mm2{0};
    double    power_mW{0};         // compute power (scaled), excludes DRAM
    double    dram_power_mW{0};    // amortized DRAM power for this workload
    double    gops_per_w{0};       // effective GOP/s per total watt
    double    latency_ms{0};
};

// ---------------------------------------------------------------------------
// Core analytical kernel: map one layer onto an XxY array.
// ---------------------------------------------------------------------------
inline LayerResult eval_layer(const Layer& L, const HwConfig& hw) {
    LayerResult r;
    r.macs = L.macs();

    const double dens = mac_density(hw.dtype);

    // ---- Utilization model -------------------------------------------------
    // Columns hold output channels K; rows hold output-spatial tiles of size
    // up to Y. Depthwise destroys the cross-channel reduction, collapsing
    // effective column use to ~1 (channel-multiplier = 1).
    int eff_K = (L.kind == LayerKind::DEPTHWISE) ? 1 : L.K;

    // Column utilization: how full the K dimension keeps the X columns.
    // If eff_K >= X, columns are full (1.0) but we pay ceil tiling overhead.
    double col_tiles = std::ceil((double)eff_K / hw.X);
    double col_fill  = (col_tiles > 0) ? (double)eff_K / (col_tiles * hw.X) : 0.0;

    // Row utilization: output width W tiled over Y rows.
    int out_cols = L.W;                 // contiguous output pixels along width
    double row_tiles = std::ceil((double)out_cols / hw.Y);
    double row_fill  = (row_tiles > 0) ? (double)out_cols / (row_tiles * hw.Y) : 0.0;

    // Depthwise also wastes rows badly when spatial is small; keep row model.
    r.pe_util = std::max(0.0, std::min(1.0, col_fill * row_fill));

    // ---- Cycle model -------------------------------------------------------
    // GeMM depth = reduction dimension. Each output tile streams 'depth' MACs.
    long long depth = (long long)L.C * L.Kh * L.Kw;
    if (L.kind == LayerKind::DEPTHWISE) depth = (long long)L.Kh * L.Kw;

    // Number of (row_tile x col_tile x spatial-height) output tiles.
    long long height_tiles = (long long)L.H;            // each output row = a pass set
    long long n_tiles = (long long)row_tiles * (long long)col_tiles * height_tiles;

    // Cycles per tile ~ depth/dens (compute) + pipeline fill (X+Y) + scan-out.
    long long fill = (long long)(hw.X + hw.Y);
    long long compute_cyc = (long long)std::ceil(depth / dens);
    long long per_tile = compute_cyc + fill;

    long long ideal_cycles = n_tiles * per_tile;

    // ---- Bandwidth / DRAM model -------------------------------------------
    // Bytes that must cross DRAM: inputs + weights + outputs (tensor-shaped,
    // because SAURIA feeds im2col on-chip -> no inflated matrix in DRAM).
    int b = bytes_of(hw.dtype);
    long long in_bytes  = (long long)L.H * L.W * L.C * b;        // ifmap (reused via tiling)
    long long wt_bytes  = (L.kind == LayerKind::DEPTHWISE)
                          ? (long long)L.C * L.Kh * L.Kw * b
                          : (long long)L.K * L.C * L.Kh * L.Kw * b;
    long long out_bytes = (long long)L.H * L.W * eff_K * b;

    // Without on-chip im2col, the ifmap is inflated by ~Kh*Kw (explicit lowering).
    double inflate = hw.onchip_im2col ? 1.0 : (double)(L.Kh * L.Kw);
    r.bytes_dram = (long long)(in_bytes * inflate) + wt_bytes + out_bytes;

    // Bandwidth needed to feed the array without stalls vs. available BW.
    double compute_time_s = (double)ideal_cycles / hw.freq_hz;
    double bytes_per_s_needed = (compute_time_s > 0)
                                ? (double)r.bytes_dram / compute_time_s : 0.0;
    double bw_avail = hw.dram_bw_GBs * 1e9;

    // If we need more BW than we have, the array stalls waiting on DRAM.
    double stall = 0.0;
    if (bytes_per_s_needed > bw_avail && bw_avail > 0) {
        stall = 1.0 - (bw_avail / bytes_per_s_needed);
    }
    r.dram_stall_frac = std::max(0.0, std::min(0.95, stall));

    // Real cycles include stall inflation.
    r.cycles = (long long)std::ceil(ideal_cycles / (1.0 - r.dram_stall_frac));
    return r;
}

// ---------------------------------------------------------------------------
// Evaluate a whole network and roll up area/power/efficiency.
// ---------------------------------------------------------------------------
inline EvalResult eval_network(const std::vector<Layer>& net,
                               const HwConfig& hw,
                               const Calib& cal = Calib()) {
    EvalResult e;
    double util_acc = 0.0; double util_w = 0.0;

    for (const auto& L : net) {
        LayerResult lr = eval_layer(L, hw);
        e.total_cycles    += lr.cycles;
        e.total_macs      += lr.macs;
        e.total_bytes_dram+= lr.bytes_dram;
        // weight utilization by MACs (big layers matter more)
        util_acc += lr.pe_util * (double)lr.macs;
        util_w   += (double)lr.macs;
    }
    e.avg_util = (util_w > 0) ? util_acc / util_w : 0.0;

    int pes = hw.X * hw.Y;
    double dens = mac_density(hw.dtype);

    // Peak throughput: 2 ops per MAC (mul+add), scaled by density.
    e.peak_gops = 2.0 * pes * dens * hw.freq_hz / 1e9;

    double exec_time_s = (double)e.total_cycles / hw.freq_hz;
    e.latency_ms = exec_time_s * 1e3;
    e.eff_gops   = (exec_time_s > 0) ? (2.0 * e.total_macs) / exec_time_s / 1e9 : 0.0;

    // Area: per-PE term (scales with PE count, and INT8 PEs are smaller) + fixed.
    double pe_area = cal.area_per_pe_mm2() * pes;
    if (hw.dtype == Dtype::INT8) pe_area *= 0.55;   // INT8 PE ~half the FP16 area
    e.area_mm2 = pe_area + cal.fixed_area_mm2();

    // Compute power: per-PE term gated by utilization (zero-gating saves idle
    // toggling) + fixed overhead.
    double pe_power = cal.power_per_pe_mW() * pes;
    if (hw.dtype == Dtype::INT8) pe_power *= 0.45;  // INT8 MAC ~less than half power
    // Active PEs burn dynamic power; idle PEs leak only (~20% of dynamic here).
    double active = e.avg_util;
    double dyn = pe_power * (active + 0.20 * (1.0 - active));
    e.power_mW = dyn + cal.fixed_power_mW();

    // DRAM power amortized over the run.
    double dram_energy_pJ = (double)e.total_bytes_dram * cal.dram_pJ_per_byte;
    double dram_energy_mJ = dram_energy_pJ / 1e9;
    e.dram_power_mW = (exec_time_s > 0) ? (dram_energy_mJ / exec_time_s) : 0.0; // mJ/s = mW

    double total_w = (e.power_mW + e.dram_power_mW) / 1e3;
    e.gops_per_w = (total_w > 0) ? e.eff_gops / total_w : 0.0;
    return e;
}

} // namespace fx1_dse

#endif // FX1_DSE_MODEL_H
