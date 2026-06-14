// SystemC Model for SAURIA NPU Core
// Controller FSM Block (Context FSM, Feeders FSM, Main Controller)

#ifndef SAURIA_MAIN_CONTROLLER_H
#define SAURIA_MAIN_CONTROLLER_H

#include "sauria_types.h"

namespace sauria {

    template <
        int X_DIM = 32,
        int Y_DIM = 32,
        int PE_LAT = X_DIM + Y_DIM,
        int EXTRA_CSREG = 1
    >
    class Control : public sc_module {
    public:
        // Clock & Reset
        sc_in<bool> i_clk{"i_clk"};
        sc_in<bool> i_rstn{"i_rstn"};
        sc_in<bool> i_soft_reset{"i_soft_reset"};

        // Host Control Inputs
        sc_in<bool> i_start{"i_start"};             // Starts NPU execution

        // Output Buffer feedback
        sc_in<bool> i_outbuf_done{"i_outbuf_done"};
        sc_in<bool> i_finalwrite{"i_finalwrite"};
        sc_in<bool> i_shift_done{"i_shift_done"};

        // Tiling & Loop limits (from Config registers)
        sc_in<uint32_t> i_incntlim{"i_incntlim"};
        sc_in<uint32_t> i_act_reps{"i_act_reps"};
        sc_in<uint32_t> i_wei_reps{"i_wei_reps"};

        // Feeder feedback signals
        sc_in<bool> i_act_done{"i_act_done"};
        sc_in<bool> i_act_til_done{"i_act_til_done"};
        sc_in<bool> i_act_fifo_empty{"i_act_fifo_empty"};
        sc_in<bool> i_act_fifo_full{"i_act_fifo_full"};
        sc_in<bool> i_act_stall{"i_act_stall"};

        sc_in<bool> i_wei_done{"i_wei_done"};
        sc_in<bool> i_wei_til_done{"i_wei_til_done"};
        sc_in<bool> i_wei_fifo_empty{"i_wei_fifo_empty"};
        sc_in<bool> i_wei_fifo_full{"i_wei_fifo_full"};
        sc_in<bool> i_wei_stall{"i_wei_stall"};

        // Feeder Control Outputs
        sc_out<bool> o_act_feeder_en{"o_act_feeder_en"};
        sc_out<bool> o_act_feeder_clear{"o_act_feeder_clear"};
        sc_out<bool> o_act_start{"o_act_start"};
        sc_out<bool> o_act_valid{"o_act_valid"};
        sc_out<bool> o_act_finalpush{"o_act_finalpush"};
        sc_out<bool> o_act_cnt_en{"o_act_cnt_en"};
        sc_out<bool> o_act_cnt_clear{"o_act_cnt_clear"};
        sc_out<bool> o_act_clearfifo{"o_act_clearfifo"};
        sc_out<bool> o_act_pop_en{"o_act_pop_en"};
        sc_out<bool> o_act_finalctx{"o_act_finalctx"};

        sc_out<bool> o_wei_feeder_en{"o_wei_feeder_en"};
        sc_out<bool> o_wei_feeder_clear{"o_wei_feeder_clear"};
        sc_out<bool> o_wei_start{"o_wei_start"};
        sc_out<bool> o_wei_valid{"o_wei_valid"};
        sc_out<bool> o_wei_finalpush{"o_wei_finalpush"};
        sc_out<bool> o_wei_cnt_en{"o_wei_cnt_en"};
        sc_out<bool> o_wei_cnt_clear{"o_wei_cnt_clear"};
        sc_out<bool> o_wei_clearfifo{"o_wei_clearfifo"};
        sc_out<bool> o_wei_pop_en{"o_wei_pop_en"};
        sc_out<bool> o_wei_cswitch{"o_wei_cswitch"};

        // Output Buffer / PSM Control Outputs
        sc_out<bool> o_outbuf_start{"o_outbuf_start"};
        sc_out<bool> o_outbuf_reset{"o_outbuf_reset"};

        // Systolic Array Control Outputs
        sc_out<bool>        o_sa_clear{"o_sa_clear"};
        sc_out<bool>        o_pipeline_en{"o_pipeline_en"};
        sc_out<sc_bv<X_DIM>> o_cswitch_arr{"o_cswitch_arr"};

        // General Done status out
        sc_out<bool> o_done{"o_done"};
        sc_out<bool> o_feed_deadlock{"o_feed_deadlock"};

        SC_CTOR(Control) {
            SC_METHOD(ctrl_process);
            sensitive << i_clk.pos();
        }

    private:
        // Internal Context FSM states matching context_fsm.sv
        enum ctrl_state_t {
            IDLE,
            START_FLAGS,
            ARRAY_PREP,
            FIRST_SHIFT,
            START_COMP,
            WAIT_CSWITCH,
            WAIT_CSWITCH_STALL,
            WAIT_OBUF,
            WAIT_OBUF_STALL,
            SCND_SHIFT,
            SCND_SHIFT_STALL,
            ALL_BUSY_SHIFT,
            ALL_BUSY,
            ARRAY_BUSY,
            OBUF_BUSY_SHIFT,
            FORCE_STALL,
            OBUF_BUSY,
            ARRAY_CSWITCH,
            ARRAY_CSWITCH_STALL,
            LAST_SHIFT,
            LAST_WAIT,
            DONE
        };

        ctrl_state_t state{IDLE};
        uint32_t cycle_cnt{0};
        uint32_t comp_cycles{0};
        uint32_t shift_cycles{0};

        void ctrl_process() {
            if (!i_rstn.read() || i_soft_reset.read()) {
                state = IDLE;
                cycle_cnt = 0;
                comp_cycles = 0;
                shift_cycles = 0;
                
                o_act_feeder_en.write(false);
                o_act_feeder_clear.write(true);
                o_act_start.write(false);
                o_act_valid.write(false);
                o_act_finalpush.write(false);
                o_act_cnt_en.write(false);
                o_act_cnt_clear.write(true);
                o_act_clearfifo.write(true);
                o_act_pop_en.write(false);
                o_act_finalctx.write(false);

                o_wei_feeder_en.write(false);
                o_wei_feeder_clear.write(true);
                o_wei_start.write(false);
                o_wei_valid.write(false);
                o_wei_finalpush.write(false);
                o_wei_cnt_en.write(false);
                o_wei_cnt_clear.write(true);
                o_wei_clearfifo.write(true);
                o_wei_pop_en.write(false);
                o_wei_cswitch.write(false);

                o_outbuf_start.write(false);
                o_outbuf_reset.write(true);

                o_sa_clear.write(true);
                o_pipeline_en.write(false);
                o_cswitch_arr.write(sc_bv<X_DIM>(0));
                
                o_done.write(false);
                o_feed_deadlock.write(false);
                return;
            }

            // Simple deadlock monitoring logic matching main_controller.sv
            bool act_deadlock = i_act_fifo_empty.read() && i_wei_fifo_full.read();
            bool wei_deadlock = i_act_fifo_full.read() && i_wei_fifo_empty.read();
            o_feed_deadlock.write(act_deadlock || wei_deadlock);

            // Fetch limits from registers
            uint32_t incntlim = i_incntlim.read();

            switch (state) {
                case IDLE:
                    o_done.write(false);
                    o_sa_clear.write(false);
                    o_act_feeder_clear.write(false);
                    o_act_clearfifo.write(false);
                    o_wei_feeder_clear.write(false);
                    o_wei_clearfifo.write(false);
                    o_outbuf_reset.write(false);

                    if (i_start.read()) {
                        state = START_FLAGS;
                        cycle_cnt = 0;
                    }
                    break;

                case START_FLAGS:
                    // Enable feeders and pulse start
                    o_act_feeder_en.write(true);
                    o_wei_feeder_en.write(true);
                    o_act_start.write(true);
                    o_wei_start.write(true);
                    o_act_valid.write(true);
                    o_wei_valid.write(true);
                    o_pipeline_en.write(true);
                    state = ARRAY_PREP;
                    break;

                case ARRAY_PREP:
                    o_act_start.write(false);
                    o_wei_start.write(false);
                    o_act_cnt_en.write(true);
                    o_wei_cnt_en.write(true);
                    o_act_pop_en.write(true);
                    o_wei_pop_en.write(true);
                    comp_cycles = 0;
                    state = START_COMP;
                    break;

                case START_COMP:
                    comp_cycles++;
                    // Run computation for loop tile limit (incntlim)
                    if (comp_cycles >= incntlim) {
                        state = ARRAY_CSWITCH;
                    }
                    break;

                case ARRAY_CSWITCH:
                    // Pulse accumulator Context Switch to grid elements
                    o_cswitch_arr.write(sc_bv<X_DIM>(~0)); // Write 1s to swap context
                    o_act_pop_en.write(false);
                    o_wei_pop_en.write(false);
                    o_act_cnt_en.write(false);
                    o_wei_cnt_en.write(false);
                    cycle_cnt = 0;
                    state = WAIT_CSWITCH;
                    break;

                case WAIT_CSWITCH:
                    o_cswitch_arr.write(sc_bv<X_DIM>(0)); // Deassert context switch
                    cycle_cnt++;
                    if (cycle_cnt >= (uint32_t)(PE_LAT)) {
                        state = WAIT_OBUF;
                    }
                    break;

                case WAIT_OBUF:
                    o_cswitch_arr.write(sc_bv<X_DIM>(0)); // Deassert context switch
                    o_outbuf_start.write(true);       // Start PSM collection
                    shift_cycles = 0;
                    state = LAST_WAIT;
                    break;

                case LAST_WAIT:
                    o_outbuf_start.write(false);
                    shift_cycles++;
                    // Wait for systolic array outputs to completely shift out via scan-chain
                    if (shift_cycles >= (uint32_t)(PE_LAT + 8)) {
                        state = DONE;
                    }
                    break;

                case DONE:
                    o_done.write(true);
                    o_act_feeder_en.write(false);
                    o_wei_feeder_en.write(false);
                    o_pipeline_en.write(false);
                    state = IDLE;
                    break;

                default:
                    state = IDLE;
                    break;
            }
        }
    };

} // namespace sauria

#endif // SAURIA_MAIN_CONTROLLER_H
