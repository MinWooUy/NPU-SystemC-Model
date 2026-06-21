// Copyright 2026.
// SPDX-License-Identifier: Apache-2.0
//
// FX1 NPU DSE sweep driver.
// Builds a camera-style CNN (depthwise-heavy, detection head with few channels),
// sweeps array size {16,32,48,64} and precision {FP16, INT8}, and prints a table
// plus CSV that answers "why 32x32, not 64x64?".

#include "dse_model.h"
#include <cstdio>
#include <vector>

using namespace fx1_dse;

// A representative camera-detection backbone+head (MobileNet-ish + tiny-YOLO head).
// Resolution 640x640 input (per FX1 KPI), INT8 no-prune baseline class of workload.
// Channels deliberately include the small detection head (where big arrays starve).
static std::vector<Layer> camera_cnn() {
    std::vector<Layer> n;
    // stem
    n.push_back({"conv_stem",   LayerKind::CONV,      320,320,  3,  32, 3,3, 2,1});
    // depthwise separable blocks (the camera-CNN workhorse)
    n.push_back({"dw1",         LayerKind::DEPTHWISE, 320,320, 32,  32, 3,3, 1,1});
    n.push_back({"pw1",         LayerKind::POINTWISE, 320,320, 32,  64, 1,1, 1,1});
    n.push_back({"dw2",         LayerKind::DEPTHWISE, 160,160, 64,  64, 3,3, 2,1});
    n.push_back({"pw2",         LayerKind::POINTWISE, 160,160, 64, 128, 1,1, 1,1});
    n.push_back({"dw3",         LayerKind::DEPTHWISE,  80, 80,128, 128, 3,3, 2,1});
    n.push_back({"pw3",         LayerKind::POINTWISE,  80, 80,128, 256, 1,1, 1,1});
    n.push_back({"dw4",         LayerKind::DEPTHWISE,  40, 40,256, 256, 3,3, 2,1});
    n.push_back({"pw4",         LayerKind::POINTWISE,  40, 40,256, 512, 1,1, 1,1});
    // a couple of standard convs (large reduction -> good for big arrays)
    n.push_back({"conv_neck",   LayerKind::CONV,       40, 40,512, 512, 3,3, 1,1});
    n.push_back({"conv_neck2",  LayerKind::CONV,       20, 20,512, 512, 3,3, 2,1});
    // detection heads: FEW output channels -> column starvation on big arrays
    n.push_back({"head_cls",    LayerKind::CONV,       20, 20,512,  80, 3,3, 1,1});
    n.push_back({"head_box",    LayerKind::CONV,       20, 20,512,  36, 3,3, 1,1}); // 4*9 anchors
    n.push_back({"head_obj",    LayerKind::CONV,       20, 20,512,   9, 1,1, 1,1}); // very few channels
    return n;
}

int main() {
    auto net = camera_cnn();
    Calib cal;

    std::vector<int> sizes = {16, 32, 48, 64};
    std::vector<Dtype> dts = {Dtype::FP16, Dtype::INT8};

    printf("\n================ FX1 NPU DSE: array-size sweep (camera CNN) ================\n");
    printf("Calibrated to SAURIA 22nm: 16x16 FP16 = %.3f mm2, %.0f mW, ref.\n\n",
           cal.ref_area_mm2, cal.ref_power_mW);

    printf("%-6s %-6s %8s %7s %9s %9s %9s %9s %9s %9s\n",
           "dtype","array","PEs","util%","peakGOPS","effGOPS","area_mm2","powmW","GOPS/W","lat_ms");
    printf("--------------------------------------------------------------------------------------------\n");

    FILE* csv = fopen("dse_sweep.csv", "w");
    fprintf(csv, "dtype,X,Y,PEs,util_pct,peak_gops,eff_gops,area_mm2,power_mW,dram_mW,gops_per_w,latency_ms,bytes_dram\n");

    for (auto dt : dts) {
        const char* dn = (dt==Dtype::INT8)?"INT8":"FP16";
        for (int s : sizes) {
            HwConfig hw;
            hw.X = s; hw.Y = s; hw.dtype = dt;
            hw.freq_hz = 500e6; hw.dram_bw_GBs = 6.4;
            EvalResult e = eval_network(net, hw, cal);

            printf("%-6s %2dx%-3d %8d %6.1f %9.0f %9.0f %9.3f %9.0f %9.0f %9.2f\n",
                   dn, s, s, s*s, e.avg_util*100.0,
                   e.peak_gops, e.eff_gops, e.area_mm2,
                   e.power_mW + e.dram_power_mW, e.gops_per_w, e.latency_ms);

            fprintf(csv, "%s,%d,%d,%d,%.2f,%.1f,%.1f,%.4f,%.1f,%.1f,%.3f,%.4f,%lld\n",
                    dn, s, s, s*s, e.avg_util*100.0, e.peak_gops, e.eff_gops,
                    e.area_mm2, e.power_mW, e.dram_power_mW, e.gops_per_w,
                    e.latency_ms, e.total_bytes_dram);
        }
        printf("--------------------------------------------------------------------------------------------\n");
    }
    fclose(csv);

    // Per-layer utilization breakdown for the headline 32 vs 64 comparison (FP16).
    printf("\n---- Per-layer PE utilization: 32x32 vs 64x64 (FP16) ----\n");
    printf("%-12s %-11s %8s %8s\n", "layer","kind","u32%","u64%");
    HwConfig h32; h32.X=32;h32.Y=32; HwConfig h64; h64.X=64;h64.Y=64;
    const char* kn[] = {"CONV","DEPTHWISE","POINTWISE","FC"};
    for (auto& L : net) {
        auto r32 = eval_layer(L, h32);
        auto r64 = eval_layer(L, h64);
        printf("%-12s %-11s %7.1f %7.1f\n", L.name.c_str(),
               kn[(int)L.kind], r32.pe_util*100.0, r64.pe_util*100.0);
    }
    printf("\nCSV written: dse_sweep.csv\n");
    return 0;
}
