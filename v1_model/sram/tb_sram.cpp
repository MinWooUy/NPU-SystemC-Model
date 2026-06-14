// SystemC Model for SAURIA NPU Core
// Standalone SRAM Testbench (tb_sram.cpp)

#include "../sauria_types.h"
#include "sram_top.h"
#include <iostream>
#include <iomanip>
#include <cassert>

using namespace sauria;

class TestbenchSram : public sc_module {
public:
    sc_in<bool> i_clk{"i_clk"};
    sc_out<bool> o_rstn{"o_rstn"};

    // Power Gating / Deep Sleep (Global)
    sc_out<bool> o_deepsleep{"o_deepsleep"};
    sc_out<bool> o_powergate{"o_powergate"};

    // Buffer Selection (Double Buffering)
    sc_out<sc_bv<3>> o_select{"o_select"};

    // Host-side Interface
    sc_out<uint32_t>     o_host_addr{"o_host_addr"};
    sc_out<bool>         o_host_wren{"o_host_wren"};
    sc_out<bool>         o_host_rden{"o_host_rden"};
    sc_out<host_data_t>  o_host_wdata{"o_host_wdata"};
    sc_out<host_mask_t>  o_host_wmask{"o_host_wmask"};
    sc_in<host_data_t>   i_host_rdata{"i_host_rdata"};

    // Accelerator-side Interface: SRAM A
    sc_out<uint32_t>             o_srama_addr{"o_srama_addr"};
    sc_out<bool>                 o_srama_rden{"o_srama_rden"};
    sc_in<act_vector_t<4, float>> i_srama_data{"i_srama_data"};

    // Accelerator-side Interface: SRAM B
    sc_out<uint32_t>             o_sramb_addr{"o_sramb_addr"};
    sc_out<bool>                 o_sramb_rden{"o_sramb_rden"};
    sc_in<wei_vector_t<4, float>> i_sramb_data{"i_sramb_data"};

    // Accelerator-side Interface: SRAM C
    sc_out<psum_vector_t<4, float>> o_sramc_wdata{"o_sramc_wdata"};
    sc_out<uint32_t>                o_sramc_addr{"o_sramc_addr"};
    sc_out<bool>                    o_sramc_wren{"o_sramc_wren"};
    sc_out<bool>                    o_sramc_rden{"o_sramc_rden"};
    sc_out<sramc_mask_t<4>>         o_sramc_wmask{"o_sramc_wmask"};
    sc_in<psum_vector_t<4, float>>  i_sramc_rdata{"i_sramc_rdata"};

    SC_HAS_PROCESS(TestbenchSram);
    TestbenchSram(sc_module_name nm) : sc_module(nm) {
        SC_THREAD(test_process);
        sensitive << i_clk.pos();
    }

private:
    void test_process() {
        std::cout << "\n=============================================================" << std::endl;
        std::cout << "          SAURIA Standalone SRAM block Testbench" << std::endl;
        std::cout << "=============================================================\n" << std::endl;

        // Initialize signals
        o_rstn.write(false);
        o_deepsleep.write(false);
        o_powergate.write(false);
        o_select.write(sc_bv<3>("000")); // Buffer selection: 000

        o_host_addr.write(0);
        o_host_wren.write(false);
        o_host_rden.write(false);
        o_host_wdata.write(host_data_t());
        o_host_wmask.write(host_mask_t());

        o_srama_addr.write(0);
        o_srama_rden.write(false);

        o_sramb_addr.write(0);
        o_sramb_rden.write(false);

        o_sramc_wdata.write(psum_vector_t<4, float>());
        o_sramc_addr.write(0);
        o_sramc_wren.write(false);
        o_sramc_rden.write(false);
        o_sramc_wmask.write(sramc_mask_t<4>(false));

        wait(3);
        o_rstn.write(true);
        std::cout << "[TB] Reset released." << std::endl;
        wait();

        // ----------------------------------------------------
        // TESTCASE 1: Host-Side Port Writes & Reads
        // ----------------------------------------------------
        std::cout << "\n>>> Starting TC 1: Host-Side Port Writes & Reads..." << std::endl;

        // Let's write to SRAM A via Host interface.
        // SRAM A address mapping uses SRAMA_OFFSET (0x00040000).
        // Let's write to address 0x00040000, which points to SRAM A buffer mapped to Host.
        // Since o_select is "000" (bit 0 = 0), Host owns buffer 1 (npu_a_idx = 0 -> buffer 0, host_a_idx = 1 -> buffer 1).
        
        host_data_t hdata;
        hdata[0] = 10.0f;
        hdata[1] = 11.0f;
        hdata[2] = 12.0f;
        hdata[3] = 13.0f;

        host_mask_t hmask;
        hmask[0] = true;
        hmask[1] = true;
        hmask[2] = true;
        hmask[3] = true;

        o_host_addr.write(SRAMA_OFFSET | 0x00); // SRAM A address 0
        o_host_wdata.write(hdata);
        o_host_wmask.write(hmask);
        o_host_wren.write(true);
        wait();
        o_host_wren.write(false);
        wait();

        // Let's write another set to address 0x00040001 (which is sub_word 1 of SRAM A address 0 when Y_DIM=4, so it writes to the next block or same physical depending on subwords).
        // For Y_DIM=4, SUBWORDS_A = 4/4 = 1. So address is (local_addr >> SHIFT_A), which is local_addr.
        // Thus, local_addr=0 is physical address 0, local_addr=1 is physical address 1.
        hdata[0] = 20.0f; hdata[1] = 21.0f; hdata[2] = 22.0f; hdata[3] = 23.0f;
        o_host_addr.write(SRAMA_OFFSET | 0x01); // SRAM A address 1
        o_host_wdata.write(hdata);
        o_host_wren.write(true);
        wait();
        o_host_wren.write(false);
        wait();

        // Write to SRAM B at address 0
        hdata[0] = 100.0f; hdata[1] = 101.0f; hdata[2] = 102.0f; hdata[3] = 103.0f;
        o_host_addr.write(SRAMB_OFFSET | 0x00); // SRAM B address 0
        o_host_wdata.write(hdata);
        o_host_wren.write(true);
        wait();
        o_host_wren.write(false);
        wait();

        // Write to SRAM C at address 0
        hdata[0] = 1000.0f; hdata[1] = 1001.0f; hdata[2] = 1002.0f; hdata[3] = 1003.0f;
        o_host_addr.write(SRAMC_OFFSET | 0x00); // SRAM C address 0
        o_host_wdata.write(hdata);
        o_host_wren.write(true);
        wait();
        o_host_wren.write(false);
        wait();

        // Read back from Host interface and verify!
        // Read SRAM A address 0
        o_host_addr.write(SRAMA_OFFSET | 0x00);
        o_host_rden.write(true);
        wait(); // 1 cycle read latency
        o_host_rden.write(false);
        wait(); // wait for output signal update
        host_data_t rdata = i_host_rdata.read();
        std::cout << " SRAM A[0] Readback: " << rdata[0] << ", " << rdata[1] << ", " << rdata[2] << ", " << rdata[3] << std::endl;
        assert(rdata[0] == 10.0f && rdata[1] == 11.0f && rdata[2] == 12.0f && rdata[3] == 13.0f);

        // Read SRAM A address 1
        o_host_addr.write(SRAMA_OFFSET | 0x01);
        o_host_rden.write(true);
        wait();
        o_host_rden.write(false);
        wait();
        rdata = i_host_rdata.read();
        std::cout << " SRAM A[1] Readback: " << rdata[0] << ", " << rdata[1] << ", " << rdata[2] << ", " << rdata[3] << std::endl;
        assert(rdata[0] == 20.0f && rdata[1] == 21.0f && rdata[2] == 22.0f && rdata[3] == 23.0f);

        // Read SRAM B address 0
        o_host_addr.write(SRAMB_OFFSET | 0x00);
        o_host_rden.write(true);
        wait();
        o_host_rden.write(false);
        wait();
        rdata = i_host_rdata.read();
        std::cout << " SRAM B[0] Readback: " << rdata[0] << ", " << rdata[1] << ", " << rdata[2] << ", " << rdata[3] << std::endl;
        assert(rdata[0] == 100.0f && rdata[1] == 101.0f && rdata[2] == 102.0f && rdata[3] == 103.0f);

        // Read SRAM C address 0
        o_host_addr.write(SRAMC_OFFSET | 0x00);
        o_host_rden.write(true);
        wait();
        o_host_rden.write(false);
        wait();
        rdata = i_host_rdata.read();
        std::cout << " SRAM C[0] Readback: " << rdata[0] << ", " << rdata[1] << ", " << rdata[2] << ", " << rdata[3] << std::endl;
        assert(rdata[0] == 1000.0f && rdata[1] == 1001.0f && rdata[2] == 1002.0f && rdata[3] == 1003.0f);

        std::cout << ">>> TC 1 PASSED SUCCESSFULLY!" << std::endl;

        // ----------------------------------------------------
        // TESTCASE 2: Double Buffering Port Selection & Buffer Isolation
        // ----------------------------------------------------
        std::cout << "\n>>> Starting TC 2: Double Buffering Port Selection & Buffer Isolation..." << std::endl;

        // Swap buffer selection: o_select = "111" (bits 0, 1, 2 = 1)
        // Now, Host points to buffer 0 (host_a_idx = 0), NPU points to buffer 1 (npu_a_idx = 1).
        // Let's write to SRAM A address 0 from Host (which writes to buffer 0)
        hdata[0] = 50.0f; hdata[1] = 51.0f; hdata[2] = 52.0f; hdata[3] = 53.0f;
        o_select.write(sc_bv<3>("111"));
        wait();

        o_host_addr.write(SRAMA_OFFSET | 0x00);
        o_host_wdata.write(hdata);
        o_host_wren.write(true);
        wait();
        o_host_wren.write(false);
        wait();

        // Now if the NPU reads SRAM A address 0:
        // Since o_select is "111", NPU reads from buffer 1 (which we wrote 10, 11, 12, 13 to in TC1).
        o_srama_addr.write(0);
        o_srama_rden.write(true);
        wait();
        o_srama_rden.write(false);
        wait();
        act_vector_t<4, float> npu_a_data = i_srama_data.read();
        std::cout << " NPU SRAM A[0] (Reads Buffer 1): " << npu_a_data[0] << ", " << npu_a_data[1] << ", " << npu_a_data[2] << ", " << npu_a_data[3] << std::endl;
        assert(npu_a_data[0] == 10.0f && npu_a_data[1] == 11.0f && npu_a_data[2] == 12.0f && npu_a_data[3] == 13.0f);

        // Swap select back to "000".
        // Now NPU points to buffer 0 (npu_a_idx = 0), where Host wrote 50, 51, 52, 53.
        o_select.write(sc_bv<3>("000"));
        wait();

        o_srama_addr.write(0);
        o_srama_rden.write(true);
        wait();
        o_srama_rden.write(false);
        wait();
        npu_a_data = i_srama_data.read();
        std::cout << " NPU SRAM A[0] (Reads Buffer 0): " << npu_a_data[0] << ", " << npu_a_data[1] << ", " << npu_a_data[2] << ", " << npu_a_data[3] << std::endl;
        assert(npu_a_data[0] == 50.0f && npu_a_data[1] == 51.0f && npu_a_data[2] == 52.0f && npu_a_data[3] == 53.0f);

        std::cout << ">>> TC 2 PASSED SUCCESSFULLY!" << std::endl;

        // ----------------------------------------------------
        // TESTCASE 3: Accelerator-Side Writes & Reads
        // ----------------------------------------------------
        std::cout << "\n>>> Starting TC 3: Accelerator-Side Writes & Reads..." << std::endl;

        // Write to SRAM C from NPU side using write masking.
        // Setup a mask to write only to rows 0 and 2.
        sramc_mask_t<4> c_mask;
        c_mask[0] = true;
        c_mask[1] = false;
        c_mask[2] = true;
        c_mask[3] = false;

        psum_vector_t<4, float> npu_c_wdata;
        npu_c_wdata[0] = 999.0f;
        npu_c_wdata[1] = -1.0f; // masked, shouldn't write
        npu_c_wdata[2] = 888.0f;
        npu_c_wdata[3] = -1.0f; // masked, shouldn't write

        // Let's first select "000" so NPU points to buffer 0 of SRAM C.
        // Currently buffer 0 has nothing written yet, only host wrote 1000, 1001, 1002, 1003 into buffer 1 in TC1.
        o_sramc_addr.write(0);
        o_sramc_wdata.write(npu_c_wdata);
        o_sramc_wmask.write(c_mask);
        o_sramc_wren.write(true);
        wait();
        o_sramc_wren.write(false);
        wait();

        // Read it back from NPU side
        o_sramc_rden.write(true);
        wait();
        o_sramc_rden.write(false);
        wait();
        psum_vector_t<4, float> npu_c_rdata = i_sramc_rdata.read();
        std::cout << " NPU SRAM C[0] Readback: " << npu_c_rdata[0] << ", " << npu_c_rdata[1] << ", " << npu_c_rdata[2] << ", " << npu_c_rdata[3] << std::endl;
        assert(npu_c_rdata[0] == 999.0f && npu_c_rdata[1] == 0.0f && npu_c_rdata[2] == 888.0f && npu_c_rdata[3] == 0.0f);

        std::cout << ">>> TC 3 PASSED SUCCESSFULLY!" << std::endl;

        // ----------------------------------------------------
        // TESTCASE 4: Deep Sleep & Power Gating States
        // ----------------------------------------------------
        std::cout << "\n>>> Starting TC 4: Deep Sleep & Power Gating States..." << std::endl;

        // 1. Deep Sleep
        o_deepsleep.write(true);
        wait();
        // Memory states are preserved, but output reads zero
        o_sramc_rden.write(true);
        wait();
        o_sramc_rden.write(false);
        wait();
        npu_c_rdata = i_sramc_rdata.read();
        std::cout << " NPU SRAM C[0] in Deep Sleep: " << npu_c_rdata[0] << ", " << npu_c_rdata[1] << ", " << npu_c_rdata[2] << ", " << npu_c_rdata[3] << std::endl;
        assert(npu_c_rdata[0] == 0.0f && npu_c_rdata[1] == 0.0f && npu_c_rdata[2] == 0.0f && npu_c_rdata[3] == 0.0f);

        // Disable Deep Sleep
        o_deepsleep.write(false);
        wait();
        // Outputs should now reflect the stored values again
        o_sramc_rden.write(true);
        wait();
        o_sramc_rden.write(false);
        wait();
        npu_c_rdata = i_sramc_rdata.read();
        std::cout << " NPU SRAM C[0] after Deep Sleep exit: " << npu_c_rdata[0] << ", " << npu_c_rdata[1] << ", " << npu_c_rdata[2] << ", " << npu_c_rdata[3] << std::endl;
        assert(npu_c_rdata[0] == 999.0f && npu_c_rdata[1] == 0.0f && npu_c_rdata[2] == 888.0f && npu_c_rdata[3] == 0.0f);

        // 2. Power Gating
        o_powergate.write(true);
        wait();
        // Memory states are lost, outputs are zero
        o_powergate.write(false);
        wait();
        o_sramc_rden.write(true);
        wait();
        o_sramc_rden.write(false);
        wait();
        npu_c_rdata = i_sramc_rdata.read();
        std::cout << " NPU SRAM C[0] after Power Gating cycle: " << npu_c_rdata[0] << ", " << npu_c_rdata[1] << ", " << npu_c_rdata[2] << ", " << npu_c_rdata[3] << std::endl;
        assert(npu_c_rdata[0] == 0.0f && npu_c_rdata[1] == 0.0f && npu_c_rdata[2] == 0.0f && npu_c_rdata[3] == 0.0f);

        std::cout << ">>> TC 4 PASSED SUCCESSFULLY!" << std::endl;

        std::cout << "\n=============================================================" << std::endl;
        std::cout << "          SAURIA Standalone SRAM ALL TESTS PASSED            " << std::endl;
        std::cout << "=============================================================\n" << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char* argv[]) {
    sc_clock clk("clk", 10, SC_NS);

    // Instantiate Sram block
    Sram<4, 4, float, float, float, 64, 64, 128> sram_inst("sram_inst");

    // Instantiate Testbench
    TestbenchSram tb("tb");

    // Signal bindings
    sc_signal<bool> s_rstn;
    sc_signal<bool> s_deepsleep;
    sc_signal<bool> s_powergate;
    sc_signal<sc_bv<3>> s_select;

    sc_signal<uint32_t>     s_host_addr;
    sc_signal<bool>         s_host_wren;
    sc_signal<bool>         s_host_rden;
    sc_signal<host_data_t>  s_host_wdata;
    sc_signal<host_mask_t>  s_host_wmask;
    sc_signal<host_data_t>  s_host_rdata;

    sc_signal<uint32_t>             s_srama_addr;
    sc_signal<bool>                 s_srama_rden;
    sc_signal<act_vector_t<4, float>> s_srama_data;

    sc_signal<uint32_t>             s_sramb_addr;
    sc_signal<bool>                 s_sramb_rden;
    sc_signal<wei_vector_t<4, float>> s_sramb_data;

    sc_signal<psum_vector_t<4, float>> s_sramc_wdata;
    sc_signal<uint32_t>                s_sramc_addr;
    sc_signal<bool>                    s_sramc_wren;
    sc_signal<bool>                    s_sramc_rden;
    sc_signal<sramc_mask_t<4>>         s_sramc_wmask;
    sc_signal<psum_vector_t<4, float>> s_sramc_rdata;

    // Connect SRAM
    sram_inst.i_clk(clk);
    sram_inst.i_rstn(s_rstn);
    sram_inst.i_deepsleep(s_deepsleep);
    sram_inst.i_powergate(s_powergate);
    sram_inst.i_select(s_select);

    sram_inst.i_host_addr(s_host_addr);
    sram_inst.i_host_wren(s_host_wren);
    sram_inst.i_host_rden(s_host_rden);
    sram_inst.i_host_wdata(s_host_wdata);
    sram_inst.i_host_wmask(s_host_wmask);
    sram_inst.o_host_rdata(s_host_rdata);

    sram_inst.i_srama_addr(s_srama_addr);
    sram_inst.i_srama_rden(s_srama_rden);
    sram_inst.o_srama_data(s_srama_data);

    sram_inst.i_sramb_addr(s_sramb_addr);
    sram_inst.i_sramb_rden(s_sramb_rden);
    sram_inst.o_sramb_data(s_sramb_data);

    sram_inst.i_sramc_wdata(s_sramc_wdata);
    sram_inst.i_sramc_addr(s_sramc_addr);
    sram_inst.i_sramc_wren(s_sramc_wren);
    sram_inst.i_sramc_rden(s_sramc_rden);
    sram_inst.i_sramc_wmask(s_sramc_wmask);
    sram_inst.o_sramc_rdata(s_sramc_rdata);

    // Connect Testbench
    tb.i_clk(clk);
    tb.o_rstn(s_rstn);
    tb.o_deepsleep(s_deepsleep);
    tb.o_powergate(s_powergate);
    tb.o_select(s_select);

    tb.o_host_addr(s_host_addr);
    tb.o_host_wren(s_host_wren);
    tb.o_host_rden(s_host_rden);
    tb.o_host_wdata(s_host_wdata);
    tb.o_host_wmask(s_host_wmask);
    tb.i_host_rdata(s_host_rdata);

    tb.o_srama_addr(s_srama_addr);
    tb.o_srama_rden(s_srama_rden);
    tb.i_srama_data(s_srama_data);

    tb.o_sramb_addr(s_sramb_addr);
    tb.o_sramb_rden(s_sramb_rden);
    tb.i_sramb_data(s_sramb_data);

    tb.o_sramc_wdata(s_sramc_wdata);
    tb.o_sramc_addr(s_sramc_addr);
    tb.o_sramc_wren(s_sramc_wren);
    tb.o_sramc_rden(s_sramc_rden);
    tb.o_sramc_wmask(s_sramc_wmask);
    tb.i_sramc_rdata(s_sramc_rdata);

    sc_start();
    return 0;
}
