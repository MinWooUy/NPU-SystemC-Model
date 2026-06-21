// Copyright 2026 Barcelona Supercomputing Center (BSC)
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
//
// SystemC Model for SAURIA NPU Core
// Advanced Multi-Scenario Stress Testbench (tb_32x32_stress.cpp)

#include "npu_top.h"
#include <iomanip>
#include <random>
#include <cmath>

using namespace sauria;

class TestbenchStress32x32 : public sc_module {
public:
    // Clock & Reset Ports
    sc_in<bool> i_clk{"i_clk"};
    sc_out<bool> o_rstn{"o_rstn"};
    sc_out<bool> o_soft_reset{"o_soft_reset"};

    // NPU Host Control Interface
    sc_out<bool> o_start{"o_start"};
    sc_in<bool>  i_done{"i_done"};
    sc_in<bool>  i_deadlock{"i_deadlock"};

    // NPU Host Memory Port (AXI interface modeling)
    sc_out<uint32_t>     o_host_addr{"o_host_addr"};
    sc_out<bool>         o_host_wren{"o_host_wren"};
    sc_out<bool>         o_host_rden{"o_host_rden"};
    sc_out<host_data_t>  o_host_wdata{"o_host_wdata"};
    sc_out<host_mask_t>  o_host_wmask{"o_host_wmask"};
    sc_in<host_data_t>   i_host_rdata{"i_host_rdata"};

    // Configurations
    sc_out<float>        o_threshold{"o_threshold"};
    sc_out<sc_bv<3>>     o_select{"o_select"};

    SC_CTOR(TestbenchStress32x32) {
        SC_THREAD(test_process);
        sensitive << i_clk.pos();
    }

private:
    void reset_system() {
        o_rstn.write(false);
        o_soft_reset.write(false);
        o_start.write(false);
        o_host_addr.write(0);
        o_host_wren.write(false);
        o_host_rden.write(false);
        o_host_wdata.write(host_data_t());
        o_host_wmask.write(host_mask_t());
        wait(3);
        o_rstn.write(true);
        wait(2);
    }

    void write_host_mem(uint32_t addr, const host_data_t& data, const host_mask_t& mask) {
        wait();
        o_host_addr.write(addr);
        o_host_wdata.write(data);
        o_host_wmask.write(mask);
        o_host_wren.write(true);
        o_host_rden.write(false);
        wait();
        o_host_wren.write(false);
    }

    host_data_t read_host_mem(uint32_t addr) {
        wait();
        o_host_addr.write(addr);
        o_host_wren.write(false);
        o_host_rden.write(true);
        wait(); // Address register latency
        wait(); // Data register latency
        o_host_rden.write(false);
        return i_host_rdata.read();
    }

    // Helper to run a matrix multiplication testcase
    bool run_matrix_testcase(const std::string& name, float mat_A[Y][64], float mat_B[64][X], float zero_gating_threshold) {
        std::cout << "\n-------------------------------------------------------------" << std::endl;
        std::cout << " RUNNING: " << name << std::endl;
        std::cout << "-------------------------------------------------------------" << std::endl;

        const int K = 64;
        float gold_C[Y][X];
        
        // Initialize Golden output
        for (int y = 0; y < Y; y++) {
            for (int x = 0; x < X; x++) {
                gold_C[y][x] = 0.0f;
            }
        }

        // Compute Golden Matrix Reference Product
        for (int y = 0; y < Y; y++) {
            for (int x = 0; x < X; x++) {
                for (int k = 0; k < K; k++) {
                    if (std::abs(mat_A[y][k]) > zero_gating_threshold && std::abs(mat_B[k][x]) > zero_gating_threshold) {
                        gold_C[y][x] += mat_A[y][k] * mat_B[k][x];
                    }
                }
            }
        }

        // Reset and Configure
        reset_system();
        o_threshold.write(zero_gating_threshold);
        o_select.write(sc_bv<3>("000")); // Host maps buffers
        wait();

        // Write Matrix A to SRAM A
        host_mask_t full_mask;
        full_mask.data.fill(true);
        for (int k = 0; k < K + X; k++) {
            for (int sub = 0; sub < SUBWORDS_A; sub++) {
                host_data_t wdata;
                for (int i = 0; i < 4; i++) {
                    int y = sub * 4 + i;
                    wdata[i] = (k < K) ? mat_A[y][k] : 0.0f;
                }
                uint32_t addr = SRAMA_OFFSET | (k << SHIFT_A) | sub;
                write_host_mem(addr, wdata, full_mask);
            }
        }

        // Write Matrix B to SRAM B
        for (int k = 0; k < K + X; k++) {
            for (int sub = 0; sub < SUBWORDS_B; sub++) {
                host_data_t wdata;
                for (int i = 0; i < 4; i++) {
                    int x = sub * 4 + i;
                    int matrix_k = k - x;
                    wdata[i] = (matrix_k >= 0 && matrix_k < K) ? mat_B[matrix_k][x] : 0.0f;
                }
                uint32_t addr = SRAMB_OFFSET | (k << SHIFT_B) | sub;
                write_host_mem(addr, wdata, full_mask);
            }
        }

        // Map SRAMs to NPU Core
        wait();
        o_select.write(sc_bv<3>("111"));
        wait();

        // Start compute
        o_start.write(true);
        wait();
        o_start.write(false);

        // Wait for Done
        int timeout = 400;
        bool completed = false;
        while (timeout-- > 0) {
            wait();
            if (i_done.read()) {
                completed = true;
                break;
            }
            if (i_deadlock.read()) {
                std::cerr << "[ERROR] Deadlock detected during compute!" << std::endl;
                sc_stop();
                return false;
            }
        }

        if (!completed) {
            std::cerr << "[ERROR] Simulation Timeout!" << std::endl;
            return false;
        }

        // Map SRAMs back to Host
        wait();
        o_select.write(sc_bv<3>("000"));
        wait();

        // Read C back from SRAM C
        float npu_C[Y][X];
        for (int x = 0; x < X; x++) {
            for (int sub = 0; sub < SUBWORDS_C; sub++) {
                uint32_t addr = SRAMC_OFFSET | (x << SHIFT_C) | sub;
                host_data_t rdata = read_host_mem(addr);
                for (int i = 0; i < 4; i++) {
                    npu_C[sub * 4 + i][x] = rdata[i];
                }
            }
        }

        // Validate results
        bool passed = true;
        double max_err = 0.0;
        for (int y = 0; y < Y; y++) {
            for (int x = 0; x < X; x++) {
                double diff = std::abs(npu_C[y][x] - gold_C[y][x]);
                if (diff > max_err) max_err = diff;
                if (diff > 1e-3) {
                    passed = false;
                }
            }
        }

        std::cout << " * Max Absolute Error: " << max_err << std::endl;
        if (passed) {
            std::cout << " >>> RESULT: [PASSED] " << name << " matches perfectly!" << std::endl;
        } else {
            std::cout << " >>> RESULT: [FAILED] " << name << " mismatched cells found!" << std::endl;
            // Print a few failure details
            int fail_print_limit = 5;
            for (int y = 0; y < Y; y++) {
                for (int x = 0; x < X; x++) {
                    double diff = std::abs(npu_C[y][x] - gold_C[y][x]);
                    if (diff > 1e-3 && fail_print_limit-- > 0) {
                        std::cerr << "   Mismatch at (" << y << "," << x << "): NPU=" << npu_C[y][x]
                                  << ", Gold=" << gold_C[y][x] << ", Diff=" << diff << std::endl;
                    }
                }
            }
        }

        return passed;
    }

    void test_process() {
        std::cout << "\n=============================================================" << std::endl;
        std::cout << "       SAURIA SystemC NPU 32x32 Multi-Scenario Stress Suite" << std::endl;
        std::cout << "=============================================================\n" << std::endl;

        const int K = 64;
        float mat_A[Y][K];
        float mat_B[K][X];
        bool overall_passed = true;

        std::mt19937 gen(1337); // Seeded
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        std::uniform_real_distribution<float> sparsity_dist(0.0f, 1.0f);

        // ==========================================
        // TESTCASE 1: Identity Matrix Multiplication
        // ==========================================
        // A is Identity Matrix, B is Random.
        for (int y = 0; y < Y; y++) {
            for (int k = 0; k < K; k++) {
                mat_A[y][k] = (y == k) ? 1.0f : 0.0f;
            }
        }
        for (int k = 0; k < K; k++) {
            for (int x = 0; x < X; x++) {
                mat_B[k][x] = dist(gen);
            }
        }
        bool tc1 = run_matrix_testcase("TC 1: Identity Matrix Routing Test", mat_A, mat_B, 0.00f);
        overall_passed &= tc1;

        // ==========================================
        // TESTCASE 2: Extreme Sparsity Test (95% zero activations)
        // ==========================================
        for (int k = 0; k < K; k++) {
            for (int y = 0; y < Y; y++) {
                if (sparsity_dist(gen) < 0.95f) {
                    mat_A[y][k] = 0.0f;
                } else {
                    mat_A[y][k] = dist(gen);
                    if (std::abs(mat_A[y][k]) < 0.001f) mat_A[y][k] = 0.5f;
                }
            }
            for (int x = 0; x < X; x++) {
                mat_B[k][x] = dist(gen);
            }
        }
        bool tc2 = run_matrix_testcase("TC 2: Extreme Sparsity (95% zero activations) Test", mat_A, mat_B, 0.05f);
        overall_passed &= tc2;

        // ==========================================
        // TESTCASE 3: Saturation & Peak Accumulation (All-Ones)
        // ==========================================
        for (int y = 0; y < Y; y++) {
            for (int k = 0; k < K; k++) {
                mat_A[y][k] = 1.0f;
            }
        }
        for (int k = 0; k < K; k++) {
            for (int x = 0; x < X; x++) {
                mat_B[k][x] = 1.0f;
            }
        }
        bool tc3 = run_matrix_testcase("TC 3: Constant Accumulation (All Ones) Test", mat_A, mat_B, 0.00f);
        overall_passed &= tc3;

        // ==========================================
        // TESTCASE 4: Threshold Boundary Decision Test
        // ==========================================
        // Generating values very close to threshold = 0.05f
        float threshold = 0.05f;
        for (int y = 0; y < Y; y++) {
            for (int k = 0; k < K; k++) {
                // Alternates between just below and just above threshold
                mat_A[y][k] = ((y + k) % 2 == 0) ? 0.052f : 0.048f;
                if ((y + k) % 3 == 0) mat_A[y][k] = -mat_A[y][k]; // Check negative thresholds
            }
        }
        for (int k = 0; k < K; k++) {
            for (int x = 0; x < X; x++) {
                mat_B[k][x] = ((k + x) % 2 == 0) ? 0.052f : 0.048f;
                if ((k + x) % 3 == 0) mat_B[k][x] = -mat_B[k][x];
            }
        }
        bool tc4 = run_matrix_testcase("TC 4: Gating Threshold Decision Boundary Test", mat_A, mat_B, threshold);
        overall_passed &= tc4;

        // ==========================================
        // FINAL SUMMARY
        // ==========================================
        std::cout << "\n=============================================================" << std::endl;
        std::cout << "                  STRESS TESTBENCH SUMMARY                   " << std::endl;
        std::cout << "=============================================================" << std::endl;
        std::cout << " * TC 1 (Identity Matrix): " << (tc1 ? "PASSED" : "FAILED") << std::endl;
        std::cout << " * TC 2 (95% Sparsity):    " << (tc2 ? "PASSED" : "FAILED") << std::endl;
        std::cout << " * TC 3 (All-Ones Peak):   " << (tc3 ? "PASSED" : "FAILED") << std::endl;
        std::cout << " * TC 4 (Decision Bounds): " << (tc4 ? "PASSED" : "FAILED") << std::endl;
        std::cout << "-------------------------------------------------------------" << std::endl;
        if (overall_passed) {
            std::cout << ">>> OVERALL STATUS: [ALL TESTS PASSED SUCCESSFULLY!]" << std::endl;
        } else {
            std::cout << ">>> OVERALL STATUS: [SOME TESTS FAILED - CHECK OUTPUTS]" << std::endl;
        }
        std::cout << "=============================================================\n" << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char* argv[]) {
    sc_clock clk("clk", 10, SC_NS);

    sc_signal<bool> rstn{"rstn"};
    sc_signal<bool> soft_reset{"soft_reset"};
    sc_signal<bool> start{"start"};
    sc_signal<bool> done{"done"};
    sc_signal<bool> deadlock{"deadlock"};

    sc_signal<uint32_t>     host_addr{"host_addr"};
    sc_signal<bool>         host_wren{"host_wren"};
    sc_signal<bool>         host_rden{"host_rden"};
    sc_signal<host_data_t>  host_wdata{"host_wdata"};
    sc_signal<host_mask_t>  host_wmask{"host_wmask"};
    sc_signal<host_data_t>  host_rdata{"host_rdata"};

    sc_signal<float>        threshold{"threshold"};
    sc_signal<sc_bv<3>>     select{"select"};

    NpuTop<32, 32> npu("NpuTop_inst");
    TestbenchStress32x32 tb("TestbenchStress32x32_inst");

    npu.i_clk(clk);
    npu.i_rstn(rstn);
    npu.i_soft_reset(soft_reset);
    npu.i_start(start);
    npu.o_done(done);
    npu.o_deadlock(deadlock);
    npu.i_host_addr(host_addr);
    npu.i_host_wren(host_wren);
    npu.i_host_rden(host_rden);
    npu.i_host_wdata(host_wdata);
    npu.i_host_wmask(host_wmask);
    npu.o_host_rdata(host_rdata);
    npu.i_threshold(threshold);
    npu.i_select(select);

    tb.i_clk(clk);
    tb.o_rstn(rstn);
    tb.o_soft_reset(soft_reset);
    tb.o_start(start);
    tb.i_done(done);
    tb.i_deadlock(deadlock);
    tb.o_host_addr(host_addr);
    tb.o_host_wren(host_wren);
    tb.o_host_rden(host_rden);
    tb.o_host_wdata(host_wdata);
    tb.o_host_wmask(host_wmask);
    tb.i_host_rdata(host_rdata);
    tb.o_threshold(threshold);
    tb.o_select(select);

    // Tracing waves
    sc_trace_file* tf = sc_create_vcd_trace_file("waves_32x32_stress");
    if (tf) {
        sc_trace(tf, clk, "clk");
        sc_trace(tf, rstn, "rstn");
        sc_trace(tf, start, "start");
        sc_trace(tf, done, "done");
        npu.trace(tf);
    }

    sc_start();

    if (tf) {
        sc_close_vcd_trace_file(tf);
    }

    return 0;
}
