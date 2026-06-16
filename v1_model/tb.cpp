// SystemC Model for SAURIA NPU Core
// Basic Testbench (tb.cpp)

#include "npu_top.h"
#include <iomanip>

using namespace sauria;

class Testbench : public sc_module {
public:
    // Clock & Reset Ports
    sc_in<bool> i_clk{"i_clk"};
    sc_out<bool> o_rstn{"o_rstn"};
    sc_out<bool> o_soft_reset{"o_soft_reset"};

    // NPU Host Control Interface
    sc_out<bool> o_start{"o_start"};
    sc_in<bool>  i_done{"i_done"};
    sc_in<bool>  i_deadlock{"i_deadlock"};

    // NPU Host Memory Port (AXI interface modeling)
    sc_out<uint32_t>     o_host_addr{"o_host_addr"};
    sc_out<bool>         o_host_wren{"o_host_wren"};
    sc_out<bool>         o_host_rden{"o_host_rden"};
    sc_out<host_data_t>  o_host_wdata{"o_host_wdata"};
    sc_out<host_mask_t>  o_host_wmask{"o_host_wmask"};
    sc_in<host_data_t>   i_host_rdata{"i_host_rdata"};

    // Configurations
    sc_out<float>        o_threshold{"o_threshold"};
    sc_out<sc_bv<3>>     o_select{"o_select"};

    SC_CTOR(Testbench) {
        SC_THREAD(test_process);
        sensitive << i_clk.pos();
    }

private:
    void write_host_mem(uint32_t addr, const host_data_t& data, const host_mask_t& mask) {
        wait();
        o_host_addr.write(addr);
        o_host_wdata.write(data);
        o_host_wmask.write(mask);
        o_host_wren.write(true);
        o_host_rden.write(false);
        wait();
        o_host_wren.write(false);
    }

    host_data_t read_host_mem(uint32_t addr) {
        wait();
        o_host_addr.write(addr);
        o_host_wren.write(false);
        o_host_rden.write(true);
        wait(); // Address registers in SRAM
        wait(); // Data registers out (1-cycle read latency)
        o_host_rden.write(false);
        return i_host_rdata.read();
    }

    // Helper write to 1 register
    void write_reg32(uint32_t addr, uint32_t value){
        host_data_t data;
        host_mask_t mask;

        data.data.fill(0.0f);
        mask.data.fill(false);

        data[0] = static_cast<float>(value);
        mask[0] = true;

        write_host_mem(addr, data, mask);
    }

    void test_process() {
        std::cout << "\n=============================================" << std::endl;
        std::cout << "       SAURIA SystemC NPU Core Testbench     " << std::endl;
        std::cout << "=============================================\n" << std::endl;

        // 1. Initialize Control signals
        o_rstn.write(false);
        o_soft_reset.write(false);
        o_start.write(false);
        o_host_addr.write(0);
        o_host_wren.write(false);
        o_host_rden.write(false);
        o_host_wdata.write(host_data_t());
        o_host_wmask.write(host_mask_t());
        o_threshold.write(0.01f);
        
        // Double-buffering SRAM selections:
        // Bit 0: SRAM A, Bit 1: SRAM B, Bit 2: SRAM C.
        // select == 0x7 maps SRAM A/B/C physical buffer 0 to NPU, and physical buffer 1 to Host AXI.
        // select == 0x0 maps physical buffer 0 to Host AXI, and physical buffer 1 to NPU.
        o_select.write(sc_bv<3>("000")); // Map physical buffer 0 to Host AXI

        // Apply Reset
        wait(3);
        o_rstn.write(true);
        std::cout << "[TB] @ " << sc_time_stamp() << " System Reset Released." << std::endl;
        wait(2);

        // ----------------------------------------------------
        // Step 1: Program SRAM A via Host Interface
        // ----------------------------------------------------
        std::cout << "[TB] @ " << sc_time_stamp() << " Writing Host Data to SRAM A (Activations)..." << std::endl;
        host_mask_t full_mask;
        full_mask.data.fill(true);

        host_data_t act_val1;
        act_val1[0] = 1.5f; act_val1[1] = 2.5f; act_val1[2] = 3.5f; act_val1[3] = 4.5f;
        
        host_data_t act_val2;
        act_val2[0] = -0.5f; act_val2[1] = 0.005f; act_val2[2] = 10.0f; act_val2[3] = 0.1f; // element 1 is under threshold!

        // SRAM A Address = SRAMA_OFFSET | (phys_addr << 1) | sub_word
        uint32_t act_base = 4;
        uint32_t act_step = 2;
        uint32_t srama_addr0_lower = SRAMA_OFFSET | ((act_step * 0 + act_base) << 1) | 0;
        uint32_t srama_addr0_upper = SRAMA_OFFSET | ((act_step * 0 + act_base) << 1) | 1;

        write_host_mem(srama_addr0_lower, act_val1, full_mask);
        write_host_mem(srama_addr0_upper, act_val2, full_mask);

        // ----------------------------------------------------
        // Step 2: Program SRAM B via Host Interface
        // ----------------------------------------------------
        std::cout << "[TB] @ " << sc_time_stamp() << " Writing Host Data to SRAM B (Weights)..." << std::endl;
        host_data_t wei_val1;
        wei_val1[0] = 0.5f; wei_val1[1] = -1.0f; wei_val1[2] = 2.0f; wei_val1[3] = 0.0f;

        uint32_t wei_base = 2;
        uint32_t wei_step = 2;
        uint32_t sramb_addr0 = SRAMB_OFFSET | (wei_step * 0 + wei_base);
        write_host_mem(sramb_addr0, wei_val1, full_mask);

        uint32_t out_base = 8;

        // ----------------------------------------------------
        // Step 3: Verification Read-Back via Host Interface
        // ----------------------------------------------------
        std::cout << "[TB] @ " << sc_time_stamp() << " Performing Read-back Verification..." << std::endl;
        
        host_data_t read_act_lower = read_host_mem(srama_addr0_lower);
        host_data_t read_act_upper = read_host_mem(srama_addr0_upper);
        host_data_t read_wei = read_host_mem(sramb_addr0);

        std::cout << std::fixed << std::setprecision(3);
        std::cout << "[TB] Read SRAM A Row 0 Lower half: " << read_act_lower << std::endl;
        std::cout << "[TB] Read SRAM A Row 0 Upper half: " << read_act_upper << std::endl;
        std::cout << "[TB] Read SRAM B Row 0:            " << read_wei << std::endl;

        // Verify values
        bool matched = true;
        for (int i = 0; i < 4; i++) {
            if (read_act_lower[i] != act_val1[i]) matched = false;
            if (read_act_upper[i] != act_val2[i]) matched = false;
            if (read_wei[i] != wei_val1[i]) matched = false;
        }

        if (matched) {
            std::cout << "[TB] >>> SUCCESS: Memory Read-back verification matches written data!" << std::endl;
        } else {
            std::cerr << "[TB] >>> ERROR: Read-back mismatch!" << std::endl;
        }

        // ----------------------------------------------------
        // Step 4: Swap Double-Buffered SRAMs to Accelerator Side
        // ----------------------------------------------------
        std::cout << "[TB] @ " << sc_time_stamp() << " Swapping Double-Buffer selections (select = 0x7)..." << std::endl;
        wait();
        o_select.write(sc_bv<3>("111")); // Swap buffers: Host maps to 1, NPU maps to 0
        wait();

        // ######################################################
        // TESTBENCH: Write to register configure before START
        //  RUN - Time parameter config
        // A - DEFAULT
        // #####################################################
        std::cout << "[TB] @ " << sc_time_stamp() << " Programming runtime configuration..." << std::endl;

        // Activation feeder
        write_reg32(CFG_ACT_OFFSET + 0x04, 96); // ACT_INCNTLIM
        write_reg32(CFG_ACT_OFFSET + 0x08, act_step); // ACT_INCNTSTEP
        write_reg32(CFG_ACT_OFFSET + 0x0C, 96); // ACT_OUTCNTLIM
        write_reg32(CFG_ACT_OFFSET + 0x10, act_step); // ACT_OUTCNTSTEP

        // Weight feeder
        write_reg32(CFG_WEI_OFFSET + 0x04, 96); // WEI_INCNTLIM
        write_reg32(CFG_WEI_OFFSET + 0x08, wei_step); // WEI_INCNTSTEP

        // PSM
        write_reg32(CFG_OUT_OFFSET + 0x04, 96); //CXLIM
        write_reg32(CFG_OUT_OFFSET + 0x08, 1); //CXSTEP
        write_reg32(CFG_OUT_OFFSET + 0x0C, 96); // CKLIM
        write_reg32(CFG_OUT_OFFSET + 0x10, 1);// CKSTEP

        // ACT_REPS & WEI_REPS
        uint32_t act_reps = 1;
        uint32_t wei_reps = 2;

        write_reg32(CFG_CON_OFFSET + 0x04, act_reps);
        write_reg32(CFG_CON_OFFSET + 0x08, wei_reps);

        //MEMORY
        // write_reg32(CFG_ACT_BASE_ADDR, 0); // ACT_BASE_ADDR
        // write_reg32(CFG_WEI_BASE_ADDR, 0); // WEI_BASE_ADDR
        // write_reg32(CFG_OUT_BASE_ADDR, 0); // OUT_BASE_ADDR

        // ######################################################
        // TESTBENCH: Write to register configure before START
        //  RUN - Time parameter config
        // B - BASE ADDRESS
        // #####################################################

        write_reg32(CFG_ACT_BASE_ADDR, act_base);
        write_reg32(CFG_WEI_BASE_ADDR, wei_base);
        write_reg32(CFG_OUT_BASE_ADDR, out_base);

        // ######################################################
        // TESTBENCH: Write to register configure before START
        //  RUN - Time parameter config
        // C - Layer
        // #####################################################
        write_reg32(CFG_LAYER_OFFSET + 0x00, 6);   // IN_H
        write_reg32(CFG_LAYER_OFFSET + 0x04, 10);  // IN_W
        write_reg32(CFG_LAYER_OFFSET + 0x08, 64);  // IN_C

        write_reg32(CFG_LAYER_OFFSET + 0x0C, 4);   // OUT_H
        write_reg32(CFG_LAYER_OFFSET + 0x10, 8);   // OUT_W
        write_reg32(CFG_LAYER_OFFSET + 0x14, 16);  // OUT_C

        write_reg32(CFG_LAYER_OFFSET + 0x18, 3);   // KERNEL_H
        write_reg32(CFG_LAYER_OFFSET + 0x1C, 3);   // KERNEL_W

        write_reg32(CFG_LAYER_OFFSET + 0x20, 1);   // STRIDE
        write_reg32(CFG_LAYER_OFFSET + 0x24, 0);   // PADDING
        write_reg32(CFG_LAYER_OFFSET + 0x28, 1);   // DILATION

        write_reg32(CFG_LAYER_OFFSET + 0x30, 16);  // TILE_X
        write_reg32(CFG_LAYER_OFFSET + 0x34, 4);   // TILE_Y
        write_reg32(CFG_LAYER_OFFSET + 0x38, 16);  // TILE_K
        write_reg32(CFG_LAYER_OFFSET + 0x3C, 64);  // TILE_C

        write_reg32(CFG_LAYER_OFFSET + 0x40, 16);  // X_USED
        write_reg32(CFG_LAYER_OFFSET + 0x44, 8);   // Y_USED

        // ----------------------------------------------------
        // Step 5: Start NPU Execution and Monitor Done
        // ----------------------------------------------------
        std::cout << "[TB] @ " << sc_time_stamp() << " Asserting Start Pulse..." << std::endl;
        wait();
        o_start.write(true);
        wait();
        o_start.write(false);

        // Simulate active processing cycles
        int timeout = 200;
        bool completed = false;
        while (timeout-- > 0) {
            wait();
            if (i_done.read()) {
                completed = true;
                break;
            }
        }

        if (completed) {
            std::cout << "[TB] @ " << sc_time_stamp() << " NPU Execution Completed Successfully!" << std::endl;
        } else {
            std::cout << "[TB] @ " << sc_time_stamp() << " NPU Execution Timeout (expected due to FSM stubs)." << std::endl;
        }

        std::cout << "\n=============================================" << std::endl;
        std::cout << "         SAURIA SystemC Simulation Ended     " << std::endl;
        std::cout << "=============================================\n" << std::endl;
        sc_stop();
    }
};

int sc_main(int argc, char* argv[]) {
    // 1. Clock declaration
    sc_clock clk("clk", 10, SC_NS);

    // 2. Local interconnect signals
    sc_signal<bool> rstn{"rstn"};
    sc_signal<bool> soft_reset{"soft_reset"};
    sc_signal<bool> start{"start"};
    sc_signal<bool> done{"done"};
    sc_signal<bool> deadlock{"deadlock"};

    sc_signal<uint32_t>     host_addr{"host_addr"};
    sc_signal<bool>         host_wren{"host_wren"};
    sc_signal<bool>         host_rden{"host_rden"};
    sc_signal<host_data_t>  host_wdata{"host_wdata"};
    sc_signal<host_mask_t>  host_wmask{"host_wmask"};
    sc_signal<host_data_t>  host_rdata{"host_rdata"};

    sc_signal<float>        threshold{"threshold"};
    sc_signal<sc_bv<3>>     select{"select"};

    // 3. Module instantiation
    NpuTop npu("NpuTop_inst");
    Testbench tb("Testbench_inst");

    // 4. Signal Bindings
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

    // 5. Enable Waveform Tracing
    sc_trace_file* tf = sc_create_vcd_trace_file("waves");
    if (tf) {
        std::cout << "[TB] Waveform tracing enabled. Creating waves.vcd..." << std::endl;
        sc_trace(tf, clk, "clk");
        sc_trace(tf, rstn, "rstn");
        sc_trace(tf, soft_reset, "soft_reset");
        sc_trace(tf, start, "start");
        sc_trace(tf, done, "done");
        sc_trace(tf, deadlock, "deadlock");
        sc_trace(tf, host_addr, "host_addr");
        sc_trace(tf, host_wren, "host_wren");
        sc_trace(tf, host_rden, "host_rden");
        sc_trace(tf, host_wdata, "host_wdata");
        sc_trace(tf, host_wmask, "host_wmask");
        sc_trace(tf, host_rdata, "host_rdata");
        sc_trace(tf, select, "select");
    }

    // 6. Start simulation
    sc_start();

    // 7. Cleanup
    if (tf) {
        sc_close_vcd_trace_file(tf);
        std::cout << "[TB] Waveform tracing closed." << std::endl;
    }

    return 0;
}
