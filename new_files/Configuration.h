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

#endif
// Standard libraries

/* Includes */
#include <stdint.h>

/* defines */

/** @note
 * 631.deepsjeng_s-928B.champsimtrace.xz has 2000000000 PIN traces
 *
 */
#endif
