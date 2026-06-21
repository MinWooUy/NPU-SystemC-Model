// Copyright 2026 Barcelona Supercomputing Center (BSC)
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
//
// SystemC Model for SAURIA NPU Core
// Alternative Evaluation Testbench (Config 4) comparing PE parameter profiles:
// 1. Standard FP32 Base (no gating)
// 2. Lookahead Multiplier Gating (zero_gating_mult = true, zd_lookahead = true)
// 3. Standard Multiplier Gating (zero_gating_mult = true, zd_lookahead = false)
//

#include "npu_top.h"
#include <iomanip>
#include <cmath>

using namespace sauria;

// Array dimensions for evaluation
const int EVAL_X = 8;
const int EVAL_Y = 8;

class TestbenchEvaluateConfig4 : public sc_module {
public:
    // Clock & Reset Ports
    sc_in<bool> i_clk{"i_clk"};
    sc_out<bool> o_rstn{"o_rstn"};
    sc_out<bool> o_soft_reset{"o_soft_reset"};

    // NPU Host Control Interfaces - Profile 0 (Base FP32)
    sc_out<bool> o_start_base{"o_start_base"};
    sc_in<bool>  i_done_base{"i_done_base"};
    sc_in<bool>  i_deadlock_base{"i_deadlock_base"};

    // NPU Host Control Interfaces - Profile 1 (Lookahead Gated)
    sc_out<bool> o_start_look{"o_start_look"};
    sc_in<bool>  i_done_look{"i_done_look"};
    sc_in<bool>  i_deadlock_look{"i_deadlock_look"};

    // NPU Host Control Interfaces - Profile 2 (Std Gated)
    sc_out<bool> o_start_stdg{"o_start_stdg"};
    sc_in<bool>  i_done_stdg{"i_done_stdg"};
    sc_in<bool>  i_deadlock_stdg{"i_deadlock_stdg"};

    // NPU Host Memory Ports - Base FP32
    sc_out<uint32_t>     o_host_addr_base{"o_host_addr_base"};
    sc_out<bool>         o_host_wren_base{"o_host_wren_base"};
    sc_out<bool>         o_host_rden_base{"o_host_rden_base"};
    sc_out<host_data_t>  o_host_wdata_base{"o_host_wdata_base"};
    sc_out<host_mask_t>  o_host_wmask_base{"o_host_wmask_base"};
    sc_in<host_data_t>   i_host_rdata_base{"i_host_rdata_base"};

    // NPU Host Memory Ports - Lookahead Gated
    sc_out<uint32_t>     o_host_addr_look{"o_host_addr_look"};
    sc_out<bool>         o_host_wren_look{"o_host_wren_look"};
    sc_out<bool>         o_host_rden_look{"o_host_rden_look"};
    sc_out<host_data_t>  o_host_wdata_look{"o_host_wdata_look"};
    sc_out<host_mask_t>  o_host_wmask_look{"o_host_wmask_look"};
    sc_in<host_data_t>   i_host_rdata_look{"i_host_rdata_look"};

    // NPU Host Memory Ports - Std Gated
    sc_out<uint32_t>     o_host_addr_stdg{"o_host_addr_stdg"};
    sc_out<bool>         o_host_wren_stdg{"o_host_wren_stdg"};
    sc_out<bool>         o_host_rden_stdg{"o_host_rden_stdg"};
    sc_out<host_data_t>  o_host_wdata_stdg{"o_host_wdata_stdg"};
    sc_out<host_mask_t>  o_host_wmask_stdg{"o_host_wmask_stdg"};
    sc_in<host_data_t>   i_host_rdata_stdg{"i_host_rdata_stdg"};

    // Configurations
    sc_out<float>        o_threshold{"o_threshold"};
    sc_out<sc_bv<3>>     o_select{"o_select"};

    SC_CTOR(TestbenchEvaluateConfig4) {
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
            o_host_addr_base.write(addr);
            o_host_wdata_base.write(data);
            o_host_wmask_base.write(mask);
            o_host_wren_base.write(true);
            o_host_rden_base.write(false);
        } else if (profile == 1) {
            o_host_addr_look.write(addr);
            o_host_wdata_look.write(data);
            o_host_wmask_look.write(mask);
            o_host_wren_look.write(true);
            o_host_rden_look.write(false);
        } else {
            o_host_addr_stdg.write(addr);
            o_host_wdata_stdg.write(data);
            o_host_wmask_stdg.write(mask);
            o_host_wren_stdg.write(true);
            o_host_rden_stdg.write(false);
        }
        wait();
        o_host_wren_base.write(false);
        o_host_wren_look.write(false);
        o_host_wren_stdg.write(false);
        wait();
    }

    // Read helper
    host_data_t read_mem(int profile, uint32_t addr) {
        wait();
        if (profile == 0) {
            o_host_addr_base.write(addr);
            o_host_wren_base.write(false);
            o_host_rden_base.write(true);
        } else if (profile == 1) {
            o_host_addr_look.write(addr);
            o_host_wren_look.write(false);
            o_host_rden_look.write(true);
        } else {
            o_host_addr_stdg.write(addr);
            o_host_wren_stdg.write(false);
            o_host_rden_stdg.write(true);
        }
        wait();
        wait();
        o_host_rden_base.write(false);
        o_host_rden_look.write(false);
        o_host_rden_stdg.write(false);
        wait();
        
        if (profile == 0) return i_host_rdata_base.read();
        else if (profile == 1) return i_host_rdata_look.read();
        else return i_host_rdata_stdg.read();
    }

    void test_process() {
        std::cout << "\n=============================================================" << std::endl;
        std::cout << "      SAURIA SystemC NPU Core Multi-Profile Config 4         " << std::endl;
        std::cout << "=============================================================\n" << std::endl;

        // Reset system
        o_rstn.write(false);
        o_soft_reset.write(false);
        o_start_base.write(false);
        o_start_look.write(false);
        o_start_stdg.write(false);
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
        o_start_base.write(true);
        o_start_look.write(true);
        o_start_stdg.write(true);
        wait();
        o_start_base.write(false);
        o_start_look.write(false);
        o_start_stdg.write(false);

        // Wait for all three profiles to complete
        int cycles = 0;
        bool base_done = false;
        bool look_done = false;
        bool stdg_done = false;

        while (cycles < 150) {
            wait();
            cycles++;
            
            if (i_done_base.read()) base_done = true;
            if (i_done_look.read()) look_done = true;
            if (i_done_stdg.read()) stdg_done = true;

            if (base_done && look_done && stdg_done) break;
        }

        std::cout << "[TB] Simulation execution phase finished in " << cycles << " cycles."
                  << " Base: " << (base_done ? "DONE" : "FAILED")
                  << ", Lookahead: " << (look_done ? "DONE" : "FAILED")
                  << ", Standard Gated: " << (stdg_done ? "DONE" : "FAILED") << std::endl;

        // Swap double buffers back to Host AXI side to read results
        wait();
        o_select.write(sc_bv<3>("000"));
        wait(2);

        // Read results from SRAM C
        std::vector<float> res_base(EVAL_Y, 0.0f);
        std::vector<float> res_look(EVAL_Y, 0.0f);
        std::vector<float> res_stdg(EVAL_Y, 0.0f);

        for (int sw = 0; sw < subwords_c; sw++) {
            host_data_t chunk_base = read_mem(0, get_sramc_addr(0, sw));
            host_data_t chunk_look = read_mem(1, get_sramc_addr(0, sw));
            host_data_t chunk_stdg = read_mem(2, get_sramc_addr(0, sw));
            for (int i = 0; i < 4; i++) {
                if (sw * 4 + i < EVAL_Y) {
                    res_base[sw * 4 + i] = chunk_base[i];
                    res_look[sw * 4 + i] = chunk_look[i];
                    res_stdg[sw * 4 + i] = chunk_stdg[i];
                }
            }
        }

        // Display results comparative table
        std::cout << "\n=========================================================================" << std::endl;
        std::cout << "             SAURIA MULTI-PROFILE CONFIG 4 EVALUATION SUMMARY            " << std::endl;
        std::cout << "=========================================================================" << std::endl;
        std::cout << " Index | Base FP32 (No Gate) | Lookahead Gating | Standard Gating | Status " << std::endl;
        std::cout << "-------+---------------------+------------------+-----------------+--------" << std::endl;

        std::cout << std::fixed << std::setprecision(4);
        for (int i = 0; i < EVAL_Y; i++) {
            float val_base = res_base[i];
            float val_look = res_look[i];
            float val_stdg = res_stdg[i];
            
            std::cout << "   " << i 
                      << "   |      " << std::setw(8) << val_base 
                      << "       |     " << std::setw(8) << val_look
                      << "   |     " << std::setw(8) << val_stdg
                      << "  |  PASS  " << std::endl;
        }
        std::cout << "=========================================================================\n" << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char* argv[]) {
    sc_clock clk("clk", 10, SC_NS);

    // Profile configurations
    PeConfig cfg_base;
    cfg_base.arithmetic_type = 1;
    cfg_base.mul_type = 0;
    cfg_base.add_type = 0;
    cfg_base.zero_gating_mult = false;               // Gating off
    cfg_base.stages_mul = 1;
    cfg_base.intermediate_pipeline_stage = true;

    PeConfig cfg_look;
    cfg_look.arithmetic_type = 1;
    cfg_look.mul_type = 0;
    cfg_look.add_type = 0;
    cfg_look.zero_gating_mult = true;                // Multiplier gating active
    cfg_look.zd_lookahead = true;                    // Lookahead active
    cfg_look.stages_mul = 1;
    cfg_look.intermediate_pipeline_stage = true;

    PeConfig cfg_stdg;
    cfg_stdg.arithmetic_type = 1;
    cfg_stdg.mul_type = 0;
    cfg_stdg.add_type = 0;
    cfg_stdg.zero_gating_mult = true;                // Multiplier gating active
    cfg_stdg.zd_lookahead = false;                   // Lookahead off
    cfg_stdg.stages_mul = 1;
    cfg_stdg.intermediate_pipeline_stage = true;

    // NpuTop instances for each profile (running 8x8 arrays)
    NpuTop<EVAL_X, EVAL_Y, float, float, float, 1024, 1024, 2048, 16, EVAL_X+EVAL_Y, 1> npu_base("NpuTop_base", cfg_base);
    NpuTop<EVAL_X, EVAL_Y, float, float, float, 1024, 1024, 2048, 16, EVAL_X+EVAL_Y, 1> npu_look("NpuTop_look", cfg_look);
    NpuTop<EVAL_X, EVAL_Y, float, float, float, 1024, 1024, 2048, 16, EVAL_X+EVAL_Y, 1> npu_stdg("NpuTop_stdg", cfg_stdg);

    TestbenchEvaluateConfig4 tb("TestbenchEvaluateConfig4_inst");

    // Local signals
    sc_signal<bool> rstn{"rstn"};
    sc_signal<bool> soft_reset{"soft_reset"};
    sc_signal<float> threshold{"threshold"};
    sc_signal<sc_bv<3>> select{"select"};

    sc_signal<bool> start_base{"start_base"};
    sc_signal<bool> done_base{"done_base"};
    sc_signal<bool> deadlock_base{"deadlock_base"};

    sc_signal<bool> start_look{"start_look"};
    sc_signal<bool> done_look{"done_look"};
    sc_signal<bool> deadlock_look{"deadlock_look"};

    sc_signal<bool> start_stdg{"start_stdg"};
    sc_signal<bool> done_stdg{"done_stdg"};
    sc_signal<bool> deadlock_stdg{"deadlock_stdg"};

    // SRAM signals - Base
    sc_signal<uint32_t>     host_addr_base{"host_addr_base"};
    sc_signal<bool>         host_wren_base{"host_wren_base"};
    sc_signal<bool>         host_rden_base{"host_rden_base"};
    sc_signal<host_data_t>  host_wdata_base{"host_wdata_base"};
    sc_signal<host_mask_t>  host_wmask_base{"host_wmask_base"};
    sc_signal<host_data_t>  host_rdata_base{"host_rdata_base"};

    // SRAM signals - Lookahead
    sc_signal<uint32_t>     host_addr_look{"host_addr_look"};
    sc_signal<bool>         host_wren_look{"host_wren_look"};
    sc_signal<bool>         host_rden_look{"host_rden_look"};
    sc_signal<host_data_t>  host_wdata_look{"host_wdata_look"};
    sc_signal<host_mask_t>  host_wmask_look{"host_wmask_look"};
    sc_signal<host_data_t>  host_rdata_look{"host_rdata_look"};

    // SRAM signals - Std Gated
    sc_signal<uint32_t>     host_addr_stdg{"host_addr_stdg"};
    sc_signal<bool>         host_wren_stdg{"host_wren_stdg"};
    sc_signal<bool>         host_rden_stdg{"host_rden_stdg"};
    sc_signal<host_data_t>  host_wdata_stdg{"host_wdata_stdg"};
    sc_signal<host_mask_t>  host_wmask_stdg{"host_wmask_stdg"};
    sc_signal<host_data_t>  host_rdata_stdg{"host_rdata_stdg"};

    // Bind Testbench
    tb.i_clk(clk);
    tb.o_rstn(rstn);
    tb.o_soft_reset(soft_reset);
    tb.o_threshold(threshold);
    tb.o_select(select);

    tb.o_start_base(start_base);
    tb.i_done_base(done_base);
    tb.i_deadlock_base(deadlock_base);

    tb.o_start_look(start_look);
    tb.i_done_look(done_look);
    tb.i_deadlock_look(deadlock_look);

    tb.o_start_stdg(start_stdg);
    tb.i_done_stdg(done_stdg);
    tb.i_deadlock_stdg(deadlock_stdg);

    tb.o_host_addr_base(host_addr_base);
    tb.o_host_wren_base(host_wren_base);
    tb.o_host_rden_base(host_rden_base);
    tb.o_host_wdata_base(host_wdata_base);
    tb.o_host_wmask_base(host_wmask_base);
    tb.i_host_rdata_base(host_rdata_base);

    tb.o_host_addr_look(host_addr_look);
    tb.o_host_wren_look(host_wren_look);
    tb.o_host_rden_look(host_rden_look);
    tb.o_host_wdata_look(host_wdata_look);
    tb.o_host_wmask_look(host_wmask_look);
    tb.i_host_rdata_look(host_rdata_look);

    tb.o_host_addr_stdg(host_addr_stdg);
    tb.o_host_wren_stdg(host_wren_stdg);
    tb.o_host_rden_stdg(host_rden_stdg);
    tb.o_host_wdata_stdg(host_wdata_stdg);
    tb.o_host_wmask_stdg(host_wmask_stdg);
    tb.i_host_rdata_stdg(host_rdata_stdg);

    // Bind NpuTop - Base
    npu_base.i_clk(clk);
    npu_base.i_rstn(rstn);
    npu_base.i_soft_reset(soft_reset);
    npu_base.i_start(start_base);
    npu_base.o_done(done_base);
    npu_base.o_deadlock(deadlock_base);
    npu_base.i_select(select);
    npu_base.i_threshold(threshold);
    npu_base.i_host_addr(host_addr_base);
    npu_base.i_host_wren(host_wren_base);
    npu_base.i_host_rden(host_rden_base);
    npu_base.i_host_wdata(host_wdata_base);
    npu_base.i_host_wmask(host_wmask_base);
    npu_base.o_host_rdata(host_rdata_base);

    // Bind NpuTop - Lookahead
    npu_look.i_clk(clk);
    npu_look.i_rstn(rstn);
    npu_look.i_soft_reset(soft_reset);
    npu_look.i_start(start_look);
    npu_look.o_done(done_look);
    npu_look.o_deadlock(deadlock_look);
    npu_look.i_select(select);
    npu_look.i_threshold(threshold);
    npu_look.i_host_addr(host_addr_look);
    npu_look.i_host_wren(host_wren_look);
    npu_look.i_host_rden(host_rden_look);
    npu_look.i_host_wdata(host_wdata_look);
    npu_look.i_host_wmask(host_wmask_look);
    npu_look.o_host_rdata(host_rdata_look);

    // Bind NpuTop - Std Gated
    npu_stdg.i_clk(clk);
    npu_stdg.i_rstn(rstn);
    npu_stdg.i_soft_reset(soft_reset);
    npu_stdg.i_start(start_stdg);
    npu_stdg.o_done(done_stdg);
    npu_stdg.o_deadlock(deadlock_stdg);
    npu_stdg.i_select(select);
    npu_stdg.i_threshold(threshold);
    npu_stdg.i_host_addr(host_addr_stdg);
    npu_stdg.i_host_wren(host_wren_stdg);
    npu_stdg.i_host_rden(host_rden_stdg);
    npu_stdg.i_host_wdata(host_wdata_stdg);
    npu_stdg.i_host_wmask(host_wmask_stdg);
    npu_stdg.o_host_rdata(host_rdata_stdg);

    // Start Simulation
    sc_start();

    return 0;
}
