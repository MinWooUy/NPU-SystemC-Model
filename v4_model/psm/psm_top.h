// Copyright 2026 Barcelona Supercomputing Center (BSC)
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
//
// SystemC Model for SAURIA NPU Core
// Partial Sum Memory (PSM) Block with Parameterized Dimensions

#ifndef SAURIA_PSM_TOP_H
#define SAURIA_PSM_TOP_H

#include "sauria_types.h"

namespace sauria {

    template <
        int X_DIM = 32,
        int Y_DIM = 32,
        typename T_PSUM = float,
        int SRAMC_CAP = 2048
    >
    class Psm : public sc_module {
    public:
        // Clock & Reset
        sc_in<bool> i_clk{"i_clk"};
        sc_in<bool> i_rstn{"i_rstn"};

        // Data Inputs from Array Scan Chain
        sc_in<psum_vector_t<Y_DIM, T_PSUM>> i_c_arr{"i_c_arr"};        // Data entering from leftmost PE column

        // Memory Interface to SRAM C
        sc_in<psum_vector_t<Y_DIM, T_PSUM>>   i_sramc_rdata{"i_sramc_rdata"};
        sc_out<uint32_t>                      o_sramc_addr{"o_sramc_addr"};
        sc_out<bool>                          o_sramc_wren{"o_sramc_wren"};
        sc_out<bool>                          o_sramc_rden{"o_sramc_rden"};
        sc_out<sramc_mask_t<Y_DIM>>           o_sramc_wmask{"o_sramc_wmask"};
        sc_out<psum_vector_t<Y_DIM, T_PSUM>>  o_sramc_wdata{"o_sramc_wdata"};

        // Config Parameters
        sc_in<uint32_t>           i_cxlim{"i_cxlim"};
        sc_in<uint32_t>           i_cxstep{"i_cxstep"};
        sc_in<uint32_t>           i_cklim{"i_cklim"};
        sc_in<uint32_t>           i_ckstep{"i_ckstep"};
        sc_in<uint32_t>           i_til_cylim{"i_til_cylim"};
        sc_in<uint32_t>           i_til_cystep{"i_til_cystep"};
        sc_in<uint32_t>           i_til_cklim{"i_til_cklim"};
        sc_in<uint32_t>           i_til_ckstep{"i_til_ckstep"};
        sc_in<uint32_t>           i_ncontexts{"i_ncontexts"};
        sc_in<bool>               i_preload_en{"i_preload_en"};
        sc_in<sramc_mask_t<Y_DIM>> i_rows_active{"i_rows_active"}; // Active Rows config (Y bits)

        // Control Inputs from global FSM
        sc_in<bool> i_fsm_start{"i_fsm_start"};
        sc_in<bool> i_fsm_reset{"i_fsm_reset"};
        sc_in<bool> i_pipeline_en{"i_pipeline_en"};

        // Control Outputs to Global FSM / Array
        sc_out<bool> o_done{"o_done"};
        sc_out<bool> o_finalwrite{"o_finalwrite"};
        sc_out<bool> o_shift_done{"o_shift_done"};
        sc_out<bool> o_cscan_en{"o_cscan_en"};           // Directs array to shift out C chain

        // Data Outputs to Array (Preload values sent right-to-left)
        sc_out<psum_vector_t<Y_DIM, T_PSUM>> o_c_arr{"o_c_arr"};

        SC_CTOR(Psm) {
            SC_METHOD(psm_process);
            sensitive << i_clk.pos();
        }

    private:
        uint32_t addr_reg{0};
        uint32_t shift_cnt{0};
        uint32_t delay_cnt{0};
        bool shifting{false};
        bool preload_mode{false};

        void psm_process() {
            if (!i_rstn.read() || i_fsm_reset.read()) {
                o_sramc_addr.write(0);
                o_sramc_wren.write(false);
                o_sramc_rden.write(false);
                o_sramc_wmask.write(sramc_mask_t<Y_DIM>());
                o_sramc_wdata.write(psum_vector_t<Y_DIM, T_PSUM>());
                o_done.write(false);
                o_finalwrite.write(false);
                o_shift_done.write(false);
                o_cscan_en.write(false);
                o_c_arr.write(psum_vector_t<Y_DIM, T_PSUM>());
                addr_reg = 0;
                shift_cnt = 0;
                delay_cnt = 0;
                shifting = false;
                preload_mode = false;
                return;
            }

            if (i_fsm_start.read() && !shifting) {
                shifting = true;
                shift_cnt = 0;
                addr_reg = 0;
                delay_cnt = 0;
                preload_mode = i_preload_en.read();
                o_cscan_en.write(true);
                o_done.write(false);
                o_shift_done.write(false);
                
                if (preload_mode) {
                    o_sramc_rden.write(true);
                    o_sramc_addr.write(0);
                    o_sramc_wren.write(false);
                } else {
                    o_sramc_rden.write(false);
                    o_sramc_wren.write(false);
                }
                return;
            }

            if (shifting) {
                if (preload_mode) {
                    if (delay_cnt == 0) {
                        delay_cnt = 1;
                        addr_reg = 1;
                        o_sramc_addr.write(addr_reg);
                        o_sramc_rden.write(true);
                    } else {
                        psum_vector_t<Y_DIM, T_PSUM> mem_data = i_sramc_rdata.read();
                        o_c_arr.write(mem_data);

                        addr_reg++;
                        shift_cnt++;

                        if (shift_cnt < X_DIM) {
                            o_sramc_addr.write(addr_reg);
                            o_sramc_rden.write(true);
                        } else {
                            shifting = false;
                            o_cscan_en.write(false);
                            o_sramc_rden.write(false);
                            o_shift_done.write(true);
                            o_done.write(true);
                        }
                    }
                } else {
                    if (delay_cnt == 0) {
                        delay_cnt = 1;
                    } else {
                        if (shift_cnt < X_DIM) {
                            psum_vector_t<Y_DIM, T_PSUM> array_out = i_c_arr.read();
                            
                            o_sramc_wren.write(true);
                            o_sramc_addr.write(addr_reg);
                            o_sramc_wmask.write(i_rows_active.read());
                            o_sramc_wdata.write(array_out);

                            addr_reg++;
                            shift_cnt++;
                        } else {
                            shifting = false;
                            o_cscan_en.write(false);
                            o_sramc_wren.write(false);
                            o_shift_done.write(true);
                            o_done.write(true);
                        }
                    }
                }
            } else {
                o_sramc_wren.write(false);
                o_sramc_rden.write(false);
            }
        }
    };

} // namespace sauria

#endif // SAURIA_PSM_TOP_H
