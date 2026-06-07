#ifndef SAURIA_FEED_LANE_H
#define SAURIA_FEED_LANE_H

#include <cstdint>
#include "SAURIA_config.h"
#include "SAURIA_sram.h"

class FeedLane{
private:
	uint64_t bytes_fetched; // The bytes read at SRAM
public:
	FeedLane();
	~FeedLane();
	
	void fetch_ifmap_block(NPU_SRAM* ifmap_sram, uint32_t base_offset, uint32_t img_width, uint32_t start_row, uint32_t start_col, npu_data_t buffer[NPU_ROWS][NPU_COLS]);
	uint64_t get_bandwidth_usage();
	void reset_profiling();
};

#endif
