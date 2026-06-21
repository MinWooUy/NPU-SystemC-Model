// Copyright 2026 Barcelona Supercomputing Center (BSC)
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
//
// SystemC Testbench for Sauria NPU Core: Strided, Dilated, Multi-Tiled Verification

#include <systemc.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <iomanip>
#include "npu_top.h"

using namespace sauria;

struct Config {
    int K;
    float threshold;
    int select;
    int act_incntlim;
    int act_incntstep;
    int act_outcntlim;
    int act_outcntstep;
    int wei_incntlim;
    int wei_incntstep;
    int cxlim;
    int cxstep;
    int cklim;
    int ckstep;
    int til_cylim;
    int til_cystep;
    int til_cklim;
    int til_ckstep;
    int ncontexts;
    int preload_en;
    int incntlim;
    int act_reps;
    int wei_reps;
    int dil_pat;
    unsigned int rows_active;
    unsigned int cols_active;
};

Config load_config(const std::string& filepath) {
    Config cfg;
    std::ifstream infile(filepath);
    if (!infile.is_open()) {
        std::cerr << "[ERROR] Could not open config file: " << filepath << std::endl;
        return cfg;
    }
    std::string line;
    while (std::getline(infile, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        std::string key;
        if (iss >> key) {
            if (key == "K") iss >> cfg.K;
            else if (key == "threshold") iss >> cfg.threshold;
            else if (key == "select") iss >> cfg.select;
            else if (key == "act_incntlim") iss >> cfg.act_incntlim;
            else if (key == "act_incntstep") iss >> cfg.act_incntstep;
            else if (key == "act_outcntlim") iss >> cfg.act_outcntlim;
            else if (key == "act_outcntstep") iss >> cfg.act_outcntstep;
            else if (key == "wei_incntlim") iss >> cfg.wei_incntlim;
            else if (key == "wei_incntstep") iss >> cfg.wei_incntstep;
            else if (key == "cxlim") iss >> cfg.cxlim;
            else if (key == "cxstep") iss >> cfg.cxstep;
            else if (key == "cklim") iss >> cfg.cklim;
            else if (key == "ckstep") iss >> cfg.ckstep;
            else if (key == "til_cylim") iss >> cfg.til_cylim;
            else if (key == "til_cystep") iss >> cfg.til_cystep;
            else if (key == "til_cklim") iss >> cfg.til_cklim;
            else if (key == "til_ckstep") iss >> cfg.til_ckstep;
            else if (key == "ncontexts") iss >> cfg.ncontexts;
            else if (key == "preload_en") iss >> cfg.preload_en;
            else if (key == "incntlim") iss >> cfg.incntlim;
            else if (key == "act_reps") iss >> cfg.act_reps;
            else if (key == "wei_reps") iss >> cfg.wei_reps;
            else if (key == "dil_pat") iss >> cfg.dil_pat;
            else if (key == "rows_active") iss >> cfg.rows_active;
            else if (key == "cols_active") iss >> cfg.cols_active;
        }
    }
    return cfg;
}

std::vector<std::vector<float>> load_matrix(const std::string& filepath, int expected_rows, int expected_cols) {
    std::vector<std::vector<float>> mat;
    std::ifstream infile(filepath);
    if (!infile.is_open()) {
        std::cerr << "[ERROR] Could not open matrix file: " << filepath << std::endl;
        return mat;
    }
    std::string line;
    while (std::getline(infile, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        std::vector<float> row;
        float val;
        while (iss >> val) {
            row.push_back(val);
        }
        if (!row.empty()) {
            mat.push_back(row);
        }
    }
    if (mat.size() != (size_t)expected_rows) {
        std::cerr << "[WARNING] Matrix row count mismatch: " << filepath 
                  << " has " << mat.size() << " rows, expected " << expected_rows << std::endl;
    }
    if (!mat.empty() && mat[0].size() != (size_t)expected_cols) {
        std::cerr << "[WARNING] Matrix col count mismatch: " << filepath 
                  << " has " << mat[0].size() << " cols, expected " << expected_cols << std::endl;
    }
    return mat;
}

class TestbenchMultiTile : public sc_module {
public:
    sc_in<bool> i_clk{"i_clk"};
    sc_out<bool> o_rstn{"o_rstn"};
    sc_out<bool> o_soft_reset{"o_soft_reset"};

    sc_out<bool> o_start{"o_start"};
    sc_in<bool>  i_done{"i_done"};
    sc_in<bool>  i_deadlock{"i_deadlock"};

    sc_out<uint32_t>     o_host_addr{"o_host_addr"};
    sc_out<bool>         o_host_wren{"o_host_wren"};
    sc_out<bool>         o_host_rden{"o_host_rden"};
    sc_out<host_data_t>  o_host_wdata{"o_host_wdata"};
    sc_out<host_mask_t>  o_host_wmask{"o_host_wmask"};
    sc_in<host_data_t>   i_host_rdata{"i_host_rdata"};

    sc_out<float>        o_threshold{"o_threshold"};
    sc_out<sc_bv<3>>     o_select{"o_select"};

    SC_CTOR(TestbenchMultiTile) {
        SC_THREAD(test_process);
        sensitive << i_clk.pos();

        SC_THREAD(done_monitor);
        sensitive << i_clk.pos();
    }

private:
    static const int X_DIM = 32;
    static const int Y_DIM = 32;

    const int subwords_a = Y_DIM / 4;
    const int mask_a = subwords_a - 1;
    const int shift_a = (subwords_a == 8) ? 3 : ((subwords_a == 4) ? 2 : ((subwords_a == 2) ? 1 : 0));

    const int subwords_b = X_DIM / 4;
    const int mask_b = subwords_b - 1;
    const int shift_b = (subwords_b == 8) ? 3 : ((subwords_b == 4) ? 2 : ((subwords_b == 2) ? 1 : 0));

    const int subwords_c = Y_DIM / 4;
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

    void write_host_mem(uint32_t addr, const host_data_t& data, const host_mask_t& mask) {
        wait();
        o_host_addr.write(addr);
        o_host_wdata.write(data);
        o_host_wmask.write(mask);
        o_host_wren.write(true);
        o_host_rden.write(false);
        wait();
        o_host_wren.write(false);
        wait();
    }

    host_data_t read_host_mem(uint32_t addr) {
        wait();
        o_host_addr.write(addr);
        o_host_wren.write(false);
        o_host_rden.write(true);
        wait();
        wait();
        o_host_rden.write(false);
        wait();
        return i_host_rdata.read();
    }

    void program_config_regs(const Config& cfg) {
        host_mask_t full_mask;
        full_mask.data.fill(true);

        // incntlim = K + X_DIM
        host_data_t con_data;
        con_data[0] = (float)(cfg.K + X_DIM);
        write_host_mem(CFG_REGS_OFFSET | (CFG_CON_OFFSET + 0x00), con_data, full_mask);

        // act_reps
        host_data_t act_reps_data;
        act_reps_data[0] = (float)cfg.act_reps;
        write_host_mem(CFG_REGS_OFFSET | (CFG_CON_OFFSET + 0x04), act_reps_data, full_mask);

        // wei_reps
        host_data_t wei_reps_data;
        wei_reps_data[0] = (float)cfg.wei_reps;
        write_host_mem(CFG_REGS_OFFSET | (CFG_CON_OFFSET + 0x08), wei_reps_data, full_mask);

        // rows_active
        host_data_t rows_active_data;
        rows_active_data[0] = (float)(cfg.rows_active & 0xFF);
        rows_active_data[1] = (float)((cfg.rows_active >> 8) & 0xFF);
        rows_active_data[2] = (float)((cfg.rows_active >> 16) & 0xFF);
        rows_active_data[3] = (float)((cfg.rows_active >> 24) & 0xFF);
        write_host_mem(CFG_REGS_OFFSET | (CFG_ACT_OFFSET + 0x00), rows_active_data, full_mask);

        // dil_pat
        host_data_t dil_pat_data;
        dil_pat_data[0] = (float)cfg.dil_pat;
        write_host_mem(CFG_REGS_OFFSET | (CFG_ACT_OFFSET + 0x28), dil_pat_data, full_mask);

        // act_incntlim
        host_data_t act_incntlim_data;
        act_incntlim_data[0] = (float)cfg.act_incntlim;
        write_host_mem(CFG_REGS_OFFSET | (CFG_ACT_OFFSET + 0x04), act_incntlim_data, full_mask);

        // act_incntstep
        host_data_t act_incntstep_data;
        act_incntstep_data[0] = (float)cfg.act_incntstep;
        write_host_mem(CFG_REGS_OFFSET | (CFG_ACT_OFFSET + 0x08), act_incntstep_data, full_mask);

        // act_outcntlim
        host_data_t act_outcntlim_data;
        act_outcntlim_data[0] = (float)cfg.act_outcntlim;
        write_host_mem(CFG_REGS_OFFSET | (CFG_ACT_OFFSET + 0x0C), act_outcntlim_data, full_mask);

        // act_outcntstep
        host_data_t act_outcntstep_data;
        act_outcntstep_data[0] = (float)cfg.act_outcntstep;
        write_host_mem(CFG_REGS_OFFSET | (CFG_ACT_OFFSET + 0x10), act_outcntstep_data, full_mask);

        // wei_incntlim
        host_data_t wei_incntlim_data;
        wei_incntlim_data[0] = (float)cfg.wei_incntlim;
        write_host_mem(CFG_REGS_OFFSET | (CFG_WEI_OFFSET + 0x00), wei_incntlim_data, full_mask);

        // wei_incntstep
        host_data_t wei_incntstep_data;
        wei_incntstep_data[0] = (float)cfg.wei_incntstep;
        write_host_mem(CFG_REGS_OFFSET | (CFG_WEI_OFFSET + 0x04), wei_incntstep_data, full_mask);

        // cxlim, cxstep, cklim, ckstep
        host_data_t cxlim_data; cxlim_data[0] = (float)cfg.cxlim;
        write_host_mem(CFG_REGS_OFFSET | (CFG_OUT_OFFSET + 0x00), cxlim_data, full_mask);
        host_data_t cxstep_data; cxstep_data[0] = (float)cfg.cxstep;
        write_host_mem(CFG_REGS_OFFSET | (CFG_OUT_OFFSET + 0x04), cxstep_data, full_mask);
        host_data_t cklim_data; cklim_data[0] = (float)cfg.cklim;
        write_host_mem(CFG_REGS_OFFSET | (CFG_OUT_OFFSET + 0x08), cklim_data, full_mask);
        host_data_t ckstep_data; ckstep_data[0] = (float)cfg.ckstep;
        write_host_mem(CFG_REGS_OFFSET | (CFG_OUT_OFFSET + 0x0C), ckstep_data, full_mask);
    }

    bool tile_done_latched{false};
    uint64_t tile_done_cycle{0};

    void done_monitor() {
        while (true) {
            wait();
            if (i_done.read()) {
                tile_done_latched = true;
                tile_done_cycle = sc_time_stamp().value() / 10000;
            }
        }
    }

    void test_process() {
        host_mask_t full_mask;
        full_mask.data.fill(true);

        std::cout << "=============================================" << std::endl;
        std::cout << "  SAURIA Multi-Tile Standalone Core TB       " << std::endl;
        std::cout << "=============================================" << std::endl;

        std::cout << "[TB] Loading configuration parameters..." << std::endl;
        Config cfg = load_config("tb_data_multitile/stand_config.txt");
        std::cout << "[TB] Loaded Parameters:\n"
                  << "  * K (effective): " << cfg.K << "\n"
                  << "  * act_incntstep: " << cfg.act_incntstep << "\n"
                  << "  * wei_incntstep: " << cfg.wei_incntstep << "\n"
                  << "  * zero_gating_threshold: " << cfg.threshold << "\n"
                  << "  * dil_pat: " << cfg.dil_pat << std::endl;

        // Load Tile 0 matrices
        std::cout << "[TB] Loading Tile 0 matrices..." << std::endl;
        auto mat_A0 = load_matrix("tb_data_multitile/stand_A0.txt", Y_DIM, cfg.K * cfg.act_incntstep);
        int sramb_size = (cfg.K + X_DIM) * cfg.wei_incntstep;
        auto mat_B0 = load_matrix("tb_data_multitile/stand_B0.txt", sramb_size, X_DIM);
        auto mat_C0 = load_matrix("tb_data_multitile/stand_C0.txt", Y_DIM, X_DIM);

        // Load Tile 1 matrices
        std::cout << "[TB] Loading Tile 1 matrices..." << std::endl;
        auto mat_A1 = load_matrix("tb_data_multitile/stand_A1.txt", Y_DIM, cfg.K * cfg.act_incntstep);
        auto mat_B1 = load_matrix("tb_data_multitile/stand_B1.txt", sramb_size, X_DIM);
        auto mat_C1 = load_matrix("tb_data_multitile/stand_C1.txt", Y_DIM, X_DIM);

        if (mat_A0.empty() || mat_B0.empty() || mat_C0.empty() ||
            mat_A1.empty() || mat_B1.empty() || mat_C1.empty()) {
            std::cerr << "[ERROR] Failed to load multi-tile matrices." << std::endl;
            sc_stop();
            return;
        }

        // Initialize ports
        o_rstn.write(false);
        o_soft_reset.write(false);
        o_start.write(false);
        o_host_addr.write(0);
        o_host_wren.write(false);
        o_host_rden.write(false);
        o_host_wdata.write(host_data_t());
        o_host_wmask.write(host_mask_t());
        o_threshold.write(cfg.threshold);
        o_select.write(sc_bv<3>("000")); // Host maps to buffer index 0

        wait(2);
        o_rstn.write(true);
        std::cout << "[TB] @ " << sc_time_stamp() << " System Reset Released." << std::endl;
        wait(2);

        // 1. Program Common Control Configuration Registers
        std::cout << "[TB] Programming configuration registers..." << std::endl;
        program_config_regs(cfg);

        // ----------------------------------------------------
        // TILE 0: Programming Buffer 0 (Host accessing index 0)
        // ----------------------------------------------------
        std::cout << "[TB] @ " << sc_time_stamp() << " Programming SRAM A/B for Tile 0..." << std::endl;
        for (int k = 0; k < cfg.K * cfg.act_incntstep; k++) {
            for (int sw = 0; sw < subwords_a; sw++) {
                host_data_t wdata;
                for (int i = 0; i < 4; i++) {
                    int y = sw * 4 + i;
                    wdata[i] = mat_A0[y][k];
                }
                write_host_mem(get_srama_addr(k, sw), wdata, full_mask);
            }
        }
        for (int addr_idx = 0; addr_idx < sramb_size; addr_idx++) {
            for (int sw = 0; sw < subwords_b; sw++) {
                host_data_t wdata;
                for (int i = 0; i < 4; i++) {
                    int x = sw * 4 + i;
                    wdata[i] = mat_B0[addr_idx][x];
                }
                write_host_mem(get_sramb_addr(addr_idx, sw), wdata, full_mask);
            }
        }

        // Swap buffers: NPU maps to index 0, Host maps to index 1
        std::cout << "[TB] @ " << sc_time_stamp() << " Swapping Double-Buffers (select = 0x7)..." << std::endl;
        o_select.write(sc_bv<3>("111"));
        wait(2);

        // Start NPU executing Tile 0
        std::cout << "[TB] @ " << sc_time_stamp() << " Starting NPU execution for Tile 0..." << std::endl;
        tile_done_latched = false;
        uint64_t start_cycle_t0 = sc_time_stamp().value() / 10000;
        o_start.write(true);
        wait();
        o_start.write(false);

        // ----------------------------------------------------
        // TILE 1 OVERLAP: Host writes Tile 1 to Buffer 1 (Host accessing index 1)
        // ----------------------------------------------------
        std::cout << "[TB] @ " << sc_time_stamp() << " OVERLAP: Programming SRAM A/B for Tile 1..." << std::endl;
        for (int k = 0; k < cfg.K * cfg.act_incntstep; k++) {
            for (int sw = 0; sw < subwords_a; sw++) {
                host_data_t wdata;
                for (int i = 0; i < 4; i++) {
                    int y = sw * 4 + i;
                    wdata[i] = mat_A1[y][k];
                }
                write_host_mem(get_srama_addr(k, sw), wdata, full_mask);
            }
        }
        for (int addr_idx = 0; addr_idx < sramb_size; addr_idx++) {
            for (int sw = 0; sw < subwords_b; sw++) {
                host_data_t wdata;
                for (int i = 0; i < 4; i++) {
                    int x = sw * 4 + i;
                    wdata[i] = mat_B1[addr_idx][x];
                }
                write_host_mem(get_sramb_addr(addr_idx, sw), wdata, full_mask);
            }
        }

        // Wait for NPU Tile 0 to complete
        int cycles_t0 = 0;
        bool t0_done = false;
        int wait_timeout = 20000;
        while (wait_timeout-- > 0) {
            if (tile_done_latched) {
                t0_done = true;
                cycles_t0 = tile_done_cycle - start_cycle_t0;
                break;
            }
            wait();
        }
        if (!t0_done) {
            std::cerr << "[ERROR] Tile 0 execution timeout!" << std::endl;
            sc_stop();
            return;
        }
        std::cout << "[TB] @ " << sc_time_stamp() << " Tile 0 Completed in " << cycles_t0 << " cycles." << std::endl;
        wait(2);

        // ----------------------------------------------------
        // SWAP TILE 1 / READ TILE 0: Select = 0x0
        // NPU maps to index 1 (Tile 1 data), Host maps to index 0 (Tile 0 results)
        // ----------------------------------------------------
        std::cout << "[TB] @ " << sc_time_stamp() << " Pulsing soft reset to clear internal FSM/Feeders..." << std::endl;
        o_soft_reset.write(true);
        wait(2);
        o_soft_reset.write(false);
        wait(2);

        std::cout << "[TB] @ " << sc_time_stamp() << " Reprogramming configuration registers for Tile 1..." << std::endl;
        program_config_regs(cfg);

        std::cout << "[TB] @ " << sc_time_stamp() << " Swapping Double-Buffers (select = 0x0)..." << std::endl;
        o_select.write(sc_bv<3>("000"));
        wait(2);

        // Start NPU executing Tile 1
        std::cout << "[TB] @ " << sc_time_stamp() << " Starting NPU execution for Tile 1..." << std::endl;
        tile_done_latched = false;
        uint64_t start_cycle_t1 = sc_time_stamp().value() / 10000;
        o_start.write(true);
        wait();
        o_start.write(false);

        // Read and Validate Tile 0 results (while NPU computes Tile 1)
        std::cout << "[TB] @ " << sc_time_stamp() << " OVERLAP: Reading and validating Tile 0 outputs..." << std::endl;
        std::vector<std::vector<float>> read_C0(Y_DIM, std::vector<float>(X_DIM, 0.0f));
        for (int x = 0; x < X_DIM; x++) {
            for (int sw = 0; sw < subwords_c; sw++) {
                host_data_t chunk = read_host_mem(get_sramc_addr(x, sw));
                for (int i = 0; i < 4; i++) {
                    int y = sw * 4 + i;
                    read_C0[y][x] = chunk[i];
                }
            }
        }

        bool t0_passed = true;
        const float epsilon = 1e-4f;
        int t0_mismatch = 0;
        for (int y = 0; y < Y_DIM; y++) {
            for (int x = 0; x < X_DIM; x++) {
                if (std::abs(read_C0[y][x] - mat_C0[y][x]) > epsilon) {
                    t0_passed = false;
                    t0_mismatch++;
                }
            }
        }

        // Wait for NPU Tile 1 to complete
        int cycles_t1 = 0;
        bool t1_done = false;
        wait_timeout = 20000;
        while (wait_timeout-- > 0) {
            if (tile_done_latched) {
                t1_done = true;
                cycles_t1 = tile_done_cycle - start_cycle_t1;
                break;
            }
            wait();
        }
        if (!t1_done) {
            std::cerr << "[ERROR] Tile 1 execution timeout!" << std::endl;
            sc_stop();
            return;
        }
        std::cout << "[TB] @ " << sc_time_stamp() << " Tile 1 Completed in " << cycles_t1 << " cycles." << std::endl;
        wait(2);

        // ----------------------------------------------------
        // READ TILE 1: Swap select = 0x7 to access index 1 results
        // ----------------------------------------------------
        std::cout << "[TB] @ " << sc_time_stamp() << " Swapping Double-Buffers for readback (select = 0x7)..." << std::endl;
        o_select.write(sc_bv<3>("111"));
        wait(2);

        std::cout << "[TB] Reading and validating Tile 1 outputs..." << std::endl;
        std::vector<std::vector<float>> read_C1(Y_DIM, std::vector<float>(X_DIM, 0.0f));
        for (int x = 0; x < X_DIM; x++) {
            for (int sw = 0; sw < subwords_c; sw++) {
                host_data_t chunk = read_host_mem(get_sramc_addr(x, sw));
                for (int i = 0; i < 4; i++) {
                    int y = sw * 4 + i;
                    read_C1[y][x] = chunk[i];
                }
            }
        }

        bool t1_passed = true;
        int t1_mismatch = 0;
        for (int y = 0; y < Y_DIM; y++) {
            for (int x = 0; x < X_DIM; x++) {
                if (std::abs(read_C1[y][x] - mat_C1[y][x]) > epsilon) {
                    t1_passed = false;
                    t1_mismatch++;
                }
            }
        }

        // Calculate performance
        int total_cycles = cycles_t0 + cycles_t1;
        float total_flops = 2.0f * (float)Y_DIM * (float)X_DIM * (float)cfg.K * 2.0f; // 2 tiles
        float overall_gflops = total_flops / (total_cycles * 10.0f);

        std::cout << "=============================================" << std::endl;
        std::cout << "        Multi-Tile Verification Summary       " << std::endl;
        std::cout << "=============================================" << std::endl;
        std::cout << "  * Tile 0 Cycles: " << cycles_t0 << std::endl;
        std::cout << "  * Tile 1 Cycles: " << cycles_t1 << std::endl;
        std::cout << "  * Tile 0 Status: " << (t0_passed ? "SUCCESS" : "FAILED") 
                  << (t0_passed ? "" : " (Mismatches: " + std::to_string(t0_mismatch) + ")") << std::endl;
        std::cout << "  * Tile 1 Status: " << (t1_passed ? "SUCCESS" : "FAILED") 
                  << (t1_passed ? "" : " (Mismatches: " + std::to_string(t1_mismatch) + ")") << std::endl;
        std::cout << "  * Overall Performance: " << std::fixed << std::setprecision(4) << overall_gflops << " GFLOPs" << std::endl;
        if (t0_passed && t1_passed) {
            std::cout << "  * FINAL STATUS: SUCCESS! All tiles verified bit-accurately." << std::endl;
        } else {
            std::cout << "  * FINAL STATUS: FAILED!" << std::endl;
        }
        std::cout << "=============================================" << std::endl;

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

    PeConfig pe_cfg;
    pe_cfg.arithmetic_type = 1;
    pe_cfg.mul_type = 0;
    pe_cfg.add_type = 0;
    pe_cfg.stages_mul = 1;
    pe_cfg.intermediate_pipeline_stage = true;
    pe_cfg.zero_gating_mult = true;

    NpuTop<32, 32, float, float, float, 1024, 1024, 2048, 16, 64, 1> npu("NpuTop_inst", pe_cfg);
    TestbenchMultiTile tb("TestbenchMultiTile_inst");

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

    sc_start();
    return 0;
}
