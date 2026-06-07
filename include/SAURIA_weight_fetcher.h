#ifndef SAURIA_WEIGHT_FETCHER_H
#define SAURIA_WEIGHT_FETCHER_H

#include "SAURIA_config.h"
#include "SAURIA_sram.h"

class WEIGHT_FETCHER{
private:
	uint64_t bytes_fetcher;
public:
	WEIGHT_FETCHER();
	~WEIGHT_FETCHER();
	
	void fetch_weight_block(NPU_SRAM* weight_sram, uint32_t base_offset, npu_data_t buffer[NPU_ROWS][NPU_COLS]);
	uint64_t get_bandwidth_usage();
	void reset_profiling();
};

#endif
