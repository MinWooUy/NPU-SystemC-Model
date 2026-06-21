// Copyright 2026 Barcelona Supercomputing Center (BSC)
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
//
// SystemC Model for SAURIA NPU Core
// Configuration Registers Block (config_regs.h)

#ifndef SAURIA_CONFIG_REGS_H
#define SAURIA_CONFIG_REGS_H

#include "sauria_types.h"

namespace sauria {

    template <
        int IF_W_PARAM = 32,
        int IF_ADR_W_PARAM = 32,
        int X_DIM = 32,
        int Y_DIM = 32,
        int TH_W_PARAM = 2,
        int ACT_IDX_W_PARAM = 15,
        int WEI_IDX_W_PARAM = 15,
        int OUT_IDX_W_PARAM = 15,
        int PARAMS_W_PARAM = 8,
        int DILP_W_PARAM = 64,
        int SRAMB_N_PARAM = 8
    >
    class ConfigRegs : public sc_module {
    public:
        // Clock & Reset
        sc_in<bool> i_clk{"i_clk"};
        sc_in<bool> i_rstn{"i_rstn"};

        // Host Config/AXI interface
        sc_in<uint32_t>     i_host_addr{"i_host_addr"};
        sc_in<bool>         i_host_wren{"i_host_wren"};
        sc_in<bool>         i_host_rden{"i_host_rden"};
        sc_in<host_data_t>  i_host_wdata{"i_host_wdata"};
        sc_in<host_mask_t>  i_host_wmask{"i_host_wmask"};
        sc_out<host_data_t> o_host_rdata{"o_host_rdata"};

        // Status inputs from NPU Core
        sc_in<bool>         i_done{"i_done"};
        sc_in<bool>         i_soft_reset_in{"i_soft_reset_in"};

        // Handshake and Reset Control Outputs
        sc_out<bool>        o_start{"o_start"};
        sc_out<bool>        o_soft_reset{"o_soft_reset"};

        // Outputs to NPU modules
        sc_out<uint32_t>                 o_incntlim{"o_incntlim"};
        sc_out<uint32_t>                 o_act_reps{"o_act_reps"};
        sc_out<uint32_t>                 o_wei_reps{"o_wei_reps"};
        sc_out<sc_bv<DILP_W_PARAM>>      o_dil_pat{"o_dil_pat"};
        sc_out<sramc_mask_t<Y_DIM>>      o_rows_active{"o_rows_active"};

        // Simple Option B Parameters
        sc_out<uint32_t>                 o_act_incntlim{"o_act_incntlim"};
        sc_out<uint32_t>                 o_act_incntstep{"o_act_incntstep"};
        sc_out<uint32_t>                 o_act_outcntlim{"o_act_outcntlim"};
        sc_out<uint32_t>                 o_act_outcntstep{"o_act_outcntstep"};
        sc_out<uint32_t>                 o_wei_incntlim{"o_wei_incntlim"};
        sc_out<uint32_t>                 o_wei_incntstep{"o_wei_incntstep"};
        sc_out<uint32_t>                 o_cxlim{"o_cxlim"};
        sc_out<uint32_t>                 o_cxstep{"o_cxstep"};
        sc_out<uint32_t>                 o_cklim{"o_cklim"};
        sc_out<uint32_t>                 o_ckstep{"o_ckstep"};
        sc_out<uint32_t>                 o_til_cylim{"o_til_cylim"};
        sc_out<uint32_t>                 o_til_cystep{"o_til_cystep"};
        sc_out<uint32_t>                 o_til_cklim{"o_til_cklim"};
        sc_out<uint32_t>                 o_til_ckstep{"o_til_ckstep"};
        sc_out<uint32_t>                 o_ncontexts{"o_ncontexts"};
        sc_out<bool>                     o_preload_en{"o_preload_en"};

        SC_CTOR(ConfigRegs) {
            SC_METHOD(reg_process);
            sensitive << i_clk.pos();
        }

    private:
        // Internal Register State
        uint32_t r_incntlim{96};
        uint32_t r_act_reps{64};
        uint32_t r_wei_reps{64};
        sc_bv<DILP_W_PARAM> r_dil_pat{1};
        sramc_mask_t<Y_DIM> r_rows_active{true};

        // Simple Option B Internal State
        uint32_t r_act_incntlim{96};
        uint32_t r_act_incntstep{1};
        uint32_t r_act_outcntlim{96};
        uint32_t r_act_outcntstep{1};
        uint32_t r_wei_incntlim{96};
        uint32_t r_wei_incntstep{1};
        uint32_t r_cxlim{96};
        uint32_t r_cxstep{1};
        uint32_t r_cklim{96};
        uint32_t r_ckstep{1};
        uint32_t r_til_cylim{96};
        uint32_t r_til_cystep{1};
        uint32_t r_til_cklim{96};
        uint32_t r_til_ckstep{1};
        uint32_t r_ncontexts{1};
        bool     r_preload_en{false};

        // Control flags
        bool r_start{false};
        bool r_done{false};
        bool r_soft_reset{false};

        void reg_process() {
            if (!i_rstn.read() || i_soft_reset_in.read()) {
                r_incntlim = 96;
                r_act_reps = 64;
                r_wei_reps = 64;
                r_dil_pat = 1;
                r_rows_active = sramc_mask_t<Y_DIM>(true);

                r_act_incntlim = 96;
                r_act_incntstep = 1;
                r_act_outcntlim = 96;
                r_act_outcntstep = 1;
                r_wei_incntlim = 96;
                r_wei_incntstep = 1;
                r_cxlim = 96;
                r_cxstep = 1;
                r_cklim = 96;
                r_ckstep = 1;
                r_til_cylim = 96;
                r_til_cystep = 1;
                r_til_cklim = 96;
                r_til_ckstep = 1;
                r_ncontexts = 1;
                r_preload_en = false;

                r_start = false;
                r_done = false;
                r_soft_reset = false;

                o_start.write(false);
                o_soft_reset.write(false);
                o_incntlim.write(r_incntlim);
                o_act_reps.write(r_act_reps);
                o_wei_reps.write(r_wei_reps);
                o_dil_pat.write(r_dil_pat);
                o_rows_active.write(r_rows_active);

                o_act_incntlim.write(r_act_incntlim);
                o_act_incntstep.write(r_act_incntstep);
                o_act_outcntlim.write(r_act_outcntlim);
                o_act_outcntstep.write(r_act_outcntstep);
                o_wei_incntlim.write(r_wei_incntlim);
                o_wei_incntstep.write(r_wei_incntstep);
                o_cxlim.write(r_cxlim);
                o_cxstep.write(r_cxstep);
                o_cklim.write(r_cklim);
                o_ckstep.write(r_ckstep);
                o_til_cylim.write(r_til_cylim);
                o_til_cystep.write(r_til_cystep);
                o_til_cklim.write(r_til_cklim);
                o_til_ckstep.write(r_til_ckstep);
                o_ncontexts.write(r_ncontexts);
                o_preload_en.write(r_preload_en);

                o_host_rdata.write(host_data_t());
                return;
            }

            // Sync from FSM done status
            if (i_done.read()) {
                r_done = true;
            }

            // Default auto-clearing signals
            if (r_start) {
                r_start = false;
            }
            if (r_soft_reset) {
                r_soft_reset = false;
            }

            // Address decoding
            uint32_t addr = i_host_addr.read();
            uint32_t mem_region = addr & SAURIA_MEM_ADDR_MASK;
            uint32_t local_addr = addr & ~SAURIA_MEM_ADDR_MASK;

            if (mem_region == CFG_REGS_OFFSET) {
                // Host Writes
                if (i_host_wren.read()) {
                    host_data_t wdata = i_host_wdata.read();
                    host_mask_t wmask = i_host_wmask.read();

                    if (local_addr == 0x00) {
                        // Control register
                        if (wmask[0]) {
                            r_start = (wdata[0] != 0.0f);
                        }
                        // COW on done
                        if (wmask[0] && (wdata[0] == 0.0f)) {
                            r_done = false;
                        }
                        // Soft reset
                        if (wmask[2]) { // bit 16-23 maps to third byte wmask[2]
                            r_soft_reset = (wdata[2] != 0.0f);
                        }
                    } else if (local_addr == CFG_CON_OFFSET + 0x00) {
                        if (wmask[0]) r_incntlim = (uint32_t)wdata[0];
                    } else if (local_addr == CFG_CON_OFFSET + 0x04) {
                        if (wmask[0]) r_act_reps = (uint32_t)wdata[0];
                    } else if (local_addr == CFG_CON_OFFSET + 0x08) {
                        if (wmask[0]) r_wei_reps = (uint32_t)wdata[0];
                    } else if (local_addr == CFG_CON_OFFSET + 0x0C) {
                        if (wmask[0]) r_ncontexts = (uint32_t)wdata[0];
                    } else if (local_addr == CFG_CON_OFFSET + 0x10) {
                        if (wmask[0]) r_preload_en = (wdata[0] != 0.0f);
                    } else if (local_addr == CFG_ACT_OFFSET + 0x00) {
                        // Rows active mask
                        for (int i = 0; i < Y_DIM; i++) {
                            int byte_idx = i / 8;
                            int bit_idx = i % 8;
                            if (byte_idx < 4 && wmask[byte_idx]) {
                                // Extract byte from wdata[byte_idx]
                                uint32_t val = (uint32_t)wdata[byte_idx];
                                r_rows_active[i] = (val & (1 << bit_idx)) != 0;
                            }
                        }
                    } else if (local_addr == CFG_ACT_OFFSET + 0x04) {
                        if (wmask[0]) r_act_incntlim = (uint32_t)wdata[0];
                    } else if (local_addr == CFG_ACT_OFFSET + 0x08) {
                        if (wmask[0]) r_act_incntstep = (uint32_t)wdata[0];
                    } else if (local_addr == CFG_ACT_OFFSET + 0x0C) {
                        if (wmask[0]) r_act_outcntlim = (uint32_t)wdata[0];
                    } else if (local_addr == CFG_ACT_OFFSET + 0x10) {
                        if (wmask[0]) r_act_outcntstep = (uint32_t)wdata[0];
                    } else if (local_addr == CFG_ACT_OFFSET + 0x28) { // 0x28 is Reg 10 for dilation
                        if (wmask[0]) r_dil_pat = (uint32_t)wdata[0];
                    } else if (local_addr == CFG_WEI_OFFSET + 0x00) {
                        if (wmask[0]) r_wei_incntlim = (uint32_t)wdata[0];
                    } else if (local_addr == CFG_WEI_OFFSET + 0x04) {
                        if (wmask[0]) r_wei_incntstep = (uint32_t)wdata[0];
                    } else if (local_addr == CFG_OUT_OFFSET + 0x00) {
                        if (wmask[0]) r_cxlim = (uint32_t)wdata[0];
                    } else if (local_addr == CFG_OUT_OFFSET + 0x04) {
                        if (wmask[0]) r_cxstep = (uint32_t)wdata[0];
                    } else if (local_addr == CFG_OUT_OFFSET + 0x08) {
                        if (wmask[0]) r_cklim = (uint32_t)wdata[0];
                    } else if (local_addr == CFG_OUT_OFFSET + 0x0C) {
                        if (wmask[0]) r_ckstep = (uint32_t)wdata[0];
                    } else if (local_addr == CFG_OUT_OFFSET + 0x10) {
                        if (wmask[0]) r_til_cylim = (uint32_t)wdata[0];
                    } else if (local_addr == CFG_OUT_OFFSET + 0x14) {
                        if (wmask[0]) r_til_cystep = (uint32_t)wdata[0];
                    } else if (local_addr == CFG_OUT_OFFSET + 0x18) {
                        if (wmask[0]) r_til_cklim = (uint32_t)wdata[0];
                    } else if (local_addr == CFG_OUT_OFFSET + 0x1C) {
                        if (wmask[0]) r_til_ckstep = (uint32_t)wdata[0];
                    }
                }

                // Host Reads
                if (i_host_rden.read()) {
                    host_data_t rdata;
                    if (local_addr == 0x00) {
                        rdata[0] = r_start ? 1.0f : 0.0f;
                        rdata[0] += r_done ? 2.0f : 0.0f;
                        rdata[0] += (!r_done) ? 4.0f : 0.0f; // idle
                        rdata[0] += (!r_done) ? 8.0f : 0.0f; // ready
                        rdata[2] = r_soft_reset ? 128.0f : 0.0f;
                    } else if (local_addr == CFG_CON_OFFSET + 0x00) {
                        rdata[0] = (float)r_incntlim;
                    } else if (local_addr == CFG_CON_OFFSET + 0x04) {
                        rdata[0] = (float)r_act_reps;
                    } else if (local_addr == CFG_CON_OFFSET + 0x08) {
                        rdata[0] = (float)r_wei_reps;
                    } else if (local_addr == CFG_CON_OFFSET + 0x0C) {
                        rdata[0] = (float)r_ncontexts;
                    } else if (local_addr == CFG_CON_OFFSET + 0x10) {
                        rdata[0] = r_preload_en ? 1.0f : 0.0f;
                    } else if (local_addr == CFG_ACT_OFFSET + 0x00) {
                        for (int b = 0; b < 4; b++) {
                            uint32_t val = 0;
                            for (int bit = 0; bit < 8; bit++) {
                                int idx = b * 8 + bit;
                                if (idx < Y_DIM && r_rows_active[idx]) {
                                    val |= (1 << bit);
                                }
                            }
                            rdata[b] = (float)val;
                        }
                    } else if (local_addr == CFG_ACT_OFFSET + 0x04) {
                        rdata[0] = (float)r_act_incntlim;
                    } else if (local_addr == CFG_ACT_OFFSET + 0x08) {
                        rdata[0] = (float)r_act_incntstep;
                    } else if (local_addr == CFG_ACT_OFFSET + 0x0C) {
                        rdata[0] = (float)r_act_outcntlim;
                    } else if (local_addr == CFG_ACT_OFFSET + 0x10) {
                        rdata[0] = (float)r_act_outcntstep;
                    } else if (local_addr == CFG_ACT_OFFSET + 0x28) {
                        rdata[0] = (float)r_dil_pat.to_uint();
                    } else if (local_addr == CFG_WEI_OFFSET + 0x00) {
                        rdata[0] = (float)r_wei_incntlim;
                    } else if (local_addr == CFG_WEI_OFFSET + 0x04) {
                        rdata[0] = (float)r_wei_incntstep;
                    } else if (local_addr == CFG_OUT_OFFSET + 0x00) {
                        rdata[0] = (float)r_cxlim;
                    } else if (local_addr == CFG_OUT_OFFSET + 0x04) {
                        rdata[0] = (float)r_cxstep;
                    } else if (local_addr == CFG_OUT_OFFSET + 0x08) {
                        rdata[0] = (float)r_cklim;
                    } else if (local_addr == CFG_OUT_OFFSET + 0x0C) {
                        rdata[0] = (float)r_ckstep;
                    } else if (local_addr == CFG_OUT_OFFSET + 0x10) {
                        rdata[0] = (float)r_til_cylim;
                    } else if (local_addr == CFG_OUT_OFFSET + 0x14) {
                        rdata[0] = (float)r_til_cystep;
                    } else if (local_addr == CFG_OUT_OFFSET + 0x18) {
                        rdata[0] = (float)r_til_cklim;
                    } else if (local_addr == CFG_OUT_OFFSET + 0x1C) {
                        rdata[0] = (float)r_til_ckstep;
                    }
                    o_host_rdata.write(rdata);
                }
            }

            // Update output ports
            o_start.write(r_start);
            o_soft_reset.write(r_soft_reset);
            o_incntlim.write(r_incntlim);
            o_act_reps.write(r_act_reps);
            o_wei_reps.write(r_wei_reps);
            o_dil_pat.write(r_dil_pat);
            o_rows_active.write(r_rows_active);

            o_act_incntlim.write(r_act_incntlim);
            o_act_incntstep.write(r_act_incntstep);
            o_act_outcntlim.write(r_act_outcntlim);
            o_act_outcntstep.write(r_act_outcntstep);
            o_wei_incntlim.write(r_wei_incntlim);
            o_wei_incntstep.write(r_wei_incntstep);
            o_cxlim.write(r_cxlim);
            o_cxstep.write(r_cxstep);
            o_cklim.write(r_cklim);
            o_ckstep.write(r_ckstep);
            o_til_cylim.write(r_til_cylim);
            o_til_cystep.write(r_til_cystep);
            o_til_cklim.write(r_til_cklim);
            o_til_ckstep.write(r_til_ckstep);
            o_ncontexts.write(r_ncontexts);
            o_preload_en.write(r_preload_en);
        }
    };

} // namespace sauria

#endif // SAURIA_CONFIG_REGS_H
