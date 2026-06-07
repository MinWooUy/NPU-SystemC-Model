#include "SAURIA_feed_lane.h"

FeedLane::FeedLane(){
	bytes_fetched = 0;
}

FeedLane::~FeedLane(){

}

void FeedLane::fetch_ifmap_block(NPU_SRAM* ifmap_sram, uint32_t base_offset, uint32_t img_width, uint32_t start_row, uint32_t start_col, npu_data_t buffer[NPU_ROWS][NPU_COLS]	){
	for(uint32_t i = 0; i < NPU_ROWS; i++){
		for(uint32_t j = 0; j < NPU_COLS; j++){
			uint32_t pixel_row = start_row + i;
			uint32_t pixel_col = start_col + j;
			
			// ADDRESS GENERATION UNIT
			uint32_t target_addr = base_offset + (pixel_row * img_width) + pixel_col;
			
			buffer[i][j] = ifmap_sram->read(target_addr);
			bytes_fetched += DATA_WIDTH_BYTES;
		}
	}
}


uint64_t FeedLane::get_bandwidth_usage() {
    return bytes_fetched;
}

void FeedLane::reset_profiling() {
    bytes_fetched = 0;
}	
