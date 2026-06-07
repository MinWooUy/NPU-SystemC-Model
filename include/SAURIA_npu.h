#ifndef SAURIA_NPU_H
#define SAURIA_NPU_H

#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_target_socket.h>

#include "SAURIA_config.h"
#include "SAURIA_sram.h"
#include "SAURIA_systolic_array.h"

using namespace tlm;
using namespace tlm_utils;

class SAURIA_NPU: public sc_module{
public:
	// Cổng giao tiếp TLM 2.0 Target Socket (Kết nối vào Bus AXI)
	simple_target_socket<SAURIA_NPU> socket;
	
	// Cổng xuất tín hiệu Ngắt cứng (Interrupt Port)
	sc_out<bool> interrupt_port_out;
	
	NPU_SRAM* sram_a; //IFMAP
	NPU_SRAM* sram_b; //WEIGHT
	NPU_SRAM* sram_c; //PSUMS
	SYSTOLIC_ARRAY* systolic_array;
	
	uint32_t reg_npu_ctrl;
	uint32_t reg_npu_stat;
	
	uint32_t reg_cfg_act;
	uint32_t reg_cfg_wei;
	uint32_t reg_cfg_out;
	
	uint32_t reg_cycle_count;
	
	sc_event ev_start_compute;
	
	SC_HAS_PROCESS(SAURIA_NPU);
	
	SAURIA_NPU(sc_module_name name);
	~SAURIA_NPU();
	
	// Hàm xử lý các giao dịch đọc/ghi từ Bus (TLM 2.0 b_transport)
	virtual void b_transport(tlm_generic_payload& trans, sc_time& delay);
	// Luồng điều khiển phần cứng chính hoạt động song song
	void npu_controller_thread();
};

#endif
