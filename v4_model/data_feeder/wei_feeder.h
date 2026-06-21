// Copyright 2026 Barcelona Supercomputing Center (BSC)
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
//
// SystemC Model for SAURIA NPU Core
// Weight Feeder Block with Parameterized FIFO Depth

#ifndef SAURIA_WEI_FEEDER_H
#define SAURIA_WEI_FEEDER_H

#include "sauria_types.h"
#include <queue>

namespace sauria {

    template <
        int X_DIM = 32,
        typename T_WEI = float,
        int SRAMB_CAP = 1024,
        int FIFO_DEPTH = 16
    >
    class WeightFeeder : public sc_module {
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
        sc_in<bool> i_cswitch{"i_cswitch"};

        // Config Parameters
        sc_in<uint32_t> i_wei_incntlim{"i_wei_incntlim"};
        sc_in<uint32_t> i_wei_incntstep{"i_wei_incntstep"};

        // Memory Interface to SRAM B
        sc_out<uint32_t>                 o_sramb_addr{"o_sramb_addr"};
        sc_out<bool>                     o_sramb_rden{"o_sramb_rden"};
        sc_in<wei_vector_t<X_DIM, T_WEI>> i_sramb_data{"i_sramb_data"};

        // Wavefront Output Vector towards Systolic Array (B ports)
        sc_out<wei_vector_t<X_DIM, T_WEI>> o_wei_arr{"o_wei_arr"};

        // Feeder Status Outputs
        sc_out<bool> o_wei_done{"o_wei_done"};
        sc_out<bool> o_wei_til_done{"o_wei_til_done"};
        sc_out<bool> o_fifo_empty{"o_fifo_empty"};
        sc_out<bool> o_fifo_full{"o_fifo_full"};
        sc_out<bool> o_stall{"o_stall"};

        SC_CTOR(WeightFeeder) {
            SC_METHOD(feeder_process);
            sensitive << i_clk.pos();
        }

    private:
        // Weight queues for each of the X columns
        std::queue<T_WEI> col_fifos[X_DIM];

        // Address tracking register
        uint32_t addr_reg{0};

        // SRAM read latency matching registers
        bool rden_q{false};
        bool rdata_valid{false};

        void feeder_process() {
            if (!i_rstn.read() || i_feeder_clear.read() || i_clearfifo.read()) {
                o_sramb_addr.write(0);
                o_sramb_rden.write(false);
                o_wei_arr.write(wei_vector_t<X_DIM, T_WEI>(static_cast<T_WEI>(0)));
                o_wei_done.write(false);
                o_wei_til_done.write(false);
                o_fifo_empty.write(true);
                o_fifo_full.write(false);
                o_stall.write(false);
                addr_reg = 0;
                rden_q = false;
                rdata_valid = false;
                for (int i = 0; i < X_DIM; i++) {
                    while (!col_fifos[i].empty()) col_fifos[i].pop();
                }
                return;
            }

            if (!i_feeder_en.read()) return;

            // 1. Fetch weight vector from SRAM B into FIFOs
            if (rdata_valid) {
                // Read from memory and push into FIFOs (contains valid data from previous cycle's read)
                wei_vector_t<X_DIM, T_WEI> mem_data = i_sramb_data.read();
                for (int x = 0; x < X_DIM; x++) {
                    col_fifos[x].push(mem_data[x]);
                }
            }

            if (i_cnt_en.read()) {
                o_sramb_rden.write(true);
                o_sramb_addr.write(addr_reg);
                addr_reg += i_wei_incntstep.read();
                rdata_valid = rden_q;
                rden_q = true;
            } else {
                o_sramb_rden.write(false);
                rdata_valid = rden_q;
                rden_q = false;
            }

            // 2. Pop weights to Systolic Array (No wavefront skew necessary on weight feeder)
            if (i_pop_en.read()) {
                wei_vector_t<X_DIM, T_WEI> wei_out;
                for (int x = 0; x < X_DIM; x++) {
                    T_WEI popped = static_cast<T_WEI>(0);
                    if (!col_fifos[x].empty()) {
                        popped = col_fifos[x].front();
                        col_fifos[x].pop();
                    }
                    wei_out[x] = popped;
                }
                o_wei_arr.write(wei_out);
            } else {
                o_wei_arr.write(wei_vector_t<X_DIM, T_WEI>(static_cast<T_WEI>(0)));
            }

            // 3. Update status flags
            bool empty = col_fifos[0].empty();
            bool full  = col_fifos[0].size() >= FIFO_DEPTH;
            o_fifo_empty.write(empty);
            o_fifo_full.write(full);
        }
    };

} // namespace sauria

#endif // SAURIA_WEI_FEEDER_H
