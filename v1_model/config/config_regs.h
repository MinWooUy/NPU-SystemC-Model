// SystemC Model for SAURIA NPU Core
// Configuration Registers Block (config_regs.h)

#ifndef SAURIA_CONFIG_REGS_H
#define SAURIA_CONFIG_REGS_H

#include "sauria_types.h"

namespace sauria
{

    template <
        int IF_W_PARAM = 32,
        int IF_ADR_W_PARAM = 32,
        int X_DIM = 32,
        int Y_DIM = 32,
        int TH_W_PARAM = 2,
        int ACT_IDX_W_PARAM = 15,
        int WEI_IDX_W_PARAM = 15,
        int OUT_IDX_W_PARAM = 15,
        // int PARAMS_W_PAPAM = 8 --> Not used in this model
        int DILP_W_PARAM = 64,
        int SRAMB_N_PARAM = 8>
    class ConfigRegs : public sc_module
    {
    public:
        // Clock & Reset
        sc_in<bool> i_clk{"i_clk"};
        sc_in<bool> i_rstn{"i_rstn"};

        // Host Config/AXI interface
        sc_in<uint32_t> i_host_addr{"i_host_addr"};
        sc_in<bool> i_host_wren{"i_host_wren"};
        sc_in<bool> i_host_rden{"i_host_rden"};
        sc_in<host_data_t> i_host_wdata{"i_host_wdata"};
        sc_in<host_mask_t> i_host_wmask{"i_host_wmask"};
        sc_out<host_data_t> o_host_rdata{"o_host_rdata"};

        // Status inputs from NPU Core
        sc_in<bool> i_done{"i_done"};
        sc_in<bool> i_soft_reset_in{"i_soft_reset_in"};

        // Handshake and Reset Control Outputs
        sc_out<bool> o_start{"o_start"};
        sc_out<bool> o_soft_reset{"o_soft_reset"};

        // Ouput to activation feeders
        sc_out<uint32_t> o_act_incntlim{"o_act_incntlim"};
        sc_out<uint32_t> o_act_incntstep{"o_act_incntstep"};
        sc_out<uint32_t> o_act_outcntlim{"o_act_outcntlim"};
        sc_out<uint32_t> o_act_outcntstep{"o_act_outcntstep"};

        // Output to weight feeders
        sc_out<uint32_t> o_wei_incntlim{"o_wei_incntlim"};
        sc_out<uint32_t> o_wei_incntstep{"o_wei_incntstep"};

        // Output to PSM feeders
        sc_out<uint32_t> o_cxlim{"o_cxlim"};
        sc_out<uint32_t> o_cxstep{"o_cxstep"};
        sc_out<uint32_t> o_cklim{"o_cklim"};
        sc_out<uint32_t> o_ckstep{"o_ckstep"};

        // Ouput to Memory (sauria_type)
        sc_out<uint32_t> o_act_base_addr{"o_act_base_addr"};
        sc_out<uint32_t> o_wei_base_addr{"o_wei_base_addr"};
        sc_out<uint32_t> o_out_base_addr{"o_out_base_addr"};

        // Outputs to NPU modules
        sc_out<uint32_t> o_incntlim{"o_incntlim"};
        sc_out<uint32_t> o_act_reps{"o_act_reps"};
        sc_out<uint32_t> o_wei_reps{"o_wei_reps"};
        sc_out<sc_bv<DILP_W_PARAM>> o_dil_pat{"o_dil_pat"};
        sc_out<sramc_mask_t<Y_DIM>> o_rows_active{"o_rows_active"};

        // output run-time
        sc_out<uint32_t> o_in_h{"o_in_h"};
        sc_out<uint32_t> o_in_w{"o_in_w"};
        sc_out<uint32_t> o_in_c{"o_in_c"};
        // sc_out<uint32_t> o_out_h{"o_out_h"};
        // sc_out<uint32_t> o_out_w{"o_out_w"};
        // sc_out<uint32_t> o_out_c{"o_out_c"};

        sc_out<uint32_t> o_kernel_h{"o_kernel_h"};
        sc_out<uint32_t> o_kernel_w{"o_kernel_w"};

        sc_out<uint32_t> o_stride{"o_stride"};
        sc_out<uint32_t> o_padding{"o_padding"};
        sc_out<uint32_t> o_dilation{"o_dilation"};

        sc_out<uint32_t> o_tile_x{"o_tile_x"};
        sc_out<uint32_t> o_tile_y{"o_tile_y"};
        sc_out<uint32_t> o_tile_k{"o_tile_k"};
        sc_out<uint32_t> o_tile_c{"o_tile_c"};

        sc_out<uint32_t> o_x_used{"o_x_used"};
        sc_out<uint32_t> o_y_used{"o_y_used"};

        SC_CTOR(ConfigRegs)
        {
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

        // Additional registers for activation feeder configuration
        uint32_t r_act_incntlim{96};
        uint32_t r_act_incntstep{1};
        uint32_t r_act_outcntlim{96};
        uint32_t r_act_outcntstep{1};

        // Additional registers for weight feeder configuration
        uint32_t r_wei_incntlim{96};
        uint32_t r_wei_incntstep{1};

        // Additional registers for PSM configuration
        uint32_t r_cxlim{96};
        uint32_t r_cxstep{1};
        uint32_t r_cklim{96};
        uint32_t r_ckstep{1};

        // Additional registers for memory
        uint32_t r_act_base_addr{0};
        uint32_t r_wei_base_addr{0};
        uint32_t r_out_base_addr{0};

        uint32_t r_in_h{0};
        uint32_t r_in_w{0};
        uint32_t r_in_c{0};

        uint32_t r_kernel_h{0};
        uint32_t r_kernel_w{0};

        uint32_t r_stride{0};
        uint32_t r_padding{0};
        uint32_t r_dilation{0};

        uint32_t r_tile_x{0};
        uint32_t r_tile_y{0};
        uint32_t r_tile_k{0};
        uint32_t r_tile_c{0};

        uint32_t r_x_used{0};
        uint32_t r_y_used{0};

        // Control flags
        bool r_start{false};
        bool r_done{false};
        bool r_soft_reset{false};

        void reg_process()
        {
            if (!i_rstn.read() || i_soft_reset_in.read())
            {
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
                r_incntlim = 96;
                r_act_reps = 64;
                r_wei_reps = 64;
                r_dil_pat = 1;
                r_rows_active = sramc_mask_t<Y_DIM>(true);
                r_start = false;
                r_done = false;
                r_soft_reset = false;

                o_start.write(false);
                o_soft_reset.write(false);
                o_incntlim.write(r_incntlim);
                o_act_incntlim.write(r_act_incntlim);
                o_act_incntstep.write(r_act_incntstep);
                o_act_outcntlim.write(r_act_outcntlim);
                o_act_outcntstep.write(r_act_outcntstep);
                o_act_reps.write(r_act_reps);
                o_wei_reps.write(r_wei_reps);
                o_dil_pat.write(r_dil_pat);
                o_rows_active.write(r_rows_active);
                o_wei_incntlim.write(r_wei_incntlim);
                o_wei_incntstep.write(r_wei_incntstep);

                o_cxlim.write(r_cxlim);
                o_cxstep.write(r_cxstep);
                o_cklim.write(r_cklim);
                o_ckstep.write(r_ckstep);

                o_act_base_addr.write(r_act_base_addr);
                o_wei_base_addr.write(r_wei_base_addr);
                o_out_base_addr.write(r_out_base_addr);
                o_host_rdata.write(host_data_t());
                return;
            }

            // Sync from FSM done status
            if (i_done.read())
            {
                r_done = true;
            }

            // Default auto-clearing signals
            if (r_start)
            {
                r_start = false;
            }
            if (r_soft_reset)
            {
                r_soft_reset = false;
            }

            // Address decoding
            uint32_t addr = i_host_addr.read();
            uint32_t mem_region = addr & SAURIA_MEM_ADDR_MASK;
            uint32_t local_addr = addr & ~SAURIA_MEM_ADDR_MASK;

            if (mem_region == CFG_REGS_OFFSET)
            {
                // Host Writes
                if (i_host_wren.read())
                {
                    host_data_t wdata = i_host_wdata.read();
                    host_mask_t wmask = i_host_wmask.read();

                    if (local_addr == 0x00)
                    {
                        // Control register
                        if (wmask[0])
                        {
                            r_start = (wdata[0] != 0.0f);
                        }
                        // COW on done
                        if (wmask[0] && (wdata[0] == 0.0f))
                        {
                            r_done = false;
                        }
                        // Soft reset
                        if (wmask[2])
                        { // bit 16-23 maps to third byte wmask[2]
                            r_soft_reset = (wdata[2] != 0.0f);
                        }
                    }
                    else if (local_addr == CFG_CON_OFFSET + 0x00)
                    {
                        if (wmask[0])
                            r_incntlim = (uint32_t)wdata[0];
                    }
                    else if (local_addr == CFG_CON_OFFSET + 0x04)
                    {
                        if (wmask[0])
                            r_act_reps = (uint32_t)wdata[0];
                    }
                    else if (local_addr == CFG_CON_OFFSET + 0x08)
                    {
                        if (wmask[0])
                            r_wei_reps = (uint32_t)wdata[0];
                    }
                    else if (local_addr == CFG_ACT_OFFSET + 0x00)
                    {
                        // Rows active mask
                        for (int i = 0; i < Y_DIM; i++)
                        {
                            int byte_idx = i / 8;
                            int bit_idx = i % 8;
                            if (byte_idx < 4 && wmask[byte_idx])
                            {
                                // Extract byte from wdata[byte_idx]
                                uint32_t val = (uint32_t)wdata[byte_idx];
                                r_rows_active[i] = (val & (1 << bit_idx)) != 0;
                            }
                        }
                    }
                    else if (local_addr == CFG_ACT_OFFSET + 0x04)
                    { // 0x04 is Reg 9 for act feeder incnt lim
                        if (wmask[0])
                            r_act_incntlim = (uint32_t)wdata[0];
                    }
                    else if (local_addr == CFG_ACT_OFFSET + 0x08)
                    { // 0x08 is Reg 11 for act feeder incnt step
                        if (wmask[0])
                            r_act_incntstep = (uint32_t)wdata[0];
                    }
                    else if (local_addr == CFG_ACT_OFFSET + 0x0C)
                    { // 0x0C is Reg 12 for act feeder outcnt lim
                        if (wmask[0])
                            r_act_outcntlim = (uint32_t)wdata[0];
                    }
                    else if (local_addr == CFG_ACT_OFFSET + 0x10)
                    { // 0x10 is Reg 9 for act feeder incnt step
                        if (wmask[0])
                            r_act_outcntstep = (uint32_t)wdata[0];
                    }
                    else if (local_addr == CFG_ACT_OFFSET + 0x28)
                    { // 0x28 is Reg 10 for dilation
                        if (wmask[0])
                            r_dil_pat = (uint32_t)wdata[0];
                    }
                    else if (local_addr == CFG_WEI_OFFSET + 0x04)
                    { // 0x04 is Reg 13 for wei feeder incnt lim
                        if (wmask[0])
                            r_wei_incntlim = (uint32_t)wdata[0];
                    }
                    else if (local_addr == CFG_WEI_OFFSET + 0x08)
                    { // 0x08 is Reg 14 for wei feeder incnt step
                        if (wmask[0])
                            r_wei_incntstep = (uint32_t)wdata[0];
                    }
                    else if (local_addr == CFG_OUT_OFFSET + 0x04)
                    { // 0x04 is Reg 15 for PSM cxlim
                        if (wmask[0])
                            r_cxlim = (uint32_t)wdata[0];
                    }
                    else if (local_addr == CFG_OUT_OFFSET + 0x08)
                    { // 0x08 is Reg 16 for PSM cxstep
                        if (wmask[0])
                            r_cxstep = (uint32_t)wdata[0];
                    }
                    else if (local_addr == CFG_OUT_OFFSET + 0x0C)
                    { // 0x0C is Reg 17 for PSM cklim
                        if (wmask[0])
                            r_cklim = (uint32_t)wdata[0];
                    }
                    else if (local_addr == CFG_OUT_OFFSET + 0x10)
                    { // 0x10 is Reg 18 for PSM ckstep
                        if (wmask[0])
                            r_ckstep = (uint32_t)wdata[0];
                    }

                    else if (local_addr == CFG_ACT_BASE_ADDR)
                    {
                        if (wmask[0])
                            r_act_base_addr = (uint32_t)wdata[0];
                    }
                    else if (local_addr == CFG_WEI_BASE_ADDR)
                    {
                        if (wmask[0])
                            r_wei_base_addr = (uint32_t)wdata[0];
                    }
                    else if (local_addr == CFG_OUT_BASE_ADDR)
                    {
                        if (wmask[0])
                            r_out_base_addr = (uint32_t)wdata[0];
                    }

                    // Layer run-time
                    else if (local_addr == IN_H)
                    {
                        if (wmask[0])
                            r_in_h = (uint32_t)wdata[0];
                    }
                    else if (local_addr == IN_W)
                    {
                        if (wmask[0])
                            r_in_w = (uint32_t)wdata[0];
                    }
                    else if (local_addr == IN_C)
                    {
                        if (wmask[0])
                            r_in_c = (uint32_t)wdata[0];
                    }
                    else if (local_addr == KERNEL_H)
                    {
                        if (wmask[0])
                            r_kernel_h = (uint32_t)wdata[0];
                    }
                    else if (local_addr == KERNEL_W)
                    {
                        if (wmask[0])
                            r_kernel_w = (uint32_t)wdata[0];
                    }
                    else if (local_addr == STRIDE)
                    {
                        if (wmask[0])
                            r_stride = (uint32_t)wdata[0];
                    }
                    else if (local_addr == PADDING)
                    {
                        if (wmask[0])
                            r_padding = (uint32_t)wdata[0];
                    }
                    else if (local_addr == DILATION)
                    {
                        if (wmask[0])
                            r_dilation = (uint32_t)wdata[0];
                    }
                    else if (local_addr == TILE_X)
                    {
                        if (wmask[0])
                            r_tile_x = (uint32_t)wdata[0];
                    }
                    else if (local_addr == TILE_Y)
                    {
                        if (wmask[0])
                            r_tile_y = (uint32_t)wdata[0];
                    }
                    else if (local_addr == TILE_K)
                    {
                        if (wmask[0])
                            r_tile_k = (uint32_t)wdata[0];
                    }
                    else if (local_addr == TILE_C)
                    {
                        if (wmask[0])
                            r_tile_c = (uint32_t)wdata[0];
                    }
                    else if (local_addr == X_USED)
                    {
                        if (wmask[0])
                            r_x_used = (uint32_t)wdata[0];
                    }
                    else if (local_addr == Y_USED)
                    {
                        if (wmask[0])
                            r_y_used = (uint32_t)wdata[0];
                    }
                }

                // Host Reads
                if (i_host_rden.read())
                {
                    host_data_t rdata;
                    if (local_addr == 0x00)
                    {
                        rdata[0] = r_start ? 1.0f : 0.0f;
                        rdata[0] += r_done ? 2.0f : 0.0f;
                        rdata[0] += (!r_done) ? 4.0f : 0.0f; // idle
                        rdata[0] += (!r_done) ? 8.0f : 0.0f; // ready
                        rdata[2] = r_soft_reset ? 128.0f : 0.0f;
                    }
                    else if (local_addr == CFG_CON_OFFSET + 0x00)
                    {
                        rdata[0] = (float)r_incntlim;
                    }
                    else if (local_addr == CFG_CON_OFFSET + 0x04)
                    {
                        rdata[0] = (float)r_act_reps;
                    }
                    else if (local_addr == CFG_CON_OFFSET + 0x08)
                    {
                        rdata[0] = (float)r_wei_reps;
                    }
                    else if (local_addr == CFG_ACT_OFFSET + 0x00)
                    {
                        for (int b = 0; b < 4; b++)
                        {
                            uint32_t val = 0;
                            for (int bit = 0; bit < 8; bit++)
                            {
                                int idx = b * 8 + bit;
                                if (idx < Y_DIM && r_rows_active[idx])
                                {
                                    val |= (1 << bit);
                                }
                            }
                            rdata[b] = (float)val;
                        }
                    }
                    else if (local_addr == CFG_ACT_OFFSET + 0x04)
                    { // 0x04 is Reg 9 for act feeder incnt lim
                        rdata[0] = (float)r_act_incntlim;
                    }
                    else if (local_addr == CFG_ACT_OFFSET + 0x08)
                    { // 0x08 is Reg 11 for act feeder outcnt step
                        rdata[0] = (float)r_act_incntstep;
                    }
                    else if (local_addr == CFG_ACT_OFFSET + 0x0C)
                    { // 0x0C is Reg 12 for act feeder outcnt lim
                        rdata[0] = (float)r_act_outcntlim;
                    }
                    else if (local_addr == CFG_ACT_OFFSET + 0x10)
                    { // 0x10 is Reg 9 for act feeder incnt step
                        rdata[0] = (float)r_act_outcntstep;
                    }
                    else if (local_addr == CFG_ACT_OFFSET + 0x28)
                    {
                        rdata[0] = (float)r_dil_pat.to_uint();
                    }
                    else if (local_addr == CFG_WEI_OFFSET + 0x04)
                    { // 0x04 is Reg 13 for wei feeder incnt lim
                        rdata[0] = (float)r_wei_incntlim;
                    }
                    else if (local_addr == CFG_WEI_OFFSET + 0x08)
                    { // 0x08 is Reg 14 for wei feeder incnt step
                        rdata[0] = (float)r_wei_incntstep;
                    }
                    else if (local_addr == CFG_OUT_OFFSET + 0x04)
                    { // 0x04 is Reg 15 for PSM cxlim
                        rdata[0] = (float)r_cxlim;
                    }
                    else if (local_addr == CFG_OUT_OFFSET + 0x08)
                    { // 0x08 is Reg 16 for PSM cxstep
                        rdata[0] = (float)r_cxstep;
                    }
                    else if (local_addr == CFG_OUT_OFFSET + 0x0C)
                    { // 0x0C is Reg 17 for PSM cklim
                        rdata[0] = (float)r_cklim;
                    }
                    else if (local_addr == CFG_OUT_OFFSET + 0x10)
                    { // 0x10 is Reg 18 for PSM ckstep
                        rdata[0] = (float)r_ckstep;
                    }

                    else if (local_addr == CFG_ACT_BASE_ADDR)
                    {
                        rdata[0] = (float)r_act_base_addr;
                    }
                    else if (local_addr == CFG_WEI_BASE_ADDR)
                    {
                        rdata[0] = (float)r_wei_base_addr;
                    }
                    else if (local_addr == CFG_OUT_BASE_ADDR)
                    {
                        rdata[0] = (float)r_out_base_addr;
                    }

                    // Layer run-time
                    else if (local_addr == IN_H)
                    {
                        rdata[0] = (float)r_in_h;
                    }
                    else if (local_addr == IN_W)
                    {
                        rdata[0] = (float)r_in_w;
                    }
                    else if (local_addr == IN_C)
                    {
                        rdata[0] = (float)r_in_c;
                    }
                    else if (local_addr == KERNEL_H)
                    {
                        rdata[0] = (float)r_kernel_h;
                    }
                    else if (local_addr == KERNEL_W)
                    {
                        rdata[0] = (float)r_kernel_w;
                    }
                    else if (local_addr == STRIDE)
                    {
                        rdata[0] = (float)r_stride;
                    }
                    else if (local_addr == PADDING)
                    {
                        rdata[0] = (float)r_padding;
                    }
                    else if (local_addr == DILATION)
                    {
                        rdata[0] = (float)r_dilation;
                    }
                    else if (local_addr == TILE_X)
                    {
                        rdata[0] = (float)r_tile_x;
                    }
                    else if (local_addr == TILE_Y)
                    {
                        rdata[0] = (float)r_tile_y;
                    }
                    else if (local_addr == TILE_K)
                    {
                        rdata[0] = (float)r_tile_k;
                    }
                    else if (local_addr == TILE_C)
                    {
                        rdata[0] = (float)r_tile_c;
                    }
                    else if (local_addr == X_USED)
                    {
                        rdata[0] = (float)r_x_used;
                    }
                    else if (local_addr == Y_USED)
                    {
                        rdata[0] = (float)r_y_used;
                    }

                    else
                    {
                        rdata = host_data_t(); // default to zero for unmapped addresses
                    }

                    o_host_rdata.write(rdata);
                }
            }

            if (r_start)
            {
                std::cout << "\n=========================================\n";
                std::cout << "NPU RUNTIME CONFIGURATION\n";
                std::cout << "=========================================\n";

                std::cout << "\nCONTROL\n";
                std::cout << "INCNTLIM        : " << r_incntlim << "\n";

                std::cout << "\nACTIVATION\n";
                std::cout << "ACT_INCNTLIM    : " << r_act_incntlim << "\n";
                std::cout << "ACT_INCNTSTEP   : " << r_act_incntstep << "\n";
                std::cout << "ACT_OUTCNTLIM   : " << r_act_outcntlim << "\n";
                std::cout << "ACT_OUTCNTSTEP  : " << r_act_outcntstep << "\n";
                std::cout << "DIL_PAT         : 0x"
                          << std::hex << r_dil_pat.to_uint64()
                          << std::dec << "\n";

                std::cout << "\nWEIGHT\n";
                std::cout << "WEI_INCNTLIM    : " << r_wei_incntlim << "\n";
                std::cout << "WEI_INCNTSTEP   : " << r_wei_incntstep << "\n";

                std::cout << "\nPSUM\n";
                std::cout << "CXLIM           : " << r_cxlim << "\n";
                std::cout << "CXSTEP          : " << r_cxstep << "\n";
                std::cout << "CKLIM           : " << r_cklim << "\n";
                std::cout << "CKSTEP          : " << r_ckstep << "\n";

                std::cout << "\nMEMORY MAP\n";
                std::cout << "ACT_BASE_ADDR   : " << r_act_base_addr << "\n";
                std::cout << "WEI_BASE_ADDR   : " << r_wei_base_addr << "\n";
                std::cout << "OUT_BASE_ADDR   : " << r_out_base_addr << "\n";

                std::cout << "\nREUSE\n";
                std::cout << "ACT_REPS        : " << r_act_reps << "\n";
                std::cout << "WEI_REPS        : " << r_wei_reps << "\n";

                std::cout << "\nARRAY\n";
                std::cout << "ROWS_ACTIVE     : 0x" << std::hex << r_rows_active << std::dec << "\n";

                std::cout << "\nLAYER DESCRIPTOR\n";

                std::cout
                    << "Input  : C="
                    << r_in_c
                    << " H="
                    << r_in_h
                    << " W="
                    << r_in_w
                    << std::endl;

                std::cout
                    << "Kernel : "
                    << r_kernel_h
                    << "x"
                    << r_kernel_w
                    << std::endl;

                std::cout
                    << "Stride : "
                    << r_stride
                    << std::endl;

                std::cout
                    << "Padding: "
                    << r_padding
                    << std::endl;

                std::cout
                    << "Dilation: "
                    << r_dilation
                    << std::endl;
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

            o_act_base_addr.write(r_act_base_addr);
            o_wei_base_addr.write(r_wei_base_addr);
            o_out_base_addr.write(r_out_base_addr);

            o_in_h.write(r_in_h);
            o_in_w.write(r_in_w);
            o_in_c.write(r_in_c);

            o_kernel_h.write(r_kernel_h);
            o_kernel_w.write(r_kernel_w);

            o_stride.write(r_stride);
            o_padding.write(r_padding);
            o_dilation.write(r_dilation);

            o_tile_x.write(r_tile_x);
            o_tile_y.write(r_tile_y);
            o_tile_k.write(r_tile_k);
            o_tile_c.write(r_tile_c);

            o_x_used.write(r_x_used);
            o_y_used.write(r_y_used);
        }
    };

} // namespace sauria

#endif // SAURIA_CONFIG_REGS_H
