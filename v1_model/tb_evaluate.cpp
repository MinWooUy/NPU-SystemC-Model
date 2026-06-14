//
// SystemC Model for SAURIA NPU Core
// Evaluation Testbench comparing PE parameter profiles:
// 1. Standard FP32 Profile
// 2. Approximate FP32 Profile
// 3. Sparsity Zero-Gating Profile
//

#include "npu_top.h"
#include <iomanip>
#include <cmath>

using namespace sauria;

// Array dimensions for evaluation
const int EVAL_X = 8;
const int EVAL_Y = 8;

class TestbenchEvaluate : public sc_module {
public:
    // Clock & Reset Ports
    sc_in<bool> i_clk{"i_clk"};
    sc_out<bool> o_rstn{"o_rstn"};
    sc_out<bool> o_soft_reset{"o_soft_reset"};

    // NPU Host Control Interfaces
    sc_out<bool> o_start_std{"o_start_std"};
    sc_in<bool>  i_done_std{"i_done_std"};
    sc_in<bool>  i_deadlock_std{"i_deadlock_std"};

    sc_out<bool> o_start_approx{"o_start_approx"};
    sc_in<bool>  i_done_approx{"i_done_approx"};
    sc_in<bool>  i_deadlock_approx{"i_deadlock_approx"};

    sc_out<bool> o_start_gated{"o_start_gated"};
    sc_in<bool>  i_done_gated{"i_done_gated"};
    sc_in<bool>  i_deadlock_gated{"i_deadlock_gated"};

    // NPU Host Memory Ports - STD
    sc_out<uint32_t>     o_host_addr_std{"o_host_addr_std"};
    sc_out<bool>         o_host_wren_std{"o_host_wren_std"};
    sc_out<bool>         o_host_rden_std{"o_host_rden_std"};
    sc_out<host_data_t>  o_host_wdata_std{"o_host_wdata_std"};
    sc_out<host_mask_t>  o_host_wmask_std{"o_host_wmask_std"};
    sc_in<host_data_t>   i_host_rdata_std{"i_host_rdata_std"};

    // NPU Host Memory Ports - APPROX
    sc_out<uint32_t>     o_host_addr_approx{"o_host_addr_approx"};
    sc_out<bool>         o_host_wren_approx{"o_host_wren_approx"};
    sc_out<bool>         o_host_rden_approx{"o_host_rden_approx"};
    sc_out<host_data_t>  o_host_wdata_approx{"o_host_wdata_approx"};
    sc_out<host_mask_t>  o_host_wmask_approx{"o_host_wmask_approx"};
    sc_in<host_data_t>   i_host_rdata_approx{"i_host_rdata_approx"};

    // NPU Host Memory Ports - GATED
    sc_out<uint32_t>     o_host_addr_gated{"o_host_addr_gated"};
    sc_out<bool>         o_host_wren_gated{"o_host_wren_gated"};
    sc_out<bool>         o_host_rden_gated{"o_host_rden_gated"};
    sc_out<host_data_t>  o_host_wdata_gated{"o_host_wdata_gated"};
    sc_out<host_mask_t>  o_host_wmask_gated{"o_host_wmask_gated"};
    sc_in<host_data_t>   i_host_rdata_gated{"i_host_rdata_gated"};

    // Configurations
    sc_out<float>        o_threshold{"o_threshold"};
    sc_out<sc_bv<3>>     o_select{"o_select"};

    SC_CTOR(TestbenchEvaluate) {
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
            o_host_addr_std.write(addr);
            o_host_wdata_std.write(data);
            o_host_wmask_std.write(mask);
            o_host_wren_std.write(true);
            o_host_rden_std.write(false);
        } else if (profile == 1) {
            o_host_addr_approx.write(addr);
            o_host_wdata_approx.write(data);
            o_host_wmask_approx.write(mask);
            o_host_wren_approx.write(true);
            o_host_rden_approx.write(false);
        } else {
            o_host_addr_gated.write(addr);
            o_host_wdata_gated.write(data);
            o_host_wmask_gated.write(mask);
            o_host_wren_gated.write(true);
            o_host_rden_gated.write(false);
        }
        wait();
        o_host_wren_std.write(false);
        o_host_wren_approx.write(false);
        o_host_wren_gated.write(false);
        wait(); // Ensure data resolves
    }

    // Read helper
    host_data_t read_mem(int profile, uint32_t addr) {
        wait();
        if (profile == 0) {
            o_host_addr_std.write(addr);
            o_host_wren_std.write(false);
            o_host_rden_std.write(true);
        } else if (profile == 1) {
            o_host_addr_approx.write(addr);
            o_host_wren_approx.write(false);
            o_host_rden_approx.write(true);
        } else {
            o_host_addr_gated.write(addr);
            o_host_wren_gated.write(false);
            o_host_rden_gated.write(true);
        }
        wait(); // Address registers in SRAM
        wait(); // Data registers out (1-cycle read latency)
        o_host_rden_std.write(false);
        o_host_rden_approx.write(false);
        o_host_rden_gated.write(false);
        wait();
        
        if (profile == 0) return i_host_rdata_std.read();
        else if (profile == 1) return i_host_rdata_approx.read();
        else return i_host_rdata_gated.read();
    }

    void test_process() {
        std::cout << "\n=============================================================" << std::endl;
        std::cout << "      SAURIA SystemC NPU Core Multi-Profile Evaluation       " << std::endl;
        std::cout << "=============================================================\n" << std::endl;

        // Reset system
        o_rstn.write(false);
        o_soft_reset.write(false);
        o_start_std.write(false);
        o_start_approx.write(false);
        o_start_gated.write(false);
        
        o_host_wren_std.write(false); o_host_rden_std.write(false);
        o_host_wren_approx.write(false); o_host_rden_approx.write(false);
        o_host_wren_gated.write(false); o_host_rden_gated.write(false);

        // Sparsity threshold = 0.5f
        o_threshold.write(0.5f);
        o_select.write(sc_bv<3>("000")); // Map physical buffer 0 to Host AXI

        wait(5);
        o_rstn.write(true);
        wait(2);

        // 1. Program inputs to all three profiles (activations and weights)
        // Set up activations: row 0 (phys_addr 0)
        host_data_t act_sub0, act_sub1;
        // Elements below 0.5 (like 0.2, 0.4, 0.01) will trigger zero-gating
        act_sub0[0] = 1.5f; act_sub0[1] = 0.2f;  act_sub0[2] = 3.5f; act_sub0[3] = 0.4f;
        act_sub1[0] = 5.5f; act_sub1[1] = 0.01f; act_sub1[2] = 7.5f; act_sub1[3] = 0.0f;

        // Set up weights: col 0 (phys_addr 0)
        host_data_t wei_sub0, wei_sub1;
        wei_sub0[0] = 2.0f; wei_sub0[1] = 0.1f;  wei_sub0[2] = 1.0f; wei_sub0[3] = 4.0f;
        wei_sub1[0] = 0.5f; wei_sub1[1] = 2.0f;  wei_sub1[2] = 0.0f; wei_sub1[3] = 1.5f;

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
            
            // Read back to verify write success
            host_data_t read_con = read_mem(p, CFG_REGS_OFFSET | (CFG_CON_OFFSET + 0x00));
            std::cout << "[TB] Profile " << p << " incntlim written = 4.0, readback = " << read_con[0] << std::endl;
        }

        std::cout << "[TB] Input activations and weights programmed to double buffers." << std::endl;
        
        // Read back inputs to verify
        for (int p = 0; p < 3; p++) {
            host_data_t read_act0 = read_mem(p, get_srama_addr(0, 0));
            host_data_t read_act1 = read_mem(p, get_srama_addr(0, 1));
            host_data_t read_wei0 = read_mem(p, get_sramb_addr(0, 0));
            host_data_t read_wei1 = read_mem(p, get_sramb_addr(0, 1));
            std::cout << "[TB] Profile " << p << " Readback SRAM A0: [" << read_act0[0] << ", " << read_act0[1] << ", " << read_act0[2] << ", " << read_act0[3] << "]" << std::endl;
            std::cout << "[TB] Profile " << p << " Readback SRAM A1: [" << read_act1[0] << ", " << read_act1[1] << ", " << read_act1[2] << ", " << read_act1[3] << "]" << std::endl;
            std::cout << "[TB] Profile " << p << " Readback SRAM B0: [" << read_wei0[0] << ", " << read_wei0[1] << ", " << read_wei0[2] << ", " << read_wei0[3] << "]" << std::endl;
            std::cout << "[TB] Profile " << p << " Readback SRAM B1: [" << read_wei1[0] << ", " << read_wei1[1] << ", " << read_wei1[2] << ", " << read_wei1[3] << "]" << std::endl;
        }

        // Swap double buffers to NPU side
        wait();
        o_select.write(sc_bv<3>("111"));
        wait(2);

        // Start NPU simulation across all three profiles
        std::cout << "[TB] Triggering execution start pulse for all profiles..." << std::endl;
        o_start_std.write(true);
        o_start_approx.write(true);
        o_start_gated.write(true);
        wait();
        o_start_std.write(false);
        o_start_approx.write(false);
        o_start_gated.write(false);
        wait();

        // Wait for FSMs to complete
        int timeout = 500;
        bool std_done = false, approx_done = false, gated_done = false;
        int cycles = 0;
        while (timeout-- > 0) {
            wait();
            cycles++;
            if (i_done_std.read()) std_done = true;
            if (i_done_approx.read()) approx_done = true;
            if (i_done_gated.read()) gated_done = true;

            if (cycles % 20 == 0) {
                std::cout << "[TB] Cycle " << cycles << " status -> Std: " 
                          << (std_done ? "DONE" : "RUNNING") << ", Approx: " 
                          << (approx_done ? "DONE" : "RUNNING") << ", Gated: " 
                          << (gated_done ? "DONE" : "RUNNING")
                          << " | Deadlock Std: " << i_deadlock_std.read()
                          << ", Approx: " << i_deadlock_approx.read()
                          << ", Gated: " << i_deadlock_gated.read() << std::endl;
            }

            if (std_done && approx_done && gated_done) {
                break;
            }
        }

        std::cout << "[TB] Simulation execution phase finished in " << cycles << " cycles. Standard: " 
                  << (std_done ? "DONE" : "TIMEOUT") << ", Approx: " 
                  << (approx_done ? "DONE" : "TIMEOUT") << ", Gated: " 
                  << (gated_done ? "DONE" : "TIMEOUT") << std::endl;

        // Swap double buffers back to Host AXI side to read results
        wait();
        o_select.write(sc_bv<3>("000"));
        wait(2);

        // Read results from SRAM C
        std::vector<float> res_std(EVAL_Y, 0.0f);
        std::vector<float> res_approx(EVAL_Y, 0.0f);
        std::vector<float> res_gated(EVAL_Y, 0.0f);

        for (int sw = 0; sw < subwords_c; sw++) {
            host_data_t chunk_std = read_mem(0, get_sramc_addr(0, sw));
            host_data_t chunk_approx = read_mem(1, get_sramc_addr(0, sw));
            host_data_t chunk_gated = read_mem(2, get_sramc_addr(0, sw));
            for (int i = 0; i < 4; i++) {
                if (sw * 4 + i < EVAL_Y) {
                    res_std[sw * 4 + i] = chunk_std[i];
                    res_approx[sw * 4 + i] = chunk_approx[i];
                    res_gated[sw * 4 + i] = chunk_gated[i];
                }
            }
        }

        // Display results comparative table
        std::cout << "\n=========================================================================" << std::endl;
        std::cout << "                 SAURIA MULTI-PROFILE EVALUATION SUMMARY                 " << std::endl;
        std::cout << "=========================================================================" << std::endl;
        std::cout << " Index | Standard (FP32) | Approx (Scale) | Gated (Sparsity) | Approx Error " << std::endl;
        std::cout << "-------+-----------------+----------------+------------------+--------------" << std::endl;

        std::cout << std::fixed << std::setprecision(4);
        for (int i = 0; i < EVAL_Y; i++) {
            float val_std = res_std[i];
            float val_approx = res_approx[i];
            float val_gated = res_gated[i];
            
            float approx_err = (val_std != 0.0f) ? std::abs((val_std - val_approx) / val_std) * 100.0f : 0.0f;
            
            std::cout << "   " << i 
                      << "   |     " << std::setw(8) << val_std 
                      << "    |    " << std::setw(8) << val_approx
                      << "    |     " << std::setw(8) << val_gated
                      << "     |    " << std::setw(6) << approx_err << "%" << std::endl;
        }
        std::cout << "=========================================================================\n" << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char* argv[]) {
    sc_clock clk("clk", 10, SC_NS);

    // Profile configurations
    PeConfig cfg_std;
    cfg_std.arithmetic_type = 1;
    cfg_std.mul_type = 0;
    cfg_std.add_type = 0;
    cfg_std.stages_mul = 1;
    cfg_std.intermediate_pipeline_stage = true;
    cfg_std.zero_gating_mult = false; // No gating

    PeConfig cfg_approx;
    cfg_approx.arithmetic_type = 1;
    cfg_approx.mul_type = 1;
    cfg_approx.add_type = 1;
    cfg_approx.m_approx = 0.85f;      // 15% scaling error
    cfg_approx.a_approx = 0.95f;      // 5% scaling error
    cfg_approx.stages_mul = 1;
    cfg_approx.intermediate_pipeline_stage = true;
    cfg_approx.zero_gating_mult = false;

    PeConfig cfg_gated;
    cfg_gated.arithmetic_type = 1;
    cfg_gated.mul_type = 0;
    cfg_gated.add_type = 0;
    cfg_gated.stages_mul = 1;
    cfg_gated.intermediate_pipeline_stage = true;
    cfg_gated.zero_gating_mult = true; // Sparsity Gating enabled!

    // NpuTop instances for each profile (running 8x8 arrays)
    NpuTop<EVAL_X, EVAL_Y, float, float, float, 1024, 1024, 2048, 16, EVAL_X+EVAL_Y, 1> npu_std("NpuTop_std", cfg_std);
    NpuTop<EVAL_X, EVAL_Y, float, float, float, 1024, 1024, 2048, 16, EVAL_X+EVAL_Y, 1> npu_approx("NpuTop_approx", cfg_approx);
    NpuTop<EVAL_X, EVAL_Y, float, float, float, 1024, 1024, 2048, 16, EVAL_X+EVAL_Y, 1> npu_gated("NpuTop_gated", cfg_gated);

    TestbenchEvaluate tb("TestbenchEvaluate_inst");

    // Local signals
    sc_signal<bool> rstn{"rstn"};
    sc_signal<bool> soft_reset{"soft_reset"};
    sc_signal<float> threshold{"threshold"};
    sc_signal<sc_bv<3>> select{"select"};

    tb.i_clk(clk);
    tb.o_rstn(rstn);
    tb.o_soft_reset(soft_reset);
    tb.o_threshold(threshold);
    tb.o_select(select);

    npu_std.i_clk(clk); npu_std.i_rstn(rstn); npu_std.i_soft_reset(soft_reset);
    npu_approx.i_clk(clk); npu_approx.i_rstn(rstn); npu_approx.i_soft_reset(soft_reset);
    npu_gated.i_clk(clk); npu_gated.i_rstn(rstn); npu_gated.i_soft_reset(soft_reset);

    npu_std.i_threshold(threshold); npu_std.i_select(select);
    npu_approx.i_threshold(threshold); npu_approx.i_select(select);
    npu_gated.i_threshold(threshold); npu_gated.i_select(select);

    // Done / start bindings
    sc_signal<bool> start_std, done_std, deadlock_std;
    sc_signal<bool> start_approx, done_approx, deadlock_approx;
    sc_signal<bool> start_gated, done_gated, deadlock_gated;

    tb.o_start_std(start_std); tb.i_done_std(done_std); tb.i_deadlock_std(deadlock_std);
    tb.o_start_approx(start_approx); tb.i_done_approx(done_approx); tb.i_deadlock_approx(deadlock_approx);
    tb.o_start_gated(start_gated); tb.i_done_gated(done_gated); tb.i_deadlock_gated(deadlock_gated);

    npu_std.i_start(start_std); npu_std.o_done(done_std); npu_std.o_deadlock(deadlock_std);
    npu_approx.i_start(start_approx); npu_approx.o_done(done_approx); npu_approx.o_deadlock(deadlock_approx);
    npu_gated.i_start(start_gated); npu_gated.o_done(done_gated); npu_gated.o_deadlock(deadlock_gated);

    // Memory ports bindings - STD
    sc_signal<uint32_t> addr_std;
    sc_signal<bool> wren_std, rden_std;
    sc_signal<host_data_t> wdata_std, rdata_std;
    sc_signal<host_mask_t> wmask_std;

    tb.o_host_addr_std(addr_std); tb.o_host_wren_std(wren_std); tb.o_host_rden_std(rden_std);
    tb.o_host_wdata_std(wdata_std); tb.o_host_wmask_std(wmask_std); tb.i_host_rdata_std(rdata_std);

    npu_std.i_host_addr(addr_std); npu_std.i_host_wren(wren_std); npu_std.i_host_rden(rden_std);
    npu_std.i_host_wdata(wdata_std); npu_std.i_host_wmask(wmask_std); npu_std.o_host_rdata(rdata_std);

    // Memory ports bindings - APPROX
    sc_signal<uint32_t> addr_approx;
    sc_signal<bool> wren_approx, rden_approx;
    sc_signal<host_data_t> wdata_approx, rdata_approx;
    sc_signal<host_mask_t> wmask_approx;

    tb.o_host_addr_approx(addr_approx); tb.o_host_wren_approx(wren_approx); tb.o_host_rden_approx(rden_approx);
    tb.o_host_wdata_approx(wdata_approx); tb.o_host_wmask_approx(wmask_approx); tb.i_host_rdata_approx(rdata_approx);

    npu_approx.i_host_addr(addr_approx); npu_approx.i_host_wren(wren_approx); npu_approx.i_host_rden(rden_approx);
    npu_approx.i_host_wdata(wdata_approx); npu_approx.i_host_wmask(wmask_approx); npu_approx.o_host_rdata(rdata_approx);

    // Memory ports bindings - GATED
    sc_signal<uint32_t> addr_gated;
    sc_signal<bool> wren_gated, rden_gated;
    sc_signal<host_data_t> wdata_gated, rdata_gated;
    sc_signal<host_mask_t> wmask_gated;

    tb.o_host_addr_gated(addr_gated); tb.o_host_wren_gated(wren_gated); tb.o_host_rden_gated(rden_gated);
    tb.o_host_wdata_gated(wdata_gated); tb.o_host_wmask_gated(wmask_gated); tb.i_host_rdata_gated(rdata_gated);

    npu_gated.i_host_addr(addr_gated); npu_gated.i_host_wren(wren_gated); npu_gated.i_host_rden(rden_gated);
    npu_gated.i_host_wdata(wdata_gated); npu_gated.i_host_wmask(wmask_gated); npu_gated.o_host_rdata(rdata_gated);

    sc_start();
    return 0;
}
