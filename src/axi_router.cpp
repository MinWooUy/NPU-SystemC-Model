#include "axi_router.h"

void AXI_ROUTER::b_transport(tlm_generic_payload& trans, sc_time& delay){
	uint64_t global_addr = trans.get_address();
	
	if(global_addr % 4 != 0){
		cout << "[AXI Error]: Address deviation detected at address 0x" << hex << global_addr << endl;
		cout << "[AXI Error]: Pause system !" << endl;
		trans.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
		return;
	}
	
	if(global_addr >= SAURIA_BASE && global_addr < (SAURIA_BASE + 0x01000000)){
		npu_socket->b_transport(trans, delay);
	}
	else if(global_addr >= DMA_BASE && global_addr < (DMA_BASE + 0x01000000)){
		dma_socket->b_transport(trans, delay);
	}
	else{
		cout << "[AXI BUS SYSTEM]: Memory address at 0x" << hex << global_addr << "does not exist!" << endl;
		// return ERROR to CPU
		trans.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
	}
}
