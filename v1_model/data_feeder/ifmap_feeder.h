// SystemC Model for SAURIA NPU Core
// Activation (IFmap) Feeder Block with Parameterized FIFO Depth

#ifndef SAURIA_IFMAP_FEEDER_H
#define SAURIA_IFMAP_FEEDER_H

#include "sauria_types.h"
#include <queue>
#include <vector>

namespace sauria
{

    template <
        int Y_DIM = 32,
        typename T_ACT = float,
        int SRAMA_CAP = 1024,
        int FIFO_DEPTH = 16>
    class IfmapFeeder : public sc_module
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
        sc_in<bool> i_finalctx{"i_finalctx"};

        // Config Parameters
        sc_in<uint32_t> i_act_incntlim{"i_act_incntlim"};
        sc_in<uint32_t> i_act_incntstep{"i_act_incntstep"};
        sc_in<uint32_t> i_act_outcntlim{"i_act_outcntlim"};
        sc_in<uint32_t> i_act_outcntstep{"i_act_outcntstep"};
        sc_in<sc_bv<DILP_W>> i_act_dil_pat{"i_act_dil_pat"};
        // Full SAURIA IFMAP address-generator runtime config
        sc_in<uint32_t> i_act_xlim{"i_act_xlim"};
        sc_in<uint32_t> i_act_xstep{"i_act_xstep"};
        sc_in<uint32_t> i_act_ylim{"i_act_ylim"};
        sc_in<uint32_t> i_act_ystep{"i_act_ystep"};
        sc_in<uint32_t> i_act_chlim{"i_act_chlim"};
        sc_in<uint32_t> i_act_chstep{"i_act_chstep"};
        sc_in<uint32_t> i_act_til_xlim{"i_act_til_xlim"};
        sc_in<uint32_t> i_act_til_xstep{"i_act_til_xstep"};
        sc_in<uint32_t> i_act_til_ylim{"i_act_til_ylim"};
        sc_in<uint32_t> i_act_til_ystep{"i_act_til_ystep"};

        // Memory Interface to SRAM A
        sc_out<uint32_t> o_srama_addr{"o_srama_addr"};
        sc_out<bool> o_srama_rden{"o_srama_rden"};
        sc_in<act_vector_t<Y_DIM, T_ACT>> i_srama_data{"i_srama_data"};
        sc_in<uint32_t> i_act_base_addr{"i_act_base_addr"};

        // Wavefront Output Vector towards Systolic Array (A ports)
        sc_out<act_vector_t<Y_DIM, T_ACT>> o_act_arr{"o_act_arr"};

        // Feeder Status Outputs
        sc_out<bool> o_act_done{"o_act_done"};
        sc_out<bool> o_act_til_done{"o_act_til_done"};
        sc_out<bool> o_fifo_empty{"o_fifo_empty"};
        sc_out<bool> o_fifo_full{"o_fifo_full"};
        sc_out<bool> o_stall{"o_stall"};

        SC_CTOR(IfmapFeeder)
        {
            SC_METHOD(feeder_process);
            sensitive << i_clk.pos();
        }

    private:
        // Internal FIFOs for each of the Y rows
        std::queue<T_ACT> row_fifos[Y_DIM];

        // Dynamic skew delay registers for systolic wavefront (A-side)
        std::vector<T_ACT> skew_regs[Y_DIM];

        // Address tracking register
        uint32_t addr_reg{0};

        // local variable
        uint32_t incnt{0};
        uint32_t dil_idx{0};

        // SAURIA-style IFMAP address generator counters
        uint32_t act_x_cnt{0};
        uint32_t act_y_cnt{0};
        uint32_t act_ch_cnt{0};

        // SRAM read latency matching registers
        bool rden_q{false};
        bool cnt_clear_q{false};

        bool use_sauria_ifmap_addr_gen()
        {
            return (
                i_act_xlim.read() != 0 &&
                i_act_xstep.read() != 0 &&
                i_act_ylim.read() != 0 &&
                i_act_ystep.read() != 0 &&
                i_act_chlim.read() != 0 &&
                i_act_chstep.read() != 0);
        }

        void advance_sauria_ifmap_addr_gen()
        {
            uint32_t next_x = act_x_cnt + i_act_xstep.read();

            if (next_x < i_act_xlim.read())
            {
                act_x_cnt = next_x;
                return;
            }

            act_x_cnt = 0;

            uint32_t next_y = act_y_cnt + i_act_ystep.read();

            if (next_y < i_act_ylim.read())
            {
                act_y_cnt = next_y;
                return;
            }

            act_y_cnt = 0;

            uint32_t next_ch = act_ch_cnt + i_act_chstep.read();

            if (next_ch < i_act_chlim.read())
            {
                act_ch_cnt = next_ch;
            }
            else
            {
                act_ch_cnt = 0;
            }
        }

        void feeder_process()
        {
            if (!i_rstn.read() || i_feeder_clear.read() || i_clearfifo.read())
            {
                o_srama_addr.write(0);
                o_srama_rden.write(false);
                o_act_arr.write(act_vector_t<Y_DIM, T_ACT>(static_cast<T_ACT>(0)));
                o_act_done.write(false);
                o_act_til_done.write(false);
                o_fifo_empty.write(true);
                o_fifo_full.write(false);
                o_stall.write(false);
                addr_reg = 0;
                incnt = 0;
                dil_idx = 0;

                act_x_cnt = 0;
                act_y_cnt = 0;
                act_ch_cnt = 0;

                cnt_clear_q = false;
                rden_q = false;
                for (int i = 0; i < Y_DIM; i++)
                {
                    while (!row_fifos[i].empty())
                        row_fifos[i].pop();
                    skew_regs[i].assign(i, static_cast<T_ACT>(0)); // Delay length matches row index i
                }
                return;
            }

            if (!i_feeder_en.read())
            {
                o_srama_rden.write(false);
                o_act_arr.write(act_vector_t<Y_DIM, T_ACT>(static_cast<T_ACT>(0)));
                return;
            }

            o_srama_rden.write(false);

            bool cnt_clear_now = i_cnt_clear.read();
            bool cnt_clear_pulse = cnt_clear_now && !cnt_clear_q;
            cnt_clear_q = cnt_clear_now;

            if (cnt_clear_pulse)
            {
                addr_reg = 0;
                incnt = 0;
                dil_idx = 0;

                act_x_cnt = 0;
                act_y_cnt = 0;
                act_ch_cnt = 0;

                rden_q = false;
                o_srama_rden.write(false);

                static int dbg_clear_count = 0;
                if (dbg_clear_count < 32)
                {
                    std::cout << "[IFMAP CLEAR PULSE]"
                              << " feeder_en=" << i_feeder_en.read()
                              << " cnt_en=" << i_cnt_en.read()
                              << " addr_reg reset to 0"
                              << std::endl;
                }
                dbg_clear_count++;
            }

            bool mem_data_valid = rden_q;
            rden_q = false;
            // 1. Fetch activation vector from SRAM A into FIFOs
            if (mem_data_valid)
            {
                // Read from memory and push into FIFOs (contains valid data from previous cycle's read)
                act_vector_t<Y_DIM, T_ACT> mem_data = i_srama_data.read();
                for (int y = 0; y < Y_DIM; y++)
                {
                    row_fifos[y].push(mem_data[y]);
                }
            }

            if (i_cnt_en.read())
            {
                bool within_limit = (incnt < i_act_incntlim.read());

                sc_bv<DILP_W> dil = i_act_dil_pat.read();
                bool dil_allow = true;

                bool dil_all_zero = true;
                for (int i = 0; i < DILP_W; i++)
                {
                    if (dil[i] == sc_dt::Log_1)
                    {
                        dil_all_zero = false;
                        break;
                    }
                }

                if (!dil_all_zero)
                    dil_allow = (dil[dil_idx % DILP_W] == sc_dt::Log_1);
                // if(DILP_W >0){
                //     dil_allow = (dil[dil_idx % DILP_W] == sc_dt::Log_1);
                // }

                if (within_limit && dil_allow)
                {
                    bool sauria_addr_mode = use_sauria_ifmap_addr_gen();

                    uint32_t final_addr = 0;

                    if (sauria_addr_mode)
                    {
                        final_addr = i_act_base_addr.read() + act_ch_cnt + act_y_cnt + act_x_cnt;
                    }
                    else
                    {
                        final_addr = i_act_base_addr.read() + addr_reg;
                    }

                    o_srama_rden.write(true);
                    o_srama_addr.write(final_addr);

                    static int dbg_ifmap_read_count = 0;

                    if (dbg_ifmap_read_count < 64)
                    {
                        std::cout << "[DEBUG IFMAP] read_count=" << dbg_ifmap_read_count
                                  << " mode=" << (sauria_addr_mode ? "SAURIA" : "LINEAR")
                                  << " feeder_en=" << i_feeder_en.read()
                                  << " cnt_en=" << i_cnt_en.read()
                                  << " cnt_clear=" << i_cnt_clear.read()
                                  << " base=" << i_act_base_addr.read()
                                  << " addr_reg=" << addr_reg
                                  << " x_cnt=" << act_x_cnt
                                  << " y_cnt=" << act_y_cnt
                                  << " ch_cnt=" << act_ch_cnt
                                  << " final_addr=" << final_addr
                                  << " incnt=" << incnt
                                  << " limit=" << i_act_incntlim.read()
                                  << " xlim=" << i_act_xlim.read()
                                  << " xstep=" << i_act_xstep.read()
                                  << " ylim=" << i_act_ylim.read()
                                  << " ystep=" << i_act_ystep.read()
                                  << " chlim=" << i_act_chlim.read()
                                  << " chstep=" << i_act_chstep.read()
                                  << std::endl;
                    }

                    dbg_ifmap_read_count++;

                    if (sauria_addr_mode)
                    {
                        advance_sauria_ifmap_addr_gen();
                    }
                    else
                    {
                        addr_reg += i_act_incntstep.read();
                    }

                    incnt++;
                    rden_q = true;
                }
                else
                {
                    o_srama_rden.write(false);
                    rden_q = false;

                    // std::cout << "[DEBUG IFMAP SKIP] incnt=" << incnt
                    //           << " dil_idx=" << dil_idx
                    //           << " within_limit=" << within_limit
                    //           << " dil_allow=" << dil_allow
                    //           << std::endl;
                }
                dil_idx++;
            }
            else
            {
                o_srama_rden.write(false);
                rden_q = false;
            }

            // 2. Pop and Shift Activations (Wavefront Skew Lines)
            if (i_pop_en.read())
            {
                act_vector_t<Y_DIM, T_ACT> act_out;
                for (int y = 0; y < Y_DIM; y++)
                {
                    T_ACT popped = static_cast<T_ACT>(0);
                    if (!row_fifos[y].empty())
                    {
                        popped = row_fifos[y].front();
                        row_fifos[y].pop();
                    }

                    // Shift wavefront skew register line
                    skew_regs[y].push_back(popped);

                    // Wavefront output is the front of the delay line (length y)
                    T_ACT out_val = (y == 0) ? popped : skew_regs[y].front();

                    if (y > 0)
                    {
                        skew_regs[y].erase(skew_regs[y].begin());
                    }

                    act_out[y] = out_val;
                }
                o_act_arr.write(act_out);
            }
            else
            {
                o_act_arr.write(act_vector_t<Y_DIM, T_ACT>(static_cast<T_ACT>(0)));
            }

            // 3. Update status flags
            bool empty = row_fifos[0].empty();
            bool full = row_fifos[0].size() >= FIFO_DEPTH;
            o_fifo_empty.write(empty);
            o_fifo_full.write(full);
        }
    };

} // namespace sauria

#endif // SAURIA_IFMAP_FEEDER_H
