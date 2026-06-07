#ifndef SAURIACONFIG_H
#define SAURIACONFIG_H
#include <cstdint>

//######################################################
// SAURIA NPU - VIRTUAL PLATFORMS GLOBAL CONFIGURATION #
//######################################################

// DATA TYPES
typedef uint32_t npu_data_t;
const uint32_t DATA_WIDTH_BYTES = sizeof(npu_data_t);

// HARDWARE PARAMETERS
const uint32_t NPU_ROWS = 16;
const uint32_t NPU_COLS = 16;

// TIMING
const uint32_t CLOCK_PERIOD_NS = 10; // Assume chip run at speed 100MHZ -> t = 10 (ns)
const uint32_t SRAM_ACCESS_TIME = 2; // Read/Write RAM: 2 cycle clock

// Configuration AXI Address Space
const uint64_t CONTROLLER_BASE 	= 0x40000000;
const uint64_t SAURIA_BASE 	= 0x50000000;
const uint64_t DMA_BASE		= 0x60000000;

// SAURIA Internal Address Space
const uint64_t OFFSET_CFG_REGS	= 0x00000000;
const uint64_t OFFSET_CFG_CON	= 0x00000200;	// Control register (Bit 0: START)
const uint64_t OFFSET_CFG_STAT	= 0x00000204;	// Status register (0: READY, 1: BUSY, 2: DONE)

const uint64_t OFFSET_CFG_CYCLE = 0x00000208;	// Cycle counter register 

const uint64_t OFFSET_CFG_ACT	= 0x00000400;
const uint64_t OFFSET_CFG_WEI	= 0x00000600;
const uint64_t OFFSET_CFG_OUT	= 0x00000800;

const uint64_t OFFSET_SRAMA	= 0x00040000;	// Image data region
const uint64_t OFFSET_SRAMB	= 0x00080000;	// Weight data region
const uint64_t OFFSET_SRAMC	= 0x000C0000;	// Partial sum data region

const uint64_t OFFSET_DMA_SRC	= 0x00000000;
const uint64_t OFFSET_DMA_DST	= 0x00000004;	// Divide SRAM_A between SRAM_B
const uint64_t OFFSET_DMA_SIZE	= 0x00000008;	// Size data need config to move (8 bit = 1 bytes)
const uint64_t OFFSET_DMA_CMD	= 0x0000000C;  	// Write 0x1 to DMA use BUS to fetch data

// MEMORY SIZE
const uint32_t SRAM_SIZE	= 0x40000;	// 256 KB / (SRAM region)
const uint32_t SRAM_WORDS	= SRAM_SIZE / DATA_WIDTH_BYTES;

#endif	
