#include "SAURIA_dma.h"

SAURIA_DMA::SAURIA_DMA(sc_module_name name/*, NPU_SRAM* sram_a, NPU_SRAM* sram_b*/): sc_module(name), socket("socket"), dma_master_socket("dma_master_socket")/*target_sram_a(sram_a), target_sram_b(sram_b)*/{
	socket.register_b_transport(this, &SAURIA_DMA::b_transport);
	SC_THREAD(dma_worker_thread);
	reg_src = 0; reg_dst = 0; reg_size = 0; reg_cmd = 0;
}

void SAURIA_DMA::b_transport(tlm_generic_payload& trans, sc_time& delay){
	tlm_command cmd = trans.get_command();
	uint64_t addr = trans.get_address();
	uint32_t* data_ptr = reinterpret_cast<uint32_t*>(trans.get_data_ptr());
	
	// Calculate local OFFSET
	uint64_t local_addr = addr - DMA_BASE;

	if(cmd == TLM_WRITE_COMMAND){
		if(local_addr == OFFSET_DMA_SRC) reg_src = *data_ptr;
		else if(local_addr == OFFSET_DMA_DST) reg_dst = *data_ptr;
		else if(local_addr == OFFSET_DMA_SIZE) reg_size = *data_ptr;
		else if(local_addr == OFFSET_DMA_CMD) {
			reg_cmd = *data_ptr;
			if(reg_cmd == 1) ev_start_dma.notify(delay); 
		}
	}
	trans.set_response_status(TLM_OK_RESPONSE);
}

void SAURIA_DMA::dma_worker_thread(){
	interrupt_port_out.write(false);
	while(true){
		wait(ev_start_dma);
		cout << "[DMA]: Fetching data to buffer..." << endl;
		
		string filename = (reg_dst == OFFSET_SRAMA) ? "image_data.bin" : "weight_data.bin";
		ifstream file(filename, ios::binary);
		if(file.is_open()){
			char* buffer = new char[reg_size];
			file.read(buffer, reg_size);
			file.close();
			
			uint32_t transfer_cycles = reg_size/4;
			wait(transfer_cycles * CLOCK_PERIOD_NS, SC_NS);
			
			for(uint32_t i = 0; i < reg_size; i+=4){
				tlm_generic_payload trans;
				sc_time delay = sc_time(0, SC_NS);
				uint32_t word_data = *reinterpret_cast<uint32_t*>(&buffer[i]);
				uint64_t target_global_addr = SAURIA_BASE + reg_dst + i;
				
				trans.set_command(TLM_WRITE_COMMAND);
				trans.set_address(target_global_addr);
				trans.set_data_ptr(reinterpret_cast<unsigned char*>(&word_data));
				trans.set_data_length(4);
				trans.set_streaming_width(4);
				trans.set_byte_enable_ptr(0);
				trans.set_dmi_allowed(false);
				trans.set_response_status(TLM_INCOMPLETE_RESPONSE);
				
				dma_master_socket->b_transport(trans, delay);
			}
			
			delete[] buffer;
			cout << "[DMA]: Received " << dec << reg_size << " Bytes from DDR" << endl;
			cout << "[" << sc_time_stamp() << "][DMA]: Transmited " << transfer_cycles << " cycles. Emitting INTERRUPT signal!" << endl;
		}else{
			cout << "[DMA]: Error! File not found." << endl;
		}
		
		interrupt_port_out.write(true);
		wait(CLOCK_PERIOD_NS, SC_NS);
		interrupt_port_out.write(false);
		reg_cmd = 0;
	}
}
