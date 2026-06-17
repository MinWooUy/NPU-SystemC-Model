//
// SystemC Model for SAURIA NPU Core
// Evaluation Testbench comparing PE parameter profiles:
// 1. Standard FP32 Profile
// 2. Approximate FP32 Profile
// 3. Sparsity Zero-Gating Profile
//

// ---  define MACRO datatype and size---
#ifndef EVAL_X
#define EVAL_X 16
#endif

#ifndef EVAL_Y
#define EVAL_Y 8
#endif

#ifndef NPU_DTYPE_IN
#define NPU_DTYPE_IN int8_t // Active int8 for NPU
#endif

#ifndef NPU_DTYPE_OUT
#define NPU_DTYPE_OUT int32_t
#endif

#ifndef A_REGION_BYTES
#define A_REGION_BYTES 3840
#endif

#ifndef B_REGION_BYTES
#define B_REGION_BYTES 9216
#endif

#ifndef C_REGION_BYTES
#define C_REGION_BYTES 2048
#endif

#include "npu_top.h"
#include <iomanip>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <iomanip>
#include <vector>
#include <unordered_map>
#include <algorithm>

using namespace sauria;

class TestbenchEvaluate : public sc_module
{
public:
    // Clock & Reset Ports
    sc_in<bool> i_clk{"i_clk"};
    sc_out<bool> o_rstn{"o_rstn"};
    sc_out<bool> o_soft_reset{"o_soft_reset"};

    // NPU Host Control Interfaces
    sc_out<bool> o_start_std{"o_start_std"};
    sc_in<bool> i_done_std{"i_done_std"};
    sc_in<bool> i_deadlock_std{"i_deadlock_std"};

    sc_out<bool> o_start_approx{"o_start_approx"};
    sc_in<bool> i_done_approx{"i_done_approx"};
    sc_in<bool> i_deadlock_approx{"i_deadlock_approx"};

    sc_out<bool> o_start_gated{"o_start_gated"};
    sc_in<bool> i_done_gated{"i_done_gated"};
    sc_in<bool> i_deadlock_gated{"i_deadlock_gated"};

    // NPU Host Memory Ports - STD
    sc_out<uint32_t> o_host_addr_std{"o_host_addr_std"};
    sc_out<bool> o_host_wren_std{"o_host_wren_std"};
    sc_out<bool> o_host_rden_std{"o_host_rden_std"};
    sc_out<host_data_t> o_host_wdata_std{"o_host_wdata_std"};
    sc_out<host_mask_t> o_host_wmask_std{"o_host_wmask_std"};
    sc_in<host_data_t> i_host_rdata_std{"i_host_rdata_std"};

    // NPU Host Memory Ports - APPROX
    sc_out<uint32_t> o_host_addr_approx{"o_host_addr_approx"};
    sc_out<bool> o_host_wren_approx{"o_host_wren_approx"};
    sc_out<bool> o_host_rden_approx{"o_host_rden_approx"};
    sc_out<host_data_t> o_host_wdata_approx{"o_host_wdata_approx"};
    sc_out<host_mask_t> o_host_wmask_approx{"o_host_wmask_approx"};
    sc_in<host_data_t> i_host_rdata_approx{"i_host_rdata_approx"};

    // NPU Host Memory Ports - GATED
    sc_out<uint32_t> o_host_addr_gated{"o_host_addr_gated"};
    sc_out<bool> o_host_wren_gated{"o_host_wren_gated"};
    sc_out<bool> o_host_rden_gated{"o_host_rden_gated"};
    sc_out<host_data_t> o_host_wdata_gated{"o_host_wdata_gated"};
    sc_out<host_mask_t> o_host_wmask_gated{"o_host_wmask_gated"};
    sc_in<host_data_t> i_host_rdata_gated{"i_host_rdata_gated"};

    // Configurations
    sc_out<float> o_threshold{"o_threshold"};
    sc_out<sc_bv<3>> o_select{"o_select"};

    SC_CTOR(TestbenchEvaluate)
    {
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

    std::vector<host_data_t> sramc_before_run;

    uint32_t get_srama_addr(uint32_t phys_addr, uint32_t sub_word)
    {
        return SRAMA_OFFSET | ((phys_addr << shift_a) | (sub_word & mask_a));
    }

    uint32_t get_sramb_addr(uint32_t phys_addr, uint32_t sub_word)
    {
        return SRAMB_OFFSET | ((phys_addr << shift_b) | (sub_word & mask_b));
    }

    uint32_t get_sramc_addr(uint32_t phys_addr, uint32_t sub_word)
    {
        return SRAMC_OFFSET | ((phys_addr << shift_c) | (sub_word & mask_c));
    }

    bool is_controller_arg_write(uint32_t raw_addr, bool wren)
    {
        if (!wren)
            return false;
        if (raw_addr < 0x40000010)
            return false;
        if (raw_addr >= 0x40000400)
            return false;
        return ((raw_addr - 0x40000010) % 4) == 0;
    }

    uint32_t get_controller_arg_index(uint32_t raw_addr)
    {
        return (raw_addr - 0x40000010) >> 2;
    }

    bool is_controller_start_write(uint32_t raw_addr, uint32_t data_in, bool wren)
    {
        return wren && (raw_addr == 0x40000000) && (data_in == 3);
    }

    enum class StimRegion
    {
        CONTROLLER,
        SAURIA_CORE,
        DMA,
        SAURIA_INTERNAL,
        UNKNOWN
    };

    StimRegion get_stim_region(uint32_t raw_addr)
    {
        if (raw_addr >= 0x40000000 && raw_addr < 0x50000000)
        {
            return StimRegion::CONTROLLER;
        }

        if (raw_addr >= 0x50000000 && raw_addr < 0x50400000)
        {
            return StimRegion::SAURIA_CORE;
        }

        if (raw_addr >= 0x60000000 && raw_addr < 0x70000000)
        {
            return StimRegion::DMA;
        }

        if (raw_addr < 0x00400000)
        {
            return StimRegion::SAURIA_INTERNAL;
        }

        return StimRegion::UNKNOWN;
    }

    std::string stim_region_name(StimRegion r)
    {
        switch (r)
        {
        case StimRegion::CONTROLLER:
            return "CONTROLLER";
        case StimRegion::SAURIA_CORE:
            return "SAURIA_CORE";
        case StimRegion::DMA:
            return "DMA";
        case StimRegion::SAURIA_INTERNAL:
            return "SAURIA_INTERNAL";
        default:
            return "UNKNOWN";
        }
    }

    uint32_t normalize_sauria_addr(uint32_t raw_addr)
    {
        if (raw_addr >= 0x50000000 && raw_addr < 0x50400000)
        {
            return raw_addr - 0x50000000;
        }

        return raw_addr;
    }

    bool is_sauria_core_transaction(uint32_t raw_addr)
    {
        StimRegion r = get_stim_region(raw_addr);

        return (r == StimRegion::SAURIA_CORE ||
                r == StimRegion::SAURIA_INTERNAL);
    }

    bool is_cfg_addr(uint32_t internal_addr)
    {
        return (
            (internal_addr >= CFG_CON_OFFSET && internal_addr < CFG_CON_OFFSET + 0x200) ||
            (internal_addr >= CFG_ACT_OFFSET && internal_addr < CFG_ACT_OFFSET + 0x200) ||
            (internal_addr >= CFG_WEI_OFFSET && internal_addr < CFG_WEI_OFFSET + 0x200) ||
            (internal_addr >= CFG_OUT_OFFSET && internal_addr < CFG_OUT_OFFSET + 0x200));
    }

    void make_stim_packet(
        uint32_t internal_addr,
        uint32_t data_in,
        bool wren,
        host_data_t &packet,
        host_mask_t &mask)
    {
        packet.data.fill(0.0f);
        mask.data.fill(false);

        if (!wren)
        {
            return;
        }

        if (is_cfg_addr(internal_addr))
        {
            packet[0] = static_cast<float>(data_in);
            mask[0] = true;
        }
        else
        {
            packet[0] = static_cast<float>((data_in >> 0) & 0xFF);
            packet[1] = static_cast<float>((data_in >> 8) & 0xFF);
            packet[2] = static_cast<float>((data_in >> 16) & 0xFF);
            packet[3] = static_cast<float>((data_in >> 24) & 0xFF);

            mask[0] = true;
            mask[1] = true;
            mask[2] = true;
            mask[3] = true;
        }
    }

    struct DecodedSauriaConfig
    {
        // CONTROL
        uint32_t incntlim = 0;
        uint32_t act_reps = 0;
        uint32_t wei_reps = 0;
        uint32_t thres = 0;

        // ACTIVATION
        uint32_t xlim = 0;
        uint32_t xstep = 0;
        uint32_t ylim = 0;
        uint32_t ystep = 0;
        uint32_t chlim = 0;
        uint32_t chstep = 0;
        uint32_t til_xlim = 0;
        uint32_t til_xstep = 0;
        uint32_t til_ylim = 0;
        uint32_t til_ystep = 0;
        uint64_t dil_pat = 0;
        uint64_t rows_active = 0;

        // WEIGHT
        uint32_t wlim = 0;
        uint32_t wstep = 0;
        uint32_t klim = 0;
        uint32_t kstep = 0;
        uint32_t til_klim = 0;
        uint32_t til_kstep = 0;
        uint32_t cols_active = 0;
        uint32_t waligned = 0;

        // OUTPUT
        uint32_t ncontexts = 0;
        uint32_t cxlim = 0;
        uint32_t cxstep = 0;
        uint32_t cklim = 0;
        uint32_t ckstep = 0;
        uint32_t til_cylim = 0;
        uint32_t til_cystep = 0;
        uint32_t til_cklim = 0;
        uint32_t til_ckstep = 0;
        uint32_t inactive_cols = 0;
        uint32_t preload_en = 0;
    };

    DecodedSauriaConfig decode_sauria_packed_config(const std::vector<uint32_t> &controller_args)
    {
        DecodedSauriaConfig cfg;

        // These must match your current SystemC/Sauria config widths.
        constexpr uint32_t ACT_IDX_W = 15;
        constexpr uint32_t WEI_IDX_W = 16;
        constexpr uint32_t OUT_IDX_W = 14;
        constexpr uint32_t TH_W = 2;
        constexpr uint32_t DILP_W = 64;
        constexpr uint32_t PARAMS_W = 8;

        // If X_DIM/Y_DIM are not visible here, replace them with constants.
        constexpr uint32_t X = EVAL_X;
        constexpr uint32_t Y = EVAL_Y;

        constexpr uint32_t START_ARG = 22;

        size_t bitpos = 0;

        auto get = [&](uint32_t width) -> uint64_t
        {
            return read_packed_bits(controller_args, START_ARG, bitpos, width);
        };

        // =========================================================
        // CONTROL_SIGNALS
        // =========================================================
        cfg.incntlim = static_cast<uint32_t>(get(ACT_IDX_W));
        cfg.act_reps = static_cast<uint32_t>(get(OUT_IDX_W));
        cfg.wei_reps = static_cast<uint32_t>(get(OUT_IDX_W));
        cfg.thres = static_cast<uint32_t>(get(TH_W));

        align_to_next_word(bitpos);

        // =========================================================
        // ACTIVATION_SIGNALS
        // =========================================================
        cfg.xlim = static_cast<uint32_t>(get(ACT_IDX_W));
        cfg.xstep = static_cast<uint32_t>(get(ACT_IDX_W));
        cfg.ylim = static_cast<uint32_t>(get(ACT_IDX_W));
        cfg.ystep = static_cast<uint32_t>(get(ACT_IDX_W));
        cfg.chlim = static_cast<uint32_t>(get(ACT_IDX_W));
        cfg.chstep = static_cast<uint32_t>(get(ACT_IDX_W));
        cfg.til_xlim = static_cast<uint32_t>(get(ACT_IDX_W));
        cfg.til_xstep = static_cast<uint32_t>(get(ACT_IDX_W));
        cfg.til_ylim = static_cast<uint32_t>(get(ACT_IDX_W));
        cfg.til_ystep = static_cast<uint32_t>(get(ACT_IDX_W));
        cfg.dil_pat = get(DILP_W);
        cfg.rows_active = get(Y);

        // Skip local weight offsets for now.
        // In Sauria this is an array of Y values, each PARAMS_W bits.
        for (uint32_t y = 0; y < Y; y++)
        {
            (void)get(PARAMS_W);
        }

        align_to_next_word(bitpos);

        // =========================================================
        // WEIGHT_SIGNALS
        // =========================================================
        cfg.wlim = static_cast<uint32_t>(get(WEI_IDX_W));
        cfg.wstep = static_cast<uint32_t>(get(WEI_IDX_W));
        cfg.klim = static_cast<uint32_t>(get(WEI_IDX_W));
        cfg.kstep = static_cast<uint32_t>(get(WEI_IDX_W));
        cfg.til_klim = static_cast<uint32_t>(get(WEI_IDX_W));
        cfg.til_kstep = static_cast<uint32_t>(get(WEI_IDX_W));
        cfg.cols_active = static_cast<uint32_t>(get(X));
        cfg.waligned = static_cast<uint32_t>(get(1));

        align_to_next_word(bitpos);

        // =========================================================
        // OUTPUT_SIGNALS
        // =========================================================
        cfg.ncontexts = static_cast<uint32_t>(get(OUT_IDX_W));
        cfg.cxlim = static_cast<uint32_t>(get(OUT_IDX_W));
        cfg.cxstep = static_cast<uint32_t>(get(OUT_IDX_W));
        cfg.cklim = static_cast<uint32_t>(get(OUT_IDX_W));
        cfg.ckstep = static_cast<uint32_t>(get(OUT_IDX_W));
        cfg.til_cylim = static_cast<uint32_t>(get(OUT_IDX_W));
        cfg.til_cystep = static_cast<uint32_t>(get(OUT_IDX_W));
        cfg.til_cklim = static_cast<uint32_t>(get(OUT_IDX_W));
        cfg.til_ckstep = static_cast<uint32_t>(get(OUT_IDX_W));
        cfg.inactive_cols = static_cast<uint32_t>(get(PARAMS_W));
        cfg.preload_en = static_cast<uint32_t>(get(1));

        return cfg;
    }

    void print_decoded_sauria_config(const DecodedSauriaConfig &cfg)
    {
        std::cout << "\n=========================================\n";
        std::cout << "DECODED SAURIA PACKED CONFIG\n";
        std::cout << "=========================================\n";

        std::cout << "\nCONTROL\n";
        std::cout << "incntlim       : " << cfg.incntlim << "\n";
        std::cout << "act_reps       : " << cfg.act_reps << "\n";
        std::cout << "wei_reps       : " << cfg.wei_reps << "\n";
        std::cout << "thres          : " << cfg.thres << "\n";

        std::cout << "\nACTIVATION\n";
        std::cout << "xlim           : " << cfg.xlim << "\n";
        std::cout << "xstep          : " << cfg.xstep << "\n";
        std::cout << "ylim           : " << cfg.ylim << "\n";
        std::cout << "ystep          : " << cfg.ystep << "\n";
        std::cout << "chlim          : " << cfg.chlim << "\n";
        std::cout << "chstep         : " << cfg.chstep << "\n";
        std::cout << "til_xlim       : " << cfg.til_xlim << "\n";
        std::cout << "til_xstep      : " << cfg.til_xstep << "\n";
        std::cout << "til_ylim       : " << cfg.til_ylim << "\n";
        std::cout << "til_ystep      : " << cfg.til_ystep << "\n";
        std::cout << "dil_pat        : 0x" << std::hex << cfg.dil_pat << std::dec << "\n";
        std::cout << "rows_active    : 0x" << std::hex << cfg.rows_active << std::dec << "\n";

        std::cout << "\nWEIGHT\n";
        std::cout << "wlim           : " << cfg.wlim << "\n";
        std::cout << "wstep          : " << cfg.wstep << "\n";
        std::cout << "klim           : " << cfg.klim << "\n";
        std::cout << "kstep          : " << cfg.kstep << "\n";
        std::cout << "til_klim       : " << cfg.til_klim << "\n";
        std::cout << "til_kstep      : " << cfg.til_kstep << "\n";
        std::cout << "cols_active    : 0x" << std::hex << cfg.cols_active << std::dec << "\n";
        std::cout << "waligned       : " << cfg.waligned << "\n";

        std::cout << "\nOUTPUT\n";
        std::cout << "ncontexts      : " << cfg.ncontexts << "\n";
        std::cout << "cxlim          : " << cfg.cxlim << "\n";
        std::cout << "cxstep         : " << cfg.cxstep << "\n";
        std::cout << "cklim          : " << cfg.cklim << "\n";
        std::cout << "ckstep         : " << cfg.ckstep << "\n";
        std::cout << "til_cylim      : " << cfg.til_cylim << "\n";
        std::cout << "til_cystep     : " << cfg.til_cystep << "\n";
        std::cout << "til_cklim      : " << cfg.til_cklim << "\n";
        std::cout << "til_ckstep     : " << cfg.til_ckstep << "\n";
        std::cout << "inactive_cols  : " << cfg.inactive_cols << "\n";
        std::cout << "preload_en     : " << cfg.preload_en << "\n";

        std::cout << "=========================================\n\n";
    }

    void write_reg32_all(uint32_t addr, uint32_t value, const std::string &name = "")
    {
        host_data_t data;
        host_mask_t mask;

        data.data.fill(0.0f);
        mask.data.fill(false);

        data[0] = static_cast<float>(value);
        mask[0] = true;

        wait();

        // STD
        o_host_addr_std.write(addr);
        o_host_wdata_std.write(data);
        o_host_wmask_std.write(mask);
        o_host_wren_std.write(true);
        o_host_rden_std.write(false);

        // APPROX
        o_host_addr_approx.write(addr);
        o_host_wdata_approx.write(data);
        o_host_wmask_approx.write(mask);
        o_host_wren_approx.write(true);
        o_host_rden_approx.write(false);

        // GATED
        o_host_addr_gated.write(addr);
        o_host_wdata_gated.write(data);
        o_host_wmask_gated.write(mask);
        o_host_wren_gated.write(true);
        o_host_rden_gated.write(false);

        wait();

        host_mask_t zero_mask;
        zero_mask.data.fill(false);

        o_host_wren_std.write(false);
        o_host_wren_approx.write(false);
        o_host_wren_gated.write(false);

        o_host_wmask_std.write(zero_mask);
        o_host_wmask_approx.write(zero_mask);
        o_host_wmask_gated.write(zero_mask);

        wait();

        if (!name.empty())
        {
            std::cout << "[APPLY CFG] " << name
                      << " addr=0x" << std::hex << addr
                      << " value=0x" << value
                      << std::dec << " (" << value << ")"
                      << std::endl;
        }
    }

    void apply_decoded_config_to_npu(const DecodedSauriaConfig &cfg)
    {
        std::cout << "\n=========================================\n";
        std::cout << "APPLY DECODED SAURIA CONFIG TO SYSTEMC NPU\n";
        std::cout << "=========================================\n";

        // ---------------------------------------------------------
        // CONTROL / REUSE
        // ---------------------------------------------------------
        uint32_t sysc_act_read_count = cfg.incntlim + 1;
        write_reg32_all(CFG_CON_OFFSET + 0x00, sysc_act_read_count, "CON.INCNTLIM");
        write_reg32_all(CFG_CON_OFFSET + 0x04, cfg.act_reps, "CON.ACT_REPS");
        write_reg32_all(CFG_CON_OFFSET + 0x08, cfg.wei_reps, "CON.WEI_REPS");

        // ---------------------------------------------------------
        // ACTIVATION / IFMAP FEEDER
        // SystemC feeder hiện đang dùng:
        // ACT_INCNTLIM, ACT_INCNTSTEP, DIL_PAT, ROWS_ACTIVE
        // ---------------------------------------------------------
        write_reg32_all(CFG_ACT_OFFSET + 0x00,
                        static_cast<uint32_t>(cfg.rows_active),
                        "ACT.ROWS_ACTIVE");

        write_reg32_all(CFG_ACT_OFFSET + 0x04,
                        sysc_act_read_count,
                        "ACT.INCNTLIM");

        write_reg32_all(CFG_ACT_OFFSET + 0x08,
                        cfg.xstep,
                        "ACT.INCNTSTEP");

        // Hai register này hiện chưa functional đầy đủ, nhưng ghi để log/tracking
        write_reg32_all(CFG_ACT_OFFSET + 0x0C,
                        cfg.xlim,
                        "ACT.OUTCNTLIM");

        write_reg32_all(CFG_ACT_OFFSET + 0x10,
                        cfg.xstep,
                        "ACT.OUTCNTSTEP");

        // Hiện ConfigRegs của bạn mới nhận 32-bit thấp của DIL_PAT.
        // Với test d=1 thì low32 = 0 cũng không sao nếu feeder xem all-zero là allow-all.
        write_reg32_all(CFG_ACT_OFFSET + 0x28,
                        static_cast<uint32_t>(cfg.dil_pat & 0xFFFFFFFFULL),
                        "ACT.DIL_PAT_LOW32");

        // Full SAURIA activation address generator parameters
        write_reg32_all(CFG_ACT_OFFSET + 0x14, cfg.xlim, "ACT.XLIM");
        write_reg32_all(CFG_ACT_OFFSET + 0x18, cfg.xstep, "ACT.XSTEP");
        write_reg32_all(CFG_ACT_OFFSET + 0x1C, cfg.ylim, "ACT.YLIM");
        write_reg32_all(CFG_ACT_OFFSET + 0x20, cfg.ystep, "ACT.YSTEP");
        write_reg32_all(CFG_ACT_OFFSET + 0x24, cfg.chlim, "ACT.CHLIM");
        write_reg32_all(CFG_ACT_OFFSET + 0x2C, cfg.chstep, "ACT.CHSTEP");

        write_reg32_all(CFG_ACT_OFFSET + 0x30, cfg.til_xlim, "ACT.TIL_XLIM");
        write_reg32_all(CFG_ACT_OFFSET + 0x34, cfg.til_xstep, "ACT.TIL_XSTEP");
        write_reg32_all(CFG_ACT_OFFSET + 0x38, cfg.til_ylim, "ACT.TIL_YLIM");
        write_reg32_all(CFG_ACT_OFFSET + 0x3C, cfg.til_ystep, "ACT.TIL_YSTEP");
        // ---------------------------------------------------------
        // WEIGHT FEEDER
        // ---------------------------------------------------------
        write_reg32_all(CFG_WEI_OFFSET + 0x04,
                        cfg.wlim,
                        "WEI.INCNTLIM");

        write_reg32_all(CFG_WEI_OFFSET + 0x08,
                        cfg.wstep,
                        "WEI.INCNTSTEP");
        // Full SAURIA weight address-generator parameters
        write_reg32_all(CFG_WEI_OFFSET + 0x10, cfg.wlim, "WEI.WLIM");
        write_reg32_all(CFG_WEI_OFFSET + 0x14, cfg.wstep, "WEI.WSTEP");
        write_reg32_all(CFG_WEI_OFFSET + 0x18, cfg.klim, "WEI.KLIM");
        write_reg32_all(CFG_WEI_OFFSET + 0x1C, cfg.kstep, "WEI.KSTEP");

        write_reg32_all(CFG_WEI_OFFSET + 0x20, cfg.til_klim, "WEI.TIL_KLIM");
        write_reg32_all(CFG_WEI_OFFSET + 0x24, cfg.til_kstep, "WEI.TIL_KSTEP");

        write_reg32_all(CFG_WEI_OFFSET + 0x28, cfg.cols_active, "WEI.COLS_ACTIVE");
        write_reg32_all(CFG_WEI_OFFSET + 0x2C, cfg.waligned, "WEI.WALIGNED");
        // ---------------------------------------------------------
        // PSM / OUTPUT
        // ---------------------------------------------------------
        write_reg32_all(CFG_OUT_OFFSET + 0x04,
                        cfg.cxlim,
                        "OUT.CXLIM");

        write_reg32_all(CFG_OUT_OFFSET + 0x08,
                        cfg.cxstep,
                        "OUT.CXSTEP");

        write_reg32_all(CFG_OUT_OFFSET + 0x0C,
                        cfg.cklim,
                        "OUT.CKLIM");

        write_reg32_all(CFG_OUT_OFFSET + 0x10,
                        cfg.ckstep,
                        "OUT.CKSTEP");

        // Full SAURIA output / PSM schedule parameters
        write_reg32_all(CFG_OUT_OFFSET + 0x00, cfg.ncontexts, "OUT.NCONTEXTS");
        write_reg32_all(CFG_OUT_OFFSET + 0x14, cfg.til_cylim, "OUT.TIL_CYLIM");
        write_reg32_all(CFG_OUT_OFFSET + 0x18, cfg.til_cystep, "OUT.TIL_CYSTEP");
        write_reg32_all(CFG_OUT_OFFSET + 0x1C, cfg.til_cklim, "OUT.TIL_CKLIM");
        write_reg32_all(CFG_OUT_OFFSET + 0x20, cfg.til_ckstep, "OUT.TIL_CKSTEP");
        write_reg32_all(CFG_OUT_OFFSET + 0x24, cfg.inactive_cols, "OUT.INACTIVE_COLS");
        write_reg32_all(CFG_OUT_OFFSET + 0x28, cfg.preload_en, "OUT.PRELOAD_EN");
        // ---------------------------------------------------------
        // BASE ADDRESS
        // Quan trọng: args[18..20] là DRAM base của Sauria controller.
        // TB hiện đã preload dữ liệu trực tiếp vào SRAMA/SRAMB/SRAMC từ offset 0,
        // nên runtime base nội bộ của NPU để 0 trước.
        // ---------------------------------------------------------
        write_reg32_all(CFG_ACT_BASE_ADDR, 0, "ACT.BASE_ADDR");
        write_reg32_all(CFG_WEI_BASE_ADDR, 0, "WEI.BASE_ADDR");
        write_reg32_all(CFG_OUT_BASE_ADDR, 0, "OUT.BASE_ADDR");

        std::cout << "=========================================\n\n";
    }

    std::vector<uint8_t> load_initial_dram_file(const std::string &path)
    {
        std::vector<uint8_t> dram;

        std::ifstream file(path);
        if (!file.is_open())
        {
            std::cerr << "[TB ERROR] Cannot open initial DRAM file: "
                      << path << std::endl;
            return dram;
        }

        std::string token;

        while (file >> token)
        {
            uint32_t value = 0;

            try
            {
                value = static_cast<uint32_t>(std::stoul(token, nullptr, 16));
            }
            catch (...)
            {
                std::cerr << "[TB WARNING] Invalid DRAM token: "
                          << token << std::endl;
                continue;
            }

            dram.push_back(static_cast<uint8_t>(value & 0xFF));
        }

        std::cout << "[TB] Loaded initial_dram.txt: "
                  << dram.size()
                  << " bytes"
                  << std::endl;

        return dram;
    }

    void print_dram_bytes(
        const std::vector<uint8_t> &dram,
        uint32_t base,
        uint32_t nbytes,
        const std::string &name)
    {
        std::cout << "\n[DRAM DEBUG] " << name
                  << " base=0x" << std::hex << base
                  << std::dec
                  << " nbytes=" << nbytes
                  << std::endl;

        if (base >= dram.size())
        {
            std::cout << "  [OUT OF RANGE] base >= dram.size()" << std::endl;
            return;
        }

        uint32_t end = std::min<uint32_t>(base + nbytes, dram.size());

        for (uint32_t addr = base; addr < end; addr++)
        {
            if ((addr - base) % 16 == 0)
            {
                std::cout << "\n  0x"
                          << std::hex << std::setw(8) << std::setfill('0')
                          << addr
                          << " : "
                          << std::dec;
            }

            std::cout << std::hex
                      << std::setw(2) << std::setfill('0')
                      << static_cast<uint32_t>(dram[addr])
                      << " "
                      << std::dec;
        }

        std::cout << std::setfill(' ') << "\n"
                  << std::endl;
    }

    // Write helper
    void write_mem(int profile, uint32_t addr, const host_data_t &data)
    {
        host_mask_t mask;
        mask.data.fill(true);
        wait();
        if (profile == 0)
        {
            o_host_addr_std.write(addr);
            o_host_wdata_std.write(data);
            o_host_wmask_std.write(mask);
            o_host_wren_std.write(true);
            o_host_rden_std.write(false);
        }
        else if (profile == 1)
        {
            o_host_addr_approx.write(addr);
            o_host_wdata_approx.write(data);
            o_host_wmask_approx.write(mask);
            o_host_wren_approx.write(true);
            o_host_rden_approx.write(false);
        }
        else
        {
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

    void preload_int8_region_to_srama(
        const std::vector<uint8_t> &dram,
        uint32_t dram_base,
        uint32_t nbytes)
    {
        std::cout << "[TB PRELOAD] ACT -> SRAMA, base=0x"
                  << std::hex << dram_base
                  << std::dec << ", bytes=" << nbytes << std::endl;

        uint32_t loaded = 0;

        for (uint32_t phys_addr = 0; loaded < nbytes; phys_addr++)
        {
            for (int sw = 0; sw < subwords_a && loaded < nbytes; sw++)
            {
                host_data_t pkt;
                pkt.data.fill(0.0f);

                for (int i = 0; i < 4 && loaded < nbytes; i++)
                {
                    uint32_t dram_idx = dram_base + loaded;

                    if (dram_idx < dram.size())
                    {
                        pkt[i] = static_cast<float>(
                            static_cast<int8_t>(dram[dram_idx]));
                    }

                    loaded++;
                }

                for (int p = 0; p < 3; p++)
                {
                    write_mem(p, get_srama_addr(phys_addr, sw), pkt);
                }
            }
        }

        std::cout << "[TB PRELOAD] ACT loaded bytes: "
                  << loaded << std::endl;
    }

    void preload_int8_region_to_sramb(
        const std::vector<uint8_t> &dram,
        uint32_t dram_base,
        uint32_t nbytes)
    {
        std::cout << "[TB PRELOAD] WEI -> SRAMB, base=0x"
                  << std::hex << dram_base
                  << std::dec << ", bytes=" << nbytes << std::endl;

        uint32_t loaded = 0;

        for (uint32_t phys_addr = 0; loaded < nbytes; phys_addr++)
        {
            for (int sw = 0; sw < subwords_b && loaded < nbytes; sw++)
            {
                host_data_t pkt;
                pkt.data.fill(0.0f);

                for (int i = 0; i < 4 && loaded < nbytes; i++)
                {
                    uint32_t dram_idx = dram_base + loaded;

                    if (dram_idx < dram.size())
                    {
                        pkt[i] = static_cast<float>(
                            static_cast<int8_t>(dram[dram_idx]));
                    }

                    loaded++;
                }

                for (int p = 0; p < 3; p++)
                {
                    write_mem(p, get_sramb_addr(phys_addr, sw), pkt);
                }
            }
        }

        std::cout << "[TB PRELOAD] WEI loaded bytes: "
                  << loaded << std::endl;
    }

    void preload_int32_region_to_sramc(
        const std::vector<uint8_t> &dram,
        uint32_t dram_base,
        uint32_t nbytes)
    {
        std::cout << "[TB PRELOAD] PSUM -> SRAMC, base=0x"
                  << std::hex << dram_base
                  << std::dec << ", bytes=" << nbytes << std::endl;

        uint32_t loaded_bytes = 0;
        uint32_t total_elems = nbytes / 4;
        uint32_t elem_idx = 0;

        for (uint32_t phys_addr = 0; elem_idx < total_elems; phys_addr++)
        {
            for (int sw = 0; sw < subwords_c && elem_idx < total_elems; sw++)
            {
                host_data_t pkt;
                pkt.data.fill(0.0f);

                for (int i = 0; i < 4 && elem_idx < total_elems; i++)
                {
                    uint32_t byte_idx = dram_base + elem_idx * 4;

                    int32_t val = 0;

                    if (byte_idx + 3 < dram.size())
                    {
                        val =
                            (static_cast<uint8_t>(dram[byte_idx + 0]) << 0) |
                            (static_cast<uint8_t>(dram[byte_idx + 1]) << 8) |
                            (static_cast<uint8_t>(dram[byte_idx + 2]) << 16) |
                            (static_cast<uint8_t>(dram[byte_idx + 3]) << 24);
                    }

                    pkt[i] = static_cast<float>(val);

                    elem_idx++;
                    loaded_bytes += 4;
                }

                for (int p = 0; p < 3; p++)
                {
                    write_mem(p, get_sramc_addr(phys_addr, sw), pkt);
                }
            }
        }

        std::cout << "[TB PRELOAD] PSUM loaded bytes: "
                  << loaded_bytes << std::endl;
    }

    int32_t read_i32_le_from_bytes(
        const std::vector<uint8_t> &mem,
        uint32_t byte_addr)
    {
        if (byte_addr + 3 >= mem.size())
        {
            return 0;
        }

        uint32_t v =
            (static_cast<uint32_t>(mem[byte_addr + 0]) << 0) |
            (static_cast<uint32_t>(mem[byte_addr + 1]) << 8) |
            (static_cast<uint32_t>(mem[byte_addr + 2]) << 16) |
            (static_cast<uint32_t>(mem[byte_addr + 3]) << 24);

        return static_cast<int32_t>(v);
    }

    std::vector<int32_t> load_gold_output_i32(
        const std::vector<uint8_t> &gold_dram,
        uint32_t out_dram_base,
        uint32_t n_elems)
    {
        std::vector<int32_t> gold;
        gold.reserve(n_elems);

        for (uint32_t i = 0; i < n_elems; i++)
        {
            uint32_t byte_addr = out_dram_base + i * 4;
            gold.push_back(read_i32_le_from_bytes(gold_dram, byte_addr));
        }

        return gold;
    }

    std::vector<int32_t> dump_sramc_linear_i32(
        uint32_t n_elems)
    {
        std::vector<int32_t> out;
        out.reserve(n_elems);

        const uint32_t elems_per_sramc_addr = subwords_c * 4;
        uint32_t n_phys_addr =
            (n_elems + elems_per_sramc_addr - 1) / elems_per_sramc_addr;

        for (uint32_t phys_addr = 0; phys_addr < n_phys_addr; phys_addr++)
        {
            for (int sw = 0; sw < subwords_c; sw++)
            {
                uint32_t host_addr = get_sramc_addr(phys_addr, sw);
                host_data_t data = read_mem(0, host_addr);

                for (int lane = 0; lane < 4; lane++)
                {
                    if (out.size() < n_elems)
                    {
                        out.push_back(static_cast<int32_t>(data[lane]));
                    }
                }
            }
        }

        return out;
    }

    std::vector<int32_t> dump_sramc_psm_layout_i32(
        const DecodedSauriaConfig &cfg)
    {
        std::vector<int32_t> out;

        uint32_t ncontexts = cfg.ncontexts;
        if (ncontexts == 0)
        {
            ncontexts = 1;
        }

        uint32_t vectors_per_context = cfg.cxlim;
        uint32_t vector_step = cfg.cxstep;
        uint32_t context_addr_stride = cfg.cxlim * cfg.cxstep;

        uint32_t expected_elems = cfg.til_cklim;
        if (expected_elems == 0)
        {
            expected_elems = ncontexts * vectors_per_context * Y_DIM;
        }

        out.reserve(expected_elems);

        std::cout << "\n=========================================\n";
        std::cout << "DUMP SRAMC USING PSM LAYOUT\n";
        std::cout << "=========================================\n";
        std::cout << "ncontexts          : " << ncontexts << "\n";
        std::cout << "vectors/context    : " << vectors_per_context << "\n";
        std::cout << "vector_step        : " << vector_step << "\n";
        std::cout << "context_addr_stride: " << context_addr_stride << "\n";
        std::cout << "expected_elems     : " << expected_elems << "\n";

        for (uint32_t ctx = 0; ctx < ncontexts; ctx++)
        {
            uint32_t ctx_base = ctx * context_addr_stride;

            std::cout << "\n[PSM LAYOUT DUMP] context=" << ctx
                      << " ctx_base=" << ctx_base << "\n";

            for (uint32_t v = 0; v < vectors_per_context; v++)
            {
                uint32_t phys_addr = ctx_base + v * vector_step;

                if (ctx < 4 && v < 4)
                {
                    std::cout << "  vector=" << v
                              << " phys_addr=" << phys_addr
                              << " data=[";
                }

                for (int sw = 0; sw < subwords_c; sw++)
                {
                    uint32_t host_addr = get_sramc_addr(phys_addr, sw);
                    host_data_t data = read_mem(0, host_addr);

                    for (int lane = 0; lane < 4; lane++)
                    {
                        if (out.size() < expected_elems)
                        {
                            int32_t val = static_cast<int32_t>(data[lane]);
                            out.push_back(val);

                            if (ctx < 4 && v < 4)
                            {
                                std::cout << val;
                                if (!(sw == subwords_c - 1 && lane == 3))
                                {
                                    std::cout << ", ";
                                }
                            }
                        }
                    }
                }

                if (ctx < 4 && v < 4)
                {
                    std::cout << "]\n";
                }
            }
        }

        std::cout << "Dumped elements     : " << out.size() << "\n";
        std::cout << "=========================================\n\n";

        return out;
    }

    void compare_sramc_with_gold(
        const std::vector<int32_t> &sramc_out,
        const std::vector<int32_t> &gold_out)
    {
        uint32_t n = std::min(sramc_out.size(), gold_out.size());
        uint32_t mismatches = 0;

        std::cout << "\n=========================================\n";
        std::cout << "SRAMC RAW OUTPUT VS GOLD_DRAM COMPARE\n";
        std::cout << "=========================================\n";

        for (uint32_t i = 0; i < n; i++)
        {
            if (sramc_out[i] != gold_out[i])
            {
                if (mismatches < 32)
                {
                    std::cout << "Mismatch[" << i << "] "
                              << "SRAMC=" << sramc_out[i]
                              << " GOLD=" << gold_out[i]
                              << std::endl;
                }
                mismatches++;
            }
        }

        std::cout << "Compared elements : " << n << std::endl;
        std::cout << "Mismatches        : " << mismatches << std::endl;

        if (mismatches == 0)
        {
            std::cout << "RAW COMPARE PASS" << std::endl;
        }
        else
        {
            std::cout << "RAW COMPARE FAIL / layout may need reorder" << std::endl;
        }

        std::cout << "=========================================\n\n";
    }

    std::vector<host_data_t> snapshot_sramc_words(
        uint32_t n_words,
        const std::string &tag)
    {
        std::vector<host_data_t> snapshot;

        std::cout << "\n=========================================\n";
        std::cout << "SRAMC SNAPSHOT: " << tag << "\n";
        std::cout << "=========================================\n";

        for (uint32_t i = 0; i < n_words; i++)
        {
            uint32_t addr = get_sramc_addr(i, 0);
            host_data_t data = read_mem(0, addr);
            snapshot.push_back(data);

            if (i < 16)
            {
                std::cout << "SRAMC word " << i
                          << " addr=0x" << std::hex << addr << std::dec
                          << " : ";

                for (int j = 0; j < 4; j++)
                {
                    std::cout << data[j] << " ";
                }

                std::cout << "\n";
            }
        }

        std::cout << "=========================================\n\n";

        return snapshot;
    }

    void compare_sramc_snapshots(
        const std::vector<host_data_t> &before,
        const std::vector<host_data_t> &after)
    {
        uint32_t changed_words = 0;

        std::cout << "\n=========================================\n";
        std::cout << "SRAMC BEFORE / AFTER CHANGE CHECK\n";
        std::cout << "=========================================\n";

        uint32_t n = std::min(before.size(), after.size());

        for (uint32_t i = 0; i < n; i++)
        {
            bool changed = false;

            for (int j = 0; j < 4; j++)
            {
                float diff = before[i][j] - after[i][j];
                if (diff < 0)
                    diff = -diff;

                if (diff > 1e-3f)
                {
                    changed = true;
                }
            }

            if (changed)
            {
                changed_words++;

                if (changed_words <= 32)
                {
                    std::cout << "SRAMC word " << i << " CHANGED\n";

                    std::cout << "  before = [";
                    for (int j = 0; j < 4; j++)
                    {
                        std::cout << before[i][j];
                        if (j < 3)
                            std::cout << ", ";
                    }
                    std::cout << "]\n";

                    std::cout << "  after  = [";
                    for (int j = 0; j < 4; j++)
                    {
                        std::cout << after[i][j];
                        if (j < 3)
                            std::cout << ", ";
                    }
                    std::cout << "]\n";
                }
            }
        }

        std::cout << "Changed SRAMC words: " << changed_words << "\n";
        std::cout << "=========================================\n\n";
    }

    // Read helper
    host_data_t read_mem(int profile, uint32_t addr)
    {
        host_data_t zero_data;
        host_mask_t zero_mask;

        zero_data.data.fill(0.0f);
        zero_mask.data.fill(false);

        if (profile == 0)
        {
            o_host_addr_std.write(addr);
            o_host_wren_std.write(false);
            o_host_rden_std.write(true);
            o_host_wdata_std.write(zero_data);
            o_host_wmask_std.write(zero_mask);
        }
        else if (profile == 1)
        {
            o_host_addr_approx.write(addr);
            o_host_wren_approx.write(false);
            o_host_rden_approx.write(true);
            o_host_wdata_approx.write(zero_data);
            o_host_wmask_approx.write(zero_mask);
        }
        else
        {
            o_host_addr_gated.write(addr);
            o_host_wren_gated.write(false);
            o_host_rden_gated.write(true);
            o_host_wdata_gated.write(zero_data);
            o_host_wmask_gated.write(zero_mask);
        }

        wait();

        host_data_t result;

        if (profile == 0)
        {
            result = i_host_rdata_std.read();
            o_host_rden_std.write(false);
        }
        else if (profile == 1)
        {
            result = i_host_rdata_approx.read();
            o_host_rden_approx.write(false);
        }
        else
        {
            result = i_host_rdata_gated.read();
            o_host_rden_gated.write(false);
        }

        wait();

        return result;
    }

    void analyze_value_match_between_sramc_and_gold(
        const std::vector<int32_t> &sramc_out,
        const std::vector<int32_t> &gold_out)
    {
        std::unordered_map<int32_t, std::vector<uint32_t>> gold_pos;

        for (uint32_t i = 0; i < gold_out.size(); i++)
        {
            gold_pos[gold_out[i]].push_back(i);
        }

        uint32_t nonzero_sramc = 0;
        uint32_t matched_values = 0;
        uint32_t printed = 0;

        std::cout << "\n=========================================\n";
        std::cout << "SRAMC VALUE SEARCH IN GOLD_DRAM\n";
        std::cout << "=========================================\n";

        for (uint32_t i = 0; i < sramc_out.size(); i++)
        {
            int32_t v = sramc_out[i];

            // Skip zero because zero is usually too common and not useful
            if (v == 0)
                continue;

            nonzero_sramc++;

            auto it = gold_pos.find(v);

            if (it != gold_pos.end())
            {
                matched_values++;

                if (printed < 32)
                {
                    std::cout << "SRAMC[" << i << "] = " << v
                              << " found in GOLD at index ";

                    for (size_t k = 0; k < it->second.size() && k < 8; k++)
                    {
                        std::cout << it->second[k];
                        if (k + 1 < it->second.size() && k < 7)
                        {
                            std::cout << ", ";
                        }
                    }

                    std::cout << "\n";
                    printed++;
                }
            }
        }

        std::cout << "Non-zero SRAMC values : " << nonzero_sramc << "\n";
        std::cout << "Matched in GOLD       : " << matched_values << "\n";

        if (nonzero_sramc == 0)
        {
            std::cout << "Result: SRAMC output is all zero or not dumped correctly.\n";
        }
        else if (matched_values > 0)
        {
            std::cout << "Result: Some values match. Likely layout/reorder issue.\n";
        }
        else
        {
            std::cout << "Result: No value match. Likely compute/datapath mismatch.\n";
        }

        std::cout << "=========================================\n\n";
    }

    void debug_readback_sram_regions()
    {
        std::cout << "\n=========================================\n";
        std::cout << "SRAM READ-BACK DEBUG AFTER PRELOAD\n";
        std::cout << "=========================================\n";

        std::cout << "\n[SRAMA READBACK]\n";
        for (int i = 0; i < 4; i++)
        {
            uint32_t addr = get_srama_addr(i, 0);
            host_data_t data = read_mem(0, addr);

            std::cout << "SRAMA addr=0x" << std::hex << addr << std::dec << " : ";
            for (int j = 0; j < 4; j++)
            {
                std::cout << data[j] << " ";
            }
            std::cout << "\n";
        }

        std::cout << "\n[SRAMB READBACK]\n";
        for (int i = 0; i < 4; i++)
        {
            uint32_t addr = get_sramb_addr(i, 0);
            host_data_t data = read_mem(0, addr);

            std::cout << "SRAMB addr=0x" << std::hex << addr << std::dec << " : ";
            for (int j = 0; j < 4; j++)
            {
                std::cout << data[j] << " ";
            }
            std::cout << "\n";
        }

        std::cout << "\n[SRAMC READBACK]\n";
        for (int i = 0; i < 4; i++)
        {
            uint32_t addr = get_sramc_addr(i, 0);
            host_data_t data = read_mem(0, addr);

            std::cout << "SRAMC addr=0x" << std::hex << addr << std::dec << " : ";
            for (int j = 0; j < 4; j++)
            {
                std::cout << data[j] << " ";
            }
            std::cout << "\n";
        }

        std::cout << "=========================================\n\n";
    }

    void debug_output_sramc_after_run()
    {
        std::cout << "\n=========================================\n";
        std::cout << "SRAMC OUTPUT READBACK AFTER NPU RUN\n";
        std::cout << "=========================================\n";

        for (int i = 0; i < 16; i++)
        {
            uint32_t addr = get_sramc_addr(i, 0);
            host_data_t data = read_mem(0, addr);

            std::cout << "SRAMC addr=0x"
                      << std::hex << addr
                      << std::dec << " : ";

            for (int j = 0; j < 4; j++)
            {
                std::cout << data[j] << " ";
            }

            std::cout << "\n";
        }

        std::cout << "=========================================\n\n";
    }

    uint64_t read_packed_bits(
        const std::vector<uint32_t> &args,
        uint32_t start_word_idx,
        size_t &bitpos,
        uint32_t width)
    {
        uint64_t value = 0;

        for (uint32_t b = 0; b < width; b++)
        {
            size_t abs_bit = bitpos + b;
            uint32_t word_idx = start_word_idx + static_cast<uint32_t>(abs_bit / 32);
            uint32_t bit_idx = static_cast<uint32_t>(abs_bit % 32);

            uint64_t bit_val = (args[word_idx] >> bit_idx) & 0x1ULL;
            value |= (bit_val << b);
        }

        bitpos += width;
        return value;
    }

    void align_to_next_word(size_t &bitpos)
    {
        if (bitpos % 32 != 0)
        {
            bitpos = ((bitpos / 32) + 1) * 32;
        }
    }

    void test_process()
    {
        // store controller_args
        std::vector<uint32_t> controller_args(256, 0);
        bool controller_started = false;

        DecodedSauriaConfig decoded_cfg_runtime;
        bool decoded_cfg_valid = false;

        uint32_t act_dram_base_runtime = 0;
        uint32_t wei_dram_base_runtime = 0;
        uint32_t out_dram_base_runtime = 0;

        std::cout << "\n=============================================================" << std::endl;
        std::cout << "      SAURIA SystemC NPU Core Multi-Profile Evaluation       " << std::endl;
        std::cout << "=============================================================\n"
                  << std::endl;

        // Reset system
        o_rstn.write(false);
        o_soft_reset.write(false);
        o_start_std.write(false);
        o_start_approx.write(false);
        o_start_gated.write(false);

        o_host_wren_std.write(false);
        o_host_rden_std.write(false);
        o_host_wren_approx.write(false);
        o_host_rden_approx.write(false);
        o_host_wren_gated.write(false);
        o_host_rden_gated.write(false);

        // Sparsity threshold = 0.5f
        o_threshold.write(0.5f);
        o_select.write(sc_bv<3>("000")); // Map physical buffer 0 to Host AXI

        wait(5);
        o_rstn.write(true);
        wait(2);

        std::cout << "[TB] Fetching initial_dram.txt into Memory Pre-load..." << std::endl;
        std::ifstream dram_file("stimuli/initial_dram.txt");
        if (!dram_file.is_open())
        {
            std::cerr << "[ERROR] Can't open initial_dram.txt file!" << std::endl;
            sc_stop();
            return;
        }

        std::string line;
        std::vector<NPU_DTYPE_OUT> dram_data;

        while (std::getline(dram_file, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back(); // Remove carriage return if present
            // Skip empty lines
            if (line.empty())
                continue;

            try
            {
                int8_t byte_val = static_cast<int8_t>(std::stoul(line, nullptr, 16));
                dram_data.push_back(byte_val);
            }
            catch (...)
            {
                continue;
            }
        }
        dram_file.close();
        std::cout << "[TB] Completed parsing initial_dram.txt with " << dram_data.size() << " floats loaded." << std::endl;

        std::cout << "[TB] Mapping data to internal SRAM A & B..." << std::endl;
        int byte_ptr = 0;

        // Fetch Tensor A data to SRAM A for all 3 profiles (STD, APPROX, GATED)
        for (int phys_addr = 0; phys_addr < 2048 && byte_ptr < A_REGION_BYTES; phys_addr++)
        {
            for (int sw = 0; sw < subwords_a; sw++)
            {
                host_data_t act_pkt;
                for (int i = 0; i < 4; i++)
                {
                    act_pkt[i] = (byte_ptr < (int)dram_data.size()) ? dram_data[byte_ptr++] : 0;
                }
                for (int p = 0; p < 3; p++)
                    write_mem(p, get_srama_addr(phys_addr, sw), act_pkt);
            }
        }

        // Fetch Tensor B data to SRAM B for all 3 profiles (STD, APPROX, GATED)
        int b_loaded = 0;
        for (int phys_addr = 0; phys_addr < 2048 && b_loaded < B_REGION_BYTES; phys_addr++)
        {
            for (int sw = 0; sw < subwords_b; sw++)
            {
                host_data_t wei_pkt;
                for (int i = 0; i < 4; i++)
                {
                    wei_pkt[i] = (byte_ptr < (int)dram_data.size()) ? dram_data[byte_ptr++] : 0;
                    b_loaded++;
                }
                for (int p = 0; p < 3; p++)
                    write_mem(p, get_sramb_addr(phys_addr, sw), wei_pkt);
            }
        }
        std::cout << "[TB] Completed memory pre-load from initial_dram.txt." << std::endl;

        /*std::cout << "[TB] Mapping Tensor C (Preload C_init) to SRAM C..." << std::endl;
        int c_loaded = 0;
        int num_rows_c_init = 512 / EVAL_Y; // 64 hàng

        for (int phys_addr = 0; phys_addr < num_rows_c_init && c_loaded < C_REGION_BYTES; phys_addr++) {
            for(int sw = 0; sw < subwords_c; sw++){
                host_data_t psum_pkt;
                for(int i = 0; i < 4; i++){
                    int32_t c_val = 0;
                    if(byte_ptr + 3 < (int)dram_data.size()){
                        c_val = static_cast<int32_t>(
                                   (static_cast<uint8_t>(dram_data[byte_ptr])) |
                                   (static_cast<uint8_t>(dram_data[byte_ptr + 1]) << 8) |
                                   (static_cast<uint8_t>(dram_data[byte_ptr + 2]) << 16) |
                                   (static_cast<uint8_t>(dram_data[byte_ptr + 3]) << 24)
                                );
                        byte_ptr += 4;
                        c_loaded += 4;
                    }
                    psum_pkt[i] = c_val;
                }
                // Write to SRAMC for 3 profiles
                for(int p = 0; p < 3; p++) write_mem(p, get_sramc_addr(phys_addr, sw), psum_pkt);
            }
        }
        std::cout << "[TB] Completed Preload C_init. Bytes loaded: " << c_loaded << std::endl;
    */

        std::cout << "[TB] Input activations and weights programmed to double buffers." << std::endl;

        // Read back inputs to verify
        for (int p = 0; p < 3; p++)
        {
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

        std::cout << "[TB] Reading and execute instructions set from GoldenStimuli.txt..." << std::endl;
        std::ifstream stim_file("stimuli/GoldenStimuli.txt");
        if (!stim_file.is_open())
        {
            std::cerr << "[ERROR] Can't open GoldenStimuli.txt file!" << std::endl;
            sc_stop();
            return;
        }

        std::string stim_line;
        int command_count = 0;

        while (std::getline(stim_file, stim_line))
        {
            if (stim_line.empty())
                continue;

            std::stringstream ss(stim_line);
            uint64_t data_in_u64, addr_u64, wren_u64, rden_u64, waitflag_u64, data_out_u64, checkflag_u64;

            ss >> std::hex >> data_in_u64 >> addr_u64 >> wren_u64 >> rden_u64 >> waitflag_u64 >> data_out_u64 >> checkflag_u64;

            uint32_t raw_addr = static_cast<uint32_t>(addr_u64);
            uint32_t data_in = static_cast<uint32_t>(data_in_u64);
            bool wren = (wren_u64 == 1);
            bool rden = (rden_u64 == 1);
            StimRegion region = get_stim_region(raw_addr);

            if (is_controller_arg_write(raw_addr, wren))
            {
                uint32_t arg_idx = get_controller_arg_index(raw_addr);

                if (arg_idx < controller_args.size())
                {
                    controller_args[arg_idx] = data_in;
                }

                std::cout << "[CTRL ARG] idx=" << arg_idx
                          << " offset=0x" << std::hex << (raw_addr - 0x40000000)
                          << " data=0x" << data_in
                          << std::dec << std::endl;
            }

            if (is_controller_start_write(raw_addr, data_in, wren))
            {
                controller_started = true;

                std::cout << "\n=========================================\n";
                std::cout << "SAURIA CONTROLLER START DETECTED\n";
                std::cout << "=========================================\n";

                std::cout << "\nController args [0..21]\n";
                for (int i = 0; i < 22; i++)
                {
                    std::cout << "arg[" << i << "] = 0x"
                              << std::hex << controller_args[i]
                              << std::dec << std::endl;
                }

                std::cout << "\nPacked SAURIA accelerator regs args[22..]\n";
                for (int i = 22; i < 64; i++)
                {
                    if (controller_args[i] != 0)
                    {
                        std::cout << "arg[" << i << "] = 0x"
                                  << std::hex << controller_args[i]
                                  << std::dec << std::endl;
                    }
                }

                decoded_cfg_runtime = decode_sauria_packed_config(controller_args);

                decoded_cfg_valid = true;

                act_dram_base_runtime = controller_args[18];
                wei_dram_base_runtime = controller_args[19];
                out_dram_base_runtime = controller_args[20];

                print_decoded_sauria_config(decoded_cfg_runtime);

                apply_decoded_config_to_npu(decoded_cfg_runtime);

                uint32_t act_dram_base = act_dram_base_runtime;
                uint32_t wei_dram_base = wei_dram_base_runtime;
                uint32_t out_dram_base = out_dram_base_runtime;

                std::cout << "\n=========================================\n";
                std::cout << "DRAM BASE ADDRESSES FROM SAURIA CONTROLLER\n";
                std::cout << "=========================================\n";
                std::cout << "ACT DRAM BASE : 0x" << std::hex << act_dram_base << std::dec << "\n";
                std::cout << "WEI DRAM BASE : 0x" << std::hex << wei_dram_base << std::dec << "\n";
                std::cout << "OUT DRAM BASE : 0x" << std::hex << out_dram_base << std::dec << "\n";
                std::cout << "=========================================\n\n";

                std::string initial_dram_path = "stimuli/initial_dram.txt";
                std::vector<uint8_t> dram = load_initial_dram_file(initial_dram_path);

                print_dram_bytes(dram, act_dram_base, 64, "ACTIVATION REGION");
                print_dram_bytes(dram, wei_dram_base, 64, "WEIGHT REGION");
                print_dram_bytes(dram, out_dram_base, 64, "PSUM / OUTPUT REGION");

                uint32_t act_bytes = decoded_cfg_runtime.chlim;          // 3840
                uint32_t wei_bytes = decoded_cfg_runtime.wlim;           // 9216
                uint32_t psum_bytes = decoded_cfg_runtime.til_cklim * 4; // 512 * 4 = 2048

                preload_int8_region_to_srama(dram, act_dram_base, act_bytes);

                preload_int8_region_to_sramb(dram, wei_dram_base, wei_bytes);

                if (decoded_cfg_runtime.preload_en)
                {
                    preload_int32_region_to_sramc(dram, out_dram_base, psum_bytes);
                }

                // ---------------------------------------------------------
                // Double-buffer protocol:
                // select=000 => host accesses physical buffer 0
                // ---------------------------------------------------------
                o_select.write(sc_bv<3>("000"));
                wait(2);

                sramc_before_run = snapshot_sramc_words(
                    256,
                    "BEFORE NPU RUN - HOST READ BUFFER 0");

                // ---------------------------------------------------------
                // select=111 => NPU accesses physical buffer 0
                // ---------------------------------------------------------
                o_select.write(sc_bv<3>("111"));
                wait(2);

                debug_readback_sram_regions();
                std::cout << "=========================================\n\n";
            }

            // Current NpuTop only accepts SAURIA internal/core address space.
            // Skip CONTROLLER/DMA for now.
            if (!is_sauria_core_transaction(raw_addr))
            {
                if (command_count < 100)
                {
                    std::cout << "[TB SKIP] region=" << stim_region_name(region)
                              << " raw_addr=0x" << std::hex << raw_addr
                              << " data=0x" << data_in
                              << std::dec << std::endl;
                }

                command_count++;

                wait();

                if (waitflag_u64 > 0)
                {
                    wait(static_cast<int>(waitflag_u64));
                }

                continue;
            }

            uint32_t internal_addr = normalize_sauria_addr(raw_addr);

            host_data_t stim_packet;
            host_mask_t stim_mask;

            make_stim_packet(
                internal_addr,
                data_in,
                wren,
                stim_packet,
                stim_mask);

            if (wren && is_cfg_addr(internal_addr))
            {
                std::cout << "[TB CFG WRITE] raw_addr=0x"
                          << std::hex << raw_addr
                          << " internal=0x" << internal_addr
                          << " data=0x" << data_in
                          << std::dec << std::endl;
            }

            // STD
            o_host_addr_std.write(internal_addr);
            o_host_wren_std.write(wren);
            o_host_rden_std.write(rden);
            o_host_wdata_std.write(stim_packet);
            o_host_wmask_std.write(stim_mask);

            // APPROX
            o_host_addr_approx.write(internal_addr);
            o_host_wren_approx.write(wren);
            o_host_rden_approx.write(rden);
            o_host_wdata_approx.write(stim_packet);
            o_host_wmask_approx.write(stim_mask);

            // GATED
            o_host_addr_gated.write(internal_addr);
            o_host_wren_gated.write(wren);
            o_host_rden_gated.write(rden);
            o_host_wdata_gated.write(stim_packet);
            o_host_wmask_gated.write(stim_mask);

            wait();
            command_count++;

            host_mask_t zero_mask;
            zero_mask.data.fill(false);

            o_host_wren_std.write(false);
            o_host_rden_std.write(false);
            o_host_wmask_std.write(zero_mask);

            o_host_wren_approx.write(false);
            o_host_rden_approx.write(false);
            o_host_wmask_approx.write(zero_mask);

            o_host_wren_gated.write(false);
            o_host_rden_gated.write(false);
            o_host_wmask_gated.write(zero_mask);

            wait();

            if (waitflag_u64 > 0)
            {
                wait(static_cast<int>(waitflag_u64));
            }
        }

        stim_file.close();
        std::cout << "[TB] Executed " << command_count << " transactions from GoldenStimuli.txt" << std::endl;

        std::cout << "[TB] Triggering NPU execution..." << std::endl;
        o_start_std.write(true);
        o_start_approx.write(true);
        o_start_gated.write(true);

        wait(2);

        o_start_std.write(false);
        o_start_approx.write(false);
        o_start_gated.write(false);

        std::cout << "[TB] Waiting for NPU to complete execution..." << std::endl;

        bool completed = false;

        for (int cycle = 0; cycle < 20000; cycle++)
        {
            wait();

            if (i_done_std.read())
            {
                completed = true;
                std::cout << "[TB] STD NPU done at cycle " << cycle << std::endl;
                break;
            }
        }

        if (!completed)
        {
            std::cout << "[TB WARNING] STD NPU execution timeout." << std::endl;
        }

        // ---------------------------------------------------------
        // Switch back so host can read physical buffer 0,
        // which was used by NPU during execution.
        // ---------------------------------------------------------
        o_select.write(sc_bv<3>("000"));
        wait(2);
        std::vector<host_data_t> sramc_after_run = snapshot_sramc_words(256, "AFTER NPU RUN");
        compare_sramc_snapshots(sramc_before_run, sramc_after_run);

        std::string gold_dram_path = "stimuli/gold_dram.txt";
        std::vector<uint8_t> gold_dram = load_initial_dram_file(gold_dram_path);
        if (!decoded_cfg_valid)
        {
            std::cerr << "[TB ERROR] decoded_cfg_runtime is not valid. "
                      << "Controller start was not detected or config was not decoded."
                      << std::endl;
        }
        else
        {
            uint32_t n_output_elems = decoded_cfg_runtime.til_cklim;

            std::vector<uint8_t> gold_dram = load_initial_dram_file(gold_dram_path);
            std::vector<int32_t> sramc_out = dump_sramc_psm_layout_i32(decoded_cfg_runtime);
            std::vector<int32_t> gold_out = load_gold_output_i32(gold_dram, out_dram_base_runtime, n_output_elems);

            compare_sramc_with_gold(sramc_out, gold_out);
            analyze_value_match_between_sramc_and_gold(sramc_out, gold_out);
        }
        if (!decoded_cfg_valid)
        {
            std::cerr << "[TB ERROR] decoded_cfg_runtime is not valid. "
                      << "Cannot compare SRAMC with gold_dram."
                      << std::endl;
        }
        else
        {
            uint32_t n_output_elems = decoded_cfg_runtime.til_cklim;

            std::vector<uint8_t> gold_dram =
                load_initial_dram_file(gold_dram_path);

            std::vector<int32_t> sramc_out =
                dump_sramc_linear_i32(n_output_elems);

            std::vector<int32_t> gold_out =
                load_gold_output_i32(
                    gold_dram,
                    out_dram_base_runtime,
                    n_output_elems);

            compare_sramc_with_gold(
                sramc_out,
                gold_out);
        }

        debug_output_sramc_after_run();

        std::cout << "[Verification] Reading gold_dram.txt to compare results..." << std::endl;
        std::ifstream gold_file("stimuli/gold_dram.txt");
        if (!gold_file.is_open())
        {
            std::cerr << "[ERROR] Can't open gold_dram.txt file!" << std::endl;
            sc_stop();
            return;
        }
        std::vector<NPU_DTYPE_IN> golden_raw_bytes;
        int bytes_per_out = sizeof(NPU_DTYPE_OUT); // 4 byte if int32_t

        std::string g_line;
        while (std::getline(gold_file, g_line))
        {
            if (!g_line.empty() && g_line.back() == '\r')
                g_line.pop_back(); // Remove carriage return if present
            if (g_line.empty())
                continue;
            uint8_t b = static_cast<uint8_t>(std::stoul(g_line, nullptr, 16));
            golden_raw_bytes.push_back(b);
        }
        gold_file.close();

        // Swap double buffers back to Host AXI side to HOST can read SRAM
        wait();
        o_select.write(sc_bv<3>("000"));
        wait(2);

        /*
            NOTE:
                - File folde_dram.txt contains entire Memory (Tensor A + Tensor B + Tensor C) in sequence, but we only care about the output Tensor C part for verification, which starts from address 0x2000 (SRAMC_OFFSET).
                - Only offset_C (the number element float of A & B) to point to the output results in SRAM C, which is 64 floats (16 packets) for 8x8 array with 4-float packet.
                - Temporary  = 0, when run test from Python, offset_C = size(A) + size(B)

            Ex:
                Input: 64x18x34
                weight: 64x64x3x3

                A size = 64 x 18 x 34 = 39168 bytes
                B size = 64 x 64 x 3 x 3 = 36864 bytes

                C size = A size + B size = 76032 bytes
        */

        // Test Sauria: Tile size [16 4 8] -> 16 * 4 * 8 = 512
        int C_total_elements = 512;
        int C_bytes = C_total_elements * 4;
        int byte_offset_C = golden_raw_bytes.size() - C_bytes; // Assuming C is at the end of gold_dram.txt, adjust if needed

        if (byte_offset_C < 0)
        {
            std::cerr << "[ERROR] File gold_dram.txt is too short, does not contain Tensor C!" << std::endl;
            byte_offset_C = 0;
        }

        int errors_std = 0;
        int total_approx_error = 0;
        int max_approx_error = 0;

        // Debug: In ra 4 byte đầu tiên tại vùng Offset dự kiến
        std::cout << "[DEBUG] Tensor C Bytes at offset " << byte_offset_C << ": ";
        for (int k = 0; k < 4; k++)
            std::cout << std::hex << (int)golden_raw_bytes[byte_offset_C + k] << " ";
        std::cout << std::dec << std::endl;

        std::cout << "\n==========================================================================================" << std::endl;
        std::cout << "   VERIFICATION REPORT: EXACT vs APPROXIMATE COMPUTING - NPU SystemC vs SAURIA core                     " << std::endl;
        std::cout << "==========================================================================================" << std::endl;
        std::cout << " Index | Golden (Python) | NPU Exact (STD) | NPU Approx (APP) | STD Match? | APP Error  " << std::endl;
        std::cout << "-------+-----------------+-----------------+------------------+------------+------------" << std::endl;

        int num_rows_c = C_total_elements / EVAL_Y; // 512 / 8 = 64 rows, each row has 8 elements, total 512 elements

        for (int phys_addr = 0; phys_addr < num_rows_c; phys_addr++)
        {
            for (int sw = 0; sw < subwords_c; sw++)
            {
                host_data_t chunk_std = read_mem(0, get_sramc_addr(phys_addr, sw));
                host_data_t chunk_approx = read_mem(1, get_sramc_addr(phys_addr, sw));
                for (int i = 0; i < 4; i++)
                {
                    // Total index on 1 Tile
                    int global_idx = (phys_addr * EVAL_Y) + (sw * 4) + i;

                    if (global_idx < C_total_elements)
                    {
                        int32_t val_std = static_cast<int32_t>(chunk_std[i]);
                        int32_t val_approx = static_cast<int32_t>(chunk_approx[i]);

                        int32_t val_gold = 0;
                        int target_byte_idx = byte_offset_C + (global_idx * bytes_per_out);

                        if (target_byte_idx + 3 < (int)golden_raw_bytes.size())
                        {
                            val_gold = static_cast<int32_t>(
                                (static_cast<uint8_t>(golden_raw_bytes[target_byte_idx])) |
                                (static_cast<uint8_t>(golden_raw_bytes[target_byte_idx + 1]) << 8) |
                                (static_cast<uint8_t>(golden_raw_bytes[target_byte_idx + 2]) << 16) |
                                (static_cast<uint8_t>(golden_raw_bytes[target_byte_idx + 3]) << 24));
                        }

                        bool match_std = (val_std == val_gold);
                        if (!match_std)
                            errors_std++;

                        int32_t app_error = std::abs(val_approx - val_std);
                        total_approx_error += app_error;
                        if (app_error > max_approx_error)
                            max_approx_error = app_error;

                        // Only print error elements
                        if (!match_std || global_idx < 5 || global_idx > 507)
                        {
                            std::cout << " " << std::setw(4) << global_idx
                                      << " | " << std::setw(15) << val_gold
                                      << " | " << std::setw(15) << val_std
                                      << " | " << std::setw(16) << val_approx
                                      << " | " << (match_std ? "   PASS   " : "   FAIL   ")
                                      << " | " << std::setw(10) << app_error << std::endl;
                        }
                    }
                }
            }
        }
        std::cout << "==========================================================================================" << std::endl;
        std::cout << "[APPROX METRICS] Total error accumulator: " << total_approx_error << std::endl;
        std::cout << "[APPROX METRICS] Max error per element : " << max_approx_error << std::endl;
        std::cout << "=============================================================================" << std::endl;

        if (errors_std == 0)
        {
            std::cout << "[RESULT] TEST PASSED: Standard INT8 profile matches golden reference with ZERO errors!" << std::endl;
        }
        else
        {
            std::cout << "[RESULT] Standard INT8 profile has " << errors_std << " mismatches with golden reference." << std::endl;
        }

        wait(20);
        sc_stop();
        return;
    }
};

int sc_main(int argc, char *argv[])
{
    sc_clock clk("clk", 10, SC_NS);

    int approx_mul_type = 0;
    int approx_add_type = 0;

    // Ex: ./tb_evaluate 1 2 -> Run with Approx Mult Type 1 and Approx Add Type 2
    if (argc >= 3)
    {
        approx_mul_type = std::stoi(argv[1]);
        approx_add_type = std::stoi(argv[2]);
        std::cout << "[TB] Approximate Multiplier Type: " << approx_mul_type << ", Approximate Adder Type: " << approx_add_type << std::endl;
    }
    else
    {
        std::cout << "[TB] No approximation types provided via command line. Using default (0, 0) for both multiplier and adder." << std::endl;
    }

    // Profile configurations
    PeConfig cfg_std;
    cfg_std.arithmetic_type = 0; // int
    cfg_std.mul_type = 0;
    cfg_std.add_type = 0;
    cfg_std.stages_mul = 1;
    cfg_std.intermediate_pipeline_stage = true;
    cfg_std.zero_gating_mult = false; // No gating

    PeConfig cfg_approx;
    cfg_approx.arithmetic_type = 0; // int
    cfg_approx.mul_type = approx_mul_type;
    cfg_approx.add_type = approx_add_type;
    cfg_approx.m_approx = 0.85f; // 15% scaling error
    cfg_approx.a_approx = 0.95f; // 5% scaling error
    cfg_approx.stages_mul = 1;
    cfg_approx.intermediate_pipeline_stage = true;
    cfg_approx.zero_gating_mult = false;

    PeConfig cfg_gated;
    cfg_gated.arithmetic_type = 0;
    cfg_gated.mul_type = 0;
    cfg_gated.add_type = 0;
    cfg_gated.zero_gating_mult = true; // skip zero

    // NpuTop instances for each profile
    NpuTop<EVAL_X, EVAL_Y, NPU_DTYPE_IN, NPU_DTYPE_IN, NPU_DTYPE_OUT, 1024, 1024, 2048, 16, EVAL_X + EVAL_Y, 1> npu_std("NpuTop_std", cfg_std);
    NpuTop<EVAL_X, EVAL_Y, NPU_DTYPE_IN, NPU_DTYPE_IN, NPU_DTYPE_OUT, 1024, 1024, 2048, 16, EVAL_X + EVAL_Y, 1> npu_approx("NpuTop_approx", cfg_approx);
    NpuTop<EVAL_X, EVAL_Y, NPU_DTYPE_IN, NPU_DTYPE_IN, NPU_DTYPE_OUT, 1024, 1024, 2048, 16, EVAL_X + EVAL_Y, 1> npu_gated("NpuTop_gated", cfg_gated);
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

    npu_std.i_clk(clk);
    npu_std.i_rstn(rstn);
    npu_std.i_soft_reset(soft_reset);
    npu_approx.i_clk(clk);
    npu_approx.i_rstn(rstn);
    npu_approx.i_soft_reset(soft_reset);
    npu_gated.i_clk(clk);
    npu_gated.i_rstn(rstn);
    npu_gated.i_soft_reset(soft_reset);

    npu_std.i_threshold(threshold);
    npu_std.i_select(select);
    npu_approx.i_threshold(threshold);
    npu_approx.i_select(select);
    npu_gated.i_threshold(threshold);
    npu_gated.i_select(select);

    // Done / start bindings
    sc_signal<bool> start_std, done_std, deadlock_std;
    sc_signal<bool> start_approx, done_approx, deadlock_approx;
    sc_signal<bool> start_gated, done_gated, deadlock_gated;

    tb.o_start_std(start_std);
    tb.i_done_std(done_std);
    tb.i_deadlock_std(deadlock_std);
    tb.o_start_approx(start_approx);
    tb.i_done_approx(done_approx);
    tb.i_deadlock_approx(deadlock_approx);
    tb.o_start_gated(start_gated);
    tb.i_done_gated(done_gated);
    tb.i_deadlock_gated(deadlock_gated);

    npu_std.i_start(start_std);
    npu_std.o_done(done_std);
    npu_std.o_deadlock(deadlock_std);
    npu_approx.i_start(start_approx);
    npu_approx.o_done(done_approx);
    npu_approx.o_deadlock(deadlock_approx);
    npu_gated.i_start(start_gated);
    npu_gated.o_done(done_gated);
    npu_gated.o_deadlock(deadlock_gated);

    // Memory ports bindings - STD
    sc_signal<uint32_t> addr_std;
    sc_signal<bool> wren_std, rden_std;
    sc_signal<host_data_t> wdata_std, rdata_std;
    sc_signal<host_mask_t> wmask_std;

    tb.o_host_addr_std(addr_std);
    tb.o_host_wren_std(wren_std);
    tb.o_host_rden_std(rden_std);
    tb.o_host_wdata_std(wdata_std);
    tb.o_host_wmask_std(wmask_std);
    tb.i_host_rdata_std(rdata_std);

    npu_std.i_host_addr(addr_std);
    npu_std.i_host_wren(wren_std);
    npu_std.i_host_rden(rden_std);
    npu_std.i_host_wdata(wdata_std);
    npu_std.i_host_wmask(wmask_std);
    npu_std.o_host_rdata(rdata_std);

    // Memory ports bindings - APPROX
    sc_signal<uint32_t> addr_approx;
    sc_signal<bool> wren_approx, rden_approx;
    sc_signal<host_data_t> wdata_approx, rdata_approx;
    sc_signal<host_mask_t> wmask_approx;

    tb.o_host_addr_approx(addr_approx);
    tb.o_host_wren_approx(wren_approx);
    tb.o_host_rden_approx(rden_approx);
    tb.o_host_wdata_approx(wdata_approx);
    tb.o_host_wmask_approx(wmask_approx);
    tb.i_host_rdata_approx(rdata_approx);

    npu_approx.i_host_addr(addr_approx);
    npu_approx.i_host_wren(wren_approx);
    npu_approx.i_host_rden(rden_approx);
    npu_approx.i_host_wdata(wdata_approx);
    npu_approx.i_host_wmask(wmask_approx);
    npu_approx.o_host_rdata(rdata_approx);

    // Memory ports bindings - GATED
    sc_signal<uint32_t> addr_gated;
    sc_signal<bool> wren_gated, rden_gated;
    sc_signal<host_data_t> wdata_gated, rdata_gated;
    sc_signal<host_mask_t> wmask_gated;

    tb.o_host_addr_gated(addr_gated);
    tb.o_host_wren_gated(wren_gated);
    tb.o_host_rden_gated(rden_gated);
    tb.o_host_wdata_gated(wdata_gated);
    tb.o_host_wmask_gated(wmask_gated);
    tb.i_host_rdata_gated(rdata_gated);

    npu_gated.i_host_addr(addr_gated);
    npu_gated.i_host_wren(wren_gated);
    npu_gated.i_host_rden(rden_gated);
    npu_gated.i_host_wdata(wdata_gated);
    npu_gated.i_host_wmask(wmask_gated);
    npu_gated.o_host_rdata(rdata_gated);

    sc_start();
    return 0;
}
