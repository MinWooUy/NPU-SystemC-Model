// SystemC Model for SAURIA NPU Core
// Weight Feeder Block with Parameterized FIFO Depth

#ifndef SAURIA_WEI_FEEDER_H
#define SAURIA_WEI_FEEDER_H

#include "sauria_types.h"
#include <queue>

namespace sauria
{

    template <
        int X_DIM = 32,
        typename T_WEI = float,
        int SRAMB_CAP = 1024,
        int FIFO_DEPTH = 16>
    class WeightFeeder : public sc_module
    {
    public:
        // Clock & Reset
        sc_in<bool> i_clk{"i_clk"};
        sc_in<bool> i_rstn{"i_rstn"};

        // Control Inputs from Global FSM
        sc_in<bool> i_feeder_en{"i_feeder_en"};
        sc_in<bool> i_feeder_clear{"i_feeder_clear"};
        sc_in<bool> i_start{"i_start"};
        sc_in<bool> i_valid{"i_valid"};
        sc_in<bool> i_finalpush{"i_finalpush"};
        sc_in<bool> i_cnt_en{"i_cnt_en"};
        sc_in<bool> i_cnt_clear{"i_cnt_clear"};
        sc_in<bool> i_clearfifo{"i_clearfifo"};
        sc_in<bool> i_pop_en{"i_pop_en"};
        sc_in<bool> i_cswitch{"i_cswitch"};

        // Config Parameters
        sc_in<uint32_t> i_wei_incntlim{"i_wei_incntlim"};
        sc_in<uint32_t> i_wei_incntstep{"i_wei_incntstep"};
        sc_in<uint32_t> i_wei_base_addr{"i_wei_base_addr"};
        // Full SAURIA WEIGHT address-generator runtime config
        sc_in<uint32_t> i_wei_wlim{"i_wei_wlim"};
        sc_in<uint32_t> i_wei_wstep{"i_wei_wstep"};
        sc_in<uint32_t> i_wei_klim{"i_wei_klim"};
        sc_in<uint32_t> i_wei_kstep{"i_wei_kstep"};
        sc_in<uint32_t> i_wei_til_klim{"i_wei_til_klim"};
        sc_in<uint32_t> i_wei_til_kstep{"i_wei_til_kstep"};
        sc_in<uint32_t> i_wei_cols_active{"i_wei_cols_active"};
        sc_in<uint32_t> i_wei_waligned{"i_wei_waligned"};

        // Memory Interface to SRAM B
        sc_out<uint32_t> o_sramb_addr{"o_sramb_addr"};
        sc_out<bool> o_sramb_rden{"o_sramb_rden"};
        sc_in<wei_vector_t<X_DIM, T_WEI>> i_sramb_data{"i_sramb_data"};

        // Wavefront Output Vector towards Systolic Array (B ports)
        sc_out<wei_vector_t<X_DIM, T_WEI>> o_wei_arr{"o_wei_arr"};

        // Feeder Status Outputs
        sc_out<bool> o_wei_done{"o_wei_done"};
        sc_out<bool> o_wei_til_done{"o_wei_til_done"};
        sc_out<bool> o_fifo_empty{"o_fifo_empty"};
        sc_out<bool> o_fifo_full{"o_fifo_full"};
        sc_out<bool> o_stall{"o_stall"};

        SC_CTOR(WeightFeeder)
        {
            SC_METHOD(feeder_process);
            sensitive << i_clk.pos();
        }

    private:
        // Weight queues for each of the X columns
        std::queue<T_WEI> col_fifos[X_DIM];

        // Address tracking register
        uint32_t addr_reg{0};
        uint32_t incnt{0};
        // SAURIA-style WEIGHT address generator counters
        uint32_t wei_w_cnt{0};
        uint32_t wei_k_cnt{0};
        uint32_t wei_til_k_cnt{0};

        // Detect rising edge of cnt_clear
        bool cnt_clear_q{false};

        // SRAM read latency matching registers
        bool rden_q{false};
        bool rdata_valid{false};

        bool use_sauria_weight_addr_gen()
        {
            return (
                i_wei_wlim.read() != 0 &&
                i_wei_wstep.read() != 0);
        }

        void advance_sauria_weight_addr_gen()
        {
            uint32_t next_w = wei_w_cnt + i_wei_wstep.read();

            if (next_w < i_wei_wlim.read())
            {
                wei_w_cnt = next_w;
                return;
            }

            wei_w_cnt = 0;

            uint32_t next_k = wei_k_cnt + i_wei_kstep.read();

            if (i_wei_klim.read() != 0 && next_k < i_wei_klim.read())
            {
                wei_k_cnt = next_k;
                return;
            }

            wei_k_cnt = 0;

            uint32_t next_til_k = wei_til_k_cnt + i_wei_til_kstep.read();

            if (i_wei_til_klim.read() != 0 && next_til_k < i_wei_til_klim.read())
            {
                wei_til_k_cnt = next_til_k;
            }
            else
            {
                wei_til_k_cnt = 0;
            }
        }

        void feeder_process()
        {
            if (!i_rstn.read() || i_feeder_clear.read() || i_clearfifo.read())
            {
                o_sramb_addr.write(0);
                o_sramb_rden.write(false);
                o_wei_arr.write(wei_vector_t<X_DIM, T_WEI>(static_cast<T_WEI>(0)));
                o_wei_done.write(false);
                o_wei_til_done.write(false);
                o_fifo_empty.write(true);
                o_fifo_full.write(false);
                o_stall.write(false);
                addr_reg = 0;
                incnt = 0;
                wei_w_cnt = 0;
                wei_k_cnt = 0;
                wei_til_k_cnt = 0;

                cnt_clear_q = false;
                rden_q = false;
                rdata_valid = false;
                for (int i = 0; i < X_DIM; i++)
                {
                    while (!col_fifos[i].empty())
                        col_fifos[i].pop();
                }
                return;
            }

            if (!i_feeder_en.read())
            {
                o_sramb_rden.write(false);
                o_wei_arr.write(wei_vector_t<X_DIM, T_WEI>(static_cast<T_WEI>(0)));
                rden_q = false;
                rdata_valid = false;
                return;
            }

            // Default SRAM read disable each cycle
            o_sramb_rden.write(false);

            // cnt_clear should behave like a pulse.
            // If controller holds it high for many cycles, feeder only clears once.
            bool cnt_clear_now = i_cnt_clear.read();
            bool cnt_clear_pulse = cnt_clear_now && !cnt_clear_q;
            cnt_clear_q = cnt_clear_now;

            if (cnt_clear_pulse)
            {
                addr_reg = 0;
                incnt = 0;

                wei_w_cnt = 0;
                wei_k_cnt = 0;
                wei_til_k_cnt = 0;

                rden_q = false;
                rdata_valid = false;

                static int dbg_weight_clear_count = 0;
                if (dbg_weight_clear_count < 16)
                {
                    std::cout << "[WEIGHT CLEAR PULSE]"
                              << " feeder_en=" << i_feeder_en.read()
                              << " cnt_en=" << i_cnt_en.read()
                              << " counters reset to 0"
                              << std::endl;
                }
                dbg_weight_clear_count++;
            }

            // SRAM read latency matching.
            // Data is valid one cycle after rden.
            bool mem_data_valid = rden_q;
            rden_q = false;

            if (mem_data_valid)
            {
                wei_vector_t<X_DIM, T_WEI> mem_data = i_sramb_data.read();

                for (int x = 0; x < X_DIM; x++)
                {
                    col_fifos[x].push(mem_data[x]);
                }
            }

            if (i_cnt_en.read())
            {
                bool within_limit = (incnt < i_wei_incntlim.read());

                if (within_limit)
                {
                    bool sauria_weight_mode = use_sauria_weight_addr_gen();

                    uint32_t final_addr = 0;

                    if (sauria_weight_mode)
                    {
                        final_addr =
                            i_wei_base_addr.read() + wei_til_k_cnt + wei_k_cnt + wei_w_cnt;
                    }
                    else
                    {
                        final_addr =
                            i_wei_base_addr.read() + addr_reg;
                    }

                    o_sramb_rden.write(true);
                    o_sramb_addr.write(final_addr);

                    static int dbg_weight_read_count = 0;

                    if (dbg_weight_read_count < 64)
                    {
                        std::cout << "[DEBUG WEIGHT] read_count=" << dbg_weight_read_count
                                  << " mode=" << (sauria_weight_mode ? "SAURIA" : "LINEAR")
                                  << " feeder_en=" << i_feeder_en.read()
                                  << " cnt_en=" << i_cnt_en.read()
                                  << " cnt_clear=" << i_cnt_clear.read()
                                  << " base=" << i_wei_base_addr.read()
                                  << " addr_reg=" << addr_reg
                                  << " w_cnt=" << wei_w_cnt
                                  << " k_cnt=" << wei_k_cnt
                                  << " til_k_cnt=" << wei_til_k_cnt
                                  << " final_addr=" << final_addr
                                  << " incnt=" << incnt
                                  << " incntlim=" << i_wei_incntlim.read()
                                  << " incntstep=" << i_wei_incntstep.read()
                                  << " | wlim=" << i_wei_wlim.read()
                                  << " wstep=" << i_wei_wstep.read()
                                  << " klim=" << i_wei_klim.read()
                                  << " kstep=" << i_wei_kstep.read()
                                  << " til_klim=" << i_wei_til_klim.read()
                                  << " til_kstep=" << i_wei_til_kstep.read()
                                  << " cols_active=0x" << std::hex << i_wei_cols_active.read()
                                  << std::dec
                                  << " waligned=" << i_wei_waligned.read()
                                  << std::endl;
                    }

                    dbg_weight_read_count++;

                    if (sauria_weight_mode)
                    {
                        advance_sauria_weight_addr_gen();
                    }
                    else
                    {
                        addr_reg += i_wei_incntstep.read();
                    }

                    incnt++;
                    rden_q = true;
                }
                else
                {
                    o_sramb_rden.write(false);
                    rden_q = false;
                }
            }
            else
            {
                o_sramb_rden.write(false);
                rden_q = false;
            }

            // 2. Pop weights to Systolic Array (No wavefront skew necessary on weight feeder)
            if (i_pop_en.read())
            {
                wei_vector_t<X_DIM, T_WEI> wei_out;
                for (int x = 0; x < X_DIM; x++)
                {
                    T_WEI popped = static_cast<T_WEI>(0);
                    if (!col_fifos[x].empty())
                    {
                        popped = col_fifos[x].front();
                        col_fifos[x].pop();
                    }
                    wei_out[x] = popped;
                }
                o_wei_arr.write(wei_out);
            }
            else
            {
                o_wei_arr.write(wei_vector_t<X_DIM, T_WEI>(static_cast<T_WEI>(0)));
            }

            // 3. Update status flags
            bool empty = col_fifos[0].empty();
            bool full = col_fifos[0].size() >= FIFO_DEPTH;
            o_fifo_empty.write(empty);
            o_fifo_full.write(full);
        }
    };

} // namespace sauria

#endif // SAURIA_WEI_FEEDER_H
