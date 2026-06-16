// SystemC Model for SAURIA NPU Core
// Standalone PSM Testbench (tb_psm.cpp)

#include "../sauria_types.h"
#include "psm_top.h"
#include <iostream>
#include <iomanip>
#include <cassert>

using namespace sauria;

// Mock SRAM C with 1-cycle read latency and Y-bit write masking
template <int Y_DIM = 4, typename T_PSUM = float, int SRAMC_CAP = 128>
class MockSramC : public sc_module {
public:
    sc_in<bool> i_clk{"i_clk"};
    sc_in<uint32_t> i_addr{"i_addr"};
    sc_in<bool> i_wren{"i_wren"};
    sc_in<bool> i_rden{"i_rden"};
    sc_in<sramc_mask_t<Y_DIM>> i_wmask{"i_wmask"};
    sc_in<psum_vector_t<Y_DIM, T_PSUM>> i_wdata{"i_wdata"};
    sc_out<psum_vector_t<Y_DIM, T_PSUM>> o_rdata{"o_rdata"};

    psum_vector_t<Y_DIM, T_PSUM> mem[SRAMC_CAP];

    SC_CTOR(MockSramC) {
        SC_METHOD(mem_process);
        sensitive << i_clk.pos();
    }

    void mem_process() {
        if (i_wren.read()) {
            uint32_t addr = i_addr.read();
            sramc_mask_t<Y_DIM> mask = i_wmask.read();
            psum_vector_t<Y_DIM, T_PSUM> wdata = i_wdata.read();
            if (addr < SRAMC_CAP) {
                for (int y = 0; y < Y_DIM; y++) {
                    if (mask[y]) {
                        mem[addr][y] = wdata[y];
                    }
                }
            }
        }
        
        static psum_vector_t<Y_DIM, T_PSUM> rdata_next;
        if (i_rden.read()) {
            uint32_t addr = i_addr.read();
            if (addr < SRAMC_CAP) {
                rdata_next = mem[addr];
            } else {
                rdata_next = psum_vector_t<Y_DIM, T_PSUM>();
            }
        }
        o_rdata.write(rdata_next);
    }
};

class TestbenchPSM : public sc_module {
public:
    sc_in<bool> i_clk{"i_clk"};
    sc_out<bool> o_rstn{"o_rstn"};

    // Data to PSM
    sc_out<psum_vector_t<4, float>> o_c_arr{"o_c_arr"};

    // Config ports
    sc_out<uint32_t> o_cxlim{"o_cxlim"};
    sc_out<uint32_t> o_cxstep{"o_cxstep"};
    sc_out<uint32_t> o_cklim{"o_cklim"};
    sc_out<uint32_t> o_ckstep{"o_ckstep"};
    sc_out<uint32_t> o_til_cylim{"o_til_cylim"};
    sc_out<uint32_t> o_til_cystep{"o_til_cystep"};
    sc_out<uint32_t> o_til_cklim{"o_til_cklim"};
    sc_out<uint32_t> o_til_ckstep{"o_til_ckstep"};
    sc_out<uint32_t> o_ncontexts{"o_ncontexts"};
    sc_out<bool>     o_preload_en{"o_preload_en"};
    sc_out<sramc_mask_t<4>> o_rows_active{"o_rows_active"};

    // FSM control
    sc_out<bool> o_fsm_start{"o_fsm_start"};
    sc_out<bool> o_fsm_reset{"o_fsm_reset"};
    sc_out<bool> o_pipeline_en{"o_pipeline_en"};

    // memory
    sc_out<uint32_t> o_out_base_addr{"o_out_base_addr"};

    // Signals from PSM
    sc_in<bool> i_done{"i_done"};
    sc_in<bool> i_finalwrite{"i_finalwrite"};
    sc_in<bool> i_shift_done{"i_shift_done"};
    sc_in<bool> i_cscan_en{"i_cscan_en"};
    sc_in<psum_vector_t<4, float>> i_c_arr{"i_c_arr"};

    // Pointer to mock memory for direct verification
    MockSramC<4, float, 128>* mock_mem{nullptr};

    SC_HAS_PROCESS(TestbenchPSM);
    TestbenchPSM(sc_module_name nm, MockSramC<4, float, 128>* mem_ptr)
        : sc_module(nm), mock_mem(mem_ptr) {
        SC_THREAD(test_process);
        sensitive << i_clk.pos();
    }

private:
    void test_process() {
        std::cout << "\n=============================================================" << std::endl;
        std::cout << "          SAURIA Standalone PSM block Testbench" << std::endl;
        std::cout << "=============================================================\n" << std::endl;

        // Initialize signals
        o_rstn.write(false);
        o_c_arr.write(psum_vector_t<4, float>());
        o_cxlim.write(4);
        o_cxstep.write(1);
        o_cklim.write(4);
        o_ckstep.write(1);
        o_til_cylim.write(4);
        o_til_cystep.write(1);
        o_til_cklim.write(4);
        o_til_ckstep.write(1);
        o_ncontexts.write(1);
        o_preload_en.write(false);
        o_out_base_addr.write(0);
        
        sramc_mask_t<4> active_mask(true); // Enable all 4 rows
        o_rows_active.write(active_mask);

        o_fsm_start.write(false);
        o_fsm_reset.write(false);
        o_pipeline_en.write(true);

        wait(3);
        o_rstn.write(true);
        std::cout << "[TB] Reset released." << std::endl;
        wait();

        // ----------------------------------------------------
        // TESTCASE 1: Scan-out / Store mode (i_preload_en = false)
        // ----------------------------------------------------
        std::cout << "\n>>> Starting TC 1: Scan-out Mode (Writing Accumulators to SRAM C)..." << std::endl;
        
        float expected_data[4][4] = {
            {1.1f, 2.2f, 3.3f, 4.4f},
            {5.5f, 6.6f, 7.7f, 8.8f},
            {9.9f, 10.1f, 11.2f, 12.3f},
            {13.4f, 14.5f, 15.6f, 16.7f}
        };

        // Assert start
        o_fsm_start.write(true);
        wait();
        o_fsm_start.write(false);

        // Feed input wavefront vectors on each clock edge while scanning is active
        for (int step = 0; step < 4; step++) {
            // Wait for cscan_en to be active
            while (!i_cscan_en.read()) {
                wait();
            }
            psum_vector_t<4, float> test_vec;
            for (int y = 0; y < 4; y++) {
                test_vec[y] = expected_data[step][y];
            }
            o_c_arr.write(test_vec);
            wait();
        }

        // Wait for done
        while (!i_done.read()) {
            wait();
        }
        std::cout << "[TB] Scan-out finished." << std::endl;

        // Verify Mock Memory Content
        bool tc1_passed = true;
        std::cout << "\n--- TC 1 SRAM C WRITE CHECKS ---" << std::endl;
        std::cout << " SRAM Address |  SRAM Written Data  |  Expected Wavefront  | Status" << std::endl;
        std::cout << "--------------+---------------------+----------------------+--------" << std::endl;
        for (int addr = 0; addr < 4; addr++) {
            psum_vector_t<4, float> val = mock_mem->mem[addr];
            bool pass = true;
            for (int y = 0; y < 4; y++) {
                if (std::abs(val[y] - expected_data[addr][y]) > 1e-3) {
                    pass = false;
                    tc1_passed = false;
                }
            }
            std::cout << "      " << addr << "       |  [" 
                      << val[0] << ", " << val[1] << ", " << val[2] << ", " << val[3] << "]  |  ["
                      << expected_data[addr][0] << ", " << expected_data[addr][1] << ", " << expected_data[addr][2] << ", " << expected_data[addr][3] << "]  | "
                      << (pass ? "PASS" : "*FAIL*") << std::endl;
        }

        if (tc1_passed) {
            std::cout << ">>> TC 1 PASSED SUCCESSFULLY!" << std::endl;
        } else {
            std::cout << ">>> TC 1 FAILED!" << std::endl;
        }

        // ----------------------------------------------------
        // TESTCASE 2: Preload / Load mode (i_preload_en = true)
        // ----------------------------------------------------
        std::cout << "\n>>> Starting TC 2: Preload Mode (Reading from SRAM C into Array)..." << std::endl;
        
        float preload_data[4][4] = {
            {100.0f, 101.0f, 102.0f, 103.0f},
            {200.0f, 201.0f, 202.0f, 203.0f},
            {300.0f, 301.0f, 302.0f, 303.0f},
            {400.0f, 401.0f, 402.0f, 403.0f}
        };

        // Populate Mock Memory
        for (int addr = 0; addr < 4; addr++) {
            psum_vector_t<4, float> vec;
            for (int y = 0; y < 4; y++) {
                vec[y] = preload_data[addr][y];
            }
            mock_mem->mem[addr] = vec;
        }

        o_preload_en.write(true);
        o_fsm_start.write(true);
        wait();
        o_fsm_start.write(false);

        float readback_C[4][4] = {0.0f};
        
        // Wait 3 cycles for data to propagate
        wait();
        wait();
        wait();

        // Capture shifted out data driven on o_c_arr from PSM
        for (int step = 0; step < 4; step++) {
            psum_vector_t<4, float> data_out = i_c_arr.read();
            for (int y = 0; y < 4; y++) {
                readback_C[step][y] = data_out[y];
            }
            wait();
        }

        while (!i_done.read()) {
            wait();
        }
        std::cout << "[TB] Preload finished." << std::endl;

        // Verify Preloaded output values
        bool tc2_passed = true;
        std::cout << "\n--- TC 2 READBACK PREVIEW ---" << std::endl;
        std::cout << " Step Index |  Shifted Array Out  |   Mock SRAM Data    | Status" << std::endl;
        std::cout << "------------+---------------------+---------------------+--------" << std::endl;
        for (int step = 0; step < 4; step++) {
            bool pass = true;
            for (int y = 0; y < 4; y++) {
                if (std::abs(readback_C[step][y] - preload_data[step][y]) > 1e-3) {
                    pass = false;
                    tc2_passed = false;
                }
            }
            std::cout << "     " << step << "      |  [" 
                      << readback_C[step][0] << ", " << readback_C[step][1] << ", " << readback_C[step][2] << ", " << readback_C[step][3] << "]  |  ["
                      << preload_data[step][0] << ", " << preload_data[step][1] << ", " << preload_data[step][2] << ", " << preload_data[step][3] << "]  | "
                      << (pass ? "PASS" : "*FAIL*") << std::endl;
        }

        if (tc2_passed) {
            std::cout << ">>> TC 2 PASSED SUCCESSFULLY!" << std::endl;
        } else {
            std::cout << ">>> TC 2 FAILED!" << std::endl;
        }

        // ----------------------------------------------------
        // TESTCASE 3: Runtime OUT_BASE_ADDR + CXSTEP + ROWS_ACTIVE
        // ----------------------------------------------------
        std::cout << "\n>>> Starting TC 3: Runtime OUT_BASE_ADDR + CXSTEP + ROWS_ACTIVE..." << std::endl;

        // Clear memory
        for (int addr = 0; addr < 128; addr++) {
            mock_mem->mem[addr] = psum_vector_t<4, float>();
        }

        // Runtime config
        o_out_base_addr.write(8);
        o_cxlim.write(4);
        o_cxstep.write(2);
        o_cklim.write(4);
        o_ckstep.write(1);
        o_preload_en.write(false);

        sramc_mask_t<4> partial_mask;
        partial_mask[0] = true;
        partial_mask[1] = false;
        partial_mask[2] = true;
        partial_mask[3] = false;
        o_rows_active.write(partial_mask);

        wait();

        float expected_data_tc3[4][4] = {
            {10.0f, 11.0f, 12.0f, 13.0f},
            {20.0f, 21.0f, 22.0f, 23.0f},
            {30.0f, 31.0f, 32.0f, 33.0f},
            {40.0f, 41.0f, 42.0f, 43.0f}
        };

        // Start PSM
        o_fsm_start.write(true);
        wait();
        o_fsm_start.write(false);

        // Feed 4 vectors
        for (int step = 0; step < 4; step++) {
            while (!i_cscan_en.read()) {
                wait();
        }

        psum_vector_t<4, float> test_vec;
            for (int y = 0; y < 4; y++) {
                test_vec[y] = expected_data_tc3[step][y];
            }

            o_c_arr.write(test_vec);
            wait();
        }

        while (!i_done.read()) {
            wait();
        }
        std::cout << "[TB] TC3 finished." << std::endl;

        bool tc3_passed = true;

        std::cout << "\n--- TC 3 SRAM C RUNTIME WRITE CHECKS ---" << std::endl;

        for (int i = 0; i < 4; i++) {
            uint32_t addr = 8 + i * 2;
            psum_vector_t<4, float> val = mock_mem->mem[addr];

            bool pass = true;

            // mask = 1010, tức row 0 và row 2 được ghi
            if (std::abs(val[0] - expected_data_tc3[i][0]) > 1e-3) pass = false;
            if (std::abs(val[2] - expected_data_tc3[i][2]) > 1e-3) pass = false;

            // row 1 và row 3 không được ghi, kỳ vọng vẫn 0
            if (std::abs(val[1] - 0.0f) > 1e-3) pass = false;
            if (std::abs(val[3] - 0.0f) > 1e-3) pass = false;

            if (!pass) tc3_passed = false;

            std::cout << "addr " << addr
                                      << " -> [" << val[0] << ", " << val[1]
                                      << ", " << val[2] << ", " << val[3] << "] "
                                      << (pass ? "PASS" : "*FAIL*")
                                      << std::endl;
            }

        // Kiểm tra các địa chỉ không nên bị ghi
        for (int addr : {0, 1, 2, 3, 8 + 1, 8 + 3}) {
            psum_vector_t<4, float> val = mock_mem->mem[addr];
            for (int y = 0; y < 4; y++) {
                if (std::abs(val[y]) > 1e-3) {
                    tc3_passed = false;
                }
            }
        }

        std::cout << (tc3_passed ? ">>> TC 3 PASSED SUCCESSFULLY!" : ">>> TC 3 FAILED!") << std::endl;

        std::cout << "\n=============================================================" << std::endl;
        std::cout << "           SAURIA Standalone PSM Simulation Ended            " << std::endl;
        std::cout << "=============================================================\n" << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char* argv[]) {
    sc_clock clk("clk", 10, SC_NS);

    sc_signal<bool> rstn{"rstn"};
    sc_signal<psum_vector_t<4, float>> c_arr_to_psm{"c_arr_to_psm"};
    sc_signal<psum_vector_t<4, float>> sramc_rdata{"sramc_rdata"};
    sc_signal<uint32_t> sramc_addr{"sramc_addr"};
    sc_signal<bool> sramc_wren{"sramc_wren"};
    sc_signal<bool> sramc_rden{"sramc_rden"};
    sc_signal<sramc_mask_t<4>> sramc_wmask{"sramc_wmask"};
    sc_signal<psum_vector_t<4, float>> sramc_wdata{"sramc_wdata"};

    sc_signal<uint32_t> cxlim{"cxlim"};
    sc_signal<uint32_t> cxstep{"cxstep"};
    sc_signal<uint32_t> cklim{"cklim"};
    sc_signal<uint32_t> ckstep{"ckstep"};
    sc_signal<uint32_t> til_cylim{"til_cylim"};
    sc_signal<uint32_t> til_cystep{"til_cystep"};
    sc_signal<uint32_t> til_cklim{"til_cklim"};
    sc_signal<uint32_t> til_ckstep{"til_ckstep"};
    sc_signal<uint32_t> ncontexts{"ncontexts"};
    sc_signal<bool> preload_en{"preload_en"};
    sc_signal<sramc_mask_t<4>> rows_active{"rows_active"};

    sc_signal<uint32_t> out_base_addr{"out_base_addr"};

    sc_signal<bool> fsm_start{"fsm_start"}; 
    sc_signal<bool> fsm_reset{"fsm_reset"};
    sc_signal<bool> pipeline_en{"pipeline_en"};

    sc_signal<bool> done{"done"};
    sc_signal<bool> finalwrite{"finalwrite"};
    sc_signal<bool> shift_done{"shift_done"};
    sc_signal<bool> cscan_en{"cscan_en"};
    sc_signal<psum_vector_t<4, float>> c_arr_from_psm{"c_arr_from_psm"};

    // Mock components
    MockSramC<4, float, 128> sramc("sramc");
    Psm<4, 4, float, 128> dut("dut");
    TestbenchPSM tb("tb", &sramc);

    // Bindings
    sramc.i_clk(clk);
    sramc.i_addr(sramc_addr);
    sramc.i_wren(sramc_wren);
    sramc.i_rden(sramc_rden);
    sramc.i_wmask(sramc_wmask);
    sramc.i_wdata(sramc_wdata);
    sramc.o_rdata(sramc_rdata);

    dut.i_clk(clk);
    dut.i_rstn(rstn);
    dut.i_c_arr(c_arr_to_psm);
    dut.i_sramc_rdata(sramc_rdata);
    dut.o_sramc_addr(sramc_addr);
    dut.o_sramc_wren(sramc_wren);
    dut.o_sramc_rden(sramc_rden);
    dut.o_sramc_wmask(sramc_wmask);
    dut.o_sramc_wdata(sramc_wdata);

    dut.i_cxlim(cxlim);
    dut.i_cxstep(cxstep);
    dut.i_cklim(cklim);
    dut.i_ckstep(ckstep);
    dut.i_til_cylim(til_cylim);
    dut.i_til_cystep(til_cystep);
    dut.i_til_cklim(til_cklim);
    dut.i_til_ckstep(til_ckstep);
    dut.i_ncontexts(ncontexts);
    dut.i_preload_en(preload_en);
    dut.i_rows_active(rows_active);

    dut.i_fsm_start(fsm_start);
    dut.i_fsm_reset(fsm_reset);
    dut.i_pipeline_en(pipeline_en);

    dut.i_out_base_addr(out_base_addr);
    
    dut.o_done(done);
    dut.o_finalwrite(finalwrite);
    dut.o_shift_done(shift_done);
    dut.o_cscan_en(cscan_en);
    dut.o_c_arr(c_arr_from_psm);

    tb.i_clk(clk);
    tb.o_rstn(rstn);
    tb.o_c_arr(c_arr_to_psm);
    tb.o_cxlim(cxlim);
    tb.o_cxstep(cxstep);
    tb.o_cklim(cklim);
    tb.o_ckstep(ckstep);
    tb.o_til_cylim(til_cylim);
    tb.o_til_cystep(til_cystep);
    tb.o_til_cklim(til_cklim);
    tb.o_til_ckstep(til_ckstep);
    tb.o_ncontexts(ncontexts);
    tb.o_preload_en(preload_en);
    tb.o_rows_active(rows_active);
    tb.o_fsm_start(fsm_start);
    tb.o_fsm_reset(fsm_reset);
    tb.o_pipeline_en(pipeline_en);
    tb.o_out_base_addr(out_base_addr);

    tb.i_done(done);
    tb.i_finalwrite(finalwrite);
    tb.i_shift_done(shift_done);
    tb.i_cscan_en(cscan_en);
    tb.i_c_arr(c_arr_from_psm);

    sc_trace_file* tf = sc_create_vcd_trace_file("waves_psm");
    if (tf) {
        sc_trace(tf, clk, "clk");
        sc_trace(tf, rstn, "rstn");
        sc_trace(tf, fsm_start, "fsm_start");
        sc_trace(tf, done, "done");
        sc_trace(tf, cscan_en, "cscan_en");
        sc_trace(tf, sramc_wren, "sramc_wren");
        sc_trace(tf, sramc_rden, "sramc_rden");
    }

    sc_start();

    if (tf) {
        sc_close_vcd_trace_file(tf);
    }
    return 0;
}
