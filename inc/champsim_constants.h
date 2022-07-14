/***
 * THIS FILE IS AUTOMATICALLY GENERATED
 * Do not edit this file. It will be overwritten when the configure script is run.
 ***/

#ifndef CHAMPSIM_CONSTANTS_H
#define CHAMPSIM_CONSTANTS_H
#include "util.h"
#include "Configuration.h" // user file

#if USER_CODES == ENABLE
 // the unit of BLOCK_SIZE is byte
#endif
#define BLOCK_SIZE 64ul
#define LOG2_BLOCK_SIZE lg2(BLOCK_SIZE)
#define PAGE_SIZE 4096ul
#define LOG2_PAGE_SIZE lg2(PAGE_SIZE)
#define STAT_PRINTING_PERIOD 10000000ul
#define NUM_CPUS 1ul
#define NUM_CACHES 7u
#define NUM_OPERABLES 10u

#if USER_CODES == ENABLE
/** @note
 *  First, a DRAM chip is the set of all cells on the same silicon die, along with the I/O circuitry which allows
 *  the cells to be accessed from outside the chip.
 *  Second, a DRAM chip is divided into multiple banks (e.g., eight for DDR3), where a bank is a set of cells that
 *  share peripheral circuitry such as the address decoders (row and column).
 *  Third, a bank is further sub-divided into tens of subarrays, where a subarray is a set of cells that share bitlines
 *  and sense-amplifiers.
 *  The main purpose of a sense-amplifier is to read from a cell by reliably detecting the very small amount of
 *  electrical charge stored in the cell. And the set of all sense-amplifiers in a subarray is called a row-buffer.
 *  [reference](https://doi.org/10.1109/HPCA.2013.6522354)
 * 
 *  In DDR, we have multiple DRAM chips (devices) on the same PCB. Because a 64-byte cache line is striped across banks
 *  in different DRAM chips to maximize parallelism -> The width of DRAM chip's data bus (Bidirectional) varies according 
 *  to the DDR configuration. This means all chips in the same rank will operate together when transferring data, so no need
 *  to set chip address field in physical address.
 *
 *  The unit of DRAM_IO_FREQ is MHz. Here if DRAM_IO_FREQ is 3200ul, the DRAM is DDR5 (2400-3600 MHz).
 */
#define DRAM_IO_FREQ (3200ul)
#define DDR_CHANNELS (1ul)
#define DDR_RANKS (1ul)
#define DDR_BANKS (8ul)
#define DDR_ROWS (32768ul)
#define DDR_COLUMNS (64ul)
#else
#define DRAM_IO_FREQ 3200ul
#define DRAM_CHANNELS 1ul
#define DRAM_RANKS 1ul
#define DRAM_BANKS 8ul
#define DRAM_ROWS 65536ul
#define DRAM_COLUMNS 128ul

#endif

#if USER_CODES == ENABLE
#define DDR_CAPACITY (768*MB)  // Note if you modify its capacity, its corresponding parameters (e.g., channel, bank, ...) need to be modified too.
//#define DDR_CAPACITY (1*GB) 

 /** @note
  *  if DDR_CHANNELS = 1, DDR_RANKS = 1, DDR_BANKS = 8, DDR_ROWS = 65536, DDR_COLUMNS = 128,
  *  then width of DDR_CHANNELS is 0 bit, width of DDR_RANKS is 0 bit, width of DDR_BANKS is 3 bit,
  *  width of DDR_ROWS is 16 bit, and width of DDR_COLUMNS is 7 bit,
  *  so, the width of physical address is 0 + 0 + 3 + 16 + 7 + 6 (block offset) = 32 bit,
  *  thus, the maximum physical memory capacity is 4 GB.
  *
  *  Physical address (32 bit for 4 GB DDR capacity)
  *  | row address | rank index | column address (bank index) | channel | block offset |
  *  |   16 bits   |    0 bit   |           10 bits (3 bits)  |  0 bit  |    6 bits    |
  *  e.g., DDR_CHANNELS = 1, DDR_RANKS = 1, DDR_BANKS = 8, DDR_ROWS = 65536, DDR_COLUMNS = 128 (10 - 3) for 4 GB DDR capacity.
  * 
  *  Physical address (30 bit for (512, 1024] MB DDR capacity)
  *  | row address | rank index | column address (bank index) | channel | block offset |
  *  |   15 bits   |    0 bit   |            9 bits (3 bits)  |  0 bit  |    6 bits    |
  *  e.g., DDR_CHANNELS = 1, DDR_RANKS = 1, DDR_BANKS = 8, DDR_ROWS = 32768, DDR_COLUMNS = 64 (9 - 3) for (512, 1024] MB DDR capacity.
  */


  // Below are parameters for 3D-stacked DRAM (or High Bandwidth Memory (HBM))
  // HBM uses the same IO frequency as DRAM
#define HBM_CHANNELS (8ul)
#define HBM_BANKS    (8ul)
#define HBM_ROWS     (1024ul)
#define HBM_COLUMNS  (64ul)
#define HBM_CAPACITY (256*MB)
//#define HBM_CAPACITY (0*MB)
 /** @note
  *  Physical address (30 bit for 1 GB HBM capacity)
  *  | row address | rank index | column address (bank index) | channel | block offset |
  *  |   11 bits   |    0 bit   |           10 bits (3 bits)  |  3 bits |    6 bits    |
  *  e.g., HBM_CHANNELS = 8, HBM_BANKS = 8, HBM_ROWS = 2048, HBM_COLUMNS = 128 (10 - 3) for 1 GB DDR capacity.
  *  
  *  Physical address (28 bit for 256 MB HBM capacity)
  *  | row address | rank index | column address (bank index) | channel | block offset |
  *  |   10 bits   |    0 bit   |            9 bits (3 bits)  |  3 bits |    6 bits    |
  *  e.g., HBM_CHANNELS = 8, HBM_BANKS = 8, HBM_ROWS = 1024, HBM_COLUMNS = 64 (9 - 3) for 256 MB HBM capacity.
  */
#endif

#if USER_CODES == ENABLE
  // the unit of DRAM_CHANNEL_WIDTH is byte
#endif
#define DRAM_CHANNEL_WIDTH 8ul
#define DRAM_WQ_SIZE 64ul
#define DRAM_RQ_SIZE 64ul
#define tRP_DRAM_NANOSECONDS 12.5
#define tRCD_DRAM_NANOSECONDS 12.5
#define tCAS_DRAM_NANOSECONDS 12.5
#define DBUS_TURN_AROUND_NANOSECONDS 7.5

#if USER_CODES == ENABLE
#define PAGE_TABLE_LEVELS (5ul)
#define MINOR_FAULT_PENALTY (200ul)

#endif

#endif
