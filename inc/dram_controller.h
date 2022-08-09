#ifndef DRAM_H
#define DRAM_H

#include <array>
#include <cmath>
#include <limits>

#include "champsim_constants.h"
#include "memory_class.h"
#include "operable.h"
#include "util.h"
#include "ProjectConfiguration.h" // user file

// these values control when to send out a burst of writes
constexpr std::size_t DRAM_WRITE_HIGH_WM = ((DRAM_WQ_SIZE * 7) >> 3);         // 7/8th
constexpr std::size_t DRAM_WRITE_LOW_WM = ((DRAM_WQ_SIZE * 6) >> 3);          // 6/8th
constexpr std::size_t MIN_DRAM_WRITES_PER_SWITCH = ((DRAM_WQ_SIZE * 1) >> 2); // 1/4

namespace detail
{
  // https://stackoverflow.com/a/31962570
  constexpr int32_t ceil(float num)
  {
    return (static_cast<float>(static_cast<int32_t>(num)) == num) ? static_cast<int32_t>(num) : static_cast<int32_t>(num) + ((num > 0) ? 1 : 0);
  }
} // namespace detail

#if USER_CODES == ENABLE

struct BANK_REQUEST
{
  bool valid = false, row_buffer_hit = false;

  // the row address will be stored here when the row is open.
  std::size_t open_row = std::numeric_limits<uint32_t>::max();

  uint64_t event_cycle = 0;

  // store the related PACKET to access
  std::vector<PACKET>::iterator pkt;
};


// DDR and HBM share a flat address space and the address space starts at HBM.
typedef enum
{
  HBM = 0,
  DDR,
} MemoryType;

struct MEMORY_STATISTICS
{
  uint64_t memory_request_total_service_time = 0, total_issued_memory_request_number = 0;
  float average_memory_access_time = 0;
};

// Note channels have their own queues, so please be careful when swapping data between different channels.
struct HBM_CHANNEL
{
  std::vector<PACKET> WQ{DRAM_WQ_SIZE};
  std::vector<PACKET> RQ{DRAM_RQ_SIZE};

  std::array<BANK_REQUEST, HBM_BANKS> bank_request = {};
  std::array<BANK_REQUEST, HBM_BANKS>::iterator active_request = std::end(bank_request);

  uint64_t dbus_cycle_available = 0, dbus_cycle_congested = 0, dbus_count_congested = 0;

  bool write_mode = false;

  unsigned WQ_ROW_BUFFER_HIT = 0, WQ_ROW_BUFFER_MISS = 0, RQ_ROW_BUFFER_HIT = 0, RQ_ROW_BUFFER_MISS = 0, WQ_FULL = 0;
};

struct DDR_CHANNEL
{
  std::vector<PACKET> WQ{DRAM_WQ_SIZE};
  std::vector<PACKET> RQ{DRAM_RQ_SIZE};

  std::array<BANK_REQUEST, DDR_RANKS* DDR_BANKS> bank_request = {};
  std::array<BANK_REQUEST, DDR_RANKS* DDR_BANKS>::iterator active_request = std::end(bank_request);

  uint64_t dbus_cycle_available = 0, dbus_cycle_congested = 0, dbus_count_congested = 0;

  bool write_mode = false;

  unsigned WQ_ROW_BUFFER_HIT = 0, WQ_ROW_BUFFER_MISS = 0, RQ_ROW_BUFFER_HIT = 0, RQ_ROW_BUFFER_MISS = 0, WQ_FULL = 0;
};

class MEMORY_CONTROLLER : public champsim::operable, public MemoryRequestConsumer
{
public:
  /** @note
   *  DRAM_IO_FREQ defined in champsim_constants.h
   *
   *  The unit of tRP/tRCD/tCAS/DRAM_DBUS_TURN_AROUND_TIME is cycle (1 cycle = 1 / DRAM_IO_FREQ microsecond)
   *  tRP is the time of precharging phase.
   *
   *  tRCD is row-to-column delay, which is the time when the subarray copies the row into the sense-amplifier
   *  after receiving ACTIVATE command from the DRAM controller.
   *
   *  tCAS is the time between sending a column address to the memory and the beginning of the data in response, or,
   *  is the delay, in clock cycles, between the internal READ command and the availability of the first bit of output
   *  data.
   *  In other words, tCAS is tCL which means the time between the data leaves the subarray and reaches the processor.
   *  Also, tCL is the abbreviation of CAS latency (CL).
   *
   *  CAS WRITE latency (CWL) is the delay, in clock cycles, between the internal WRITE command and the availability of
   *  the first bit of output data.
   *
   *  Thus, READ latency (RL) is controlled by the sum of the tRCD and tCAS register settings.
   *  WRITE latency (WL) is controlled by the sum of the tRCD and tCWL register settings.
   *  [reference](https://doi.org/10.1109/HPCA.2013.6522354)
   *  [reference](https://www.micron.com/-/media/client/global/documents/products/data-sheet/ddr4/8gb_ddr4_sdram.pdf, Micron 8Gb: x4, x8, x16 DDR4 SDRAM Features)
   */
  const static uint64_t tRP = detail::ceil(1.0 * tRP_DRAM_NANOSECONDS * DRAM_IO_FREQ / 1000);
  const static uint64_t tRCD = detail::ceil(1.0 * tRCD_DRAM_NANOSECONDS * DRAM_IO_FREQ / 1000);
  const static uint64_t tCAS = detail::ceil(1.0 * tCAS_DRAM_NANOSECONDS * DRAM_IO_FREQ / 1000);
  const static uint64_t DRAM_DBUS_TURN_AROUND_TIME = detail::ceil(1.0 * DBUS_TURN_AROUND_NANOSECONDS * DRAM_IO_FREQ / 1000);
  const static uint64_t DRAM_DBUS_RETURN_TIME = detail::ceil(1.0 * BLOCK_SIZE / DRAM_CHANNEL_WIDTH);

  MEMORY_STATISTICS statistics;

  std::array<DDR_CHANNEL, DDR_CHANNELS> ddr_channels;
  std::array<HBM_CHANNEL, HBM_CHANNELS> hbm_channels;

  MEMORY_CONTROLLER(double freq_scale) : champsim::operable(freq_scale), MemoryRequestConsumer(std::numeric_limits<unsigned>::max()) {}

  int add_rq(PACKET* packet) override;
  int add_wq(PACKET* packet) override;
  int add_pq(PACKET* packet) override;

  void operate() override;

  /** @note get the number of valid(active) member in the write/read queue.
   */
  uint32_t get_occupancy(uint8_t queue_type, uint64_t address) override;
  uint32_t get_size(uint8_t queue_type, uint64_t address) override;

  uint32_t memory_get_channel(uint64_t address);
  uint32_t memory_get_bank(uint64_t address);
  uint32_t memory_get_column(uint64_t address);
  uint32_t memory_get_row(uint64_t address);

  // HBM doesn't have rank.
  uint32_t hbm_get_channel(uint64_t address);
  uint32_t hbm_get_bank(uint64_t address);
  uint32_t hbm_get_column(uint64_t address);
  uint32_t hbm_get_row(uint64_t address);

  uint32_t ddr_get_channel(uint64_t address);
  uint32_t ddr_get_rank(uint64_t address);
  uint32_t ddr_get_bank(uint64_t address);
  uint32_t ddr_get_column(uint64_t address);
  uint32_t ddr_get_row(uint64_t address);

  MemoryType get_memory_type(uint64_t address);

  float get_average_memory_access_time();

private:
  void operate_hbm(HBM_CHANNEL& channel);
  void operate_ddr(DDR_CHANNEL& channel);
};

#else
struct BANK_REQUEST
{
  bool valid = false, row_buffer_hit = false;

  std::size_t open_row = std::numeric_limits<uint32_t>::max();

  uint64_t event_cycle = 0;

  std::vector<PACKET>::iterator pkt;
};

struct DRAM_CHANNEL
{
  std::vector<PACKET> WQ{DRAM_WQ_SIZE};
  std::vector<PACKET> RQ{DRAM_RQ_SIZE};

  std::array<BANK_REQUEST, DRAM_RANKS* DRAM_BANKS> bank_request = {};
  std::array<BANK_REQUEST, DRAM_RANKS* DRAM_BANKS>::iterator active_request = std::end(bank_request);

  uint64_t dbus_cycle_available = 0, dbus_cycle_congested = 0, dbus_count_congested = 0;

  bool write_mode = false;

  unsigned WQ_ROW_BUFFER_HIT = 0, WQ_ROW_BUFFER_MISS = 0, RQ_ROW_BUFFER_HIT = 0, RQ_ROW_BUFFER_MISS = 0, WQ_FULL = 0;
};

class MEMORY_CONTROLLER : public champsim::operable, public MemoryRequestConsumer
{
public:
  // DRAM_IO_FREQ defined in champsim_constants.h
  const static uint64_t tRP = detail::ceil(1.0 * tRP_DRAM_NANOSECONDS * DRAM_IO_FREQ / 1000);
  const static uint64_t tRCD = detail::ceil(1.0 * tRCD_DRAM_NANOSECONDS * DRAM_IO_FREQ / 1000);
  const static uint64_t tCAS = detail::ceil(1.0 * tCAS_DRAM_NANOSECONDS * DRAM_IO_FREQ / 1000);
  const static uint64_t DRAM_DBUS_TURN_AROUND_TIME = detail::ceil(1.0 * DBUS_TURN_AROUND_NANOSECONDS * DRAM_IO_FREQ / 1000);
  const static uint64_t DRAM_DBUS_RETURN_TIME = detail::ceil(1.0 * BLOCK_SIZE / DRAM_CHANNEL_WIDTH);

  std::array<DRAM_CHANNEL, DRAM_CHANNELS> channels;

  MEMORY_CONTROLLER(double freq_scale) : champsim::operable(freq_scale), MemoryRequestConsumer(std::numeric_limits<unsigned>::max()) {}

  int add_rq(PACKET* packet) override;
  int add_wq(PACKET* packet) override;
  int add_pq(PACKET* packet) override;

  void operate() override;

  uint32_t get_occupancy(uint8_t queue_type, uint64_t address) override;
  uint32_t get_size(uint8_t queue_type, uint64_t address) override;

  uint32_t dram_get_channel(uint64_t address);
  uint32_t dram_get_rank(uint64_t address);
  uint32_t dram_get_bank(uint64_t address);
  uint32_t dram_get_row(uint64_t address);
  uint32_t dram_get_column(uint64_t address);
};
#endif

#endif
