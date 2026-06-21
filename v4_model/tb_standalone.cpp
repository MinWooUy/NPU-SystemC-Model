// Copyright 2026 Barcelona Supercomputing Center (BSC)
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
//
// Standalone SystemC Testbench for SAURIA NPU Core (model v4)
//

#include "npu_top.h"
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace sauria;

struct Config {
  int K = 64;
  float threshold = 0.05f;
  int select = 0;
  int act_incntlim = 32;
  int act_incntstep = 32;
  int act_outcntlim = 32;
  int act_outcntstep = 32;
  int wei_incntlim = 2048;
  int wei_incntstep = 32;
  int cxlim = 64;
  int cxstep = 32;
  int cklim = 1024;
  int ckstep = 32;
  int incntlim = 63;
  int act_reps = 1;
  int wei_reps = 1;
  int dil_pat = 1;
  unsigned int rows_active = 0xFFFFFFFF;
  unsigned int cols_active = 0xFFFFFFFF;
};

Config load_config(const std::string &filepath) {
  Config cfg;
  std::ifstream infile(filepath);
  if (!infile.is_open()) {
    std::cerr << "[ERROR] Could not open config file: " << filepath
              << std::endl;
    return cfg;
  }
  std::string line;
  while (std::getline(infile, line)) {
    if (line.empty() || line[0] == '#')
      continue;
    std::istringstream iss(line);
    std::string key;
    if (iss >> key) {
      if (key == "K")
        iss >> cfg.K;
      else if (key == "threshold")
        iss >> cfg.threshold;
      else if (key == "select")
        iss >> cfg.select;
      else if (key == "act_incntlim")
        iss >> cfg.act_incntlim;
      else if (key == "act_incntstep")
        iss >> cfg.act_incntstep;
      else if (key == "act_outcntlim")
        iss >> cfg.act_outcntlim;
      else if (key == "act_outcntstep")
        iss >> cfg.act_outcntstep;
      else if (key == "wei_incntlim")
        iss >> cfg.wei_incntlim;
      else if (key == "wei_incntstep")
        iss >> cfg.wei_incntstep;
      else if (key == "cxlim")
        iss >> cfg.cxlim;
      else if (key == "cxstep")
        iss >> cfg.cxstep;
      else if (key == "cklim")
        iss >> cfg.cklim;
      else if (key == "ckstep")
        iss >> cfg.ckstep;
      else if (key == "incntlim")
        iss >> cfg.incntlim;
      else if (key == "act_reps")
        iss >> cfg.act_reps;
      else if (key == "wei_reps")
        iss >> cfg.wei_reps;
      else if (key == "dil_pat")
        iss >> cfg.dil_pat;
      else if (key == "rows_active")
        iss >> cfg.rows_active;
      else if (key == "cols_active")
        iss >> cfg.cols_active;
    }
  }
  return cfg;
}

std::vector<std::vector<float>>
load_matrix(const std::string &filepath, int expected_rows, int expected_cols) {
  std::vector<std::vector<float>> mat;
  std::ifstream infile(filepath);
  if (!infile.is_open()) {
    std::cerr << "[ERROR] Could not open matrix file: " << filepath
              << std::endl;
    return mat;
  }
  std::string line;
  while (std::getline(infile, line)) {
    if (line.empty())
      continue;
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
    std::cerr << "[WARNING] Matrix row count mismatch: " << filepath << " has "
              << mat.size() << " rows, expected " << expected_rows << std::endl;
  }
  if (!mat.empty() && mat[0].size() != (size_t)expected_cols) {
    std::cerr << "[WARNING] Matrix col count mismatch: " << filepath << " has "
              << mat[0].size() << " cols, expected " << expected_cols
              << std::endl;
  }
  return mat;
}

class TestbenchStandalone : public sc_module {
public:
  // Clock & Reset Ports
  sc_in<bool> i_clk{"i_clk"};
  sc_out<bool> o_rstn{"o_rstn"};
  sc_out<bool> o_soft_reset{"o_soft_reset"};

  // NPU Host Control Interface
  sc_out<bool> o_start{"o_start"};
  sc_in<bool> i_done{"i_done"};
  sc_in<bool> i_deadlock{"i_deadlock"};

  // NPU Host Memory Port (AXI interface modeling)
  sc_out<uint32_t> o_host_addr{"o_host_addr"};
  sc_out<bool> o_host_wren{"o_host_wren"};
  sc_out<bool> o_host_rden{"o_host_rden"};
  sc_out<host_data_t> o_host_wdata{"o_host_wdata"};
  sc_out<host_mask_t> o_host_wmask{"o_host_wmask"};
  sc_in<host_data_t> i_host_rdata{"i_host_rdata"};

  // Configurations
  sc_out<float> o_threshold{"o_threshold"};
  sc_out<sc_bv<3>> o_select{"o_select"};

  SC_CTOR(TestbenchStandalone) {
    SC_THREAD(test_process);
    sensitive << i_clk.pos();
  }

private:
  static const int X_DIM = 32;
  static const int Y_DIM = 32;

  const int subwords_a = Y_DIM / 4;
  const int mask_a = subwords_a - 1;
  const int shift_a =
      (subwords_a == 8) ? 3
                        : ((subwords_a == 4) ? 2 : ((subwords_a == 2) ? 1 : 0));

  const int subwords_b = X_DIM / 4;
  const int mask_b = subwords_b - 1;
  const int shift_b =
      (subwords_b == 8) ? 3
                        : ((subwords_b == 4) ? 2 : ((subwords_b == 2) ? 1 : 0));

  const int subwords_c = Y_DIM / 4;
  const int mask_c = subwords_c - 1;
  const int shift_c =
      (subwords_c == 8) ? 3
                        : ((subwords_c == 4) ? 2 : ((subwords_c == 2) ? 1 : 0));

  uint32_t get_srama_addr(uint32_t phys_addr, uint32_t sub_word) {
    return SRAMA_OFFSET | ((phys_addr << shift_a) | (sub_word & mask_a));
  }

  uint32_t get_sramb_addr(uint32_t phys_addr, uint32_t sub_word) {
    return SRAMB_OFFSET | ((phys_addr << shift_b) | (sub_word & mask_b));
  }

  uint32_t get_sramc_addr(uint32_t phys_addr, uint32_t sub_word) {
    return SRAMC_OFFSET | ((phys_addr << shift_c) | (sub_word & mask_c));
  }

  void write_host_mem(uint32_t addr, const host_data_t &data,
                      const host_mask_t &mask) {
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
    wait(); // Address registers in SRAM
    wait(); // Data registers out (1-cycle read latency)
    o_host_rden.write(false);
    wait();
    return i_host_rdata.read();
  }

  void test_process() {
    std::cout << "=============================================" << std::endl;
    std::cout << "    SAURIA Standalone Core Verification TB     " << std::endl;
    std::cout << "=============================================" << std::endl;

    std::cout << "[TB] Loading configuration parameters..." << std::endl;
    Config cfg = load_config("tb_data/stand_config.txt");
    std::cout << "[TB] Loaded Parameters:\n"
              << "  * K: " << cfg.K << "\n"
              << "  * zero_gating_threshold: " << cfg.threshold << "\n"
              << "  * rows_active_mask: 0x" << std::hex << cfg.rows_active
              << std::dec << "\n"
              << "  * dil_pat: " << cfg.dil_pat << std::endl;

    std::cout << "[TB] Loading input activations (Mat A)..." << std::endl;
    auto mat_A = load_matrix("tb_data/stand_A.txt", Y_DIM, cfg.K);

    std::cout << "[TB] Loading weights (Mat B)..." << std::endl;
    auto mat_B = load_matrix("tb_data/stand_B.txt", cfg.K + X_DIM, X_DIM);

    std::cout << "[TB] Loading golden outputs (Mat C)..." << std::endl;
    auto mat_C = load_matrix("tb_data/stand_C.txt", Y_DIM, X_DIM);

    if (mat_A.empty() || mat_B.empty() || mat_C.empty()) {
      std::cerr << "[ERROR] Failed to load simulation files." << std::endl;
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
    o_select.write(sc_bv<3>("000")); // Map buffer 0 to Host AXI

    wait(2);
    o_rstn.write(true);
    std::cout << "[TB] @ " << sc_time_stamp() << " System Reset Released."
              << std::endl;
    wait(2);

    std::cout << "[TB] Programming configuration registers..." << std::endl;
    host_mask_t full_mask;
    full_mask.data.fill(true);

    // incntlim = K + X_DIM
    host_data_t con_data;
    con_data[0] = (float)(cfg.K + X_DIM);
    write_host_mem(CFG_REGS_OFFSET | (CFG_CON_OFFSET + 0x00), con_data,
                   full_mask);

    // act_reps
    host_data_t act_reps_data;
    act_reps_data[0] = (float)cfg.act_reps;
    write_host_mem(CFG_REGS_OFFSET | (CFG_CON_OFFSET + 0x04), act_reps_data,
                   full_mask);

    // wei_reps
    host_data_t wei_reps_data;
    wei_reps_data[0] = (float)cfg.wei_reps;
    write_host_mem(CFG_REGS_OFFSET | (CFG_CON_OFFSET + 0x08), wei_reps_data,
                   full_mask);

    // rows_active
    host_data_t rows_active_data;
    rows_active_data[0] = (float)(cfg.rows_active & 0xFF);
    rows_active_data[1] = (float)((cfg.rows_active >> 8) & 0xFF);
    rows_active_data[2] = (float)((cfg.rows_active >> 16) & 0xFF);
    rows_active_data[3] = (float)((cfg.rows_active >> 24) & 0xFF);
    write_host_mem(CFG_REGS_OFFSET | (CFG_ACT_OFFSET + 0x00), rows_active_data,
                   full_mask);

    // dil_pat
    host_data_t dil_pat_data;
    dil_pat_data[0] = (float)cfg.dil_pat;
    write_host_mem(CFG_REGS_OFFSET | (CFG_ACT_OFFSET + 0x28), dil_pat_data,
                   full_mask);

    // Write SRAM A
    std::cout << "[TB] Programming SRAM A (Activations) via Host AXI..."
              << std::endl;
    for (int k = 0; k < cfg.K; k++) {
      for (int sw = 0; sw < subwords_a; sw++) {
        host_data_t wdata;
        for (int i = 0; i < 4; i++) {
          int y = sw * 4 + i;
          wdata[i] = mat_A[y][k];
        }
        write_host_mem(get_srama_addr(k, sw), wdata, full_mask);
      }
    }

    // Write SRAM B
    std::cout << "[TB] Programming SRAM B (Weights) via Host AXI..."
              << std::endl;
    for (int addr_idx = 0; addr_idx < cfg.K + X_DIM; addr_idx++) {
      for (int sw = 0; sw < subwords_b; sw++) {
        host_data_t wdata;
        for (int i = 0; i < 4; i++) {
          int x = sw * 4 + i;
          wdata[i] = mat_B[addr_idx][x];
        }
        write_host_mem(get_sramb_addr(addr_idx, sw), wdata, full_mask);
      }
    }

    // Swap double buffers
    std::cout << "[TB] @ " << sc_time_stamp()
              << " Swapping Double-Buffers (select = 0x7)..." << std::endl;
    wait();
    o_select.write(sc_bv<3>("111"));
    wait(2);

    // Start execution
    std::cout << "[TB] @ " << sc_time_stamp() << " Asserting Core Start..."
              << std::endl;
    wait();
    o_start.write(true);
    wait();
    o_start.write(false);

    // Monitor completion
    int timeout = 50000;
    bool completed = false;
    int cycles = 0;
    while (timeout-- > 0) {
      wait();
      cycles++;
      if (i_done.read()) {
        completed = true;
        break;
      }
    }

    if (completed) {
      std::cout << "[TB] @ " << sc_time_stamp()
                << " Core Completed Execution Successfully!" << std::endl;
    } else {
      std::cerr << "[ERROR] Core execution timeout!" << std::endl;
      sc_stop();
      return;
    }

    // Swap buffers back
    std::cout << "[TB] @ " << sc_time_stamp()
              << " Swapping Double-Buffers back (select = 0x0)..." << std::endl;
    wait();
    o_select.write(sc_bv<3>("000"));
    wait(2);

    // Read results
    std::cout << "[TB] Reading back SRAM C results..." << std::endl;
    std::vector<std::vector<float>> read_C(Y_DIM,
                                           std::vector<float>(X_DIM, 0.0f));
    for (int x = 0; x < X_DIM; x++) {
      for (int sw = 0; sw < subwords_c; sw++) {
        host_data_t chunk = read_host_mem(get_sramc_addr(x, sw));
        for (int i = 0; i < 4; i++) {
          int y = sw * 4 + i;
          read_C[y][x] = chunk[i];
        }
      }
    }

    std::cout << "[TB] Validating outputs against golden reference..."
              << std::endl;
    bool test_passed = true;
    int mismatch_count = 0;
    const float epsilon = 1e-4f;
    for (int y = 0; y < Y_DIM; y++) {
      for (int x = 0; x < X_DIM; x++) {
        if (std::abs(read_C[y][x] - mat_C[y][x]) > epsilon) {
          test_passed = false;
          mismatch_count++;
          if (mismatch_count <= 10) {
            std::cerr << "[TB] Mismatch at (" << y << ", " << x << "): got "
                      << read_C[y][x] << ", expected " << mat_C[y][x]
                      << std::endl;
          }
        }
      }
    }

    // Calculate performance
    float flops = 2.0f * (float)Y_DIM * (float)X_DIM * (float)cfg.K;
    float gflops = flops / (cycles * 10.0f);

    std::cout << "=============================================" << std::endl;
    std::cout << "               Verification Summary          " << std::endl;
    std::cout << "=============================================" << std::endl;
    std::cout << "  * Simulation Cycles: " << cycles << " (clock period = 10ns)"
              << std::endl;
    std::cout << "  * Effective Performance: " << std::fixed
              << std::setprecision(4) << gflops << " GFLOPs" << std::endl;
    if (test_passed) {
      std::cout
          << "  * STATUS: SUCCESS! All active outputs match golden reference."
          << std::endl;
    } else {
      std::cout << "  * STATUS: FAILED! Mismatch count: " << mismatch_count
                << std::endl;
    }
    std::cout << "=============================================" << std::endl;

    sc_stop();
  }
};

int sc_main(int argc, char *argv[]) {
  sc_clock clk("clk", 10, SC_NS);

  sc_signal<bool> rstn{"rstn"};
  sc_signal<bool> soft_reset{"soft_reset"};
  sc_signal<bool> start{"start"};
  sc_signal<bool> done{"done"};
  sc_signal<bool> deadlock{"deadlock"};

  sc_signal<uint32_t> host_addr{"host_addr"};
  sc_signal<bool> host_wren{"host_wren"};
  sc_signal<bool> host_rden{"host_rden"};
  sc_signal<host_data_t> host_wdata{"host_wdata"};
  sc_signal<host_mask_t> host_wmask{"host_wmask"};
  sc_signal<host_data_t> host_rdata{"host_rdata"};

  sc_signal<float> threshold{"threshold"};
  sc_signal<sc_bv<3>> select{"select"};

  // Instantiate NpuTop with zero-gating enabled
  PeConfig pe_cfg;
  pe_cfg.arithmetic_type = 1;
  pe_cfg.mul_type = 0;
  pe_cfg.add_type = 0;
  pe_cfg.stages_mul = 1;
  pe_cfg.intermediate_pipeline_stage = true;
  pe_cfg.zero_gating_mult = true;

  NpuTop<32, 32, float, float, float, 1024, 1024, 2048, 16, 64, 1> npu(
      "NpuTop_inst", pe_cfg);
  TestbenchStandalone tb("TestbenchStandalone_inst");

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
