#ifndef AXI_ROUTER_H
#define AXI_ROUTER_H

#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_target_socket.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "SAURIA_config.h"

using namespace tlm_utils;
using namespace tlm;
using namespace sc_core;
using namespace std;

class AXI_ROUTER: public sc_module{
public:	
	// port target received instruction from CPU(Master)
	simple_target_socket<AXI_ROUTER> cpu_socket;
	simple_target_socket<AXI_ROUTER> dma_in_socket;
	
	// ports Initiator forward instruction to slaves
	simple_initiator_socket<AXI_ROUTER> npu_socket;
	simple_initiator_socket<AXI_ROUTER> dma_socket;
	
	SC_CTOR(AXI_ROUTER) : cpu_socket("cpu_socket"), npu_socket("npu_socket"), dma_socket("dma_socket"){
		// Register b_transport to handle data of CPU 
		cpu_socket.register_b_transport(this, &AXI_ROUTER::b_transport);
		dma_in_socket.register_b_transport(this, &AXI_ROUTER::b_transport);
	}
	
private:
	// Decoding and Routing address
	void b_transport(tlm_generic_payload& trans, sc_time& delay);
};

#endif
