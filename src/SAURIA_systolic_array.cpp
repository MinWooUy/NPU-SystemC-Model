#include "SAURIA_systolic_array.h"
#include <iostream>

using namespace std;

SYSTOLIC_ARRAY::SYSTOLIC_ARRAY(){}

SYSTOLIC_ARRAY::~SYSTOLIC_ARRAY(){}

void SYSTOLIC_ARRAY::execute_mac_wave(NPU_SRAM* ifmap_sram, uint32_t ifmap_offset, NPU_SRAM* weight_sram, uint32_t weight_offset, NPU_SRAM* psums_sram, uint32_t psums_offset, bool accumulate){
	// FETCH
	uint32_t image_width_in_sram = 16;
	feed_lane.fetch_ifmap_block(ifmap_sram, ifmap_offset, image_width_in_sram, 0, 0, ifmap_buffer);
	weight_fetcher.fetch_weight_block(weight_sram, weight_offset, weight_buffer);
	
	for(uint32_t i = 0; i < NPU_ROWS; i++){
		for(uint32_t j = 0; j < NPU_COLS; j++){
			psums_buffer[i][j] = 0;
		}
	}
	
	// EXECUTE
	for(uint32_t i = 0; i < NPU_ROWS; i++){
		for(uint32_t j = 0; j < NPU_COLS; j++){
			for(uint32_t k = 0; k < NPU_ROWS; k++){
				npu_data_t a = ifmap_buffer[i][k];
				npu_data_t b = weight_buffer[k][j];
				
				psums_buffer[i][j] = pe_array[i][j].computeMAC(a,b,psums_buffer[i][j]);
			}
		}
	}
	
	// WRITEBACK
	psums_manager.write_results(psums_sram, psums_offset, psums_buffer, accumulate);
}

void SYSTOLIC_ARRAY::print_performance_report(){
	uint64_t total_mac = 0;
	
	for(uint32_t i = 0; i < NPU_ROWS; i++){
		for(uint32_t j = 0; j < NPU_COLS; j++){
			total_mac += pe_array[i][j].mac_count;
		}
	}
	
	cout << "\n=======================================================" << endl;
    	cout << "          SAURIA NPU - HARDWARE PERFORMANCE REPORT     " << endl;
    	cout << "=======================================================" << endl;
    	cout << " - Total MAC Operations : " << dec << total_mac << " ops" << endl;
   	cout << " - SRAM Read Bandwidth  : " << feed_lane.get_bandwidth_usage() << " bytes" << endl;
    	cout << " - SRAM Write Bandwidth : " << psums_manager.get_write_bandwidth() << " bytes" << endl;
    	cout << "=======================================================\n" << endl;
}

void SYSTOLIC_ARRAY::reset_all_profiling(){
	for (uint32_t i = 0; i < NPU_ROWS; i++) {
	        for (uint32_t j = 0; j < NPU_COLS; j++) {
	            pe_array[i][j].reset_profiling();
	        }
	}
    	feed_lane.reset_profiling();
    	psums_manager.reset_profiling();
}
