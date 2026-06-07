#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>

#include "SAURIA_npu.h"
#include "SAURIA_dma.h"
#include "axi_router.h"

using namespace std;
using namespace sc_core;
using namespace tlm;
using namespace tlm_utils;

class DummyCPU : public sc_module {
public:
	// port Bus (TLM Initiator Socket)
	simple_initiator_socket<DummyCPU> socket;
    
	sc_in<bool> npu_interrupt_in;
	sc_in<bool> dma_interrupt_in;

	sc_event ev_npu_interrupt;
	sc_event ev_dma_interrupt;	
	
	SC_HAS_PROCESS(DummyCPU);

	DummyCPU(sc_module_name name) : sc_module(name), socket("socket"), npu_interrupt_in("npu_interrupt_in"), dma_interrupt_in("dma_interrupt_in") {
		SC_THREAD(run_software_flow);
        	SC_METHOD(interrupt_handle);
        	dont_initialize();
        	sensitive << npu_interrupt_in.pos() << dma_interrupt_in.pos(); // Kích hoạt ISR khi đường ngắt chuyển từ LOW lên HIGH
    	}
    	
    	void interrupt_handle(){
    		if(npu_interrupt_in.read() == true) ev_npu_interrupt.notify(SC_ZERO_TIME);
    		if(dma_interrupt_in.read() == true) ev_dma_interrupt.notify(SC_ZERO_TIME);
    	}

	void run_software_flow() {
        	wait(10, SC_NS);
        	cout << "\n====================================================" << endl;
        	cout << "[Virtual Platform]: Start FX1 system" << endl;
        	cout << "====================================================" << endl;

        	cout << "[Driver]: Checking connection to NPU..." << endl;
        	uint32_t status = bus_read(SAURIA_BASE + OFFSET_CFG_STAT);
        	if (status == 0) {
            		cout << "[Driver]: Successfully Init. Found SAURIA NPU in READY state." << endl;
        	}

		cout << "[Driver]: DMA fetching ifmap..." << endl;
		bus_write(DMA_BASE + 0x04, OFFSET_SRAMA);	// Destination
		bus_write(DMA_BASE + 0x08, 65536);		// Size
		bus_write(DMA_BASE + 0x0C, 1);			// Run
		wait(ev_dma_interrupt);
		wait(10, SC_NS);
		cout << "[Driver]: DMA fetching weight.." << endl;
		bus_write(DMA_BASE + 0x04, OFFSET_SRAMB);	// Destination
		bus_write(DMA_BASE + 0x08, 65536);		// Size
		bus_write(DMA_BASE + 0x0C, 1);			// Run
		wait(ev_dma_interrupt);

		wait(10, SC_NS);

		cout << "[Driver]: Configuring memory pointers..." << endl;
		bus_write(SAURIA_BASE + OFFSET_CFG_ACT, 0x00000000);
		bus_write(SAURIA_BASE + OFFSET_CFG_WEI, 0x00000000);
		bus_write(SAURIA_BASE + OFFSET_CFG_OUT, 0x00000000);

        	cout << "[Driver]: Writing 0x1 to CTRL register to START NPU..." << endl;
        	bus_write(SAURIA_BASE + OFFSET_CFG_CON, 0x1);

        	cout << "[Driver]: CPU is idle, waiting for INTERRUPT from NPU..." << endl;
        	wait(ev_npu_interrupt);

        	cout << "[CPU Interrupt Handler]: Interrupt received. Processing verification..." << endl;
        
        	npu_data_t* golden_array = new npu_data_t[SRAM_WORDS];
        	ifstream file("golden_data.bin", ios::binary);
        
        	if (file.is_open()) {
            		file.read(reinterpret_cast<char*>(golden_array), SRAM_SIZE);
            		file.close();
        	} else {
            		cout << "[Error]: Cannot find file golden_data.bin" << endl;
            		sc_stop();
            		return;
        	}

        	// Đọc kết quả trực tiếp từ vùng nhớ PSUMS SRAM của NPU thông qua Bus
       		uint32_t error_count = 0;
        	uint32_t check_elements = NPU_ROWS * NPU_COLS; //Ex: 16x16

        	for (uint32_t i = 0; i < check_elements; i++) {
            		uint64_t target_bus_addr = SAURIA_BASE + OFFSET_SRAMC + (i * DATA_WIDTH_BYTES);
            		npu_data_t hardware_result = bus_read(target_bus_addr);

            		if (hardware_result != golden_array[i]) {
                		error_count++;
                		if (error_count <= 10) { // Limit error
                    			cout << "--> Mismatch at Index " << i << ": NPU (Bus) = 0x" << hex << hardware_result 
                         			<< " | Golden Model = 0x" << golden_array[i] << dec << endl;
                		}
            		}
        	}

		cout << "\n[Verification]: Result SRAM C of NPU (4x4 left corner)" << endl;
        	for (uint32_t r = 0; r < 4; r++) {
            		cout << "Row " << r << ":\t";
            		for (uint32_t c = 0; c < 4; c++) {
                		uint32_t idx = r * NPU_COLS + c;
                		uint64_t target_bus_addr = SAURIA_BASE + OFFSET_SRAMC + (idx * DATA_WIDTH_BYTES);
                		cout << bus_read(target_bus_addr) << "\t";
            		}
            		cout << endl;
        	}

        	cout << "##############################################################" << endl;
        	if (error_count == 0) {
            		cout << "SUCCESSFULLY! Virtual NPU exactly calculated!" << endl;
        	} else {
            		cout << "FAILED! Wrong calculation!" << endl;
            		cout << "Total Errors: " << error_count << " / " << check_elements << endl;
        	}
        	cout << "##############################################################" << endl;

        	delete[] golden_array;
        	sc_stop();
    }

private:
    	// ---------------------------------------------------------
    	//  FIRMWARE WRITE/READ HAL
    	// ---------------------------------------------------------
    	void bus_write(uint64_t addr, uint32_t data) {
        	tlm_generic_payload trans;
        	sc_time delay = sc_time(0, SC_NS);

        	trans.set_command(TLM_WRITE_COMMAND);
        	trans.set_address(addr);
        	trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        	trans.set_data_length(4);
        	trans.set_streaming_width(4);
        	trans.set_byte_enable_ptr(0);
        	trans.set_dmi_allowed(false);
        	trans.set_response_status(TLM_INCOMPLETE_RESPONSE);

        	socket->b_transport(trans, delay);
    	}

    	uint32_t bus_read(uint64_t addr) {
        	tlm_generic_payload trans;
        	sc_time delay = sc_time(0, SC_NS);
        	uint32_t data = 0;

        	trans.set_command(TLM_READ_COMMAND);
	        trans.set_address(addr);
	        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
	        trans.set_data_length(4);
	        trans.set_streaming_width(4);
	        trans.set_byte_enable_ptr(0);
	        trans.set_dmi_allowed(false);
	        trans.set_response_status(TLM_INCOMPLETE_RESPONSE);

        	socket->b_transport(trans, delay);
        	return data;
    	}
};

int sc_main(int argc, char* argv[]) {
    	DummyCPU cpu("Cortex_A32");
    	AXI_ROUTER router("AXI_Interconnect");
    	SAURIA_NPU npu("SAURIA_NPU_Accelerator");
	SAURIA_DMA dma("DMA_Engine");

    	// SYSTEM INTERCONNECTED
    	// AXI Bus TLM 2.0: CPU (Master) -> NPU (Slave)
	cpu.socket.bind(router.cpu_socket);	// CPU -> ROUTER
	dma.dma_master_socket.bind(router.dma_in_socket); // DMA -> ROUTER	
	
	router.npu_socket.bind(npu.socket);	// ROUTER out 1 --> NPU
	router.dma_socket.bind(dma.socket);	// ROUTER out 2 --> DMA

	sc_signal<bool> sig_npu_irq;
	sc_signal<bool> sig_dma_irq;	
	
    	// wire INTERRUPT: NPU (Output Port) -> CPU (Input Port)
    	npu.interrupt_port_out.bind(sig_npu_irq);
    	cpu.npu_interrupt_in.bind(sig_npu_irq);
    	
    	dma.interrupt_port_out.bind(sig_dma_irq);
    	cpu.dma_interrupt_in.bind(sig_dma_irq);

    	cout << "SystemC Environment Setup Successful!" << endl;
    	cout << "Ready to build SAURIA NPU Virtual Platform" << endl;

    	sc_start();
    	return 0;
}
