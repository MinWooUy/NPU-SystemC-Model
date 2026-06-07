#ifndef SAURIA_SYSTOLIC_ARRAY_H
#define SAURIA_SYSTOLIC_ARRAY_H

#include "SAURIA_config.h"
#include "SAURIA_sram.h"
#include "SAURIA_pe.h"
#include "SAURIA_feed_lane.h"
#include "SAURIA_weight_fetcher.h"
#include "SAURIA_partial_sums_manager.h"

class SYSTOLIC_ARRAY{
private:
	// Ex: 16x16 PE (systolic array)
	PE pe_array[NPU_ROWS][NPU_COLS];
	
	FeedLane feed_lane;
	WEIGHT_FETCHER weight_fetcher;
	PSUMS_MANAGER psums_manager;
	
	npu_data_t ifmap_buffer[NPU_ROWS][NPU_COLS];
	npu_data_t weight_buffer[NPU_ROWS][NPU_COLS];
	npu_data_t psums_buffer[NPU_ROWS][NPU_COLS];
public:
	SYSTOLIC_ARRAY();
	~SYSTOLIC_ARRAY();
	
	void execute_mac_wave(NPU_SRAM* ifmap_sram, uint32_t ifmap_offset, NPU_SRAM* weight_sram, uint32_t weight_offset, NPU_SRAM* psums_sram, uint32_t psums_offset, bool accumulate);
	void print_performance_report();
    	void reset_all_profiling();
};

#endif
