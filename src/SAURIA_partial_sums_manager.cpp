#include "SAURIA_partial_sums_manager.h"
using namespace std;

PSUMS_MANAGER::PSUMS_MANAGER(){
	bytes_written = 0;
}

PSUMS_MANAGER::~PSUMS_MANAGER(){}

void PSUMS_MANAGER::write_results(NPU_SRAM* psums_sram, uint32_t base_offset, npu_data_t results[NPU_ROWS][NPU_COLS], bool accumulate){
	for(uint32_t i = 0; i < NPU_ROWS; i++){
		for(uint32_t j = 0; j < NPU_COLS; j++){
			uint32_t target_addr = base_offset + i * NPU_COLS + j;
			npu_data_t final_val = results[i][j];
			
			if(accumulate){
				npu_data_t old_val = psums_sram->read(target_addr);
				final_val += old_val;
			}
			
			psums_sram->write(target_addr, final_val);
			bytes_written += DATA_WIDTH_BYTES;
		}
	}
}

uint64_t PSUMS_MANAGER::get_write_bandwidth() {
    return bytes_written;
}

void PSUMS_MANAGER::reset_profiling() {
    bytes_written = 0;
}
