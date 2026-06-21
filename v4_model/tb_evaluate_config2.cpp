// Copyright 2026 Barcelona Supercomputing Center (BSC)
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
//
// SystemC Model for SAURIA NPU Core
// Alternative Evaluation Testbench (Config 2) comparing PE parameter profiles:
// 1. Deeply Pipelined FP32 Profile (stages_mul = 3, intermediate = true)
// 2. Integer-Converted Zero-Gated Profile (arithmetic_type = 0, zero_gating = true)
// 3. Strongly Approximate Profile (m_approx = 0.50, a_approx = 0.80)
//

#include "npu_top.h"
#include <iomanip>
#include <cmath>

using namespace sauria;

// Array dimensions for evaluation
const int EVAL_X = 8;
const int EVAL_Y = 8;

class TestbenchEvaluateConfig2 : public sc_module {
public:
    // Clock & Reset Ports
    sc_in<bool> i_clk{"i_clk"};
    sc_out<bool> o_rstn{"o_rstn"};
    sc_out<bool> o_soft_reset{"o_soft_reset"};

    // NPU Host Control Interfaces - Profile 0 (Deep Pipe)
    sc_out<bool> o_start_pipe{"o_start_pipe"};
    sc_in<bool>  i_done_pipe{"i_done_pipe"};
    sc_in<bool>  i_deadlock_pipe{"i_deadlock_pipe"};

    // NPU Host Control Interfaces - Profile 1 (Integer Zero-Gated)
    sc_out<bool> o_start_int{"o_start_int"};
    sc_in<bool>  i_done_int{"i_done_int"};
    sc_in<bool>  i_deadlock_int{"i_deadlock_int"};

    // NPU Host Control Interfaces - Profile 2 (Strong Approx)
    sc_out<bool> o_start_strong{"o_start_strong"};
    sc_in<bool>  i_done_strong{"i_done_strong"};
    sc_in<bool>  i_deadlock_strong{"i_deadlock_strong"};

    // NPU Host Memory Ports - STD/PIPE
    sc_out<uint32_t>     o_host_addr_pipe{"o_host_addr_pipe"};
    sc_out<bool>         o_host_wren_pipe{"o_host_wren_pipe"};
    sc_out<bool>         o_host_rden_pipe{"o_host_rden_pipe"};
    sc_out<host_data_t>  o_host_wdata_pipe{"o_host_wdata_pipe"};
    sc_out<host_mask_t>  o_host_wmask_pipe{"o_host_wmask_pipe"};
    sc_in<host_data_t>   i_host_rdata_pipe{"i_host_rdata_pipe"};

    // NPU Host Memory Ports - INT
    sc_out<uint32_t>     o_host_addr_int{"o_host_addr_int"};
    sc_out<bool>         o_host_wren_int{"o_host_wren_int"};
    sc_out<bool>         o_host_rden_int{"o_host_rden_int"};
    sc_out<host_data_t>  o_host_wdata_int{"o_host_wdata_int"};
    sc_out<host_mask_t>  o_host_wmask_int{"o_host_wmask_int"};
    sc_in<host_data_t>   i_host_rdata_int{"i_host_rdata_int"};

    // NPU Host Memory Ports - STRONG APPROX
    sc_out<uint32_t>     o_host_addr_strong{"o_host_addr_strong"};
    sc_out<bool>         o_host_wren_strong{"o_host_wren_strong"};
    sc_out<bool>         o_host_rden_strong{"o_host_rden_strong"};
    sc_out<host_data_t>  o_host_wdata_strong{"o_host_wdata_strong"};
    sc_out<host_mask_t>  o_host_wmask_strong{"o_host_wmask_strong"};
    sc_in<host_data_t>   i_host_rdata_strong{"i_host_rdata_strong"};

    // Configurations
    sc_out<float>        o_threshold{"o_threshold"};
    sc_out<sc_bv<3>>     o_select{"o_select"};

    SC_CTOR(TestbenchEvaluateConfig2) {
        SC_THREAD(test_process);
        sensitive << i_clk.pos();
    }

private:
    // Dynamic address calculator helpers
    const int subwords_a = EVAL_Y / 4;
    const int mask_a = subwords_a - 1;
    const int shift_a = (subwords_a == 8) ? 3 : ((subwords_a == 4) ? 2 : ((subwords_a == 2) ? 1 : 0));

    const int subwords_b = EVAL_X / 4;
    const int mask_b = subwords_b - 1;
    const int shift_b = (subwords_b == 8) ? 3 : ((subwords_b == 4) ? 2 : ((subwords_b == 2) ? 1 : 0));

    const int subwords_c = EVAL_Y / 4;
    const int mask_c = subwords_c - 1;
    const int shift_c = (subwords_c == 8) ? 3 : ((subwords_c == 4) ? 2 : ((subwords_c == 2) ? 1 : 0));

    uint32_t get_srama_addr(uint32_t phys_addr, uint32_t sub_word) {
        return SRAMA_OFFSET | ((phys_addr << shift_a) | (sub_word & mask_a));
    }

    uint32_t get_sramb_addr(uint32_t phys_addr, uint32_t sub_word) {
        return SRAMB_OFFSET | ((phys_addr << shift_b) | (sub_word & mask_b));
    }

    uint32_t get_sramc_addr(uint32_t phys_addr, uint32_t sub_word) {
        return SRAMC_OFFSET | ((phys_addr << shift_c) | (sub_word & mask_c));
    }

    // Write helper
    void write_mem(int profile, uint32_t addr, const host_data_t& data) {
        host_mask_t mask;
        mask.data.fill(true);
        wait();
        if (profile == 0) {
            o_host_addr_pipe.write(addr);
            o_host_wdata_pipe.write(data);
            o_host_wmask_pipe.write(mask);
            o_host_wren_pipe.write(true);
            o_host_rden_pipe.write(false);
        } else if (profile == 1) {
            o_host_addr_int.write(addr);
            o_host_wdata_int.write(data);
            o_host_wmask_int.write(mask);
            o_host_wren_int.write(true);
            o_host_rden_int.write(false);
        } else {
            o_host_addr_strong.write(addr);
            o_host_wdata_strong.write(data);
            o_host_wmask_strong.write(mask);
            o_host_wren_strong.write(true);
            o_host_rden_strong.write(false);
        }
        wait();
        o_host_wren_pipe.write(false);
        o_host_wren_int.write(false);
        o_host_wren_strong.write(false);
        wait(); // Ensure data resolves
    }

    // Read helper
    host_data_t read_mem(int profile, uint32_t addr) {
        wait();
        if (profile == 0) {
            o_host_addr_pipe.write(addr);
            o_host_wren_pipe.write(false);
            o_host_rden_pipe.write(true);
        } else if (profile == 1) {
            o_host_addr_int.write(addr);
            o_host_wren_int.write(false);
            o_host_rden_int.write(true);
        } else {
            o_host_addr_strong.write(addr);
            o_host_wren_strong.write(false);
            o_host_rden_strong.write(true);
        }
        wait(); // Address registers in SRAM
        wait(); // Data registers out (1-cycle read latency)
        o_host_rden_pipe.write(false);
        o_host_rden_int.write(false);
        o_host_rden_strong.write(false);
        wait();
        
        if (profile == 0) return i_host_rdata_pipe.read();
        else if (profile == 1) return i_host_rdata_int.read();
        else return i_host_rdata_strong.read();
    }

    void test_process() {
        std::cout << "\n=============================================================" << std::endl;
        std::cout << "      SAURIA SystemC NPU Core Multi-Profile Config 2         " << std::endl;
        std::cout << "=============================================================\n" << std::endl;

        // Reset system
        o_rstn.write(false);
        o_soft_reset.write(false);
        o_start_pipe.write(false);
        o_start_int.write(false);
        o_start_strong.write(false);
        o_threshold.write(0.5f);
        o_select.write(sc_bv<3>("000")); // Host AXI access mode
        wait(5);
        o_rstn.write(true);
        wait();

        // 1. Program register configurations
        // All profiles configure: incntlim = 4, act_reps = 1, wei_reps = 1
        // (Config registers are automatically updated inside NpuTop via config_regs)
        
        // 2. Set up activations: row 0 (phys_addr 0)
        host_data_t act_sub0, act_sub1;
        act_sub0[0] = 1.5f; act_sub0[1] = 0.2f;  act_sub0[2] = 3.5f; act_sub0[3] = 0.4f;
        act_sub1[0] = 5.5f; act_sub1[1] = 0.01f; act_sub1[2] = 7.5f; act_sub1[3] = 0.0f;

        // Set up weights: col 0 (phys_addr 0)
        host_data_t wei_sub0, wei_sub1;
        wei_sub0[0] = 2.0f; wei_sub0[1] = 0.1f; wei_sub0[2] = 1.0f; wei_sub0[3] = 4.0f;
        wei_sub1[0] = 0.5f; wei_sub1[1] = 2.0f; wei_sub1[2] = 0.0f; wei_sub1[3] = 1.5f;

        for (int p = 0; p < 3; p++) {
            // Write SRAM A (Activations)
            write_mem(p, get_srama_addr(0, 0), act_sub0);
            write_mem(p, get_srama_addr(0, 1), act_sub1);

            // Write SRAM B (Weights)
            write_mem(p, get_sramb_addr(0, 0), wei_sub0);
            write_mem(p, get_sramb_addr(0, 1), wei_sub1);

            // Write Configuration registers: Program incntlim = 4 to speed up execution
            host_data_t con_data;
            con_data[0] = 4.0f;
            write_mem(p, CFG_REGS_OFFSET | (CFG_CON_OFFSET + 0x00), con_data);
        }

        std::cout << "[TB] Input activations, weights, and config registers programmed to SRAM double buffers." << std::endl;

        // Swap double buffers to Accelerator (NPU) side
        wait();
        o_select.write(sc_bv<3>("111"));
        wait(2);

        // Trigger execution start pulse
        std::cout << "[TB] Triggering execution start pulse for all profiles..." << std::endl;
        o_start_pipe.write(true);
        o_start_int.write(true);
        o_start_strong.write(true);
        wait();
        o_start_pipe.write(false);
        o_start_int.write(false);
        o_start_strong.write(false);

        // Wait for all three profiles to complete
        int cycles = 0;
        bool pipe_done = false;
        bool int_done = false;
        bool strong_done = false;

        while (cycles < 200) {
            wait();
            cycles++;
            
            if (i_done_pipe.read()) pipe_done = true;
            if (i_done_int.read()) int_done = true;
            if (i_done_strong.read()) strong_done = true;

            if (pipe_done && int_done && strong_done) break;

            if (cycles % 20 == 0) {
                std::cout << "[TB] Cycle " << cycles 
                          << " status -> Pipe: " << (pipe_done ? "DONE" : "RUNNING")
                          << ", Int: " << (int_done ? "DONE" : "RUNNING")
                          << ", Strong: " << (strong_done ? "DONE" : "RUNNING")
                          << " | Deadlock Pipe: " << i_deadlock_pipe.read()
                          << ", Int: " << i_deadlock_int.read()
                          << ", Strong: " << i_deadlock_strong.read() << std::endl;
            }
        }

        std::cout << "[TB] Simulation execution phase finished in " << cycles << " cycles."
                  << " Pipe: " << (pipe_done ? "DONE" : "FAILED")
                  << ", Int: " << (int_done ? "DONE" : "FAILED")
                  << ", Strong: " << (strong_done ? "DONE" : "FAILED") << std::endl;

        // Swap double buffers back to Host AXI side to read results
        wait();
        o_select.write(sc_bv<3>("000"));
        wait(2);

        // Read results from SRAM C
        std::vector<float> res_pipe(EVAL_Y, 0.0f);
        std::vector<float> res_int(EVAL_Y, 0.0f);
        std::vector<float> res_strong(EVAL_Y, 0.0f);

        for (int sw = 0; sw < subwords_c; sw++) {
            host_data_t chunk_pipe = read_mem(0, get_sramc_addr(0, sw));
            host_data_t chunk_int = read_mem(1, get_sramc_addr(0, sw));
            host_data_t chunk_strong = read_mem(2, get_sramc_addr(0, sw));
            for (int i = 0; i < 4; i++) {
                if (sw * 4 + i < EVAL_Y) {
                    res_pipe[sw * 4 + i] = chunk_pipe[i];
                    res_int[sw * 4 + i] = chunk_int[i];
                    res_strong[sw * 4 + i] = chunk_strong[i];
                }
            }
        }

        // Display results comparative table
        std::cout << "\n=========================================================================" << std::endl;
        std::cout << "             SAURIA MULTI-PROFILE CONFIG 2 EVALUATION SUMMARY            " << std::endl;
        std::cout << "=========================================================================" << std::endl;
        std::cout << " Index | Deep-Pipe (FP32) | Integer (Gated) | Strong Approx | Approx Error " << std::endl;
        std::cout << "-------+------------------+-----------------+---------------+--------------" << std::endl;

        std::cout << std::fixed << std::setprecision(4);
        for (int i = 0; i < EVAL_Y; i++) {
            float val_pipe = res_pipe[i];
            float val_int = res_int[i];
            float val_strong = res_strong[i];
            
            float approx_err = (val_pipe != 0.0f) ? std::abs((val_pipe - val_strong) / val_pipe) * 100.0f : 0.0f;
            
            std::cout << "   " << i 
                      << "   |      " << std::setw(8) << val_pipe 
                      << "    |     " << std::setw(8) << val_int
                      << "    |    " << std::setw(8) << val_strong
                      << "   |    " << std::setw(6) << approx_err << "%" << std::endl;
        }
        std::cout << "=========================================================================\n" << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char* argv[]) {
    sc_clock clk("clk", 10, SC_NS);

    // Profile configurations
    PeConfig cfg_pipe;
    cfg_pipe.arithmetic_type = 1;
    cfg_pipe.mul_type = 0;
    cfg_pipe.add_type = 0;
    cfg_pipe.stages_mul = 3;                       // Deep multiplier pipeline stage (3)
    cfg_pipe.intermediate_pipeline_stage = true;   // 1 extra intermediate pipeline register
    cfg_pipe.zero_gating_mult = false;

    PeConfig cfg_int;
    cfg_int.arithmetic_type = 0;                   // Integer / Fixed-Point Cast
    cfg_int.mul_type = 0;
    cfg_int.add_type = 0;
    cfg_int.stages_mul = 1;
    cfg_int.intermediate_pipeline_stage = false;   // Shorter pipeline
    cfg_int.zero_gating_mult = true;               // Zero gating active

    PeConfig cfg_strong;
    cfg_strong.arithmetic_type = 1;
    cfg_strong.mul_type = 1;
    cfg_strong.add_type = 1;
    cfg_strong.m_approx = 0.50f;                   // Strong multiplication scaling (50% error)
    cfg_strong.a_approx = 0.80f;                   // Strong addition scaling (20% error)
    cfg_strong.stages_mul = 1;
    cfg_strong.intermediate_pipeline_stage = true;
    cfg_strong.zero_gating_mult = false;

    // NpuTop instances for each profile (running 8x8 arrays)
    // Note: for cfg_pipe, the array latency is increased, but the controller handles it dynamically.
    NpuTop<EVAL_X, EVAL_Y, float, float, float, 1024, 1024, 2048, 16, EVAL_X+EVAL_Y + 4, 1> npu_pipe("NpuTop_pipe", cfg_pipe);
    NpuTop<EVAL_X, EVAL_Y, float, float, float, 1024, 1024, 2048, 16, EVAL_X+EVAL_Y, 1> npu_int("NpuTop_int", cfg_int);
    NpuTop<EVAL_X, EVAL_Y, float, float, float, 1024, 1024, 2048, 16, EVAL_X+EVAL_Y, 1> npu_strong("NpuTop_strong", cfg_strong);

    TestbenchEvaluateConfig2 tb("TestbenchEvaluateConfig2_inst");

    // Local signals
    sc_signal<bool> rstn{"rstn"};
    sc_signal<bool> soft_reset{"soft_reset"};
    sc_signal<float> threshold{"threshold"};
    sc_signal<sc_bv<3>> select{"select"};

    sc_signal<bool> start_pipe{"start_pipe"};
    sc_signal<bool> done_pipe{"done_pipe"};
    sc_signal<bool> deadlock_pipe{"deadlock_pipe"};

    sc_signal<bool> start_int{"start_int"};
    sc_signal<bool> done_int{"done_int"};
    sc_signal<bool> deadlock_int{"deadlock_int"};

    sc_signal<bool> start_strong{"start_strong"};
    sc_signal<bool> done_strong{"done_strong"};
    sc_signal<bool> deadlock_strong{"deadlock_strong"};

    // SRAM signals - Pipe
    sc_signal<uint32_t>     host_addr_pipe{"host_addr_pipe"};
    sc_signal<bool>         host_wren_pipe{"host_wren_pipe"};
    sc_signal<bool>         host_rden_pipe{"host_rden_pipe"};
    sc_signal<host_data_t>  host_wdata_pipe{"host_wdata_pipe"};
    sc_signal<host_mask_t>  host_wmask_pipe{"host_wmask_pipe"};
    sc_signal<host_data_t>  host_rdata_pipe{"host_rdata_pipe"};

    // SRAM signals - Int
    sc_signal<uint32_t>     host_addr_int{"host_addr_int"};
    sc_signal<bool>         host_wren_int{"host_wren_int"};
    sc_signal<bool>         host_rden_int{"host_rden_int"};
    sc_signal<host_data_t>  host_wdata_int{"host_wdata_int"};
    sc_signal<host_mask_t>  host_wmask_int{"host_wmask_int"};
    sc_signal<host_data_t>  host_rdata_int{"host_rdata_int"};

    // SRAM signals - Strong
    sc_signal<uint32_t>     host_addr_strong{"host_addr_strong"};
    sc_signal<bool>         host_wren_strong{"host_wren_strong"};
    sc_signal<bool>         host_rden_strong{"host_rden_strong"};
    sc_signal<host_data_t>  host_wdata_strong{"host_wdata_strong"};
    sc_signal<host_mask_t>  host_wmask_strong{"host_wmask_strong"};
    sc_signal<host_data_t>  host_rdata_strong{"host_rdata_strong"};

    // Bind Testbench
    tb.i_clk(clk);
    tb.o_rstn(rstn);
    tb.o_soft_reset(soft_reset);
    tb.o_threshold(threshold);
    tb.o_select(select);

    tb.o_start_pipe(start_pipe);
    tb.i_done_pipe(done_pipe);
    tb.i_deadlock_pipe(deadlock_pipe);

    tb.o_start_int(start_int);
    tb.i_done_int(done_int);
    tb.i_deadlock_int(deadlock_int);

    tb.o_start_strong(start_strong);
    tb.i_done_strong(done_strong);
    tb.i_deadlock_strong(deadlock_strong);

    tb.o_host_addr_pipe(host_addr_pipe);
    tb.o_host_wren_pipe(host_wren_pipe);
    tb.o_host_rden_pipe(host_rden_pipe);
    tb.o_host_wdata_pipe(host_wdata_pipe);
    tb.o_host_wmask_pipe(host_wmask_pipe);
    tb.i_host_rdata_pipe(host_rdata_pipe);

    tb.o_host_addr_int(host_addr_int);
    tb.o_host_wren_int(host_wren_int);
    tb.o_host_rden_int(host_rden_int);
    tb.o_host_wdata_int(host_wdata_int);
    tb.o_host_wmask_int(host_wmask_int);
    tb.i_host_rdata_int(host_rdata_int);

    tb.o_host_addr_strong(host_addr_strong);
    tb.o_host_wren_strong(host_wren_strong);
    tb.o_host_rden_strong(host_rden_strong);
    tb.o_host_wdata_strong(host_wdata_strong);
    tb.o_host_wmask_strong(host_wmask_strong);
    tb.i_host_rdata_strong(host_rdata_strong);

    // Bind NpuTop - Pipe
    npu_pipe.i_clk(clk);
    npu_pipe.i_rstn(rstn);
    npu_pipe.i_soft_reset(soft_reset);
    npu_pipe.i_start(start_pipe);
    npu_pipe.o_done(done_pipe);
    npu_pipe.o_deadlock(deadlock_pipe);
    npu_pipe.i_select(select);
    npu_pipe.i_threshold(threshold);
    npu_pipe.i_host_addr(host_addr_pipe);
    npu_pipe.i_host_wren(host_wren_pipe);
    npu_pipe.i_host_rden(host_rden_pipe);
    npu_pipe.i_host_wdata(host_wdata_pipe);
    npu_pipe.i_host_wmask(host_wmask_pipe);
    npu_pipe.o_host_rdata(host_rdata_pipe);

    // Bind NpuTop - Int
    npu_int.i_clk(clk);
    npu_int.i_rstn(rstn);
    npu_int.i_soft_reset(soft_reset);
    npu_int.i_start(start_int);
    npu_int.o_done(done_int);
    npu_int.o_deadlock(deadlock_int);
    npu_int.i_select(select);
    npu_int.i_threshold(threshold);
    npu_int.i_host_addr(host_addr_int);
    npu_int.i_host_wren(host_wren_int);
    npu_int.i_host_rden(host_rden_int);
    npu_int.i_host_wdata(host_wdata_int);
    npu_int.i_host_wmask(host_wmask_int);
    npu_int.o_host_rdata(host_rdata_int);

    // Bind NpuTop - Strong
    npu_strong.i_clk(clk);
    npu_strong.i_rstn(rstn);
    npu_strong.i_soft_reset(soft_reset);
    npu_strong.i_start(start_strong);
    npu_strong.o_done(done_strong);
    npu_strong.o_deadlock(deadlock_strong);
    npu_strong.i_select(select);
    npu_strong.i_threshold(threshold);
    npu_strong.i_host_addr(host_addr_strong);
    npu_strong.i_host_wren(host_wren_strong);
    npu_strong.i_host_rden(host_rden_strong);
    npu_strong.i_host_wdata(host_wdata_strong);
    npu_strong.i_host_wmask(host_wmask_strong);
    npu_strong.o_host_rdata(host_rdata_strong);

    // Start Simulation
    sc_start();

    return 0;
}
