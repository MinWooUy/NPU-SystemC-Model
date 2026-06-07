#ifndef SAURIA_PE_H
#define SAURIA_PE_H

#include "SAURIA_config.h"
#include <cstdint>

class PE{
public:
	uint64_t mac_count;
	
	PE();
	npu_data_t computeMAC(npu_data_t ifmap_in, npu_data_t weight_in, npu_data_t psums_in);
	void reset_profiling();
};

#endif
