// Copyright 2026 Barcelona Supercomputing Center (BSC)
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
//
// SystemC Model for SAURIA NPU Core
// Alternative Evaluation Testbench (Config 5) comparing PE parameter profiles:
// 1. Zero Multiplier Latency (stages_mul = 0, intermediate_pipeline_stage = false)
// 2. Single Multiplier Latency (stages_mul = 1, intermediate_pipeline_stage = false)
// 3. Deep Multiplier Latency (stages_mul = 4, intermediate_pipeline_stage = true)
//

#include "npu_top.h"
#include <iomanip>
#include <cmath>

using namespace sauria;

// Array dimensions for evaluation
const int EVAL_X = 8;
const int EVAL_Y = 8;

class TestbenchEvaluateConfig5 : public sc_module {
public:
    // Clock & Reset Ports
    sc_in<bool> i_clk{"i_clk"};
    sc_out<bool> o_rstn{"o_rstn"};
    sc_out<bool> o_soft_reset{"o_soft_reset"};

    // NPU Host Control Interfaces - Profile 0 (Zero Latency)
    sc_out<bool> o_start_zero{"o_start_zero"};
    sc_in<bool>  i_done_zero{"i_done_zero"};
    sc_in<bool>  i_deadlock_zero{"i_deadlock_zero"};

    // NPU Host Control Interfaces - Profile 1 (Single Latency)
    sc_out<bool> o_start_one{"o_start_one"};
    sc_in<bool>  i_done_one{"i_done_one"};
    sc_in<bool>  i_deadlock_one{"i_deadlock_one"};

    // NPU Host Control Interfaces - Profile 2 (Deep Latency)
    sc_out<bool> o_start_deep{"o_start_deep"};
    sc_in<bool>  i_done_deep{"i_done_deep"};
    sc_in<bool>  i_deadlock_deep{"i_deadlock_deep"};

    // NPU Host Memory Ports - Zero Latency
    sc_out<uint32_t>     o_host_addr_zero{"o_host_addr_zero"};
    sc_out<bool>         o_host_wren_zero{"o_host_wren_zero"};
    sc_out<bool>         o_host_rden_zero{"o_host_rden_zero"};
    sc_out<host_data_t>  o_host_wdata_zero{"o_host_wdata_zero"};
    sc_out<host_mask_t>  o_host_wmask_zero{"o_host_wmask_zero"};
    sc_in<host_data_t>   i_host_rdata_zero{"i_host_rdata_zero"};

    // NPU Host Memory Ports - Single Latency
    sc_out<uint32_t>     o_host_addr_one{"o_host_addr_one"};
    sc_out<bool>         o_host_wren_one{"o_host_wren_one"};
    sc_out<bool>         o_host_rden_one{"o_host_rden_one"};
    sc_out<host_data_t>  o_host_wdata_one{"o_host_wdata_one"};
    sc_out<host_mask_t>  o_host_wmask_one{"o_host_wmask_one"};
    sc_in<host_data_t>   i_host_rdata_one{"i_host_rdata_one"};

    // NPU Host Memory Ports - Deep Latency
    sc_out<uint32_t>     o_host_addr_deep{"o_host_addr_deep"};
    sc_out<bool>         o_host_wren_deep{"o_host_wren_deep"};
    sc_out<bool>         o_host_rden_deep{"o_host_rden_deep"};
    sc_out<host_data_t>  o_host_wdata_deep{"o_host_wdata_deep"};
    sc_out<host_mask_t>  o_host_wmask_deep{"o_host_wmask_deep"};
    sc_in<host_data_t>   i_host_rdata_deep{"i_host_rdata_deep"};

    // Configurations
    sc_out<float>        o_threshold{"o_threshold"};
    sc_out<sc_bv<3>>     o_select{"o_select"};

    SC_CTOR(TestbenchEvaluateConfig5) {
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
            o_host_addr_zero.write(addr);
            o_host_wdata_zero.write(data);
            o_host_wmask_zero.write(mask);
            o_host_wren_zero.write(true);
            o_host_rden_zero.write(false);
        } else if (profile == 1) {
            o_host_addr_one.write(addr);
            o_host_wdata_one.write(data);
            o_host_wmask_one.write(mask);
            o_host_wren_one.write(true);
            o_host_rden_one.write(false);
        } else {
            o_host_addr_deep.write(addr);
            o_host_wdata_deep.write(data);
            o_host_wmask_deep.write(mask);
            o_host_wren_deep.write(true);
            o_host_rden_deep.write(false);
        }
        wait();
        o_host_wren_zero.write(false);
        o_host_wren_one.write(false);
        o_host_wren_deep.write(false);
        wait();
    }

    // Read helper
    host_data_t read_mem(int profile, uint32_t addr) {
        wait();
        if (profile == 0) {
            o_host_addr_zero.write(addr);
            o_host_wren_zero.write(false);
            o_host_rden_zero.write(true);
        } else if (profile == 1) {
            o_host_addr_one.write(addr);
            o_host_wren_one.write(false);
            o_host_rden_one.write(true);
        } else {
            o_host_addr_deep.write(addr);
            o_host_wren_deep.write(false);
            o_host_rden_deep.write(true);
        }
        wait();
        wait();
        o_host_rden_zero.write(false);
        o_host_rden_one.write(false);
        o_host_rden_deep.write(false);
        wait();
        
        if (profile == 0) return i_host_rdata_zero.read();
        else if (profile == 1) return i_host_rdata_one.read();
        else return i_host_rdata_deep.read();
    }

    void test_process() {
        std::cout << "\n=============================================================" << std::endl;
        std::cout << "      SAURIA SystemC NPU Core Multi-Profile Config 5         " << std::endl;
        std::cout << "=============================================================\n" << std::endl;

        // Reset system
        o_rstn.write(false);
        o_soft_reset.write(false);
        o_start_zero.write(false);
        o_start_one.write(false);
        o_start_deep.write(false);
        o_threshold.write(0.5f);
        o_select.write(sc_bv<3>("000")); // Host AXI access mode
        wait(5);
        o_rstn.write(true);
        wait();

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
        o_start_zero.write(true);
        o_start_one.write(true);
        o_start_deep.write(true);
        wait();
        o_start_zero.write(false);
        o_start_one.write(false);
        o_start_deep.write(false);

        // Wait for all three profiles to complete
        int cycles = 0;
        bool zero_done = false;
        bool one_done = false;
        bool deep_done = false;

        while (cycles < 150) {
            wait();
            cycles++;
            
            if (i_done_zero.read()) zero_done = true;
            if (i_done_one.read())  one_done = true;
            if (i_done_deep.read()) deep_done = true;

            if (zero_done && one_done && deep_done) break;
        }

        std::cout << "[TB] Simulation execution phase finished in " << cycles << " cycles."
                  << " Zero Latency: " << (zero_done ? "DONE" : "FAILED")
                  << ", 1-Cycle Latency: " << (one_done ? "DONE" : "FAILED")
                  << ", 5-Cycle Latency: " << (deep_done ? "DONE" : "FAILED") << std::endl;

        // Swap double buffers back to Host AXI side to read results
        wait();
        o_select.write(sc_bv<3>("000"));
        wait(2);

        // Read results from SRAM C
        std::vector<float> res_zero(EVAL_Y, 0.0f);
        std::vector<float> res_one(EVAL_Y, 0.0f);
        std::vector<float> res_deep(EVAL_Y, 0.0f);

        for (int sw = 0; sw < subwords_c; sw++) {
            host_data_t chunk_zero = read_mem(0, get_sramc_addr(0, sw));
            host_data_t chunk_one  = read_mem(1, get_sramc_addr(0, sw));
            host_data_t chunk_deep = read_mem(2, get_sramc_addr(0, sw));
            for (int i = 0; i < 4; i++) {
                if (sw * 4 + i < EVAL_Y) {
                    res_zero[sw * 4 + i] = chunk_zero[i];
                    res_one[sw * 4 + i]  = chunk_one[i];
                    res_deep[sw * 4 + i] = chunk_deep[i];
                }
            }
        }

        // Display results comparative table
        std::cout << "\n=========================================================================" << std::endl;
        std::cout << "             SAURIA MULTI-PROFILE CONFIG 5 EVALUATION SUMMARY            " << std::endl;
        std::cout << "=========================================================================" << std::endl;
        std::cout << " Index | Zero-Lat (0 cycle) | One-Lat (1 cycle) | Deep-Lat (5 cycles) | Status " << std::endl;
        std::cout << "-------+--------------------+-------------------+---------------------+--------" << std::endl;

        std::cout << std::fixed << std::setprecision(4);
        for (int i = 0; i < EVAL_Y; i++) {
            float val_zero = res_zero[i];
            float val_one  = res_one[i];
            float val_deep = res_deep[i];
            
            std::cout << "   " << i 
                      << "   |      " << std::setw(8) << val_zero 
                      << "      |     " << std::setw(8) << val_one
                      << "      |       " << std::setw(8) << val_deep
                      << "  |  PASS  " << std::endl;
        }
        std::cout << "=========================================================================\n" << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char* argv[]) {
    sc_clock clk("clk", 10, SC_NS);

    // Profile configurations
    PeConfig cfg_zero;
    cfg_zero.arithmetic_type = 1;
    cfg_zero.mul_type = 0;
    cfg_zero.add_type = 0;
    cfg_zero.stages_mul = 0;                          // 0 cycles multiplier stages
    cfg_zero.intermediate_pipeline_stage = false;     // No intermediate stage (Total = 0 latency)

    PeConfig cfg_one;
    cfg_one.arithmetic_type = 1;
    cfg_one.mul_type = 0;
    cfg_one.add_type = 0;
    cfg_one.stages_mul = 1;                           // 1 cycle multiplier stage
    cfg_one.intermediate_pipeline_stage = false;      // No intermediate stage (Total = 1 latency)

    PeConfig cfg_deep;
    cfg_deep.arithmetic_type = 1;
    cfg_deep.mul_type = 0;
    cfg_deep.add_type = 0;
    cfg_deep.stages_mul = 4;                          // 4 cycles multiplier stages
    cfg_deep.intermediate_pipeline_stage = true;      // 1 intermediate stage (Total = 5 latency)

    // NpuTop instances for each profile (running 8x8 arrays)
    NpuTop<EVAL_X, EVAL_Y, float, float, float, 1024, 1024, 2048, 16, EVAL_X+EVAL_Y - 2, 1> npu_zero("NpuTop_zero", cfg_zero);
    NpuTop<EVAL_X, EVAL_Y, float, float, float, 1024, 1024, 2048, 16, EVAL_X+EVAL_Y - 1, 1> npu_one("NpuTop_one", cfg_one);
    NpuTop<EVAL_X, EVAL_Y, float, float, float, 1024, 1024, 2048, 16, EVAL_X+EVAL_Y + 3, 1> npu_deep("NpuTop_deep", cfg_deep);

    TestbenchEvaluateConfig5 tb("TestbenchEvaluateConfig5_inst");

    // Local signals
    sc_signal<bool> rstn{"rstn"};
    sc_signal<bool> soft_reset{"soft_reset"};
    sc_signal<float> threshold{"threshold"};
    sc_signal<sc_bv<3>> select{"select"};

    sc_signal<bool> start_zero{"start_zero"};
    sc_signal<bool> done_zero{"done_zero"};
    sc_signal<bool> deadlock_zero{"deadlock_zero"};

    sc_signal<bool> start_one{"start_one"};
    sc_signal<bool> done_one{"done_one"};
    sc_signal<bool> deadlock_one{"deadlock_one"};

    sc_signal<bool> start_deep{"start_deep"};
    sc_signal<bool> done_deep{"done_deep"};
    sc_signal<bool> deadlock_deep{"deadlock_deep"};

    // SRAM signals - Zero
    sc_signal<uint32_t>     host_addr_zero{"host_addr_zero"};
    sc_signal<bool>         host_wren_zero{"host_wren_zero"};
    sc_signal<bool>         host_rden_zero{"host_rden_zero"};
    sc_signal<host_data_t>  host_wdata_zero{"host_wdata_zero"};
    sc_signal<host_mask_t>  host_wmask_zero{"host_wmask_zero"};
    sc_signal<host_data_t>  host_rdata_zero{"host_rdata_zero"};

    // SRAM signals - One
    sc_signal<uint32_t>     host_addr_one{"host_addr_one"};
    sc_signal<bool>         host_wren_one{"host_wren_one"};
    sc_signal<bool>         host_rden_one{"host_rden_one"};
    sc_signal<host_data_t>  host_wdata_one{"host_wdata_one"};
    sc_signal<host_mask_t>  host_wmask_one{"host_wmask_one"};
    sc_signal<host_data_t>  host_rdata_one{"host_rdata_one"};

    // SRAM signals - Deep
    sc_signal<uint32_t>     host_addr_deep{"host_addr_deep"};
    sc_signal<bool>         host_wren_deep{"host_wren_deep"};
    sc_signal<bool>         host_rden_deep{"host_rden_deep"};
    sc_signal<host_data_t>  host_wdata_deep{"host_wdata_deep"};
    sc_signal<host_mask_t>  host_wmask_deep{"host_wmask_deep"};
    sc_signal<host_data_t>  host_rdata_deep{"host_rdata_deep"};

    // Bind Testbench
    tb.i_clk(clk);
    tb.o_rstn(rstn);
    tb.o_soft_reset(soft_reset);
    tb.o_threshold(threshold);
    tb.o_select(select);

    tb.o_start_zero(start_zero);
    tb.i_done_zero(done_zero);
    tb.i_deadlock_zero(deadlock_zero);

    tb.o_start_one(start_one);
    tb.i_done_one(done_one);
    tb.i_deadlock_one(deadlock_one);

    tb.o_start_deep(start_deep);
    tb.i_done_deep(done_deep);
    tb.i_deadlock_deep(deadlock_deep);

    tb.o_host_addr_zero(host_addr_zero);
    tb.o_host_wren_zero(host_wren_zero);
    tb.o_host_rden_zero(host_rden_zero);
    tb.o_host_wdata_zero(host_wdata_zero);
    tb.o_host_wmask_zero(host_wmask_zero);
    tb.i_host_rdata_zero(host_rdata_zero);

    tb.o_host_addr_one(host_addr_one);
    tb.o_host_wren_one(host_wren_one);
    tb.o_host_rden_one(host_rden_one);
    tb.o_host_wdata_one(host_wdata_one);
    tb.o_host_wmask_one(host_wmask_one);
    tb.i_host_rdata_one(host_rdata_one);

    tb.o_host_addr_deep(host_addr_deep);
    tb.o_host_wren_deep(host_wren_deep);
    tb.o_host_rden_deep(host_rden_deep);
    tb.o_host_wdata_deep(host_wdata_deep);
    tb.o_host_wmask_deep(host_wmask_deep);
    tb.i_host_rdata_deep(host_rdata_deep);

    // Bind NpuTop - Zero
    npu_zero.i_clk(clk);
    npu_zero.i_rstn(rstn);
    npu_zero.i_soft_reset(soft_reset);
    npu_zero.i_start(start_zero);
    npu_zero.o_done(done_zero);
    npu_zero.o_deadlock(deadlock_zero);
    npu_zero.i_select(select);
    npu_zero.i_threshold(threshold);
    npu_zero.i_host_addr(host_addr_zero);
    npu_zero.i_host_wren(host_wren_zero);
    npu_zero.i_host_rden(host_rden_zero);
    npu_zero.i_host_wdata(host_wdata_zero);
    npu_zero.i_host_wmask(host_wmask_zero);
    npu_zero.o_host_rdata(host_rdata_zero);

    // Bind NpuTop - One
    npu_one.i_clk(clk);
    npu_one.i_rstn(rstn);
    npu_one.i_soft_reset(soft_reset);
    npu_one.i_start(start_one);
    npu_one.o_done(done_one);
    npu_one.o_deadlock(deadlock_one);
    npu_one.i_select(select);
    npu_one.i_threshold(threshold);
    npu_one.i_host_addr(host_addr_one);
    npu_one.i_host_wren(host_wren_one);
    npu_one.i_host_rden(host_rden_one);
    npu_one.i_host_wdata(host_wdata_one);
    npu_one.i_host_wmask(host_wmask_one);
    npu_one.o_host_rdata(host_rdata_one);

    // Bind NpuTop - Deep
    npu_deep.i_clk(clk);
    npu_deep.i_rstn(rstn);
    npu_deep.i_soft_reset(soft_reset);
    npu_deep.i_start(start_deep);
    npu_deep.o_done(done_deep);
    npu_deep.o_deadlock(deadlock_deep);
    npu_deep.i_select(select);
    npu_deep.i_threshold(threshold);
    npu_deep.i_host_addr(host_addr_deep);
    npu_deep.i_host_wren(host_wren_deep);
    npu_deep.i_host_rden(host_rden_deep);
    npu_deep.i_host_wdata(host_wdata_deep);
    npu_deep.i_host_wmask(host_wmask_deep);
    npu_deep.o_host_rdata(host_rdata_deep);

    // Start Simulation
    sc_start();

    return 0;
}
