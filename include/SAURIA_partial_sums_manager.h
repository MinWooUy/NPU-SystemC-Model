#ifndef SAURIA_PARTIAL_SUMS_MANAGER_H
#define SAURIA_PARTIAL_SUMS_MANAGER_H

#include "SAURIA_config.h"
#include "SAURIA_sram.h"

class PSUMS_MANAGER{
private:
	uint64_t bytes_written;
public:
	PSUMS_MANAGER();
	~PSUMS_MANAGER();
	
	// Biến accumulate = true: Cộng dồn vào giá trị cũ. False: Ghi đè giá trị mới.
	void write_results(NPU_SRAM* psums_sram, uint32_t base_offset, npu_data_t results[NPU_ROWS][NPU_COLS], bool accumulate);
	uint64_t get_write_bandwidth();
	void reset_profiling();
};

#endif
