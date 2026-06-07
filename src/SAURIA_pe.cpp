#include "SAURIA_pe.h"

PE::PE(){
	mac_count = 0;
}

npu_data_t PE::computeMAC(npu_data_t ifmap_in, npu_data_t weight_in, npu_data_t psums_in){
	mac_count++;
	return psums_in + (ifmap_in * weight_in);
}

void PE::reset_profiling(){
	mac_count = 0;
}
