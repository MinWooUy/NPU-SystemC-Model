#include "SAURIA_weight_fetcher.h"

WEIGHT_FETCHER::WEIGHT_FETCHER(){bytes_fetcher = 0;}
WEIGHT_FETCHER::~WEIGHT_FETCHER(){}

void WEIGHT_FETCHER::fetch_weight_block(NPU_SRAM* weight_sram, uint32_t base_offset, npu_data_t buffer[NPU_ROWS][NPU_COLS]){
	uint32_t current_addr = base_offset;
	for(uint32_t i = 0; i < NPU_ROWS; i++){
		for(uint32_t j = 0; j < NPU_COLS; j++){
			buffer[i][j] = weight_sram->read(current_addr);
			current_addr++;
			bytes_fetcher += DATA_WIDTH_BYTES;
		}
	}
}

uint64_t WEIGHT_FETCHER::get_bandwidth_usage() { 
	return bytes_fetcher; 
}

void WEIGHT_FETCHER::reset_profiling() { 
	bytes_fetcher = 0; 
}
