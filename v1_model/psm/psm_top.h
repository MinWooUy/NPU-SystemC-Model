// SystemC Model for SAURIA NPU Core
// Partial Sum Memory (PSM) Block with Parameterized Dimensions

#ifndef SAURIA_PSM_TOP_H
#define SAURIA_PSM_TOP_H

#include "sauria_types.h"

namespace sauria
{

    template <
        int X_DIM = 32,
        int Y_DIM = 32,
        typename T_PSUM = float,
        int SRAMC_CAP = 2048>
    class Psm : public sc_module
    {
    public:
        // Clock & Reset
        sc_in<bool> i_clk{"i_clk"};
        sc_in<bool> i_rstn{"i_rstn"};

        // Data Inputs from Array Scan Chain
        sc_in<psum_vector_t<Y_DIM, T_PSUM>> i_c_arr{"i_c_arr"}; // Data entering from leftmost PE column

        // Memory Interface to SRAM C
        sc_in<uint32_t> i_out_base_addr{"i_out_base_addr"};
        sc_in<psum_vector_t<Y_DIM, T_PSUM>> i_sramc_rdata{"i_sramc_rdata"};
        sc_out<uint32_t> o_sramc_addr{"o_sramc_addr"};
        sc_out<bool> o_sramc_wren{"o_sramc_wren"};
        sc_out<bool> o_sramc_rden{"o_sramc_rden"};
        sc_out<sramc_mask_t<Y_DIM>> o_sramc_wmask{"o_sramc_wmask"};
        sc_out<psum_vector_t<Y_DIM, T_PSUM>> o_sramc_wdata{"o_sramc_wdata"};

        // Config Parameters
        sc_in<uint32_t> i_cxlim{"i_cxlim"};
        sc_in<uint32_t> i_cxstep{"i_cxstep"};
        sc_in<uint32_t> i_cklim{"i_cklim"};
        sc_in<uint32_t> i_ckstep{"i_ckstep"};
        sc_in<uint32_t> i_til_cylim{"i_til_cylim"};
        sc_in<uint32_t> i_til_cystep{"i_til_cystep"};
        sc_in<uint32_t> i_til_cklim{"i_til_cklim"};
        sc_in<uint32_t> i_til_ckstep{"i_til_ckstep"};
        sc_in<uint32_t> i_ncontexts{"i_ncontexts"};
        sc_in<bool> i_preload_en{"i_preload_en"};
        sc_in<sramc_mask_t<Y_DIM>> i_rows_active{"i_rows_active"}; // Active Rows config (Y bits)

        // Control Inputs from global FSM
        sc_in<bool> i_fsm_start{"i_fsm_start"};
        sc_in<bool> i_fsm_reset{"i_fsm_reset"};
        sc_in<bool> i_pipeline_en{"i_pipeline_en"};

        // Control Outputs to Global FSM / Array
        sc_out<bool> o_done{"o_done"};
        sc_out<bool> o_finalwrite{"o_finalwrite"};
        sc_out<bool> o_shift_done{"o_shift_done"};
        sc_out<bool> o_cscan_en{"o_cscan_en"}; // Directs array to shift out C chain

        // Data Outputs to Array (Preload values sent right-to-left)
        sc_out<psum_vector_t<Y_DIM, T_PSUM>> o_c_arr{"o_c_arr"};

        SC_CTOR(Psm)
        {
            SC_METHOD(psm_process);
            sensitive << i_clk.pos();
        }

    private:
        uint32_t addr_reg{0};
        uint32_t shift_cnt{0};
        uint32_t delay_cnt{0};
        uint32_t write_limit_vectors{0};
        bool shifting{false};
        bool preload_mode{false};

        uint32_t psm_context_cnt{0};
        uint32_t context_addr_base{0};

        uint32_t get_write_limit_vectors()
        {
            // One PSM start corresponds to one output context scan.
            // For current controller, one compute phase only produces CXLIM vectors.
            // Full tile output requires repeating this for NCONTEXTS contexts.
            if (i_cxlim.read() != 0)
            {
                return i_cxlim.read();
            }

            return 0;
        }

        void psm_process()
        {
            if (!i_rstn.read() || i_fsm_reset.read())
            {
                // o_sramc_addr.write(0);
                o_sramc_addr.write(i_out_base_addr.read() + addr_reg);
                o_sramc_wren.write(false);
                o_sramc_rden.write(false);
                o_sramc_wmask.write(sramc_mask_t<Y_DIM>());
                o_sramc_wdata.write(psum_vector_t<Y_DIM, T_PSUM>());
                o_done.write(false);
                o_finalwrite.write(false);
                o_shift_done.write(false);
                o_cscan_en.write(false);
                o_c_arr.write(psum_vector_t<Y_DIM, T_PSUM>());
                addr_reg = 0;
                shift_cnt = 0;
                delay_cnt = 0;
                write_limit_vectors = 0;

                psm_context_cnt = 0;
                context_addr_base = 0;

                shifting = false;
                preload_mode = false;
                return;
            }

            if (i_fsm_start.read() && !shifting)
            {
                shifting = true;
                shift_cnt = 0;
                delay_cnt = 0;

                preload_mode = false;

                write_limit_vectors = get_write_limit_vectors();

                // One context writes CXLIM vectors.
                // Each vector address increments by CXSTEP.
                // Therefore each context should start at:
                // context_idx * CXLIM * CXSTEP
                uint32_t one_context_addr_span =
                    i_cxlim.read() * i_cxstep.read();

                context_addr_base =
                    psm_context_cnt * one_context_addr_span;

                addr_reg = context_addr_base;

                o_cscan_en.write(true);
                o_done.write(false);
                o_shift_done.write(false);
                o_finalwrite.write(false);

                o_sramc_rden.write(false);
                o_sramc_wren.write(false);

                static int dbg_psm_start_count = 0;
                if (dbg_psm_start_count < 16)
                {
                    std::cout << "[PSM START]"
                              << " context=" << psm_context_cnt
                              << " / " << i_ncontexts.read()
                              << " preload_en_cfg=" << i_preload_en.read()
                              << " forced_write_mode=1"
                              << " cxlim=" << i_cxlim.read()
                              << " cxstep=" << i_cxstep.read()
                              << " context_addr_base=" << context_addr_base
                              << " one_context_vectors=" << write_limit_vectors
                              << " full_tile_vectors=" << ((i_til_cklim.read() + Y_DIM - 1) / Y_DIM)
                              << " til_cklim=" << i_til_cklim.read()
                              << " til_ckstep=" << i_til_ckstep.read()
                              << std::endl;
                }
                dbg_psm_start_count++;

                return;
            }

            if (shifting)
            {
                if (preload_mode)
                {
                    if (delay_cnt == 0)
                    {
                        delay_cnt = 1;
                        addr_reg = i_cxstep.read();
                        // o_sramc_addr.write(addr_reg);
                        //  std::cout << "[DEBUG PSM] addr =" << (i_out_base_addr.read() + addr_reg) << std::endl;
                        o_sramc_addr.write(i_out_base_addr.read() + addr_reg);
                        o_sramc_rden.write(true);
                    }
                    else
                    {
                        psum_vector_t<Y_DIM, T_PSUM> mem_data = i_sramc_rdata.read();
                        o_c_arr.write(mem_data);

                        addr_reg += i_cxstep.read();
                        shift_cnt++;

                        if (shift_cnt < i_cxlim.read())
                        {
                            // o_sramc_addr.write(addr_reg);
                            o_sramc_addr.write(i_out_base_addr.read() + addr_reg);
                            o_sramc_rden.write(true);
                        }
                        else
                        {
                            shifting = false;
                            o_cscan_en.write(false);
                            o_sramc_rden.write(false);
                            o_shift_done.write(true);
                            o_done.write(true);
                        }
                    }
                }
                else
                {
                    if (delay_cnt == 0)
                    {
                        delay_cnt = 1;
                    }
                    else
                    {
                        if (shift_cnt < write_limit_vectors)
                        {
                            psum_vector_t<Y_DIM, T_PSUM> array_out = i_c_arr.read();
                            static int dbg_psm_write_count = 0;

                            if (dbg_psm_write_count < 128)
                            {
                                std::cout << "[PSM DEBUG] write_count=" << dbg_psm_write_count
                                          << " context=" << psm_context_cnt
                                          << " context_addr_base=" << context_addr_base
                                          << " addr=" << (i_out_base_addr.read() + addr_reg)
                                          << " shift_cnt=" << shift_cnt
                                          << " write_limit_vectors=" << write_limit_vectors
                                          << " cxlim=" << i_cxlim.read()
                                          << " cxstep=" << i_cxstep.read()
                                          << " i_c_arr=[";

                                for (int i = 0; i < Y_DIM; i++)
                                {
                                    std::cout << array_out[i];
                                    if (i < Y_DIM - 1)
                                        std::cout << ", ";
                                }

                                std::cout << "]" << std::endl;
                                dbg_psm_write_count++;
                            }

                            o_sramc_wren.write(true);
                            // o_sramc_addr.write(addr_reg);
                            o_sramc_addr.write(i_out_base_addr.read() + addr_reg);
                            o_sramc_wmask.write(i_rows_active.read());
                            o_sramc_wdata.write(array_out);

                            addr_reg += i_cxstep.read();
                            shift_cnt++;
                        }
                        else
                        {
                            shifting = false;

                            o_sramc_wren.write(false);
                            o_sramc_rden.write(false);

                            o_done.write(true);
                            o_shift_done.write(true);
                            o_cscan_en.write(false);
                            o_finalwrite.write(false);

                            uint32_t nctx = i_ncontexts.read();
                            if (nctx == 0)
                            {
                                nctx = 1;
                            }

                            if ((psm_context_cnt + 1) < nctx)
                            {
                                psm_context_cnt++;
                            }
                            else
                            {
                                psm_context_cnt = 0;
                            }

                            static int dbg_psm_done_count = 0;
                            if (dbg_psm_done_count < 16)
                            {
                                std::cout << "[PSM DONE]"
                                          << " completed_context="
                                          << ((psm_context_cnt == 0) ? (nctx - 1) : (psm_context_cnt - 1))
                                          << " next_context=" << psm_context_cnt
                                          << std::endl;
                            }
                            dbg_psm_done_count++;
                        }
                    }
                }
            }
            else
            {
                o_sramc_wren.write(false);
                o_sramc_rden.write(false);
            }
        }
    };

} // namespace sauria

#endif // SAURIA_PSM_TOP_H
