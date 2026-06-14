// SystemC Model for SAURIA NPU Core
// Standalone Control & ConfigRegs Testbench (tb_control_cfg.cpp)

#include "../sauria_types.h"
#include "main_controller.h"
#include "../config/config_regs.h"
#include <iostream>
#include <iomanip>
#include <cassert>

using namespace sauria;

class TestbenchControlCfg : public sc_module {
public:
    sc_in<bool> i_clk{"i_clk"};
    sc_out<bool> o_rstn{"o_rstn"};

    // Host Config/AXI interface
    sc_out<uint32_t>     o_host_addr{"o_host_addr"};
    sc_out<bool>         o_host_wren{"o_host_wren"};
    sc_out<bool>         o_host_rden{"o_host_rden"};
    sc_out<host_data_t>  o_host_wdata{"o_host_wdata"};
    sc_out<host_mask_t>  o_host_wmask{"o_host_wmask"};
    sc_in<host_data_t>   i_host_rdata{"i_host_rdata"};

    // Control FSM Feedback signals
    sc_out<bool> o_soft_reset_in{"o_soft_reset_in"};
    sc_out<bool> o_outbuf_done{"o_outbuf_done"};
    sc_out<bool> o_finalwrite{"o_finalwrite"};
    sc_out<bool> o_shift_done{"o_shift_done"};

    sc_out<bool> o_act_done{"o_act_done"};
    sc_out<bool> o_act_til_done{"o_act_til_done"};
    sc_out<bool> o_act_fifo_empty{"o_act_fifo_empty"};
    sc_out<bool> o_act_fifo_full{"o_act_fifo_full"};
    sc_out<bool> o_act_stall{"o_act_stall"};

    sc_out<bool> o_wei_done{"o_wei_done"};
    sc_out<bool> o_wei_til_done{"o_wei_til_done"};
    sc_out<bool> o_wei_fifo_empty{"o_wei_fifo_empty"};
    sc_out<bool> o_wei_fifo_full{"o_wei_fifo_full"};
    sc_out<bool> o_wei_stall{"o_wei_stall"};

    // Monitor signals from Control / ConfigRegs
    sc_in<bool> i_done{"i_done"};
    sc_in<bool> i_feed_deadlock{"i_feed_deadlock"};
    sc_in<bool> i_start_pulse{"i_start_pulse"};

    SC_HAS_PROCESS(TestbenchControlCfg);
    TestbenchControlCfg(sc_module_name nm) : sc_module(nm) {
        SC_THREAD(test_process);
        sensitive << i_clk.pos();
    }

private:
    void test_process() {
        std::cout << "\n=============================================================" << std::endl;
        std::cout << "      SAURIA Standalone Control & ConfigRegs Testbench" << std::endl;
        std::cout << "=============================================================\n" << std::endl;

        // Initialize signals
        o_rstn.write(false);
        o_host_addr.write(0);
        o_host_wren.write(false);
        o_host_rden.write(false);
        o_host_wdata.write(host_data_t());
        o_host_wmask.write(host_mask_t());

        o_soft_reset_in.write(false);
        o_outbuf_done.write(false);
        o_finalwrite.write(false);
        o_shift_done.write(false);

        o_act_done.write(false);
        o_act_til_done.write(false);
        o_act_fifo_empty.write(false);
        o_act_fifo_full.write(false);
        o_act_stall.write(false);

        o_wei_done.write(false);
        o_wei_til_done.write(false);
        o_wei_fifo_empty.write(false);
        o_wei_fifo_full.write(false);
        o_wei_stall.write(false);

        wait(3);
        o_rstn.write(true);
        std::cout << "[TB] Reset released." << std::endl;
        wait();

        // ----------------------------------------------------
        // TESTCASE 1: Host Register Write & Read
        // ----------------------------------------------------
        std::cout << "\n>>> Starting TC 1: Host Register Write & Read..." << std::endl;

        // Write to incntlim (CFG_CON_OFFSET + 0x00) -> set limit to 10
        host_data_t wdata;
        wdata[0] = 10.0f;
        host_mask_t wmask;
        wmask[0] = true;

        o_host_addr.write(CFG_REGS_OFFSET | (CFG_CON_OFFSET + 0x00));
        o_host_wdata.write(wdata);
        o_host_wmask.write(wmask);
        o_host_wren.write(true);
        wait();
        o_host_wren.write(false);
        wait();

        // Read it back and verify
        o_host_addr.write(CFG_REGS_OFFSET | (CFG_CON_OFFSET + 0x00));
        o_host_rden.write(true);
        wait();
        o_host_rden.write(false);
        wait();

        host_data_t rdata = i_host_rdata.read();
        std::cout << " Readback incntlim: " << rdata[0] << " (Expected: 10)" << std::endl;
        assert(rdata[0] == 10.0f);

        // Write to act_reps (CFG_CON_OFFSET + 0x04) -> set limit to 12
        wdata[0] = 12.0f;
        o_host_addr.write(CFG_REGS_OFFSET | (CFG_CON_OFFSET + 0x04));
        o_host_wdata.write(wdata);
        o_host_wren.write(true);
        wait();
        o_host_wren.write(false);
        wait();

        // Read back
        o_host_addr.write(CFG_REGS_OFFSET | (CFG_CON_OFFSET + 0x04));
        o_host_rden.write(true);
        wait();
        o_host_rden.write(false);
        wait();
        rdata = i_host_rdata.read();
        std::cout << " Readback act_reps: " << rdata[0] << " (Expected: 12)" << std::endl;
        assert(rdata[0] == 12.0f);

        std::cout << ">>> TC 1 PASSED SUCCESSFULLY!" << std::endl;

        // ----------------------------------------------------
        // TESTCASE 2: Host-Triggered NPU FSM Execution
        // ----------------------------------------------------
        std::cout << "\n>>> Starting TC 2: Host-Triggered FSM Start and Execution..." << std::endl;

        // Set start bit (bit 0 of control register at local address 0x00)
        wdata[0] = 1.0f;
        o_host_addr.write(CFG_REGS_OFFSET | 0x00);
        o_host_wdata.write(wdata);
        o_host_wren.write(true);
        wait();
        o_host_wren.write(false);
        wait();

        // The start pulse should trigger the FSM to transition from IDLE to START_FLAGS, then START_COMP
        // FSM will stay in START_COMP for incntlim (which we set to 10) cycles
        std::cout << " FSM triggered. Waiting for completion..." << std::endl;

        // Let's monitor i_done to go high. We'll wait up to 100 cycles.
        int cycles = 0;
        while (!i_done.read() && cycles < 100) {
            wait();
            cycles++;
        }

        std::cout << " FSM finished in " << cycles << " cycles." << std::endl;
        assert(i_done.read());

        // Read Control register to check the done bit (bit 1 of control register, so val & 2 != 0)
        o_host_addr.write(CFG_REGS_OFFSET | 0x00);
        o_host_rden.write(true);
        wait();
        o_host_rden.write(false);
        wait();
        rdata = i_host_rdata.read();
        std::cout << " Control Register status: " << rdata[0] << std::endl;
        // Check if done bit is high (bit 1 -> value contains 2.0f)
        assert(((int)rdata[0] & 2) != 0);

        // Clear COW on done
        wdata[0] = 0.0f;
        o_host_addr.write(CFG_REGS_OFFSET | 0x00);
        o_host_wdata.write(wdata);
        o_host_wren.write(true);
        wait();
        o_host_wren.write(false);
        wait();

        // Read Control register again to verify done bit is cleared
        o_host_addr.write(CFG_REGS_OFFSET | 0x00);
        o_host_rden.write(true);
        wait();
        o_host_rden.write(false);
        wait();
        rdata = i_host_rdata.read();
        std::cout << " Control Register status after COW clear: " << rdata[0] << std::endl;
        assert(((int)rdata[0] & 2) == 0);

        std::cout << ">>> TC 2 PASSED SUCCESSFULLY!" << std::endl;

        // ----------------------------------------------------
        // TESTCASE 3: Deadlock Flag Detection
        // ----------------------------------------------------
        std::cout << "\n>>> Starting TC 3: Deadlock Flag Detection..." << std::endl;

        // Assert activation queue empty and weight queue full (defines deadlock)
        o_act_fifo_empty.write(true);
        o_wei_fifo_full.write(true);
        wait(2);

        std::cout << " Deadlock output: " << i_feed_deadlock.read() << " (Expected: 1)" << std::endl;
        assert(i_feed_deadlock.read());

        // Reset inputs
        o_act_fifo_empty.write(false);
        o_wei_fifo_full.write(false);
        wait(2);
        std::cout << " Deadlock output: " << i_feed_deadlock.read() << " (Expected: 0)" << std::endl;
        assert(!i_feed_deadlock.read());

        std::cout << ">>> TC 3 PASSED SUCCESSFULLY!" << std::endl;

        std::cout << "\n=============================================================" << std::endl;
        std::cout << "    SAURIA Standalone Control & ConfigRegs ALL TESTS PASSED   " << std::endl;
        std::cout << "=============================================================\n" << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char* argv[]) {
    sc_clock clk("clk", 10, SC_NS);

    // Instantiate ConfigRegs and Control (configure to 4x4 array size for quick test)
    ConfigRegs<32, 32, 4, 4, 2, 15, 15, 15, 8, 64, 8> cfg_inst("cfg_inst");
    Control<4, 4, 8, 1> ctrl_inst("ctrl_inst");

    TestbenchControlCfg tb("tb");

    // Signals
    sc_signal<bool> s_rstn;
    sc_signal<uint32_t>     s_host_addr;
    sc_signal<bool>         s_host_wren;
    sc_signal<bool>         s_host_rden;
    sc_signal<host_data_t>  s_host_wdata;
    sc_signal<host_mask_t>  s_host_wmask;
    sc_signal<host_data_t>  s_host_rdata;

    sc_signal<bool> s_done;
    sc_signal<bool> s_soft_reset_in;
    sc_signal<bool> s_start;
    sc_signal<bool> s_soft_reset;

    sc_signal<uint32_t> s_incntlim;
    sc_signal<uint32_t> s_act_reps;
    sc_signal<uint32_t> s_wei_reps;
    sc_signal<sc_bv<64>> s_dil_pat;
    sc_signal<sramc_mask_t<4>> s_rows_active;

    sc_signal<bool> s_outbuf_done;
    sc_signal<bool> s_finalwrite;
    sc_signal<bool> s_shift_done;
    sc_signal<bool> s_act_done;
    sc_signal<bool> s_act_til_done;
    sc_signal<bool> s_act_fifo_empty;
    sc_signal<bool> s_act_fifo_full;
    sc_signal<bool> s_act_stall;
    sc_signal<bool> s_wei_done;
    sc_signal<bool> s_wei_til_done;
    sc_signal<bool> s_wei_fifo_empty;
    sc_signal<bool> s_wei_fifo_full;
    sc_signal<bool> s_wei_stall;

    sc_signal<bool> s_feed_deadlock;

    // Remaining ctrl_inst output dummy signals
    sc_signal<bool> s_act_feeder_en;
    sc_signal<bool> s_act_feeder_clear;
    sc_signal<bool> s_act_start;
    sc_signal<bool> s_act_valid;
    sc_signal<bool> s_act_finalpush;
    sc_signal<bool> s_act_cnt_en;
    sc_signal<bool> s_act_cnt_clear;
    sc_signal<bool> s_act_clearfifo;
    sc_signal<bool> s_act_pop_en;
    sc_signal<bool> s_act_finalctx;

    sc_signal<bool> s_wei_feeder_en;
    sc_signal<bool> s_wei_feeder_clear;
    sc_signal<bool> s_wei_start;
    sc_signal<bool> s_wei_valid;
    sc_signal<bool> s_wei_finalpush;
    sc_signal<bool> s_wei_cnt_en;
    sc_signal<bool> s_wei_cnt_clear;
    sc_signal<bool> s_wei_clearfifo;
    sc_signal<bool> s_wei_pop_en;
    sc_signal<bool> s_wei_cswitch;

    sc_signal<bool> s_outbuf_start;
    sc_signal<bool> s_outbuf_reset;

    sc_signal<bool> s_sa_clear;
    sc_signal<bool> s_pipeline_en;
    sc_signal<sc_bv<4>> s_cswitch_arr;

    // Connect ConfigRegs
    cfg_inst.i_clk(clk);
    cfg_inst.i_rstn(s_rstn);
    cfg_inst.i_host_addr(s_host_addr);
    cfg_inst.i_host_wren(s_host_wren);
    cfg_inst.i_host_rden(s_host_rden);
    cfg_inst.i_host_wdata(s_host_wdata);
    cfg_inst.i_host_wmask(s_host_wmask);
    cfg_inst.o_host_rdata(s_host_rdata);

    cfg_inst.i_done(s_done);
    cfg_inst.i_soft_reset_in(s_soft_reset_in);
    cfg_inst.o_start(s_start);
    cfg_inst.o_soft_reset(s_soft_reset);

    cfg_inst.o_incntlim(s_incntlim);
    cfg_inst.o_act_reps(s_act_reps);
    cfg_inst.o_wei_reps(s_wei_reps);
    cfg_inst.o_dil_pat(s_dil_pat);
    cfg_inst.o_rows_active(s_rows_active);

    // Connect Control
    ctrl_inst.i_clk(clk);
    ctrl_inst.i_rstn(s_rstn);
    ctrl_inst.i_soft_reset(s_soft_reset);
    ctrl_inst.i_start(s_start);

    ctrl_inst.i_outbuf_done(s_outbuf_done);
    ctrl_inst.i_finalwrite(s_finalwrite);
    ctrl_inst.i_shift_done(s_shift_done);

    ctrl_inst.i_incntlim(s_incntlim);
    ctrl_inst.i_act_reps(s_act_reps);
    ctrl_inst.i_wei_reps(s_wei_reps);

    ctrl_inst.i_act_done(s_act_done);
    ctrl_inst.i_act_til_done(s_act_til_done);
    ctrl_inst.i_act_fifo_empty(s_act_fifo_empty);
    ctrl_inst.i_act_fifo_full(s_act_fifo_full);
    ctrl_inst.i_act_stall(s_act_stall);

    ctrl_inst.i_wei_done(s_wei_done);
    ctrl_inst.i_wei_til_done(s_wei_til_done);
    ctrl_inst.i_wei_fifo_empty(s_wei_fifo_empty);
    ctrl_inst.i_wei_fifo_full(s_wei_fifo_full);
    ctrl_inst.i_wei_stall(s_wei_stall);

    // outputs from Control FSM
    ctrl_inst.o_done(s_done);
    ctrl_inst.o_feed_deadlock(s_feed_deadlock);

    ctrl_inst.o_act_feeder_en(s_act_feeder_en);
    ctrl_inst.o_act_feeder_clear(s_act_feeder_clear);
    ctrl_inst.o_act_start(s_act_start);
    ctrl_inst.o_act_valid(s_act_valid);
    ctrl_inst.o_act_finalpush(s_act_finalpush);
    ctrl_inst.o_act_cnt_en(s_act_cnt_en);
    ctrl_inst.o_act_cnt_clear(s_act_cnt_clear);
    ctrl_inst.o_act_clearfifo(s_act_clearfifo);
    ctrl_inst.o_act_pop_en(s_act_pop_en);
    ctrl_inst.o_act_finalctx(s_act_finalctx);

    ctrl_inst.o_wei_feeder_en(s_wei_feeder_en);
    ctrl_inst.o_wei_feeder_clear(s_wei_feeder_clear);
    ctrl_inst.o_wei_start(s_wei_start);
    ctrl_inst.o_wei_valid(s_wei_valid);
    ctrl_inst.o_wei_finalpush(s_wei_finalpush);
    ctrl_inst.o_wei_cnt_en(s_wei_cnt_en);
    ctrl_inst.o_wei_cnt_clear(s_wei_cnt_clear);
    ctrl_inst.o_wei_clearfifo(s_wei_clearfifo);
    ctrl_inst.o_wei_pop_en(s_wei_pop_en);
    ctrl_inst.o_wei_cswitch(s_wei_cswitch);

    ctrl_inst.o_outbuf_start(s_outbuf_start);
    ctrl_inst.o_outbuf_reset(s_outbuf_reset);

    ctrl_inst.o_sa_clear(s_sa_clear);
    ctrl_inst.o_pipeline_en(s_pipeline_en);
    ctrl_inst.o_cswitch_arr(s_cswitch_arr);

    // Connect Testbench
    tb.i_clk(clk);
    tb.o_rstn(s_rstn);
    tb.o_host_addr(s_host_addr);
    tb.o_host_wren(s_host_wren);
    tb.o_host_rden(s_host_rden);
    tb.o_host_wdata(s_host_wdata);
    tb.o_host_wmask(s_host_wmask);
    tb.i_host_rdata(s_host_rdata);

    tb.o_soft_reset_in(s_soft_reset_in);
    tb.o_outbuf_done(s_outbuf_done);
    tb.o_finalwrite(s_finalwrite);
    tb.o_shift_done(s_shift_done);
    tb.o_act_done(s_act_done);
    tb.o_act_til_done(s_act_til_done);
    tb.o_act_fifo_empty(s_act_fifo_empty);
    tb.o_act_fifo_full(s_act_fifo_full);
    tb.o_act_stall(s_act_stall);
    tb.o_wei_done(s_wei_done);
    tb.o_wei_til_done(s_wei_til_done);
    tb.o_wei_fifo_empty(s_wei_fifo_empty);
    tb.o_wei_fifo_full(s_wei_fifo_full);
    tb.o_wei_stall(s_wei_stall);

    tb.i_done(s_done);
    tb.i_feed_deadlock(s_feed_deadlock);
    tb.i_start_pulse(s_start);

    sc_start();
    return 0;
}
