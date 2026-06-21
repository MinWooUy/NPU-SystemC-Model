// Copyright 2026 Barcelona Supercomputing Center (BSC)
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
//
// SystemC Model for SAURIA NPU Core
// Systolic Array 2D Grid Block with configurable hardware parameters

#ifndef SAURIA_SA_ARRAY_H
#define SAURIA_SA_ARRAY_H

#include "sauria_types.h"
#include "sa_processing_element.h"
#include <vector>

namespace sauria {

    template <
        int X_DIM = 32,
        int Y_DIM = 32,
        typename T_ACT = float,
        typename T_WEI = float,
        typename T_PSUM = float
    >
    class SystolicArray : public sc_module {
    public:
        // Clock & Reset
        sc_in<bool> i_clk{"i_clk"};
        sc_in<bool> i_rstn{"i_rstn"};

        // Dynamic threshold for zero-detection/negligence
        sc_in<float> i_threshold{"i_threshold"};

        // Wavefront inputs from feeders
        sc_in<act_vector_t<Y_DIM, T_ACT>> i_act_arr{"i_act_arr"};     // Size Y
        sc_in<wei_vector_t<X_DIM, T_WEI>> i_wei_arr{"i_wei_arr"};     // Size X

        // Scan-chain input/output connections with PSM (Right-to-Left chain)
        sc_in<psum_vector_t<Y_DIM, T_PSUM>>  i_c_arr{"i_c_arr"};       // Size Y
        sc_out<psum_vector_t<Y_DIM, T_PSUM>> o_c_arr{"o_c_arr"};       // Size Y

        // Control Inputs
        sc_in<bool>         i_pipeline_en{"i_pipeline_en"};
        sc_in<bool>         i_cscan_en{"i_cscan_en"};
        sc_in<sc_bv<X_DIM>> i_cswitch_arr{"i_cswitch_arr"}; // Precise delays per row
        sc_in<bool>         i_sa_clear{"i_sa_clear"};

        PeConfig config;

        // Custom Constructor
        SC_HAS_PROCESS(SystolicArray);
        SystolicArray(sc_module_name nm, const PeConfig& cfg = PeConfig())
            : sc_module(nm), config(cfg) {
            
            // Apply configuration to all processing elements in grid
            for (int y = 0; y < Y_DIM; y++) {
                for (int x = 0; x < X_DIM; x++) {
                    grid[y][x].config = config;
                }
            }

            SC_METHOD(grid_process);
            sensitive << i_clk.pos();
        }

        void grid_process() {
            if (!i_rstn.read() || i_sa_clear.read()) {
                o_c_arr.write(psum_vector_t<Y_DIM, T_PSUM>());
                for (int y = 0; y < Y_DIM; y++) {
                    for (int x = 0; x < X_DIM; x++) {
                        grid[y][x].reset();
                        int delay_len = y + x + 1 + (config.extra_csreg ? 1 : 0);
                        cs_delay[y][x].assign(delay_len, false);
                    }
                }
                return;
            }

            if (!i_pipeline_en.read()) return;

            act_vector_t<Y_DIM, T_ACT>  act_in  = i_act_arr.read();
            wei_vector_t<X_DIM, T_WEI>  wei_in  = i_wei_arr.read();
            psum_vector_t<Y_DIM, T_PSUM> scan_in = i_c_arr.read();
            sc_bv<X_DIM>                cswitch = i_cswitch_arr.read();

            // 1. Capture current pipeline state (from previous cycle)
            static T_ACT prev_a[Y_DIM][X_DIM];
            static T_WEI prev_b[Y_DIM][X_DIM];
            static T_PSUM prev_sc[Y_DIM][X_DIM];

            for (int y = 0; y < Y_DIM; y++) {
                for (int x = 0; x < X_DIM; x++) {
                    prev_a[y][x]  = grid[y][x].a_q;
                    prev_b[y][x]  = grid[y][x].b_q;
                    prev_sc[y][x] = grid[y][x].mac_sc_q;
                }
            }

            psum_vector_t<Y_DIM, T_PSUM> scan_out;

            // 2. Step execution with pipeline registered values
            for (int y = 0; y < Y_DIM; y++) {
                for (int x = 0; x < X_DIM; x++) {
                    // Determine activation input (A-port, registered horizontally)
                    T_ACT a_val = (x == 0) ? act_in[y] : prev_a[y][x - 1];

                    // Determine weight input (B-port, registered vertically)
                    T_WEI b_val = (y == 0) ? wei_in[x] : prev_b[y - 1][x];

                    // Determine scan-chain input (C-port, registered right-to-left)
                    T_PSUM c_val = (x == X_DIM - 1) ? scan_in[y] : prev_sc[y][x + 1];

                    // Cycle-accurate context-switch staggering delay
                    bool cs_in = cswitch[x].to_bool();
                    cs_delay[y][x].push_back(cs_in);
                    
                    bool cs_bit = cs_delay[y][x].front();
                    cs_delay[y][x].erase(cs_delay[y][x].begin());

                    // Step execution
                    grid[y][x].step(a_val, b_val, c_val, cs_bit, 
                                    i_cscan_en.read(), i_pipeline_en.read(), 
                                    i_threshold.read());

                    // Output of leftmost column shifts out to PSM/Outputs
                    if (x == 0) {
                        scan_out[y] = prev_sc[y][0];
                    }
                }
            }

            o_c_arr.write(scan_out);
        }

        // Direct read access to PE accumulator state for standalone testing verification
        T_PSUM get_pe_mac(int y, int x) const {
            return grid[y][x].mac_q;
        }

        T_PSUM get_pe_mac_sc(int y, int x) const {
            return grid[y][x].mac_sc_q;
        }

    private:
        // 2D matrix of processing elements
        ProcessingElement<T_ACT, T_WEI, T_PSUM> grid[Y_DIM][X_DIM];

        // 2D context switch delay buffers
        std::vector<bool> cs_delay[Y_DIM][X_DIM];
    };

} // namespace sauria

#endif // SAURIA_SA_ARRAY_H
