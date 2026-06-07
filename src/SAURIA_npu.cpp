#include "SAURIA_npu.h"

using namespace std;
using namespace tlm;

SAURIA_NPU::SAURIA_NPU(sc_module_name name): sc_module(name), socket("socket"), interrupt_port_out("interrupt_port_out"){
	// Register function receive data from Bus	
	socket.register_b_transport(this, &SAURIA_NPU::b_transport);
	
	sram_a = new NPU_SRAM("IFMAP_SRAM");
	sram_b = new NPU_SRAM("WEIGHT_SRAM");
	sram_c = new NPU_SRAM("PSUMS_SRAM");
	systolic_array = new SYSTOLIC_ARRAY();
	
	// init register
    	reg_npu_ctrl = 0;
    	reg_npu_stat = 0; // 0: READY/IDLE
    	
    	reg_cfg_act = 0; 
	reg_cfg_wei = 0; 
	reg_cfg_out = 0; 
    	
    	// Register data flow of system
    	SC_THREAD(npu_controller_thread);
}

SAURIA_NPU::~SAURIA_NPU(){
	delete sram_a;
    	delete sram_b;
    	delete sram_c;
    	delete systolic_array;
}

void SAURIA_NPU::b_transport(tlm_generic_payload& trans, sc_time& delay){
	tlm_command cmd = trans.get_command();
	uint64_t addr = trans.get_address();
    	unsigned char* ptr = trans.get_data_ptr();
    	
	// compute relatively address in NPU
    	uint64_t local_addr = addr - SAURIA_BASE;
    	
    	if (cmd == TLM_WRITE_COMMAND) {
        	uint32_t write_data = *(reinterpret_cast<uint32_t*>(ptr));

        	// Write to CONTROL register
        	if (local_addr == OFFSET_CFG_CON) {
            		reg_npu_ctrl = write_data;
            		// Bit 0 = 1, NPU compute
            		if (reg_npu_ctrl & 0x1) {
                		ev_start_compute.notify(delay); 
            		}
        	}else if(local_addr == OFFSET_CFG_ACT){
        		reg_cfg_act = write_data;
        	}else if(local_addr == OFFSET_CFG_WEI){
        		reg_cfg_wei = write_data;
        	}
        	else if(local_addr == OFFSET_CFG_OUT){
        		reg_cfg_out = write_data;
        	}
        	else if(local_addr == OFFSET_CFG_CYCLE){
        		reg_cycle_count = write_data;
        	}
        	// Write to IFMAP SRAM
        	else if (local_addr >= OFFSET_SRAMA && local_addr < OFFSET_SRAMA + SRAM_SIZE) {
            		uint32_t word_offset = (local_addr - OFFSET_SRAMA) / DATA_WIDTH_BYTES;
            		sram_a->write(word_offset, write_data);
        	}
        	// Write to WEIGHT SRAM
        	else if (local_addr >= OFFSET_SRAMB && local_addr < OFFSET_SRAMB + SRAM_SIZE) {
            		uint32_t word_offset = (local_addr - OFFSET_SRAMB) / DATA_WIDTH_BYTES;
            		sram_b->write(word_offset, write_data);
        	}
        	// Write to PSUMS SRAM
        	else if (local_addr >= OFFSET_SRAMC && local_addr < OFFSET_SRAMC + SRAM_SIZE) {
            		uint32_t word_offset = (local_addr - OFFSET_SRAMC) / DATA_WIDTH_BYTES;
            		sram_c->write(word_offset, write_data);
        	}
        	trans.set_response_status(tlm::TLM_OK_RESPONSE);
    	}	 
    	else if (cmd == TLM_READ_COMMAND) {
        	uint32_t read_data = 0;

        	// Read CONTROL register
        	if (local_addr == OFFSET_CFG_CON) {
            		read_data = reg_npu_ctrl;
        	}
        	// Read STATUS register
        	else if (local_addr == OFFSET_CFG_STAT) {
            		read_data = reg_npu_stat;
        	}
        	// Read from IFMap SRAM
        	else if (local_addr >= OFFSET_SRAMA && local_addr < OFFSET_SRAMA + SRAM_SIZE) {
            		uint32_t word_offset = (local_addr - OFFSET_SRAMA) / DATA_WIDTH_BYTES;
            		read_data = sram_a->read(word_offset);
        	}
        	// Read from WEIGHT SRAM
        	else if (local_addr >= OFFSET_SRAMB && local_addr < OFFSET_SRAMB + SRAM_SIZE) {
            		uint32_t word_offset = (local_addr - OFFSET_SRAMB) / DATA_WIDTH_BYTES;
            		read_data = sram_b->read(word_offset);
        	}
        	// Read from PSUMS SRAM
        	else if (local_addr >= OFFSET_SRAMC && local_addr < OFFSET_SRAMC + SRAM_SIZE) {
            		uint32_t word_offset = (local_addr - OFFSET_SRAMC) / DATA_WIDTH_BYTES;
            		read_data = sram_c->read(word_offset);
        	}

        	*(reinterpret_cast<uint32_t*>(ptr)) = read_data;
        	trans.set_response_status(TLM_OK_RESPONSE);
    	}
}

void SAURIA_NPU::npu_controller_thread() {
	interrupt_port_out.write(false); // init INTERRUPT - LOW

    	while (true) {
        	wait(ev_start_compute); // Wait start instruction of CPU

        	reg_npu_stat = 1; // BUSY
        	cout << "[Sauria NPU Hardware]: START. Configure data flow..." << endl;
        
        	uint32_t fill_drain_cycles = (NPU_ROWS + NPU_COLS - 1);
        	uint32_t steady_state_cycles = 16;
        	uint32_t total_cycles_run = fill_drain_cycles + steady_state_cycles - 1;
        
        	reg_cycle_count += total_cycles_run;
        
        	cout << "[Sauria NPU Hardware]: It take " << dec << total_cycles_run << " cycles for this group data" << endl;
        	wait(total_cycles_run * CLOCK_PERIOD_NS, SC_NS);

        	// Execute systolic array
        	systolic_array->execute_mac_wave(sram_a, reg_cfg_act, sram_b, reg_cfg_wei, sram_c, reg_cfg_out, false);

        	reg_npu_stat = 2; // DONE 
        	cout << "[Sauria NPU Hardware]: Calculated complete. Emitting INTERRUPT signal..." << endl;

        	// INTERRUPT - High notification for CPU
        	interrupt_port_out.write(true);
        	wait(CLOCK_PERIOD_NS, SC_NS); // keep 1 clock
        	interrupt_port_out.write(false); // INTERRUPT - LOW

        	// Reset control register
        	reg_npu_ctrl = 0; 
    	}
}
