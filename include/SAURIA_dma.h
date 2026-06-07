#ifndef SAURIA_DMA_H
#define SAURIA_DMA_H

#include <systemc.h>
#include "tlm.h"
#include "tlm_utils/simple_target_socket.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "SAURIA_config.h"
#include "SAURIA_sram.h"
#include <fstream>

using namespace sc_core;
using namespace tlm;
using namespace tlm_utils;

class SAURIA_DMA: public sc_module{
public:
	// Target port: received config register from CPU (DMA assigned in SLAVE)
	simple_target_socket<SAURIA_DMA> socket;
	// Initiator port: DMA fetch data to Bus (DMA assigned in MASTER)
	simple_initiator_socket<SAURIA_DMA> dma_master_socket;
	
	// Interrupt for DMA
	sc_out<bool> interrupt_port_out;
	
	// Pointer direct connect SRAM of NPU
	// NPU_SRAM* target_sram_a;
	// NPU_SRAM* target_sram_b;
	
	SC_HAS_PROCESS(SAURIA_DMA);
	SAURIA_DMA(sc_module_name name/*, NPU_SRAM* sram_a, NPU_SRAM* sram_b*/);
private:
	uint32_t reg_src, reg_dst, reg_size, reg_cmd;
	sc_event ev_start_dma;
	
	void b_transport(tlm_generic_payload& trans, sc_time& delay);
	void dma_worker_thread();
};

#endif
