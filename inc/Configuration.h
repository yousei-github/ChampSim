#ifndef __CONFIGURATION_H
#define __CONFIGURATION_H

#define ENABLE (1)
#define DISABLE (0)

#define DEBUG_PRINTF (ENABLE)
#define USER_CODES (ENABLE)

#if USER_CODES == ENABLE
#define PREFETCHER_USE_NO_AND_NO_INSTR (ENABLE)
#define BRANCH_USE_BIMODAL (ENABLE)
#define REPLACEMENT_USE_LRU (ENABLE)
#define BTB_USE_BASIC (ENABLE)

#define KB (1024ul) // unit is byte
#define MB (KB*KB)
#define GB (MB*KB)
// Standard libraries

/* Includes */
#include <stdint.h>

/* defines */
typedef enum
{
    read_default = 0,
    read_end = 1,
} ReadPINTraceType;

typedef struct
{
    uint64_t PIN_trace_number;
    uint64_t total_PIN_trace_number;
    ReadPINTraceType read_PIN_trace_end;
} PINTraceReadType;

typedef struct
{
    FILE* trace_file;
    char trace_string[1024];
    uint64_t address;    // full_addr >> 6 = address (granularity: cache line or 64 Byte)
    uint8_t access_type; // read access = 0, write access = 1
} MemoryTraceFileType;

#endif

/** @note
 * 631.deepsjeng_s-928B.champsimtrace.xz has 2000000000 PIN traces
 *
 */
#endif
