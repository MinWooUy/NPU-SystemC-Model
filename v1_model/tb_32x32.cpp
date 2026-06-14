// Copyright 2026 Barcelona Supercomputing Center (BSC)
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
//
// SystemC Model for SAURIA NPU Core
// Advanced 32x32 Verification Testbench (tb_32x32.cpp)

#include "npu_top.h"
#include <iomanip>
#include <random>
#include <cmath>

using namespace sauria;

class Testbench32x32 : public sc_module {
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

    SC_CTOR(Testbench32x32) {
        SC_THREAD(test_process);
        sensitive << i_clk.pos();
    }

private:
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

    void test_process() {
        std::cout << "\n=============================================================" << std::endl;
        std::cout << "       SAURIA SystemC NPU 32x32 Advanced Verification Suit" << std::endl;
        std::cout << "=============================================================\n" << std::endl;

        // 1. Initialize Control signals
        o_rstn.write(false);
        o_soft_reset.write(false);
        o_start.write(false);
        o_host_addr.write(0);
        o_host_wren.write(false);
        o_host_rden.write(false);
        o_host_wdata.write(host_data_t());
        o_host_wmask.write(host_mask_t());
        
        float zero_gating_threshold = 0.05f;
        o_threshold.write(zero_gating_threshold);
        
        // Select physical buffer 0 for Host AXI writes
        o_select.write(sc_bv<3>("000"));

        // Release Reset
        wait(3);
        o_rstn.write(true);
        std::cout << "[TB] @ " << sc_time_stamp() << " System Reset Released." << std::endl;
        wait(2);

        // 2. Stimulus Parameters
        const int K = 64; // Tiling depth (number of loops)
        float mat_A[Y][K]; // Activations [32][64]
        float mat_B[K][X]; // Weights [64][32]
        float gold_C[Y][X]; // Golden reference product [32][32]

        // Initialize Golden output
        for (int y = 0; y < Y; y++) {
            for (int x = 0; x < X; x++) {
                gold_C[y][x] = 0.0f;
            }
        }

        // Random Number Generator
        std::mt19937 gen(42); // Seeded for reproducibility
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        std::uniform_real_distribution<float> sparsity_dist(0.0f, 1.0f);

        // Populate matrices with 40% zero sparsity to trigger zero-gating logic
        int zero_count_A = 0;
        int total_count_A = Y * K;

        for (int k = 0; k < K; k++) {
            for (int y = 0; y < Y; y++) {
                if (sparsity_dist(gen) < 0.40f) {
                    mat_A[y][k] = 0.0f;
                    zero_count_A++;
                } else {
                    mat_A[y][k] = dist(gen);
                    // Avoid exactly zero to keep metric clean
                    if (std::abs(mat_A[y][k]) < 0.001f) mat_A[y][k] = 0.1f;
                }
            }
            for (int x = 0; x < X; x++) {
                mat_B[k][x] = dist(gen);
            }
        }

        // Compute Golden Matrix Reference Product
        for (int y = 0; y < Y; y++) {
            for (int x = 0; x < X; x++) {
                for (int k = 0; k < K; k++) {
                    // Models hardware zero-gating skip (gates if either input is below threshold)
                    if (std::abs(mat_A[y][k]) > zero_gating_threshold && std::abs(mat_B[k][x]) > zero_gating_threshold) {
                        gold_C[y][x] += mat_A[y][k] * mat_B[k][x];
                    }
                }
            }
        }

        // 3. Write Matrices to Double-Buffered SRAMs via AXI
        std::cout << "[TB] @ " << sc_time_stamp() << " Programming SRAM A (Activations) via Host AXI..." << std::endl;
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

        std::cout << "[TB] @ " << sc_time_stamp() << " Programming SRAM B (Weights) via Host AXI..." << std::endl;
        for (int k = 0; k < K + X; k++) {
            for (int sub = 0; sub < SUBWORDS_B; sub++) {
                host_data_t wdata;
                for (int i = 0; i < 4; i++) {
                    int x = sub * 4 + i;
                    // Pre-skew the weight matrix in software to align with physical systolic streaming delay without wrapping
                    int matrix_k = k - x;
                    wdata[i] = (matrix_k >= 0 && matrix_k < K) ? mat_B[matrix_k][x] : 0.0f;
                }
                uint32_t addr = SRAMB_OFFSET | (k << SHIFT_B) | sub;
                write_host_mem(addr, wdata, full_mask);
            }
        }

        // 4. Swap Double-Buffered SRAMs to Accelerator Side
        std::cout << "[TB] @ " << sc_time_stamp() << " Swapping Double-Buffers (select = 0x7)..." << std::endl;
        wait();
        o_select.write(sc_bv<3>("111"));
        wait();

        // 5. Start Execution
        std::cout << "[TB] @ " << sc_time_stamp() << " Asserting NPU Start Interrupt..." << std::endl;
        sc_time start_sim_time = sc_time_stamp();
        wait();
        o_start.write(true);
        wait();
        o_start.write(false);

        // Monitor NPU Completion
        int timeout = 350;
        bool completed = false;
        while (timeout-- > 0) {
            wait();
            if (i_done.read()) {
                completed = true;
                break;
            }
        }

        sc_time end_sim_time = sc_time_stamp();

        if (completed) {
            std::cout << "[TB] @ " << sc_time_stamp() << " NPU Completed Execution Successfully!" << std::endl;
        } else {
            std::cerr << "[TB] @ " << sc_time_stamp() << " ERROR: Simulation Timeout!" << std::endl;
            sc_stop();
            return;
        }

        // 6. Swap Double-Buffered SRAMs back to Host Side for verification
        std::cout << "[TB] @ " << sc_time_stamp() << " Swapping Double-Buffers back (select = 0x0)..." << std::endl;
        wait();
        o_select.write(sc_bv<3>("000"));
        wait();

        // 7. Read back Results from SRAM C
        std::cout << "[TB] Reading back SRAM C results..." << std::endl;
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

        // 8. Output Analysis and Statistics
        std::cout << std::fixed << std::setprecision(5);
        std::cout << "\n=============================================================" << std::endl;
        std::cout << "               SAURIA Core Execution Metrics                  " << std::endl;
        std::cout << "=============================================================" << std::endl;
        
        double elapsed_ns = (end_sim_time - start_sim_time).to_double() / 1000.0; // ps to ns
        uint64_t total_ops = 2ULL * X * Y * K; // Multiplier-accumulator represents 2 ops
        double gflops = (double)total_ops / elapsed_ns;

        std::cout << " * Systolic Array Size:        " << X << " x " << Y << std::endl;
        std::cout << " * Computation Steps (K):      " << K << " cycles" << std::endl;
        std::cout << " * Elapsed Simulation Time:    " << elapsed_ns << " ns" << std::endl;
        std::cout << " * Total FLOP Executed:        " << total_ops << " FLOPs" << std::endl;
        std::cout << " * Achieved Throughput:        " << gflops << " GFLOPS" << std::endl;
        
        // Sparsity Zero-Gating Savings
        double sparsity_pct = ((double)zero_count_A / total_count_A) * 100.0;
        std::cout << " * Input Sparsity:             " << sparsity_pct << " %" << std::endl;
        std::cout << " * Zero-Gating Power Saving:   " << sparsity_pct << " % (Theoretical Multiplier Savings)" << std::endl;
        std::cout << "=============================================================\n" << std::endl;

        // Verify Results & Output Matrix Preview
        std::cout << "-------------------------------------------------------------" << std::endl;
        std::cout << "           SRAM C Matrix Results (First 5x5 Grid Preview)    " << std::endl;
        std::cout << "-------------------------------------------------------------" << std::endl;
        std::cout << " Row,Col |   NPU Value   |  Golden Value |   Difference   | Status" << std::endl;
        std::cout << "---------+---------------+---------------+----------------+--------" << std::endl;
        
        bool all_correct = true;
        int display_limit = 5;
        double max_err = 0.0;

        for (int y = 0; y < Y; y++) {
            for (int x = 0; x < X; x++) {
                double diff = std::abs(npu_C[y][x] - gold_C[y][x]);
                if (diff > max_err) max_err = diff;
                if (diff > 1e-3) all_correct = false;

                if (y < display_limit && x < display_limit) {
                    std::cout << " (" << std::setw(2) << y << "," << std::setw(2) << x << ") |  "
                              << std::setw(12) << npu_C[y][x] << " |  "
                              << std::setw(12) << gold_C[y][x] << " |  "
                              << std::setw(14) << diff << " | "
                              << (diff <= 1e-3 ? " PASS" : "*FAIL*") << std::endl;
                }
            }
        }
        std::cout << "-------------------------------------------------------------" << std::endl;
        std::cout << " * Maximum absolute error: " << max_err << std::endl;

        if (all_correct) {
            std::cout << "\n>>> [VERIFICATION SUCCESS] NPU array outputs match C++ Golden Reference perfectly!" << std::endl;
            std::cout << ">>> All 1024 accumulator elements verified successfully.\n" << std::endl;
        } else {
            std::cerr << "\n>>> [VERIFICATION ERROR] Accumulator read-back mismatch found!" << std::endl;
            for (int y = 0; y < Y; y++) {
                for (int x = 0; x < X; x++) {
                    double diff = std::abs(npu_C[y][x] - gold_C[y][x]);
                    if (diff > 1e-3) {
                        std::cerr << "  PE(" << y << "," << x << "): NPU=" << npu_C[y][x] 
                                  << ", Gold=" << gold_C[y][x] << ", Diff=" << diff << std::endl;
                    }
                }
            }
        }

        std::cout << "=============================================================" << std::endl;
        std::cout << "         SAURIA Advanced Simulation Ended Successfully      " << std::endl;
        std::cout << "=============================================================\n" << std::endl;
        
        sc_stop();
    }
};

int sc_main(int argc, char* argv[]) {
    // 1. Clock declaration (100MHz clock period = 10ns)
    sc_clock clk("clk", 10, SC_NS);

    // 2. Local interconnect signals
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

    // 3. Module instantiation
    NpuTop<32, 32> npu("NpuTop_inst");
    Testbench32x32 tb("Testbench32x32_inst");

    // 4. Signal Bindings
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

    // 5. Enable Waveform Tracing
    sc_trace_file* tf = sc_create_vcd_trace_file("waves_32x32");
    if (tf) {
        std::cout << "[TB] Waveform tracing enabled. Creating waves_32x32.vcd..." << std::endl;
        sc_trace(tf, clk, "clk");
        sc_trace(tf, rstn, "rstn");
        sc_trace(tf, start, "start");
        sc_trace(tf, done, "done");
        sc_trace(tf, host_addr, "host_addr");
        sc_trace(tf, host_wren, "host_wren");
        sc_trace(tf, select, "select");
        
        // Trace NPU Core internal signals
        npu.trace(tf);
    }

    // 6. Start simulation
    sc_start();

    // 7. Cleanup
    if (tf) {
        sc_close_vcd_trace_file(tf);
        std::cout << "[TB] Waveform tracing closed." << std::endl;
    }

    return 0;
}
