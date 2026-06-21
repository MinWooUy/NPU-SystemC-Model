// Copyright 2026 Barcelona Supercomputing Center (BSC)
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
//
// SystemC Model for SAURIA NPU Core
// Standalone Systolic Array Testbench (tb_sa_array.cpp)

#include "../sauria_types.h"
#include "sa_array.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <cassert>

using namespace sauria;

class TestbenchSA : public sc_module {
public:
    sc_in<bool> i_clk{"i_clk"};
    sc_out<bool> o_rstn{"o_rstn"};
    
    sc_out<float> o_threshold{"o_threshold"};
    sc_out<act_vector_t<4, float>> o_act_arr{"o_act_arr"};
    sc_out<wei_vector_t<4, float>> o_wei_arr{"o_wei_arr"};
    sc_out<psum_vector_t<4, float>> o_c_arr{"o_c_arr"};
    
    sc_out<bool> o_pipeline_en{"o_pipeline_en"};
    sc_out<bool> o_cscan_en{"o_cscan_en"};
    sc_out<sc_bv<4>> o_cswitch_arr{"o_cswitch_arr"};
    sc_out<bool> o_sa_clear{"o_sa_clear"};
    
    sc_in<psum_vector_t<4, float>> i_c_arr{"i_c_arr"};

    // Pointer to the array under test to inspect internal state
    SystolicArray<4, 4, float, float, float>* dut{nullptr};

    SC_HAS_PROCESS(TestbenchSA);
    TestbenchSA(sc_module_name nm, SystolicArray<4, 4, float, float, float>* array_dut) 
        : sc_module(nm), dut(array_dut) {
        SC_THREAD(test_process);
        sensitive << i_clk.pos();
    }

private:
    void test_process() {
        std::cout << "\n=============================================================" << std::endl;
        std::cout << "       SAURIA Standalone 4x4 Systolic Array Testbench" << std::endl;
        std::cout << "=============================================================\n" << std::endl;

        // Initialize ports
        o_rstn.write(false);
        o_threshold.write(0.05f);
        o_act_arr.write(act_vector_t<4, float>());
        o_wei_arr.write(wei_vector_t<4, float>());
        o_c_arr.write(psum_vector_t<4, float>());
        o_pipeline_en.write(false);
        o_cscan_en.write(false);
        o_cswitch_arr.write(sc_bv<4>(0));
        o_sa_clear.write(false);

        wait(3);
        o_rstn.write(true);
        std::cout << "[TB] Reset released." << std::endl;
        wait();

        // ----------------------------------------------------
        // TESTCASE 1: Standard 4x4 Matrix Multiplication
        // ----------------------------------------------------
        std::cout << "\n>>> Starting TC 1: Standard 4x4 Wavefront Matrix Multiplication..." << std::endl;
        o_pipeline_en.write(true);
        
        // Define matrices (K = 4)
        // A is activations, B is weights
        float A[4][4] = {
            {1.0f, 2.0f, 3.0f, 4.0f},
            {1.5f, 2.5f, 3.5f, 4.5f},
            {2.0f, 3.0f, 4.0f, 5.0f},
            {2.5f, 3.5f, 4.5f, 5.5f}
        };

        float B[4][4] = {
            {0.5f, 1.0f, 1.5f, 2.0f},
            {0.8f, 1.2f, 1.6f, 2.0f},
            {1.1f, 1.5f, 1.9f, 2.3f},
            {1.4f, 1.8f, 2.2f, 2.6f}
        };

        // Compute Golden Reference
        float Gold[4][4] = {0.0f};
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 4; x++) {
                for (int k = 0; k < 4; k++) {
                    Gold[y][x] += A[y][k] * B[k][x];
                }
            }
        }

        // Run systolic execution
        // Inputs must be skewed (offset by x horizontally, offset by y vertically)
        // Total cycles to execute: K + X + Y = 4 + 4 + 4 = 12 cycles
        for (int cycle = 0; cycle < 12; cycle++) {
            act_vector_t<4, float> act_in;
            wei_vector_t<4, float> wei_in;

            for (int y = 0; y < 4; y++) {
                // Skew activations: A[y][k] where k = cycle - y
                int k_act = cycle - y;
                act_in[y] = (k_act >= 0 && k_act < 4) ? A[y][k_act] : 0.0f;
            }

            for (int x = 0; x < 4; x++) {
                // Skew weights: B[k][x] where k = cycle - x
                int k_wei = cycle - x;
                wei_in[x] = (k_wei >= 0 && k_wei < 4) ? B[k_wei][x] : 0.0f;
            }

            o_act_arr.write(act_in);
            o_wei_arr.write(wei_in);
            wait();
        }

        o_pipeline_en.write(false);
        o_act_arr.write(act_vector_t<4, float>());
        o_wei_arr.write(wei_vector_t<4, float>());
        wait();

        // Verify accumulated MAC registers directly in DUT
        bool tc1_passed = true;
        std::cout << "\n--- TC 1 RESULTS PREVIEW ---" << std::endl;
        std::cout << " PE (Y,X) |  NPU Accumulator  |  Golden Reference  | Status" << std::endl;
        std::cout << "----------+-------------------+--------------------+--------" << std::endl;
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 4; x++) {
                float npu_val = dut->get_pe_mac(y, x);
                float gold_val = Gold[y][x];
                float diff = std::abs(npu_val - gold_val);
                bool pass = (diff <= 1e-3);
                if (!pass) tc1_passed = false;
                std::cout << "   (" << y << "," << x << ")  |  " 
                          << std::setw(15) << npu_val << "  |  "
                          << std::setw(17) << gold_val << "  | "
                          << (pass ? "PASS" : "*FAIL*") << std::endl;
            }
        }
        if (tc1_passed) {
            std::cout << ">>> TC 1 PASSED SUCCESSFULLY!" << std::endl;
        } else {
            std::cout << ">>> TC 1 FAILED!" << std::endl;
        }

        // ----------------------------------------------------
        // TESTCASE 2: Context Switch & Scan-out
        // ----------------------------------------------------
        std::cout << "\n>>> Starting TC 2: Context Swapping & Scan-out chain test..." << std::endl;
        o_pipeline_en.write(true);
        wait();
        
        // Assert context switch to swap mac_q (active) with mac_sc_q (shadow context)
        // Staggered context switch is enabled, we need to assert for 1 cycle per column
        for (int x = 0; x < 4; x++) {
            sc_bv<4> cswitch_mask(0);
            cswitch_mask[x] = true;
            o_cswitch_arr.write(cswitch_mask);
            wait();
        }
        o_cswitch_arr.write(sc_bv<4>(0));

        // Wait for context switches to propagate completely
        wait(8);

        // Check if values have been successfully swapped to shadow context (mac_sc_q)
        bool tc2_cswitch_passed = true;
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 4; x++) {
                float npu_sc_val = dut->get_pe_mac_sc(y, x);
                float gold_val = Gold[y][x];
                if (std::abs(npu_sc_val - gold_val) > 1e-3) {
                    tc2_cswitch_passed = false;
                }
            }
        }
        std::cout << " * Context swap check: " << (tc2_cswitch_passed ? "PASS" : "FAIL") << std::endl;

        // Shift out shadow accumulators using Right-to-Left scan chain
        o_cscan_en.write(true);
        wait(); // Wait 1 cycle for o_cscan_en to propagate to the next clock edge
        float readback_C[4][4] = {0.0f};

        // Shift out column by column
        for (int step = 0; step < 4; step++) {
            psum_vector_t<4, float> scan_out = i_c_arr.read();
            for (int y = 0; y < 4; y++) {
                readback_C[y][step] = scan_out[y];
            }
            wait();
        }
        o_cscan_en.write(false);
        o_pipeline_en.write(false);

        bool tc2_scan_passed = true;
        std::cout << "\n--- TC 2 READBACK PREVIEW ---" << std::endl;
        std::cout << " PE (Y,X) |  Scan-out Value   |  Golden Reference  | Status" << std::endl;
        std::cout << "----------+-------------------+--------------------+--------" << std::endl;
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 4; x++) {
                float scan_val = readback_C[y][x];
                float gold_val = Gold[y][x];
                float diff = std::abs(scan_val - gold_val);
                bool pass = (diff <= 1e-3);
                if (!pass) tc2_scan_passed = false;
                std::cout << "   (" << y << "," << x << ")  |  " 
                          << std::setw(15) << scan_val << "  |  "
                          << std::setw(17) << gold_val << "  | "
                          << (pass ? "PASS" : "*FAIL*") << std::endl;
            }
        }

        if (tc2_cswitch_passed && tc2_scan_passed) {
            std::cout << ">>> TC 2 PASSED SUCCESSFULLY!" << std::endl;
        } else {
            std::cout << ">>> TC 2 FAILED!" << std::endl;
        }

        // ----------------------------------------------------
        // TESTCASE 3: Approximate Arithmetic Gating & Negligence Gating
        // ----------------------------------------------------
        std::cout << "\n>>> Starting TC 3: Zero Gating Threshold Negligence test..." << std::endl;
        o_sa_clear.write(true);
        wait(2);
        o_sa_clear.write(false);
        o_pipeline_en.write(true);
        wait();

        // Feed inputs that are below the threshold (0.05f)
        float below_A[4][4] = {
            {0.01f, 0.02f, 0.01f, 0.03f},
            {0.02f, 0.01f, 0.02f, 0.01f},
            {0.01f, 0.03f, 0.01f, 0.02f},
            {0.03f, 0.01f, 0.02f, 0.01f}
        };
        float normal_B[4][4] = {
            {1.0f, 1.0f, 1.0f, 1.0f},
            {1.0f, 1.0f, 1.0f, 1.0f},
            {1.0f, 1.0f, 1.0f, 1.0f},
            {1.0f, 1.0f, 1.0f, 1.0f}
        };

        for (int cycle = 0; cycle < 12; cycle++) {
            act_vector_t<4, float> act_in;
            wei_vector_t<4, float> wei_in;

            for (int y = 0; y < 4; y++) {
                int k_act = cycle - y;
                act_in[y] = (k_act >= 0 && k_act < 4) ? below_A[y][k_act] : 0.0f;
            }
            for (int x = 0; x < 4; x++) {
                int k_wei = cycle - x;
                wei_in[x] = (k_wei >= 0 && k_wei < 4) ? normal_B[k_wei][x] : 0.0f;
            }

            o_act_arr.write(act_in);
            o_wei_arr.write(wei_in);
            wait();
        }
        o_pipeline_en.write(false);
        wait();

        // Under zero gating logic, all values below threshold should be treated as zero,
        // resulting in exactly 0.0f accumulated.
        bool tc3_passed = true;
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 4; x++) {
                float val = dut->get_pe_mac(y, x);
                if (val != 0.0f) tc3_passed = false;
            }
        }
        std::cout << ">>> TC 3 RESULT: " << (tc3_passed ? "PASSED (all gated to zero)" : "FAILED (leaked accumulation)") << std::endl;

        std::cout << "\n=============================================================" << std::endl;
        std::cout << "         SAURIA Standalone Array Simulation Ended            " << std::endl;
        std::cout << "=============================================================\n" << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char* argv[]) {
    sc_clock clk("clk", 10, SC_NS);
    
    sc_signal<bool> rstn{"rstn"};
    sc_signal<float> threshold{"threshold"};
    sc_signal<act_vector_t<4, float>> act_arr{"act_arr"};
    sc_signal<wei_vector_t<4, float>> wei_arr{"wei_arr"};
    sc_signal<psum_vector_t<4, float>> c_arr_in{"c_arr_in"};
    sc_signal<psum_vector_t<4, float>> c_arr_out{"c_arr_out"};
    sc_signal<bool> pipeline_en{"pipeline_en"};
    sc_signal<bool> cscan_en{"cscan_en"};
    sc_signal<sc_bv<4>> cswitch_arr{"cswitch_arr"};
    sc_signal<bool> sa_clear{"sa_clear"};

    // Setup configuration
    PeConfig config;
    config.arithmetic_type = 1; // FP
    config.stages_mul = 1;
    config.intermediate_pipeline_stage = true;
    config.zero_gating_mult = true;
    config.zero_gating_add = false;

    SystolicArray<4, 4, float, float, float> dut("dut", config);
    TestbenchSA tb("tb", &dut);

    // Bindings
    dut.i_clk(clk);
    dut.i_rstn(rstn);
    dut.i_threshold(threshold);
    dut.i_act_arr(act_arr);
    dut.i_wei_arr(wei_arr);
    dut.i_c_arr(c_arr_in);
    dut.o_c_arr(c_arr_out);
    dut.i_pipeline_en(pipeline_en);
    dut.i_cscan_en(cscan_en);
    dut.i_cswitch_arr(cswitch_arr);
    dut.i_sa_clear(sa_clear);

    tb.i_clk(clk);
    tb.o_rstn(rstn);
    tb.o_threshold(threshold);
    tb.o_act_arr(act_arr);
    tb.o_wei_arr(wei_arr);
    tb.o_c_arr(c_arr_in);
    tb.o_pipeline_en(pipeline_en);
    tb.o_cscan_en(cscan_en);
    tb.o_cswitch_arr(cswitch_arr);
    tb.o_sa_clear(sa_clear);
    tb.i_c_arr(c_arr_out);

    // Waveform tracing
    sc_trace_file* tf = sc_create_vcd_trace_file("waves_sa_array");
    if (tf) {
        sc_trace(tf, clk, "clk");
        sc_trace(tf, rstn, "rstn");
        sc_trace(tf, pipeline_en, "pipeline_en");
        sc_trace(tf, cscan_en, "cscan_en");
    }

    sc_start();

    if (tf) {
        sc_close_vcd_trace_file(tf);
    }
    return 0;
}
