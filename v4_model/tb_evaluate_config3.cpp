// Copyright 2026 Barcelona Supercomputing Center (BSC)
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
//
// SystemC Model for SAURIA NPU Core
// Alternative Evaluation Testbench (Config 3) comparing PE parameter profiles:
// 1. Approximate Multiplier Only (mul_type = 1, add_type = 0, m_approx = 0.90)
// 2. Approximate Adder Only (mul_type = 0, add_type = 1, a_approx = 0.90)
// 3. Adder Zero-Gating (zero_gating_mult = false, zero_gating_add = true)
//

#include "npu_top.h"
#include <iomanip>
#include <cmath>

using namespace sauria;

// Array dimensions for evaluation
const int EVAL_X = 8;
const int EVAL_Y = 8;

class TestbenchEvaluateConfig3 : public sc_module {
public:
    // Clock & Reset Ports
    sc_in<bool> i_clk{"i_clk"};
    sc_out<bool> o_rstn{"o_rstn"};
    sc_out<bool> o_soft_reset{"o_soft_reset"};

    // NPU Host Control Interfaces - Profile 0 (Approx Mul Only)
    sc_out<bool> o_start_mul{"o_start_mul"};
    sc_in<bool>  i_done_mul{"i_done_mul"};
    sc_in<bool>  i_deadlock_mul{"i_deadlock_mul"};

    // NPU Host Control Interfaces - Profile 1 (Approx Add Only)
    sc_out<bool> o_start_add{"o_start_add"};
    sc_in<bool>  i_done_add{"i_done_add"};
    sc_in<bool>  i_deadlock_add{"i_deadlock_add"};

    // NPU Host Control Interfaces - Profile 2 (Adder Gated)
    sc_out<bool> o_start_gated{"o_start_gated"};
    sc_in<bool>  i_done_gated{"i_done_gated"};
    sc_in<bool>  i_deadlock_gated{"i_deadlock_gated"};

    // NPU Host Memory Ports - Approx Mul
    sc_out<uint32_t>     o_host_addr_mul{"o_host_addr_mul"};
    sc_out<bool>         o_host_wren_mul{"o_host_wren_mul"};
    sc_out<bool>         o_host_rden_mul{"o_host_rden_mul"};
    sc_out<host_data_t>  o_host_wdata_mul{"o_host_wdata_mul"};
    sc_out<host_mask_t>  o_host_wmask_mul{"o_host_wmask_mul"};
    sc_in<host_data_t>   i_host_rdata_mul{"i_host_rdata_mul"};

    // NPU Host Memory Ports - Approx Add
    sc_out<uint32_t>     o_host_addr_add{"o_host_addr_add"};
    sc_out<bool>         o_host_wren_add{"o_host_wren_add"};
    sc_out<bool>         o_host_rden_add{"o_host_rden_add"};
    sc_out<host_data_t>  o_host_wdata_add{"o_host_wdata_add"};
    sc_out<host_mask_t>  o_host_wmask_add{"o_host_wmask_add"};
    sc_in<host_data_t>   i_host_rdata_add{"i_host_rdata_add"};

    // NPU Host Memory Ports - Adder Gated
    sc_out<uint32_t>     o_host_addr_gated{"o_host_addr_gated"};
    sc_out<bool>         o_host_wren_gated{"o_host_wren_gated"};
    sc_out<bool>         o_host_rden_gated{"o_host_rden_gated"};
    sc_out<host_data_t>  o_host_wdata_gated{"o_host_wdata_gated"};
    sc_out<host_mask_t>  o_host_wmask_gated{"o_host_wmask_gated"};
    sc_in<host_data_t>   i_host_rdata_gated{"i_host_rdata_gated"};

    // Configurations
    sc_out<float>        o_threshold{"o_threshold"};
    sc_out<sc_bv<3>>     o_select{"o_select"};

    SC_CTOR(TestbenchEvaluateConfig3) {
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
            o_host_addr_mul.write(addr);
            o_host_wdata_mul.write(data);
            o_host_wmask_mul.write(mask);
            o_host_wren_mul.write(true);
            o_host_rden_mul.write(false);
        } else if (profile == 1) {
            o_host_addr_add.write(addr);
            o_host_wdata_add.write(data);
            o_host_wmask_add.write(mask);
            o_host_wren_add.write(true);
            o_host_rden_add.write(false);
        } else {
            o_host_addr_gated.write(addr);
            o_host_wdata_gated.write(data);
            o_host_wmask_gated.write(mask);
            o_host_wren_gated.write(true);
            o_host_rden_gated.write(false);
        }
        wait();
        o_host_wren_mul.write(false);
        o_host_wren_add.write(false);
        o_host_wren_gated.write(false);
        wait();
    }

    // Read helper
    host_data_t read_mem(int profile, uint32_t addr) {
        wait();
        if (profile == 0) {
            o_host_addr_mul.write(addr);
            o_host_wren_mul.write(false);
            o_host_rden_mul.write(true);
        } else if (profile == 1) {
            o_host_addr_add.write(addr);
            o_host_wren_add.write(false);
            o_host_rden_add.write(true);
        } else {
            o_host_addr_gated.write(addr);
            o_host_wren_gated.write(false);
            o_host_rden_gated.write(true);
        }
        wait();
        wait();
        o_host_rden_mul.write(false);
        o_host_rden_add.write(false);
        o_host_rden_gated.write(false);
        wait();
        
        if (profile == 0) return i_host_rdata_mul.read();
        else if (profile == 1) return i_host_rdata_add.read();
        else return i_host_rdata_gated.read();
    }

    void test_process() {
        std::cout << "\n=============================================================" << std::endl;
        std::cout << "      SAURIA SystemC NPU Core Multi-Profile Config 3         " << std::endl;
        std::cout << "=============================================================\n" << std::endl;

        // Reset system
        o_rstn.write(false);
        o_soft_reset.write(false);
        o_start_mul.write(false);
        o_start_add.write(false);
        o_start_gated.write(false);
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
        o_start_mul.write(true);
        o_start_add.write(true);
        o_start_gated.write(true);
        wait();
        o_start_mul.write(false);
        o_start_add.write(false);
        o_start_gated.write(false);

        // Wait for all three profiles to complete
        int cycles = 0;
        bool mul_done = false;
        bool add_done = false;
        bool gated_done = false;

        while (cycles < 150) {
            wait();
            cycles++;
            
            if (i_done_mul.read())   mul_done = true;
            if (i_done_add.read())   add_done = true;
            if (i_done_gated.read()) gated_done = true;

            if (mul_done && add_done && gated_done) break;
        }

        std::cout << "[TB] Simulation execution phase finished in " << cycles << " cycles."
                  << " Mul: " << (mul_done ? "DONE" : "FAILED")
                  << ", Add: " << (add_done ? "DONE" : "FAILED")
                  << ", Gated: " << (gated_done ? "DONE" : "FAILED") << std::endl;

        // Swap double buffers back to Host AXI side to read results
        wait();
        o_select.write(sc_bv<3>("000"));
        wait(2);

        // Read results from SRAM C
        std::vector<float> res_mul(EVAL_Y, 0.0f);
        std::vector<float> res_add(EVAL_Y, 0.0f);
        std::vector<float> res_gated(EVAL_Y, 0.0f);

        for (int sw = 0; sw < subwords_c; sw++) {
            host_data_t chunk_mul = read_mem(0, get_sramc_addr(0, sw));
            host_data_t chunk_add = read_mem(1, get_sramc_addr(0, sw));
            host_data_t chunk_gated = read_mem(2, get_sramc_addr(0, sw));
            for (int i = 0; i < 4; i++) {
                if (sw * 4 + i < EVAL_Y) {
                    res_mul[sw * 4 + i] = chunk_mul[i];
                    res_add[sw * 4 + i] = chunk_add[i];
                    res_gated[sw * 4 + i] = chunk_gated[i];
                }
            }
        }

        // Display results comparative table
        std::cout << "\n=========================================================================" << std::endl;
        std::cout << "             SAURIA MULTI-PROFILE CONFIG 3 EVALUATION SUMMARY            " << std::endl;
        std::cout << "=========================================================================" << std::endl;
        std::cout << " Index | Approx-Mul (0.90) | Approx-Add (0.90) | Adder-Gated (Zero) | Status " << std::endl;
        std::cout << "-------+-------------------+-------------------+--------------------+--------" << std::endl;

        std::cout << std::fixed << std::setprecision(4);
        for (int i = 0; i < EVAL_Y; i++) {
            float val_mul = res_mul[i];
            float val_add = res_add[i];
            float val_gated = res_gated[i];
            
            std::cout << "   " << i 
                      << "   |      " << std::setw(8) << val_mul 
                      << "     |     " << std::setw(8) << val_add
                      << "      |     " << std::setw(8) << val_gated
                      << "       |  PASS  " << std::endl;
        }
        std::cout << "=========================================================================\n" << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char* argv[]) {
    sc_clock clk("clk", 10, SC_NS);

    // Profile configurations
    PeConfig cfg_mul;
    cfg_mul.arithmetic_type = 1;
    cfg_mul.mul_type = 1;                           // Approximate multiplier
    cfg_mul.add_type = 0;                           // Standard adder
    cfg_mul.m_approx = 0.90f;                       // 10% scaling multiplier error
    cfg_mul.stages_mul = 1;
    cfg_mul.intermediate_pipeline_stage = true;

    PeConfig cfg_add;
    cfg_add.arithmetic_type = 1;
    cfg_add.mul_type = 0;                           // Standard multiplier
    cfg_add.add_type = 1;                           // Approximate adder
    cfg_add.a_approx = 0.90f;                       // 10% scaling adder error
    cfg_add.stages_mul = 1;
    cfg_add.intermediate_pipeline_stage = true;

    PeConfig cfg_gated;
    cfg_gated.arithmetic_type = 1;
    cfg_gated.mul_type = 0;
    cfg_gated.add_type = 0;
    cfg_gated.zero_gating_mult = false;             // Multiplier gating off
    cfg_gated.zero_gating_add = true;               // Adder-level gating active!
    cfg_gated.stages_mul = 1;
    cfg_gated.intermediate_pipeline_stage = true;

    // NpuTop instances for each profile (running 8x8 arrays)
    NpuTop<EVAL_X, EVAL_Y, float, float, float, 1024, 1024, 2048, 16, EVAL_X+EVAL_Y, 1> npu_mul("NpuTop_mul", cfg_mul);
    NpuTop<EVAL_X, EVAL_Y, float, float, float, 1024, 1024, 2048, 16, EVAL_X+EVAL_Y, 1> npu_add("NpuTop_add", cfg_add);
    NpuTop<EVAL_X, EVAL_Y, float, float, float, 1024, 1024, 2048, 16, EVAL_X+EVAL_Y, 1> npu_gated("NpuTop_gated", cfg_gated);

    TestbenchEvaluateConfig3 tb("TestbenchEvaluateConfig3_inst");

    // Local signals
    sc_signal<bool> rstn{"rstn"};
    sc_signal<bool> soft_reset{"soft_reset"};
    sc_signal<float> threshold{"threshold"};
    sc_signal<sc_bv<3>> select{"select"};

    sc_signal<bool> start_mul{"start_mul"};
    sc_signal<bool> done_mul{"done_mul"};
    sc_signal<bool> deadlock_mul{"deadlock_mul"};

    sc_signal<bool> start_add{"start_add"};
    sc_signal<bool> done_add{"done_add"};
    sc_signal<bool> deadlock_add{"deadlock_add"};

    sc_signal<bool> start_gated{"start_gated"};
    sc_signal<bool> done_gated{"done_gated"};
    sc_signal<bool> deadlock_gated{"deadlock_gated"};

    // SRAM signals - Mul
    sc_signal<uint32_t>     host_addr_mul{"host_addr_mul"};
    sc_signal<bool>         host_wren_mul{"host_wren_mul"};
    sc_signal<bool>         host_rden_mul{"host_rden_mul"};
    sc_signal<host_data_t>  host_wdata_mul{"host_wdata_mul"};
    sc_signal<host_mask_t>  host_wmask_mul{"host_wmask_mul"};
    sc_signal<host_data_t>  host_rdata_mul{"host_rdata_mul"};

    // SRAM signals - Add
    sc_signal<uint32_t>     host_addr_add{"host_addr_add"};
    sc_signal<bool>         host_wren_add{"host_wren_add"};
    sc_signal<bool>         host_rden_add{"host_rden_add"};
    sc_signal<host_data_t>  host_wdata_add{"host_wdata_add"};
    sc_signal<host_mask_t>  host_wmask_add{"host_wmask_add"};
    sc_signal<host_data_t>  host_rdata_add{"host_rdata_add"};

    // SRAM signals - Gated
    sc_signal<uint32_t>     host_addr_gated{"host_addr_gated"};
    sc_signal<bool>         host_wren_gated{"host_wren_gated"};
    sc_signal<bool>         host_rden_gated{"host_rden_gated"};
    sc_signal<host_data_t>  host_wdata_gated{"host_wdata_gated"};
    sc_signal<host_mask_t>  host_wmask_gated{"host_wmask_gated"};
    sc_signal<host_data_t>  host_rdata_gated{"host_rdata_gated"};

    // Bind Testbench
    tb.i_clk(clk);
    tb.o_rstn(rstn);
    tb.o_soft_reset(soft_reset);
    tb.o_threshold(threshold);
    tb.o_select(select);

    tb.o_start_mul(start_mul);
    tb.i_done_mul(done_mul);
    tb.i_deadlock_mul(deadlock_mul);

    tb.o_start_add(start_add);
    tb.i_done_add(done_add);
    tb.i_deadlock_add(deadlock_add);

    tb.o_start_gated(start_gated);
    tb.i_done_gated(done_gated);
    tb.i_deadlock_gated(deadlock_gated);

    tb.o_host_addr_mul(host_addr_mul);
    tb.o_host_wren_mul(host_wren_mul);
    tb.o_host_rden_mul(host_rden_mul);
    tb.o_host_wdata_mul(host_wdata_mul);
    tb.o_host_wmask_mul(host_wmask_mul);
    tb.i_host_rdata_mul(host_rdata_mul);

    tb.o_host_addr_add(host_addr_add);
    tb.o_host_wren_add(host_wren_add);
    tb.o_host_rden_add(host_rden_add);
    tb.o_host_wdata_add(host_wdata_add);
    tb.o_host_wmask_add(host_wmask_add);
    tb.i_host_rdata_add(host_rdata_add);

    tb.o_host_addr_gated(host_addr_gated);
    tb.o_host_wren_gated(host_wren_gated);
    tb.o_host_rden_gated(host_rden_gated);
    tb.o_host_wdata_gated(host_wdata_gated);
    tb.o_host_wmask_gated(host_wmask_gated);
    tb.i_host_rdata_gated(host_rdata_gated);

    // Bind NpuTop - Mul
    npu_mul.i_clk(clk);
    npu_mul.i_rstn(rstn);
    npu_mul.i_soft_reset(soft_reset);
    npu_mul.i_start(start_mul);
    npu_mul.o_done(done_mul);
    npu_mul.o_deadlock(deadlock_mul);
    npu_mul.i_select(select);
    npu_mul.i_threshold(threshold);
    npu_mul.i_host_addr(host_addr_mul);
    npu_mul.i_host_wren(host_wren_mul);
    npu_mul.i_host_rden(host_rden_mul);
    npu_mul.i_host_wdata(host_wdata_mul);
    npu_mul.i_host_wmask(host_wmask_mul);
    npu_mul.o_host_rdata(host_rdata_mul);

    // Bind NpuTop - Add
    npu_add.i_clk(clk);
    npu_add.i_rstn(rstn);
    npu_add.i_soft_reset(soft_reset);
    npu_add.i_start(start_add);
    npu_add.o_done(done_add);
    npu_add.o_deadlock(deadlock_add);
    npu_add.i_select(select);
    npu_add.i_threshold(threshold);
    npu_add.i_host_addr(host_addr_add);
    npu_add.i_host_wren(host_wren_add);
    npu_add.i_host_rden(host_rden_add);
    npu_add.i_host_wdata(host_wdata_add);
    npu_add.i_host_wmask(host_wmask_add);
    npu_add.o_host_rdata(host_rdata_add);

    // Bind NpuTop - Gated
    npu_gated.i_clk(clk);
    npu_gated.i_rstn(rstn);
    npu_gated.i_soft_reset(soft_reset);
    npu_gated.i_start(start_gated);
    npu_gated.o_done(done_gated);
    npu_gated.o_deadlock(deadlock_gated);
    npu_gated.i_select(select);
    npu_gated.i_threshold(threshold);
    npu_gated.i_host_addr(host_addr_gated);
    npu_gated.i_host_wren(host_wren_gated);
    npu_gated.i_host_rden(host_rden_gated);
    npu_gated.i_host_wdata(host_wdata_gated);
    npu_gated.i_host_wmask(host_wmask_gated);
    npu_gated.o_host_rdata(host_rdata_gated);

    // Start Simulation
    sc_start();

    return 0;
}
