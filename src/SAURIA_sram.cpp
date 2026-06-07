#include <iostream>
#include "SAURIA_sram.h"

using namespace std;

NPU_SRAM::NPU_SRAM(string _name) : name(_name){
	memory = new npu_data_t[SRAM_WORDS];
	for(uint32_t i = 0; i < SRAM_WORDS; i++) memory[i] = 0;
}

NPU_SRAM::~NPU_SRAM(){
	delete[] memory;
}

void NPU_SRAM::write(uint32_t offset, npu_data_t data) {
    if (offset < SRAM_WORDS) {
        memory[offset] = data;
    } else {
        cout << "[HARDWARE FAULT- " << name << "]: Error write overflow memory offset 0x" 
             << hex << offset << dec << endl;
    }
}

npu_data_t NPU_SRAM::read(uint32_t offset) {
    if (offset < SRAM_WORDS) {
        return memory[offset];
    } else {
        cout << "[HARDWARE FAULT - " << name << "]: Error read overflow memory offset 0x" 
             << hex << offset << dec << endl;
        return 0;
    }
}

npu_data_t* NPU_SRAM::get_raw_ptr() {
    return memory;
}
