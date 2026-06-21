// Copyright 2026 Barcelona Supercomputing Center (BSC)
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
//
// SystemC Model for SAURIA NPU Core
// Activation (IFmap) Feeder Block with Parameterized FIFO Depth

#ifndef SAURIA_IFMAP_FEEDER_H
#define SAURIA_IFMAP_FEEDER_H

#include "sauria_types.h"
#include <queue>
#include <vector>

namespace sauria {

    template <
        int Y_DIM = 32,
        typename T_ACT = float,
        int SRAMA_CAP = 1024,
        int FIFO_DEPTH = 16
    >
    class IfmapFeeder : public sc_module {
    public:
        // Clock & Reset
        sc_in<bool> i_clk{"i_clk"};
        sc_in<bool> i_rstn{"i_rstn"};

        // Control Inputs from Global FSM
        sc_in<bool> i_feeder_en{"i_feeder_en"};
        sc_in<bool> i_feeder_clear{"i_feeder_clear"};
        sc_in<bool> i_start{"i_start"};
        sc_in<bool> i_valid{"i_valid"};
        sc_in<bool> i_finalpush{"i_finalpush"};
        sc_in<bool> i_cnt_en{"i_cnt_en"};
        sc_in<bool> i_cnt_clear{"i_cnt_clear"};
        sc_in<bool> i_clearfifo{"i_clearfifo"};
        sc_in<bool> i_pop_en{"i_pop_en"};
        sc_in<bool> i_finalctx{"i_finalctx"};

        // Config Parameters
        sc_in<uint32_t> i_act_incntlim{"i_act_incntlim"};
        sc_in<uint32_t> i_act_incntstep{"i_act_incntstep"};
        sc_in<uint32_t> i_act_outcntlim{"i_act_outcntlim"};
        sc_in<uint32_t> i_act_outcntstep{"i_act_outcntstep"};
        sc_in<sc_bv<DILP_W>> i_act_dil_pat{"i_act_dil_pat"};

        // Memory Interface to SRAM A
        sc_out<uint32_t>                 o_srama_addr{"o_srama_addr"};
        sc_out<bool>                     o_srama_rden{"o_srama_rden"};
        sc_in<act_vector_t<Y_DIM, T_ACT>> i_srama_data{"i_srama_data"};

        // Wavefront Output Vector towards Systolic Array (A ports)
        sc_out<act_vector_t<Y_DIM, T_ACT>> o_act_arr{"o_act_arr"};

        // Feeder Status Outputs
        sc_out<bool> o_act_done{"o_act_done"};
        sc_out<bool> o_act_til_done{"o_act_til_done"};
        sc_out<bool> o_fifo_empty{"o_fifo_empty"};
        sc_out<bool> o_fifo_full{"o_fifo_full"};
        sc_out<bool> o_stall{"o_stall"};

        SC_CTOR(IfmapFeeder) {
            SC_METHOD(feeder_process);
            sensitive << i_clk.pos();
        }

    private:
        // Internal FIFOs for each of the Y rows
        std::queue<T_ACT> row_fifos[Y_DIM];
        
        // Dynamic skew delay registers for systolic wavefront (A-side)
        std::vector<T_ACT> skew_regs[Y_DIM];

        // Address tracking register
        uint32_t addr_reg{0};

        // SRAM read latency matching registers
        bool rden_q{false};
        bool rdata_valid{false};

        void feeder_process() {
            if (!i_rstn.read() || i_feeder_clear.read() || i_clearfifo.read()) {
                o_srama_addr.write(0);
                o_srama_rden.write(false);
                o_act_arr.write(act_vector_t<Y_DIM, T_ACT>(static_cast<T_ACT>(0)));
                o_act_done.write(false);
                o_act_til_done.write(false);
                o_fifo_empty.write(true);
                o_fifo_full.write(false);
                o_stall.write(false);
                addr_reg = 0;
                rden_q = false;
                rdata_valid = false;
                for (int i = 0; i < Y_DIM; i++) {
                    while (!row_fifos[i].empty()) row_fifos[i].pop();
                    skew_regs[i].assign(i, static_cast<T_ACT>(0)); // Delay length matches row index i
                }
                return;
            }

            if (!i_feeder_en.read()) return;

            // 1. Fetch activation vector from SRAM A into FIFOs
            if (rdata_valid) {
                // Read from memory and push into FIFOs (contains valid data from previous cycle's read)
                act_vector_t<Y_DIM, T_ACT> mem_data = i_srama_data.read();
                for (int y = 0; y < Y_DIM; y++) {
                    row_fifos[y].push(mem_data[y]);
                }
            }

            if (i_cnt_en.read()) {
                o_srama_rden.write(true);
                o_srama_addr.write(addr_reg);
                addr_reg += i_act_incntstep.read();
                rdata_valid = rden_q;
                rden_q = true;
            } else {
                o_srama_rden.write(false);
                rdata_valid = rden_q;
                rden_q = false;
            }

            // 2. Pop and Shift Activations (Wavefront Skew Lines)
            if (i_pop_en.read()) {
                act_vector_t<Y_DIM, T_ACT> act_out;
                for (int y = 0; y < Y_DIM; y++) {
                    T_ACT popped = static_cast<T_ACT>(0);
                    if (!row_fifos[y].empty()) {
                        popped = row_fifos[y].front();
                        row_fifos[y].pop();
                    }

                    // Shift wavefront skew register line
                    skew_regs[y].push_back(popped);
                    
                    // Wavefront output is the front of the delay line (length y)
                    T_ACT out_val = (y == 0) ? popped : skew_regs[y].front();
                    
                    if (y > 0) {
                        skew_regs[y].erase(skew_regs[y].begin());
                    }
                    
                    act_out[y] = out_val;
                }
                o_act_arr.write(act_out);
            } else {
                o_act_arr.write(act_vector_t<Y_DIM, T_ACT>(static_cast<T_ACT>(0)));
            }

            // 3. Update status flags
            bool empty = row_fifos[0].empty();
            bool full  = row_fifos[0].size() >= FIFO_DEPTH;
            o_fifo_empty.write(empty);
            o_fifo_full.write(full);
        }
    };

} // namespace sauria

#endif // SAURIA_IFMAP_FEEDER_H
