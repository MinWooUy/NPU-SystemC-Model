#ifndef SAURIA_SRAM_H
#define SAURIA_SRAM_H

#include <string>
#include <iostream>
#include "SAURIA_config.h"

using namespace std;

class NPU_SRAM{
private:
	npu_data_t* memory;
	string name;
public:
	NPU_SRAM(string _name);
	~NPU_SRAM();
	
	void write(uint32_t offset, npu_data_t data);
	npu_data_t read(uint32_t offset);
	npu_data_t* get_raw_ptr();
};

#endif
