// Copyright 2026 Barcelona Supercomputing Center (BSC)
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
//
// SystemC Model for SAURIA NPU Core
// Top-Level NPU Wrapper (Connecting Sram, Control, Feeders, Array, PSM, ConfigRegs)

#ifndef SAURIA_NPU_TOP_H
#define SAURIA_NPU_TOP_H

#include "sauria_types.h"
#include "config/config_regs.h"
#include "control/main_controller.h"
#include "data_feeder/ifmap_feeder.h"
#include "data_feeder/wei_feeder.h"
#include "systolic_array/sa_array.h"
#include "psm/psm_top.h"
#include "sram/sram_top.h"

namespace sauria {

    template <
        int X_DIM = 32,
        int Y_DIM = 32,
        typename T_ACT = float,
        typename T_WEI = float,
        typename T_PSUM = float,
        int SRAMA_CAP = 1024,
        int SRAMB_CAP = 1024,
        int SRAMC_CAP = 2048,
        int FIFO_DEPTH = 16,
        int PE_LAT = X_DIM + Y_DIM,
        int EXTRA_CSREG = 1
    >
    class NpuTop : public sc_module {
    public:
        // Clocks & Resets
        sc_in<bool> i_clk{"i_clk"};
        sc_in<bool> i_rstn{"i_rstn"};
        sc_in<bool> i_soft_reset{"i_soft_reset"};

        // Host Control Interface
        sc_in<bool>  i_start{"i_start"};
        sc_out<bool> o_done{"o_done"};
        sc_out<bool> o_deadlock{"o_deadlock"};

        // Host Memory Port (AXI interface modeling)
        sc_in<uint32_t>     i_host_addr{"i_host_addr"};
        sc_in<bool>         i_host_wren{"i_host_wren"};
        sc_in<bool>         i_host_rden{"i_host_rden"};
        sc_in<host_data_t>  i_host_wdata{"i_host_wdata"};
        sc_in<host_mask_t>  i_host_wmask{"i_host_wmask"};
        sc_out<host_data_t> o_host_rdata{"o_host_rdata"};

        // Runtime config configurations (threshold, select, and tiled loop counts)
        sc_in<float>        i_threshold{"i_threshold"};
        sc_in<sc_bv<3>>     i_select{"i_select"};

        // Constructor
        SC_HAS_PROCESS(NpuTop);
        NpuTop(sc_module_name nm, const PeConfig& pe_cfg = PeConfig()) : sc_module(nm) {
            // Instantiate submodules
            sram_inst        = new Sram<X_DIM, Y_DIM, T_ACT, T_WEI, T_PSUM, SRAMA_CAP, SRAMB_CAP, SRAMC_CAP>("sram_inst");
            ctrl_inst        = new Control<X_DIM, Y_DIM, PE_LAT, EXTRA_CSREG>("ctrl_inst");
            act_feeder       = new IfmapFeeder<Y_DIM, T_ACT, SRAMA_CAP, FIFO_DEPTH>("act_feeder");
            wei_feeder       = new WeightFeeder<X_DIM, T_WEI, SRAMB_CAP, FIFO_DEPTH>("wei_feeder");
            array_inst       = new SystolicArray<X_DIM, Y_DIM, T_ACT, T_WEI, T_PSUM>("array_inst", pe_cfg);
            psm_inst         = new Psm<X_DIM, Y_DIM, T_PSUM, SRAMC_CAP>("psm_inst");
            config_regs_inst = new ConfigRegs<32, 32, X_DIM, Y_DIM, 2, 15, 15, 15, 8, DILP_W, 8>("config_regs_inst");

            SC_METHOD(host_rdata_mux);
            sensitive << i_host_addr << s_host_rdata_sram << s_host_rdata_cfg;

            SC_METHOD(start_reset_logic);
            sensitive << i_start << s_cfg_start << i_soft_reset << s_cfg_soft_reset << s_ctrl_done;

            SC_METHOD(debug_print);
            sensitive << i_clk.pos();

            // ----------------------------------------------------
            // Signal Interconnections
            // ----------------------------------------------------

            // 1. Clock and Reset routing
            sram_inst->i_clk(i_clk);
            sram_inst->i_rstn(i_rstn);
            sram_inst->i_deepsleep(s_false);
            sram_inst->i_powergate(s_false);
            sram_inst->i_select(i_select);

            ctrl_inst->i_clk(i_clk);
            ctrl_inst->i_rstn(i_rstn);
            ctrl_inst->i_soft_reset(s_ctrl_reset_internal);

            act_feeder->i_clk(i_clk);
            act_feeder->i_rstn(i_rstn);

            wei_feeder->i_clk(i_clk);
            wei_feeder->i_rstn(i_rstn);

            array_inst->i_clk(i_clk);
            array_inst->i_rstn(i_rstn);

            psm_inst->i_clk(i_clk);
            psm_inst->i_rstn(i_rstn);

            config_regs_inst->i_clk(i_clk);
            config_regs_inst->i_rstn(i_rstn);
            config_regs_inst->i_host_addr(i_host_addr);
            config_regs_inst->i_host_wren(i_host_wren);
            config_regs_inst->i_host_rden(i_host_rden);
            config_regs_inst->i_host_wdata(i_host_wdata);
            config_regs_inst->i_host_wmask(i_host_wmask);
            config_regs_inst->o_host_rdata(s_host_rdata_cfg);
            config_regs_inst->i_done(s_ctrl_done);
            config_regs_inst->i_soft_reset_in(i_soft_reset);
            config_regs_inst->o_start(s_cfg_start);
            config_regs_inst->o_soft_reset(s_cfg_soft_reset);

            // 2. Host Interface Routing to SRAM
            sram_inst->i_host_addr(i_host_addr);
            sram_inst->i_host_wren(i_host_wren);
            sram_inst->i_host_rden(i_host_rden);
            sram_inst->i_host_wdata(i_host_wdata);
            sram_inst->i_host_wmask(i_host_wmask);
            sram_inst->o_host_rdata(s_host_rdata_sram);

            // 3. FSM Controller bindings
            ctrl_inst->i_start(s_start_internal);
            ctrl_inst->o_done(s_ctrl_done);
            ctrl_inst->o_feed_deadlock(o_deadlock);

            // PSM done feedbacks to Control FSM
            ctrl_inst->i_outbuf_done(s_psm_done);
            ctrl_inst->i_finalwrite(s_psm_finalwrite);
            ctrl_inst->i_shift_done(s_psm_shift_done);

            // Feeder done feedbacks to Control FSM
            ctrl_inst->i_act_done(s_act_done);
            ctrl_inst->i_act_til_done(s_act_til_done);
            ctrl_inst->i_act_fifo_empty(s_act_fifo_empty);
            ctrl_inst->i_act_fifo_full(s_act_fifo_full);
            ctrl_inst->i_act_stall(s_act_stall);

            ctrl_inst->i_wei_done(s_wei_done);
            ctrl_inst->i_wei_til_done(s_wei_til_done);
            ctrl_inst->i_wei_fifo_empty(s_wei_fifo_empty);
            ctrl_inst->i_wei_fifo_full(s_wei_fifo_full);
            ctrl_inst->i_wei_stall(s_wei_stall);

            // Control outputs routed to Feeders
            ctrl_inst->o_act_feeder_en(s_act_feeder_en);
            ctrl_inst->o_act_feeder_clear(s_act_feeder_clear);
            ctrl_inst->o_act_start(s_act_start);
            ctrl_inst->o_act_valid(s_act_valid);
            ctrl_inst->o_act_finalpush(s_act_finalpush);
            ctrl_inst->o_act_cnt_en(s_act_cnt_en);
            ctrl_inst->o_act_cnt_clear(s_act_cnt_clear);
            ctrl_inst->o_act_clearfifo(s_act_clearfifo);
            ctrl_inst->o_act_pop_en(s_act_pop_en);
            ctrl_inst->o_act_finalctx(s_act_finalctx);

            ctrl_inst->o_wei_feeder_en(s_wei_feeder_en);
            ctrl_inst->o_wei_feeder_clear(s_wei_feeder_clear);
            ctrl_inst->o_wei_start(s_wei_start);
            ctrl_inst->o_wei_valid(s_wei_valid);
            ctrl_inst->o_wei_finalpush(s_wei_finalpush);
            ctrl_inst->o_wei_cnt_en(s_wei_cnt_en);
            ctrl_inst->o_wei_cnt_clear(s_wei_cnt_clear);
            ctrl_inst->o_wei_clearfifo(s_wei_clearfifo);
            ctrl_inst->o_wei_pop_en(s_wei_pop_en);
            ctrl_inst->o_wei_cswitch(s_wei_cswitch);

            // Control outputs routed to PSM
            ctrl_inst->o_outbuf_start(s_psm_start);
            ctrl_inst->o_outbuf_reset(s_psm_reset);

            // Control outputs routed to Systolic Array
            ctrl_inst->o_sa_clear(s_sa_clear);
            ctrl_inst->o_pipeline_en(s_pipeline_en);
            ctrl_inst->o_cswitch_arr(s_cswitch_arr);

            // 4. Feeder bindings
            act_feeder->i_feeder_en(s_act_feeder_en);
            act_feeder->i_feeder_clear(s_act_feeder_clear);
            act_feeder->i_start(s_act_start);
            act_feeder->i_valid(s_act_valid);
            act_feeder->i_finalpush(s_act_finalpush);
            act_feeder->i_cnt_en(s_act_cnt_en);
            act_feeder->i_cnt_clear(s_act_cnt_clear);
            act_feeder->i_clearfifo(s_act_clearfifo);
            act_feeder->i_pop_en(s_act_pop_en);
            act_feeder->i_finalctx(s_act_finalctx);

            act_feeder->o_act_done(s_act_done);
            act_feeder->o_act_til_done(s_act_til_done);
            act_feeder->o_fifo_empty(s_act_fifo_empty);
            act_feeder->o_fifo_full(s_act_fifo_full);
            act_feeder->o_stall(s_act_stall);

            act_feeder->o_srama_addr(s_srama_addr);
            act_feeder->o_srama_rden(s_srama_rden);
            act_feeder->i_srama_data(s_srama_data);
            act_feeder->o_act_arr(s_act_arr);

            wei_feeder->i_feeder_en(s_wei_feeder_en);
            wei_feeder->i_feeder_clear(s_wei_feeder_clear);
            wei_feeder->i_start(s_wei_start);
            wei_feeder->i_valid(s_wei_valid);
            wei_feeder->i_finalpush(s_wei_finalpush);
            wei_feeder->i_cnt_en(s_wei_cnt_en);
            wei_feeder->i_cnt_clear(s_wei_cnt_clear);
            wei_feeder->i_clearfifo(s_wei_clearfifo);
            wei_feeder->i_pop_en(s_wei_pop_en);
            wei_feeder->i_cswitch(s_wei_cswitch);

            wei_feeder->o_wei_done(s_wei_done);
            wei_feeder->o_wei_til_done(s_wei_til_done);
            wei_feeder->o_fifo_empty(s_wei_fifo_empty);
            wei_feeder->o_fifo_full(s_wei_fifo_full);
            wei_feeder->o_stall(s_wei_stall);

            wei_feeder->o_sramb_addr(s_sramb_addr);
            wei_feeder->o_sramb_rden(s_sramb_rden);
            wei_feeder->i_sramb_data(s_sramb_data);
            wei_feeder->o_wei_arr(s_wei_arr);

            // 5. SRAM Core Interface bindings (Accelerator-side)
            sram_inst->i_srama_addr(s_srama_addr);
            sram_inst->i_srama_rden(s_srama_rden);
            sram_inst->o_srama_data(s_srama_data);

            sram_inst->i_sramb_addr(s_sramb_addr);
            sram_inst->i_sramb_rden(s_sramb_rden);
            sram_inst->o_sramb_data(s_sramb_data);

            sram_inst->i_sramc_wdata(s_sramc_wdata);
            sram_inst->i_sramc_addr(s_sramc_addr);
            sram_inst->i_sramc_wren(s_sramc_wren);
            sram_inst->i_sramc_rden(s_sramc_rden);
            sram_inst->i_sramc_wmask(s_sramc_wmask);
            sram_inst->o_sramc_rdata(s_sramc_rdata);

            // 6. Systolic Array bindings
            array_inst->i_threshold(i_threshold);
            array_inst->i_act_arr(s_act_arr);
            array_inst->i_wei_arr(s_wei_arr);
            array_inst->i_c_arr(s_psm_to_sa_c);
            array_inst->o_c_arr(s_sa_to_psm_c);
            array_inst->i_pipeline_en(s_pipeline_en);
            array_inst->i_cscan_en(s_cscan_en);
            array_inst->i_cswitch_arr(s_cswitch_arr);
            array_inst->i_sa_clear(s_sa_clear);

            // 7. PSM bindings
            psm_inst->i_c_arr(s_sa_to_psm_c);
            psm_inst->o_c_arr(s_psm_to_sa_c);
            
            psm_inst->i_sramc_rdata(s_sramc_rdata);
            psm_inst->o_sramc_addr(s_sramc_addr);
            psm_inst->o_sramc_wren(s_sramc_wren);
            psm_inst->o_sramc_rden(s_sramc_rden);
            psm_inst->o_sramc_wmask(s_sramc_wmask);
            psm_inst->o_sramc_wdata(s_sramc_wdata);

            psm_inst->i_fsm_start(s_psm_start);
            psm_inst->i_fsm_reset(s_psm_reset);
            psm_inst->i_pipeline_en(s_pipeline_en);

            psm_inst->o_done(s_psm_done);
            psm_inst->o_finalwrite(s_psm_finalwrite);
            psm_inst->o_shift_done(s_psm_shift_done);
            psm_inst->o_cscan_en(s_cscan_en);

            // Connect configurations from Config Registers
            ctrl_inst->i_incntlim(s_incntlim);
            ctrl_inst->i_act_reps(s_act_reps);
            ctrl_inst->i_wei_reps(s_wei_reps);

            act_feeder->i_act_incntlim(s_act_incntlim);
            act_feeder->i_act_incntstep(s_act_incntstep);
            act_feeder->i_act_outcntlim(s_act_outcntlim);
            act_feeder->i_act_outcntstep(s_act_outcntstep);
            act_feeder->i_act_dil_pat(s_dil_pat);

            wei_feeder->i_wei_incntlim(s_wei_incntlim);
            wei_feeder->i_wei_incntstep(s_wei_incntstep);

            psm_inst->i_cxlim(s_cxlim);
            psm_inst->i_cxstep(s_cxstep);
            psm_inst->i_cklim(s_cklim);
            psm_inst->i_ckstep(s_ckstep);
            psm_inst->i_til_cylim(s_til_cylim);
            psm_inst->i_til_cystep(s_til_cystep);
            psm_inst->i_til_cklim(s_til_cklim);
            psm_inst->i_til_ckstep(s_til_ckstep);
            psm_inst->i_ncontexts(s_ncontexts);
            psm_inst->i_preload_en(s_preload_en);
            psm_inst->i_rows_active(s_rows_active);

            // Config registers module bindings
            config_regs_inst->o_incntlim(s_incntlim);
            config_regs_inst->o_act_reps(s_act_reps);
            config_regs_inst->o_wei_reps(s_wei_reps);
            config_regs_inst->o_dil_pat(s_dil_pat);
            config_regs_inst->o_rows_active(s_rows_active);

            config_regs_inst->o_act_incntlim(s_act_incntlim);
            config_regs_inst->o_act_incntstep(s_act_incntstep);
            config_regs_inst->o_act_outcntlim(s_act_outcntlim);
            config_regs_inst->o_act_outcntstep(s_act_outcntstep);
            config_regs_inst->o_wei_incntlim(s_wei_incntlim);
            config_regs_inst->o_wei_incntstep(s_wei_incntstep);
            config_regs_inst->o_cxlim(s_cxlim);
            config_regs_inst->o_cxstep(s_cxstep);
            config_regs_inst->o_cklim(s_cklim);
            config_regs_inst->o_ckstep(s_ckstep);
            config_regs_inst->o_til_cylim(s_til_cylim);
            config_regs_inst->o_til_cystep(s_til_cystep);
            config_regs_inst->o_til_cklim(s_til_cklim);
            config_regs_inst->o_til_ckstep(s_til_ckstep);
            config_regs_inst->o_ncontexts(s_ncontexts);
            config_regs_inst->o_preload_en(s_preload_en);
        }

        void host_rdata_mux() {
            uint32_t addr = i_host_addr.read();
            uint32_t mem_region = addr & SAURIA_MEM_ADDR_MASK;
            if (mem_region == CFG_REGS_OFFSET) {
                o_host_rdata.write(s_host_rdata_cfg.read());
            } else {
                o_host_rdata.write(s_host_rdata_sram.read());
            }
        }

        void start_reset_logic() {
            s_start_internal.write(i_start.read() || s_cfg_start.read());
            s_ctrl_reset_internal.write(i_soft_reset.read() || s_cfg_soft_reset.read());
            o_done.write(s_ctrl_done.read());
        }

        void debug_print() {
            if (std::string(name()) == "NpuTop_std") {
                act_vector_t<Y_DIM, T_ACT> act = s_act_arr.read();
                wei_vector_t<X_DIM, T_WEI> wei = s_wei_arr.read();
                bool act_nonzero = false;
                for (int y = 0; y < Y_DIM; y++) {
                    if (act[y] != 0.0f) act_nonzero = true;
                }
                bool wei_nonzero = false;
                for (int x = 0; x < X_DIM; x++) {
                    if (wei[x] != 0.0f) wei_nonzero = true;
                }
                
                bool cswitch_active = (s_cswitch_arr.read().to_uint() != 0);
                if (act_nonzero || wei_nonzero || s_sramc_wren.read() || s_ctrl_done.read() || cswitch_active || s_pipeline_en.read()) {
                    std::cout << "[DEBUG " << name() << " @ " << sc_time_stamp() << "] "
                              << "act_en=" << s_act_feeder_en.read() 
                              << " wei_en=" << s_wei_feeder_en.read()
                              << " pipeline_en=" << s_pipeline_en.read()
                              << " cscan_en=" << s_cscan_en.read()
                              << " s_ctrl_done=" << s_ctrl_done.read()
                              << std::endl;
                              
                    if (act_nonzero) {
                        std::cout << "  Activations fed: [";
                        for (int y = 0; y < Y_DIM; y++) std::cout << act[y] << (y==Y_DIM-1 ? "" : ", ");
                        std::cout << "]" << std::endl;
                    }
                    if (wei_nonzero) {
                        std::cout << "  Weights fed: [";
                        for (int x = 0; x < X_DIM; x++) std::cout << wei[x] << (x==X_DIM-1 ? "" : ", ");
                        std::cout << "]" << std::endl;
                    }
                    if (cswitch_active) {
                        std::cout << "  Context Switch pulsed: " << s_cswitch_arr.read() << std::endl;
                    }
                    
                    // Print selected PE accumulator values
                    std::cout << "  PE(0,0) mac=" << array_inst->get_pe_mac(0,0) << " mac_sc=" << array_inst->get_pe_mac_sc(0,0)
                              << " | PE(1,0) mac=" << array_inst->get_pe_mac(1,0) << " mac_sc=" << array_inst->get_pe_mac_sc(1,0)
                              << " | PE(0,1) mac=" << array_inst->get_pe_mac(0,1) << " mac_sc=" << array_inst->get_pe_mac_sc(0,1)
                              << " | PE(7,7) mac=" << array_inst->get_pe_mac(7,7) << " mac_sc=" << array_inst->get_pe_mac_sc(7,7)
                              << std::endl;

                    if (s_sramc_wren.read()) {
                        psum_vector_t<Y_DIM, T_PSUM> cdata = s_sramc_wdata.read();
                        std::cout << "  SRAM C Write at addr " << s_sramc_addr.read() << ": [";
                        for (int y = 0; y < Y_DIM; y++) std::cout << cdata[y] << (y==Y_DIM-1 ? "" : ", ");
                        std::cout << "]" << std::endl;
                    }
                }
            }
        }

        ~NpuTop() {
            delete sram_inst;
            delete ctrl_inst;
            delete act_feeder;
            delete wei_feeder;
            delete array_inst;
            delete psm_inst;
            delete config_regs_inst;
        }

        void trace(sc_trace_file* tf) {
            if (!tf) return;
            // Trace configurations
            sc_trace(tf, i_start, std::string(name()) + ".i_start");
            sc_trace(tf, o_done, std::string(name()) + ".o_done");
            sc_trace(tf, o_deadlock, std::string(name()) + ".o_deadlock");
            sc_trace(tf, i_threshold, std::string(name()) + ".i_threshold");
            sc_trace(tf, i_select, std::string(name()) + ".i_select");
            
            // Trace internal signals
            sc_trace(tf, s_act_feeder_en, std::string(name()) + ".s_act_feeder_en");
            sc_trace(tf, s_act_start, std::string(name()) + ".s_act_start");
            sc_trace(tf, s_act_valid, std::string(name()) + ".s_act_valid");
            sc_trace(tf, s_act_pop_en, std::string(name()) + ".s_act_pop_en");
            sc_trace(tf, s_wei_feeder_en, std::string(name()) + ".s_wei_feeder_en");
            sc_trace(tf, s_wei_start, std::string(name()) + ".s_wei_start");
            sc_trace(tf, s_wei_valid, std::string(name()) + ".s_wei_valid");
            sc_trace(tf, s_wei_pop_en, std::string(name()) + ".s_wei_pop_en");
            sc_trace(tf, s_pipeline_en, std::string(name()) + ".s_pipeline_en");
            sc_trace(tf, s_cscan_en, std::string(name()) + ".s_cscan_en");
            sc_trace(tf, s_psm_start, std::string(name()) + ".s_psm_start");
            sc_trace(tf, s_psm_done, std::string(name()) + ".s_psm_done");
            sc_trace(tf, s_psm_shift_done, std::string(name()) + ".s_psm_shift_done");
            sc_trace(tf, s_psm_finalwrite, std::string(name()) + ".s_psm_finalwrite");
            
            // Trace memory controls
            sc_trace(tf, s_srama_addr, std::string(name()) + ".s_srama_addr");
            sc_trace(tf, s_srama_rden, std::string(name()) + ".s_srama_rden");
            sc_trace(tf, s_sramb_addr, std::string(name()) + ".s_sramb_addr");
            sc_trace(tf, s_sramb_rden, std::string(name()) + ".s_sramb_rden");
            sc_trace(tf, s_sramc_addr, std::string(name()) + ".s_sramc_addr");
            sc_trace(tf, s_sramc_wren, std::string(name()) + ".s_sramc_wren");
            sc_trace(tf, s_sramc_rden, std::string(name()) + ".s_sramc_rden");
            
            // Trace data vectors
            sc_trace(tf, s_act_arr, std::string(name()) + ".s_act_arr");
            sc_trace(tf, s_wei_arr, std::string(name()) + ".s_wei_arr");
            sc_trace(tf, s_sa_to_psm_c, std::string(name()) + ".s_sa_to_psm_c");
        }

    private:
        // Submodules instances
        Sram<X_DIM, Y_DIM, T_ACT, T_WEI, T_PSUM, SRAMA_CAP, SRAMB_CAP, SRAMC_CAP>* sram_inst{nullptr};
        Control<X_DIM, Y_DIM, PE_LAT, EXTRA_CSREG>*                               ctrl_inst{nullptr};
        IfmapFeeder<Y_DIM, T_ACT, SRAMA_CAP, FIFO_DEPTH>*                         act_feeder{nullptr};
        WeightFeeder<X_DIM, T_WEI, SRAMB_CAP, FIFO_DEPTH>*                        wei_feeder{nullptr};
        SystolicArray<X_DIM, Y_DIM, T_ACT, T_WEI, T_PSUM>*                        array_inst{nullptr};
        Psm<X_DIM, Y_DIM, T_PSUM, SRAMC_CAP>*                                     psm_inst{nullptr};
        ConfigRegs<32, 32, X_DIM, Y_DIM, 2, 15, 15, 15, 8, DILP_W, 8>*            config_regs_inst{nullptr};

        // Host mux and FSM handshake internal signals
        sc_signal<host_data_t> s_host_rdata_sram{"s_host_rdata_sram"};
        sc_signal<host_data_t> s_host_rdata_cfg{"s_host_rdata_cfg"};
        sc_signal<bool> s_ctrl_done{"s_ctrl_done"};
        sc_signal<bool> s_cfg_start{"s_cfg_start"};
        sc_signal<bool> s_cfg_soft_reset{"s_cfg_soft_reset"};
        sc_signal<bool> s_start_internal{"s_start_internal"};
        sc_signal<bool> s_ctrl_reset_internal{"s_ctrl_reset_internal"};

        // Option B parameters
        sc_signal<bool>     s_false{"s_false", false};
        sc_signal<uint32_t> s_incntlim{"s_incntlim"};
        sc_signal<uint32_t> s_act_reps{"s_act_reps"};
        sc_signal<uint32_t> s_wei_reps{"s_wei_reps"};
        sc_signal<uint32_t> s_one{"s_one", 1};
        sc_signal<sc_bv<DILP_W>> s_dil_pat{"s_dil_pat"};
        sc_signal<sramc_mask_t<Y_DIM>>  s_rows_active{"s_rows_active"};

        sc_signal<uint32_t> s_act_incntlim{"s_act_incntlim"};
        sc_signal<uint32_t> s_act_incntstep{"s_act_incntstep"};
        sc_signal<uint32_t> s_act_outcntlim{"s_act_outcntlim"};
        sc_signal<uint32_t> s_act_outcntstep{"s_act_outcntstep"};
        sc_signal<uint32_t> s_wei_incntlim{"s_wei_incntlim"};
        sc_signal<uint32_t> s_wei_incntstep{"s_wei_incntstep"};
        sc_signal<uint32_t> s_cxlim{"s_cxlim"};
        sc_signal<uint32_t> s_cxstep{"s_cxstep"};
        sc_signal<uint32_t> s_cklim{"s_cklim"};
        sc_signal<uint32_t> s_ckstep{"s_ckstep"};
        sc_signal<uint32_t> s_til_cylim{"s_til_cylim"};
        sc_signal<uint32_t> s_til_cystep{"s_til_cystep"};
        sc_signal<uint32_t> s_til_cklim{"s_til_cklim"};
        sc_signal<uint32_t> s_til_ckstep{"s_til_ckstep"};
        sc_signal<uint32_t> s_ncontexts{"s_ncontexts"};
        sc_signal<bool>     s_preload_en{"s_preload_en"};

        // Internal Signals: Controller <-> Feeders
        sc_signal<bool> s_act_feeder_en{"s_act_feeder_en"};
        sc_signal<bool> s_act_feeder_clear{"s_act_feeder_clear"};
        sc_signal<bool> s_act_start{"s_act_start"};
        sc_signal<bool> s_act_valid{"s_act_valid"};
        sc_signal<bool> s_act_finalpush{"s_act_finalpush"};
        sc_signal<bool> s_act_cnt_en{"s_act_cnt_en"};
        sc_signal<bool> s_act_cnt_clear{"s_act_cnt_clear"};
        sc_signal<bool> s_act_clearfifo{"s_act_clearfifo"};
        sc_signal<bool> s_act_pop_en{"s_act_pop_en"};
        sc_signal<bool> s_act_finalctx{"s_act_finalctx"};

        sc_signal<bool> s_wei_feeder_en{"s_wei_feeder_en"};
        sc_signal<bool> s_wei_feeder_clear{"s_wei_feeder_clear"};
        sc_signal<bool> s_wei_start{"s_wei_start"};
        sc_signal<bool> s_wei_valid{"s_wei_valid"};
        sc_signal<bool> s_wei_finalpush{"s_wei_finalpush"};
        sc_signal<bool> s_wei_cnt_en{"s_wei_cnt_en"};
        sc_signal<bool> s_wei_cnt_clear{"s_wei_cnt_clear"};
        sc_signal<bool> s_wei_clearfifo{"s_wei_clearfifo"};
        sc_signal<bool> s_wei_pop_en{"s_wei_pop_en"};
        sc_signal<bool> s_wei_cswitch{"s_wei_cswitch"};

        // Internal Signals: Controller <-> PSM
        sc_signal<bool> s_psm_start{"s_psm_start"};
        sc_signal<bool> s_psm_reset{"s_psm_reset"};

        // Internal Signals: Controller <-> Systolic Array
        sc_signal<bool>         s_sa_clear{"s_sa_clear"};
        sc_signal<bool>         s_pipeline_en{"s_pipeline_en"};
        sc_signal<sc_bv<X_DIM>> s_cswitch_arr{"s_cswitch_arr"};

        // Internal Signals: Feeders <-> Control (Feedbacks)
        sc_signal<bool> s_act_done{"s_act_done"};
        sc_signal<bool> s_act_til_done{"s_act_til_done"};
        sc_signal<bool> s_act_fifo_empty{"s_act_fifo_empty"};
        sc_signal<bool> s_act_fifo_full{"s_act_fifo_full"};
        sc_signal<bool> s_act_stall{"s_act_stall"};

        sc_signal<bool> s_wei_done{"s_wei_done"};
        sc_signal<bool> s_wei_til_done{"s_wei_til_done"};
        sc_signal<bool> s_wei_fifo_empty{"s_wei_fifo_empty"};
        sc_signal<bool> s_wei_fifo_full{"s_wei_fifo_full"};
        sc_signal<bool> s_wei_stall{"s_wei_stall"};

        // Internal Signals: PSM <-> Control (Feedbacks)
        sc_signal<bool> s_psm_done{"s_psm_done"};
        sc_signal<bool> s_psm_finalwrite{"s_psm_finalwrite"};
        sc_signal<bool> s_psm_shift_done{"s_psm_shift_done"};
        sc_signal<bool> s_cscan_en{"s_cscan_en"};

        // Internal Signals: Feeders <-> SRAM
        sc_signal<uint32_t>                 s_srama_addr{"s_srama_addr"};
        sc_signal<bool>                     s_srama_rden{"s_srama_rden"};
        sc_signal<act_vector_t<Y_DIM, T_ACT>> s_srama_data{"s_srama_data"};

        sc_signal<uint32_t>                 s_sramb_addr{"s_sramb_addr"};
        sc_signal<bool>                     s_sramb_rden{"s_sramb_rden"};
        sc_signal<wei_vector_t<X_DIM, T_WEI>> s_sramb_data{"s_sramb_data"};

        // Internal Signals: PSM <-> SRAM
        sc_signal<psum_vector_t<Y_DIM, T_PSUM>> s_sramc_wdata{"s_sramc_wdata"};
        sc_signal<uint32_t>                      s_sramc_addr{"s_sramc_addr"};
        sc_signal<bool>                          s_sramc_wren{"s_sramc_wren"};
        sc_signal<bool>                          s_sramc_rden{"s_sramc_rden"};
        sc_signal<sramc_mask_t<Y_DIM>>           s_sramc_wmask{"s_sramc_wmask"};
        sc_signal<psum_vector_t<Y_DIM, T_PSUM>> s_sramc_rdata{"s_sramc_rdata"};

        // Internal Signals: Feeders <-> Systolic Array
        sc_signal<act_vector_t<Y_DIM, T_ACT>> s_act_arr{"s_act_arr"};
        sc_signal<wei_vector_t<X_DIM, T_WEI>> s_wei_arr{"s_wei_arr"};

        // Internal Signals: PSM <-> Systolic Array
        sc_signal<psum_vector_t<Y_DIM, T_PSUM>> s_psm_to_sa_c{"s_psm_to_sa_c"};
        sc_signal<psum_vector_t<Y_DIM, T_PSUM>> s_sa_to_psm_c{"s_sa_to_psm_c"};
    };

} // namespace sauria

#endif // SAURIA_NPU_TOP_H
