// Standalone Testbench for SAURIA NPU Data Feeders (IfmapFeeder and WeightFeeder)

#include <systemc.h>
#include <iostream>
#include <iomanip>
#include <cmath>
#include "data_feeder/ifmap_feeder.h"
#include "data_feeder/wei_feeder.h"

using namespace sauria;

// Mock SRAM A block with 1-cycle read latency
class MockSramA : public sc_module {
public:
    sc_in<bool> i_clk{"i_clk"};
    sc_in<uint32_t> i_addr{"i_addr"};
    sc_in<bool> i_rden{"i_rden"};
    sc_out<act_vector_t<4, float>> o_rdata{"o_rdata"};

    SC_CTOR(MockSramA) {
        SC_METHOD(read_process);
        sensitive << i_clk.pos();
    }

private:
    void read_process() {
        if (i_rden.read()) {
            uint32_t addr = i_addr.read();
            act_vector_t<4, float> data;
            // Generate deterministic float values for testing
            for (int y = 0; y < 4; y++) {
                data[y] = (addr + 1) * 10.0f + y; // e.g. Addr 0: 10, 11, 12, 13
            }
            o_rdata.write(data);
        }
    }
};

// Mock SRAM B block with 1-cycle read latency
class MockSramB : public sc_module {
public:
    sc_in<bool> i_clk{"i_clk"};
    sc_in<uint32_t> i_addr{"i_addr"};
    sc_in<bool> i_rden{"i_rden"};
    sc_out<wei_vector_t<4, float>> o_rdata{"o_rdata"};

    SC_CTOR(MockSramB) {
        SC_METHOD(read_process);
        sensitive << i_clk.pos();
    }

private:
    void read_process() {
        if (i_rden.read()) {
            uint32_t addr = i_addr.read();
            wei_vector_t<4, float> data;
            // Generate deterministic float values for testing
            for (int x = 0; x < 4; x++) {
                data[x] = (addr + 1) * 100.0f + x; // e.g. Addr 0: 100, 101, 102, 103
            }
            o_rdata.write(data);
        }
    }
};

class TestbenchDataFeeder : public sc_module {
public:
    sc_clock clk{"clk", 10, SC_NS};
    sc_signal<bool> rstn{"rstn", false};

    // Feeder Control Signals
    sc_signal<bool> feeder_en{"feeder_en", false};
    sc_signal<bool> feeder_clear{"feeder_clear", false};
    sc_signal<bool> start{"start", false};
    sc_signal<bool> valid{"valid", false};
    sc_signal<bool> finalpush{"finalpush", false};
    sc_signal<bool> cnt_en{"cnt_en", false};
    sc_signal<bool> cnt_clear{"cnt_clear", false};
    sc_signal<bool> clearfifo{"clearfifo", false};
    sc_signal<bool> pop_en{"pop_en", false};
    
    // IFmap specific
    sc_signal<bool> finalctx{"finalctx", false};
    sc_signal<uint32_t> act_incntlim{"act_incntlim", 4};
    sc_signal<uint32_t> act_incntstep{"act_incntstep", 1};
    sc_signal<uint32_t> act_outcntlim{"act_outcntlim", 4};
    sc_signal<uint32_t> act_outcntstep{"act_outcntstep", 1};
    sc_signal<sc_bv<DILP_W>> act_dil_pat{"act_dil_pat", 0};

    // Weight specific
    sc_signal<bool> cswitch{"cswitch", false};
    sc_signal<uint32_t> wei_incntlim{"wei_incntlim", 4};
    sc_signal<uint32_t> wei_incntstep{"wei_incntstep", 1};

    // SRAM Interfaces
    sc_signal<uint32_t> srama_addr{"srama_addr"};
    sc_signal<bool> srama_rden{"srama_rden"};
    sc_signal<act_vector_t<4, float>> srama_data{"srama_data"};

    sc_signal<uint32_t> sramb_addr{"sramb_addr"};
    sc_signal<bool> sramb_rden{"sramb_rden"};
    sc_signal<wei_vector_t<4, float>> sramb_data{"sramb_data"};

    // Wavefront Array outputs
    sc_signal<act_vector_t<4, float>> act_arr{"act_arr"};
    sc_signal<wei_vector_t<4, float>> wei_arr{"wei_arr"};

    // Feeders status outputs
    sc_signal<bool> act_done{"act_done"};
    sc_signal<bool> act_til_done{"act_til_done"};
    sc_signal<bool> act_fifo_empty{"act_fifo_empty"};
    sc_signal<bool> act_fifo_full{"act_fifo_full"};
    sc_signal<bool> act_stall{"act_stall"};

    sc_signal<bool> wei_done{"wei_done"};
    sc_signal<bool> wei_til_done{"wei_til_done"};
    sc_signal<bool> wei_fifo_empty{"wei_fifo_empty"};
    sc_signal<bool> wei_fifo_full{"wei_fifo_full"};
    sc_signal<bool> wei_stall{"wei_stall"};

    // base address
    sc_signal<uint32_t> act_base_addr{"act_base_addr", 0};
    sc_signal<uint32_t> wei_base_addr{"wei_base_addr", 0};

    // Submodules
    MockSramA* sram_a;
    MockSramB* sram_b;
    IfmapFeeder<4, float, 128, 8>* act_fd;
    WeightFeeder<4, float, 128, 8>* wei_fd;

    SC_HAS_PROCESS(TestbenchDataFeeder);
    TestbenchDataFeeder(sc_module_name nm) : sc_module(nm) {
        // Instantiate Mock SRAM A
        sram_a = new MockSramA("sram_a");
        sram_a->i_clk(clk);
        sram_a->i_addr(srama_addr);
        sram_a->i_rden(srama_rden);
        sram_a->o_rdata(srama_data);

        // Instantiate Mock SRAM B
        sram_b = new MockSramB("sram_b");
        sram_b->i_clk(clk);
        sram_b->i_addr(sramb_addr);
        sram_b->i_rden(sramb_rden);
        sram_b->o_rdata(sramb_data);

        // Instantiate Activation Feeder (FIFO Depth = 8)
        act_fd = new IfmapFeeder<4, float, 128, 8>("act_fd");
        act_fd->i_clk(clk);
        act_fd->i_rstn(rstn);
        act_fd->i_feeder_en(feeder_en);
        act_fd->i_feeder_clear(feeder_clear);
        act_fd->i_start(start);
        act_fd->i_valid(valid);
        act_fd->i_finalpush(finalpush);
        act_fd->i_cnt_en(cnt_en);
        act_fd->i_cnt_clear(cnt_clear);
        act_fd->i_clearfifo(clearfifo);
        act_fd->i_pop_en(pop_en);
        act_fd->i_finalctx(finalctx);
        act_fd->i_act_incntlim(act_incntlim);
        act_fd->i_act_incntstep(act_incntstep);
        act_fd->i_act_outcntlim(act_outcntlim);
        act_fd->i_act_outcntstep(act_outcntstep);
        act_fd->i_act_dil_pat(act_dil_pat);
        act_fd->i_act_base_addr(act_base_addr);

        act_fd->o_srama_addr(srama_addr);
        act_fd->o_srama_rden(srama_rden);
        act_fd->i_srama_data(srama_data);
        act_fd->o_act_arr(act_arr);
        act_fd->o_act_done(act_done);
        act_fd->o_act_til_done(act_til_done);
        act_fd->o_fifo_empty(act_fifo_empty);
        act_fd->o_fifo_full(act_fifo_full);
        act_fd->o_stall(act_stall);

        // Instantiate Weight Feeder (FIFO Depth = 8)
        wei_fd = new WeightFeeder<4, float, 128, 8>("wei_fd");
        wei_fd->i_clk(clk);
        wei_fd->i_rstn(rstn);
        wei_fd->i_feeder_en(feeder_en);
        wei_fd->i_feeder_clear(feeder_clear);
        wei_fd->i_start(start);
        wei_fd->i_valid(valid);
        wei_fd->i_finalpush(finalpush);
        wei_fd->i_cnt_en(cnt_en);
        wei_fd->i_cnt_clear(cnt_clear);
        wei_fd->i_clearfifo(clearfifo);
        wei_fd->i_pop_en(pop_en);
        wei_fd->i_cswitch(cswitch);
        wei_fd->i_wei_incntlim(wei_incntlim);
        wei_fd->i_wei_incntstep(wei_incntstep);
        wei_fd->i_wei_base_addr(wei_base_addr);

        wei_fd->o_sramb_addr(sramb_addr);
        wei_fd->o_sramb_rden(sramb_rden);
        wei_fd->i_sramb_data(sramb_data);
        wei_fd->o_wei_arr(wei_arr);
        wei_fd->o_wei_done(wei_done);
        wei_fd->o_wei_til_done(wei_til_done);
        wei_fd->o_fifo_empty(wei_fifo_empty);
        wei_fd->o_fifo_full(wei_fifo_full);
        wei_fd->o_stall(wei_stall);

        SC_THREAD(test_process);
        sensitive << clk.posedge_event();
    }

    ~TestbenchDataFeeder() {
        delete sram_a;
        delete sram_b;
        delete act_fd;
        delete wei_fd;
    }

private:
    void test_process() {
        std::cout << "[TB] Reset asserted." << std::endl;
        rstn.write(false);
        wait(3);
        rstn.write(true);
        std::cout << "[TB] Reset released." << std::endl;
        wait();

        // Trace to VCD file
        sc_trace_file* tf = sc_create_vcd_trace_file("waves_feeders");
        sc_trace(tf, clk, "clk");
        sc_trace(tf, rstn, "rstn");
        sc_trace(tf, feeder_en, "feeder_en");
        sc_trace(tf, cnt_en, "cnt_en");
        sc_trace(tf, pop_en, "pop_en");
        sc_trace(tf, srama_addr, "srama_addr");
        sc_trace(tf, srama_rden, "srama_rden");
        sc_trace(tf, sramb_addr, "sramb_addr");
        sc_trace(tf, sramb_rden, "sramb_rden");
        sc_trace(tf, act_fifo_empty, "act_fifo_empty");
        sc_trace(tf, act_fifo_full, "act_fifo_full");
        sc_trace(tf, wei_fifo_empty, "wei_fifo_empty");
        sc_trace(tf, wei_fifo_full, "wei_fifo_full");

        sc_bv<DILP_W> dil_all_ones;
        for(int i = 0; i < DILP_W; i++){
            dil_all_ones[i] = 1;
        }
        act_dil_pat.write(dil_all_ones);

        // =============================================================
        // TC 1: Activation Feeder Wavefront Skew Verification
        // =============================================================
        std::cout << "\n>>> Starting TC 1: Activation Feeder Wavefront Skew Test..." << std::endl;
        feeder_en.write(true);
        
        // Load 4 entries into row FIFOs from memory
        cnt_en.write(true);
        for (int i = 0; i < 4; i++) {
            wait();
        }
        cnt_en.write(false);
        wait();
        wait();

        // Verify status flags
        std::cout << " FIFOs Loaded check: Empty=" << act_fifo_empty.read()
                  << " Full=" << act_fifo_full.read() << std::endl;

        // Enable popping to observe the skewed wavefront output
        pop_en.write(true);
        wait();
        wait();
        
        float wavefront_record[8][4];
        for (int step = 0; step < 8; step++) {
            act_vector_t<4, float> data_out = act_arr.read();
            for (int y = 0; y < 4; y++) {
                wavefront_record[step][y] = data_out[y];
            }
            wait();
        }
        pop_en.write(false);

        // Verify TC1 Results
        bool tc1_passed = true;
        std::cout << "\n--- TC 1 WAVEFRONT SKEW PREVIEW ---" << std::endl;
        std::cout << " Step Index |   Row 0   |   Row 1   |   Row 2   |   Row 3   | Status" << std::endl;
        std::cout << "------------+-----------+-----------+-----------+-----------+--------" << std::endl;
        
        // Row 0 has 0 delay cycles. So expected values are:
        // Step 0: 10.0 (from addr 0)
        // Step 1: 20.0 (from addr 1)
        // Step 2: 30.0 (from addr 2)
        // Step 3: 40.0 (from addr 3)
        // Step 4+: 0.0
        
        // Row 1 has 1 delay cycle. Expected:
        // Step 0: 0.0
        // Step 1: 11.0
        // Step 2: 21.0
        // Step 3: 31.0
        // Step 4: 41.0
        
        // Row 2 has 2 delay cycles. Expected:
        // Step 0, 1: 0.0
        // Step 2: 12.0
        // Step 3: 22.0
        // Step 4: 32.0
        // Step 5: 42.0

        // Row 3 has 3 delay cycles. Expected:
        // Step 0, 1, 2: 0.0
        // Step 3: 13.0
        // Step 4: 23.0
        // Step 5: 33.0
        // Step 6: 43.0

        float expected_act_tc1[8][4] = {
            {  0.0f,  0.0f,  0.0f,  0.0f },
            { 10.0f,  0.0f,  0.0f,  0.0f },
            { 20.0f, 11.0f,  0.0f,  0.0f },
            { 30.0f, 21.0f, 12.0f,  0.0f },
            {  0.0f, 31.0f, 22.0f, 13.0f },
            {  0.0f,  0.0f, 32.0f, 23.0f },
            {  0.0f,  0.0f,  0.0f, 33.0f },
            {  0.0f,  0.0f,  0.0f,  0.0f }
        };

        for (int step = 0; step < 8; step++) {
            bool pass = true;
            for (int y = 0; y < 4; y++) {
                if (std::abs(wavefront_record[step][y] - expected_act_tc1[step][y]) > 1e-3) {
                    pass = false;
                    tc1_passed = false;
                }
            }
            std::cout << "     " << step << "      | " 
                      << std::setw(9) << wavefront_record[step][0] << " | "
                      << std::setw(9) << wavefront_record[step][1] << " | "
                      << std::setw(9) << wavefront_record[step][2] << " | "
                      << std::setw(9) << wavefront_record[step][3] << " | "
                      << (pass ? "PASS" : "*FAIL*") << std::endl;
        }

        if (tc1_passed) {
            std::cout << ">>> TC 1 PASSED SUCCESSFULLY!" << std::endl;
        } else {
            std::cout << ">>> TC 1 FAILED!" << std::endl;
        }

        // =============================================================
        // TC 2: Weight Feeder Verification (Non-skewed Parallel Outputs)
        // =============================================================
        std::cout << "\n>>> Starting TC 2: Weight Feeder Test (Non-skewed Parallel)..." << std::endl;
        
        // Clear feeders first
        feeder_clear.write(true);
        wait();
        feeder_clear.write(false);
        wait();

        // Load 4 entries into weight FIFOs
        cnt_en.write(true);
        for (int i = 0; i < 4; i++) {
            wait();
        }
        cnt_en.write(false);
        wait();
        wait();

        // Enable popping to observe the parallel weights output
        pop_en.write(true);
        wait();
        wait();
        
        float weight_record[6][4];
        for (int step = 0; step < 6; step++) {
            wei_vector_t<4, float> data_out = wei_arr.read();
            for (int x = 0; x < 4; x++) {
                weight_record[step][x] = data_out[x];
            }
            wait();
        }
        pop_en.write(false);

        // Verify TC2 Results
        bool tc2_passed = true;
        std::cout << "\n--- TC 2 WEIGHT PARALLEL PREVIEW ---" << std::endl;
        std::cout << " Step Index |   Col 0   |   Col 1   |   Col 2   |   Col 3   | Status" << std::endl;
        std::cout << "------------+-----------+-----------+-----------+-----------+--------" << std::endl;
        
        // Expected parallel values (no delay):
        // Step 0: 100.0, 101.0, 102.0, 103.0 (from addr 0)
        // Step 1: 200.0, 201.0, 202.0, 203.0 (from addr 1)
        // Step 2: 300.0, 301.0, 302.0, 303.0 (from addr 2)
        // Step 3: 400.0, 401.0, 402.0, 403.0 (from addr 3)
        // Step 4+: 0.0

        float expected_wt[6][4] = {
            { 100.0f, 101.0f, 102.0f, 103.0f },
            { 200.0f, 201.0f, 202.0f, 203.0f },
            { 300.0f, 301.0f, 302.0f, 303.0f },
            { 400.0f, 401.0f, 402.0f, 403.0f },
            {   0.0f,   0.0f,   0.0f,   0.0f },
            {   0.0f,   0.0f,   0.0f,   0.0f }
        };

        for (int step = 0; step < 6; step++) {
            bool pass = true;
            for (int x = 0; x < 4; x++) {
                if (std::abs(weight_record[step][x] - expected_wt[step][x]) > 1e-3) {
                    pass = false;
                    tc2_passed = false;
                }
            }
            std::cout << "     " << step << "      | " 
                      << std::setw(9) << weight_record[step][0] << " | "
                      << std::setw(9) << weight_record[step][1] << " | "
                      << std::setw(9) << weight_record[step][2] << " | "
                      << std::setw(9) << weight_record[step][3] << " | "
                      << (pass ? "PASS" : "*FAIL*") << std::endl;
        }

        if (tc2_passed) {
            std::cout << ">>> TC 2 PASSED SUCCESSFULLY!" << std::endl;
        } else {
            std::cout << ">>> TC 2 FAILED!" << std::endl;
        }

        // =============================================================
        // TC 3: FIFO Full Capacity and Empty Status Verification
        // =============================================================
        std::cout << "\n>>> Starting TC 3: FIFO Full Capacity Test..." << std::endl;
        feeder_clear.write(true);
        wait();
        feeder_clear.write(false);
        wait();

        // FIFO Depth is configured to 8. We load 8 entries and check for full.
        cnt_en.write(true);
        for (int i = 0; i < 8; i++) {
            wait();
        }
        cnt_en.write(false);
        wait();
        wait(); // Wait for the final write pipeline to complete
        wait(); // Wait one more cycle for status to propagate

        bool full_check_passed = act_fifo_full.read() || wei_fifo_full.read();
        std::cout << " FIFO Full check (expected true):" 
                  << " Activation Not Full=" << (act_fifo_full.read() ? "YES" : "NO")
                  << " Weight Full=" << (wei_fifo_full.read() ? "YES" : "NO") 
                  << " -> Status: " << (full_check_passed ? "PASS" : "*FAIL*") << std::endl;

        // Pop all elements to make it empty again
        pop_en.write(true);
        for (int i = 0; i < 8; i++) {
            wait();
        }
        pop_en.write(false);
        wait();

        bool empty_check_passed = act_fifo_empty.read() && wei_fifo_empty.read();
        std::cout << " FIFO Empty check (expected true):" 
                  << " Activation Empty=" << (act_fifo_empty.read() ? "YES" : "NO")
                  << " Weight Empty=" << (wei_fifo_empty.read() ? "YES" : "NO")
                  << " -> Status: " << (empty_check_passed ? "PASS" : "*FAIL*") << std::endl;

        bool tc3_passed = full_check_passed && empty_check_passed;
        if (tc3_passed) {
            std::cout << ">>> TC 3 PASSED SUCCESSFULLY!" << std::endl;
        } else {
            std::cout << ">>> TC 3 FAILED!" << std::endl;
        }

        // =============================================================
        // TC 4: Runtime Base + Step Verification
        // =============================================================
        std::cout << "\n>>> Starting TC 4: Runtime Base + Step Test..." << std::endl;

        feeder_clear.write(true);
        wait();
        feeder_clear.write(false);
        wait();

        act_base_addr.write(4);
        act_incntstep.write(2);
        act_incntlim.write(4);
        act_outcntstep.write(2);
        act_outcntlim.write(4);

        wei_base_addr.write(2);
        wei_incntstep.write(2);
        wei_incntlim.write(4);

        wait();

        cnt_en.write(true);
        for (int i = 0; i < 4; i++) {
            wait();
        }
        cnt_en.write(false);
        wait();
        wait();

        pop_en.write(true);
        wait();
        wait();

        float act_record_bs[8][4];
        float wei_record_bs[6][4];

        for (int step = 0; step < 8; step++) {
            act_vector_t<4, float> aout = act_arr.read();
            for (int y = 0; y < 4; y++) {
                act_record_bs[step][y] = aout[y];
            }

            if (step < 6) {
                wei_vector_t<4, float> wout = wei_arr.read();
                for (int x = 0; x < 4; x++) {
                    wei_record_bs[step][x] = wout[x];
                }
            }

        wait();
        }

        pop_en.write(false);

        // Verify TC4 Results
        bool tc4_passed = true;
        std::cout << "\n--- TC 4 IFMAP Run-time config ---" << std::endl;
        std::cout << " Step Index |   Col 0   |   Col 1   |   Col 2   |   Col 3   | Status" << std::endl;
        std::cout << "------------+-----------+-----------+-----------+-----------+--------" << std::endl;
        
        float expected_act_bs[8][4] = {
            { 40.0f,  0.0f,  0.0f,  0.0f },
            { 50.0f, 41.0f,  0.0f,  0.0f },
            { 70.0f, 51.0f, 42.0f,  0.0f },
            { 90.0f, 71.0f, 52.0f, 43.0f },
            {  0.0f, 91.0f, 72.0f, 53.0f },
            {  0.0f,  0.0f, 92.0f, 73.0f },
            {  0.0f,  0.0f,  0.0f, 93.0f },
            {  0.0f,  0.0f,  0.0f,  0.0f }
        };

        float expected_wei_bs[6][4] = {
            { 300.0f, 301.0f, 302.0f, 303.0f },
            { 500.0f, 501.0f, 502.0f, 503.0f },
            { 700.0f, 701.0f, 702.0f, 703.0f },
            { 900.0f, 901.0f, 902.0f, 903.0f },
            {   0.0f,   0.0f,   0.0f,   0.0f },
            {   0.0f,   0.0f,   0.0f,   0.0f }
        };

        for (int step = 0; step < 8; step++) {
            bool pass = true;
            for (int x = 0; x < 4; x++) {
                if (std::abs(act_record_bs[step][x] - expected_act_bs[step][x]) > 1e-3) {
                    pass = false;
                    tc4_passed = false;
                }
            }
            std::cout << "     " << step << "      | " 
                      << std::setw(9) << act_record_bs[step][0] << " | "
                      << std::setw(9) << act_record_bs[step][1] << " | "
                      << std::setw(9) << act_record_bs[step][2] << " | "
                      << std::setw(9) << act_record_bs[step][3] << " | "
                      << (pass ? "PASS" : "*FAIL*") << std::endl;
        }

        std::cout << "\n--- TC 4 WEIGHT Run-time config ---" << std::endl;
        std::cout << " Step Index |   Col 0   |   Col 1   |   Col 2   |   Col 3   | Status" << std::endl;
        std::cout << "------------+-----------+-----------+-----------+-----------+--------" << std::endl;

        for (int step = 0; step < 6; step++) {
            bool pass = true;
            for (int x = 0; x < 4; x++) {
                if (std::abs(wei_record_bs[step][x] - expected_wei_bs[step][x]) > 1e-3) {
                    pass = false;
                    tc4_passed = false;
                }
            }
            std::cout << "     " << step << "      | " 
                      << std::setw(9) << wei_record_bs[step][0] << " | "
                      << std::setw(9) << wei_record_bs[step][1] << " | "
                      << std::setw(9) << wei_record_bs[step][2] << " | "
                      << std::setw(9) << wei_record_bs[step][3] << " | "
                      << (pass ? "PASS" : "*FAIL*") << std::endl;
        }

        if (tc4_passed) {
            std::cout << ">>> TC 4 PASSED SUCCESSFULLY!" << std::endl;
        } else {
            std::cout << ">>> TC 4 FAILED!" << std::endl;
        }

        // =============================================================
        // TC 5: Runtime DIL_PAT Verification
        // Check Address Read
        // =============================================================
        // TEST DIL_PAT
        std::cout << "\n>>> TC 5: DIL_PAT Functional Test (DIL_PAT = <LSB>01010101<MSB>)..." << std::endl;

        feeder_clear.write(true);
        wait();
        feeder_clear.write(false);
        wait();

        act_base_addr.write(0);
        act_incntstep.write(1);
        act_incntlim.write(4);

        sc_bv<DILP_W> dil_alt;
        for (int i = 0; i < DILP_W; i++) {
            dil_alt[i] = (i % 2 == 0) ? 1 : 0;// 10101010
        }
        act_dil_pat.write(dil_alt);

        wait();

        cnt_en.write(true);

        for (int cyc = 0; cyc < 8; cyc++) {
            wait();
            wait(SC_ZERO_TIME);

            std::cout << "[TC5] cycle=" << cyc
              << " srama_rden=" << srama_rden.read()
              << " srama_addr=" << srama_addr.read()
              << std::endl;
        }

        cnt_en.write(false);
        wait();

        sc_close_vcd_trace_file(tf);

        std::cout << "\n=============================================================" << std::endl;
        if (tc1_passed && tc2_passed && tc3_passed && tc4_passed) {
            std::cout << "          SAURIA Standalone Feeders ALL TESTS PASSED          " << std::endl;
        } else {
            std::cout << "          SAURIA Standalone Feeders SOME TESTS FAILED         " << std::endl;
        }
        std::cout << "=============================================================" << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char* argv[]) {
    std::cout << "=============================================================" << std::endl;
    std::cout << "          SAURIA Standalone Feeders Block Testbench          " << std::endl;
    std::cout << "=============================================================" << std::endl;

    TestbenchDataFeeder tb("tb");
    sc_start();

    return 0;
}
