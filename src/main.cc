#include <algorithm>
#include <array>
#include <fstream>
#include <functional>
#include <getopt.h>
#include <iomanip>
#include <signal.h>
#include <string.h>
#include <vector>

#include "cache.h"
#include "champsim.h"
#include "champsim_constants.h"
#include "dram_controller.h"
#include "ooo_cpu.h"
#include "operable.h"
#include "tracereader.h"
#include "vmem.h"

uint8_t warmup_complete[NUM_CPUS] = {}, simulation_complete[NUM_CPUS] = {}, all_warmup_complete = 0, all_simulation_complete = 0,
MAX_INSTR_DESTINATIONS = NUM_INSTR_DESTINATIONS, knob_cloudsuite = 0, knob_low_bandwidth = 0;

uint64_t warmup_instructions = 1000000, simulation_instructions = 10000000;

auto start_time = time(NULL);

// For backwards compatibility with older module source.
champsim::deprecated_clock_cycle current_core_cycle;

extern MEMORY_CONTROLLER memory;
extern VirtualMemory vmem;
extern std::array<O3_CPU*, NUM_CPUS> ooo_cpu;
extern std::array<CACHE*, NUM_CACHES> caches;
extern std::array<champsim::operable*, NUM_OPERABLES> operables;

std::vector<tracereader*> traces;

void record_roi_stats(uint32_t cpu, CACHE* cache);
void print_roi_stats(uint32_t cpu, CACHE* cache);
void print_sim_stats(uint32_t cpu, CACHE* cache);
void print_branch_stats();
#if USER_CODES == ENABLE
void print_memory_stats();
void print_configuration_details();
#else
void print_dram_stats();
#endif
void reset_cache_stats(uint32_t cpu, CACHE* cache);
void finish_warmup();
void signal_handler(int signal);

int main(int argc, char** argv)
{
  // interrupt signal hanlder
  struct sigaction sigIntHandler;
  sigIntHandler.sa_handler = signal_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, NULL);

#if PRINT_STATISTICS_INTO_FILE == ENABLE
#else
  cout << endl << "*** ChampSim Multicore Out-of-Order Simulator ***" << endl << endl;
#endif


  // initialize knobs
  uint8_t show_heartbeat = 1;

  // check to see if knobs changed using getopt_long()
  int traces_encountered = 0;
#if USER_CODES == ENABLE
  uint64_t start_position_of_traces = 0;
  for (auto i = 1; i < argc; i++)
  {
    static uint8_t abort_flag = 0;
    if (strcmp(argv[i], "--warmup_instructions") == 0)
    {
      if (i + 1 < argc)
      {
        warmup_instructions = atol(argv[++i]);
        start_position_of_traces = i + 1;
        continue;
      }
      else
      {
        std::cout << __func__ << ": Need parameter behind --warmup_instructions." << std::endl;
        abort_flag++;
      }
    }

    if (strcmp(argv[i], "--simulation_instructions") == 0)
    {
      if (i + 1 < argc)
      {
        simulation_instructions = atol(argv[++i]);
        start_position_of_traces = i + 1;
        continue;
      }
      else
      {
        std::cout << __func__ << ": Need parameter behind --simulation_instructions." << std::endl;
        abort_flag++;
      }
    }

    if (strcmp(argv[i], "--hide_heartbeat") == 0)
    {
      show_heartbeat = 0;
      start_position_of_traces++;
      continue;
    }

    if (strcmp(argv[i], "--cloudsuite") == 0)
    {
      knob_cloudsuite = 1;
      MAX_INSTR_DESTINATIONS = NUM_INSTR_DESTINATIONS_SPARC;
      start_position_of_traces++;
      continue;
    }

    if (strcmp(argv[i], "--traces") == 0)
    {
      traces_encountered = 1;
      start_position_of_traces++;
      break;
    }

    if (abort_flag)
    {
      abort();
    }
  }
#else
  static struct option long_options[] = {{"warmup_instructions", required_argument, 0, 'w'},
                                           {"simulation_instructions", required_argument, 0, 'i'},
                                           {"hide_heartbeat", no_argument, 0, 'h'},
                                           {"cloudsuite", no_argument, 0, 'c'},
                                           {"traces", no_argument, &traces_encountered, 1},
                                           {0, 0, 0, 0}};

  int c;
  while ((c = getopt_long_only(argc, argv, "w:i:hc", long_options, NULL)) != -1 && !traces_encountered)
  {
    switch (c)
    {
    case 'w':
      warmup_instructions = atol(optarg);
      break;
    case 'i':
      simulation_instructions = atol(optarg);
      break;
    case 'h':
      show_heartbeat = 0;
      break;
    case 'c':
      knob_cloudsuite = 1;
      MAX_INSTR_DESTINATIONS = NUM_INSTR_DESTINATIONS_SPARC;
      break;
    case 0:
      break;
    default:
      abort();
    }
  }
#endif


#if USER_CODES == ENABLE
  // prepare file for recording memory traces.
  output_memory_trace_initialization(argv[start_position_of_traces]);
#if PRINT_STATISTICS_INTO_FILE == ENABLE
  output_champsim_statistics_initialization(argv[start_position_of_traces]);
  fprintf(outputchampsimstatistics.trace_file, "\n*** ChampSim Multicore Out-of-Order Simulator ***\n\n");
#else
#endif
  // Output configuration details
  print_configuration_details();
#else
  cout << "Warmup Instructions: " << warmup_instructions << endl;
  cout << "Simulation Instructions: " << simulation_instructions << endl;
  cout << "Number of CPUs: " << NUM_CPUS << endl;

  long long int dram_size = DRAM_CHANNELS * DRAM_RANKS * DRAM_BANKS * DRAM_ROWS * DRAM_COLUMNS * BLOCK_SIZE / 1024 / 1024; // in MiB
  std::cout << "Off-chip DRAM Size: ";
  if (dram_size > 1024)
    std::cout << dram_size / 1024 << " GiB";
  else
    std::cout << dram_size << " MiB";
  std::cout << " Channels: " << DRAM_CHANNELS << " Width: " << 8 * DRAM_CHANNEL_WIDTH << "-bit Data Rate: " << DRAM_IO_FREQ << " MT/s" << std::endl;

  std::cout << std::endl;
  std::cout << "VirtualMemory physical capacity: " << std::size(vmem.ppage_free_list) * vmem.page_size;
  std::cout << " num_ppages: " << std::size(vmem.ppage_free_list) << std::endl;
  std::cout << "VirtualMemory page size: " << PAGE_SIZE << " log2_page_size: " << LOG2_PAGE_SIZE << std::endl;

  std::cout << std::endl;
#endif

#if USER_CODES == ENABLE
  for (auto i = start_position_of_traces; i < argc; i++)
  {
#if PRINT_STATISTICS_INTO_FILE == ENABLE
    fprintf(outputchampsimstatistics.trace_file, "CPU %ld runs %s\n", traces.size(), argv[i]);
#else
    std::cout << "CPU " << traces.size() << " runs " << argv[i] << std::endl;
#endif

    traces.push_back(get_tracereader(argv[i], traces.size(), knob_cloudsuite));

    if (traces.size() > NUM_CPUS)
    {
      printf("\n*** Too many traces for the configured number of cores ***\n\n");
      assert(0);
    }
  }
#else
  for (int i = optind; i < argc; i++)
  {
    std::cout << "CPU " << traces.size() << " runs " << argv[i] << std::endl;

    traces.push_back(get_tracereader(argv[i], traces.size(), knob_cloudsuite));

    if (traces.size() > NUM_CPUS)
    {
      printf("\n*** Too many traces for the configured number of cores ***\n\n");
      assert(0);
    }
  }
#endif

  if (traces.size() != NUM_CPUS)
  {
    printf("\n*** Not enough traces for the configured number of cores ***\n\n");
    assert(0);
  }
  // end trace file setup

  // SHARED CACHE
  for (O3_CPU* cpu : ooo_cpu)
  {
    cpu->initialize_core();
  }

  for (auto it = caches.rbegin(); it != caches.rend(); ++it)
  {
    (*it)->impl_prefetcher_initialize();
    (*it)->impl_replacement_initialize();
  }

  // simulation entry point
  while (std::any_of(std::begin(simulation_complete), std::end(simulation_complete), std::logical_not<uint8_t>()))
  {

    uint64_t elapsed_second = (uint64_t)(time(NULL) - start_time), elapsed_minute = elapsed_second / 60, elapsed_hour = elapsed_minute / 60;
    elapsed_minute -= elapsed_hour * 60;
    elapsed_second -= (elapsed_hour * 3600 + elapsed_minute * 60);

    for (auto op : operables)
    {
      try
      {
        op->_operate();
      }
      catch (champsim::deadlock& dl)
      {
        // ooo_cpu[dl.which]->print_deadlock();
        // std::cout << std::endl;
        // for (auto c : caches)
        for (auto c : operables)
        {
          c->print_deadlock();
          std::cout << std::endl;
        }

        abort();
      }
    }
    std::sort(std::begin(operables), std::end(operables), champsim::by_next_operate());

    for (std::size_t i = 0; i < ooo_cpu.size(); ++i)
    {
      // read from trace
      while (ooo_cpu[i]->fetch_stall == 0 && ooo_cpu[i]->instrs_to_read_this_cycle > 0)
      {
        ooo_cpu[i]->init_instruction(traces[i]->get());
      }

      // heartbeat information
      if (show_heartbeat && (ooo_cpu[i]->num_retired >= ooo_cpu[i]->next_print_instruction))
      {
        float cumulative_ipc;
        if (warmup_complete[i])
          cumulative_ipc = (1.0 * (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) / (ooo_cpu[i]->current_cycle - ooo_cpu[i]->begin_sim_cycle);
        else
          cumulative_ipc = (1.0 * ooo_cpu[i]->num_retired) / ooo_cpu[i]->current_cycle;
        float heartbeat_ipc = (1.0 * ooo_cpu[i]->num_retired - ooo_cpu[i]->last_sim_instr) / (ooo_cpu[i]->current_cycle - ooo_cpu[i]->last_sim_cycle);

#if PRINT_STATISTICS_INTO_FILE == ENABLE
        //fprintf(outputchampsimstatistics.trace_file, "Heartbeat CPU %ld instructions: %ld cycles: %ld", i, ooo_cpu[i]->num_retired, ooo_cpu[i]->current_cycle);
        //fprintf(outputchampsimstatistics.trace_file, " heartbeat IPC: %f cumulative IPC: %f", heartbeat_ipc, cumulative_ipc);
        //fprintf(outputchampsimstatistics.trace_file, " (Simulation time: %ld hr %ld min %ld sec) \n", elapsed_hour, elapsed_minute, elapsed_second);
        cout << "Heartbeat CPU " << i << " instructions: " << ooo_cpu[i]->num_retired << " cycles: " << ooo_cpu[i]->current_cycle;
        cout << " heartbeat IPC: " << heartbeat_ipc << " cumulative IPC: " << cumulative_ipc;
        cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << endl;
#else
        cout << "Heartbeat CPU " << i << " instructions: " << ooo_cpu[i]->num_retired << " cycles: " << ooo_cpu[i]->current_cycle;
        cout << " heartbeat IPC: " << heartbeat_ipc << " cumulative IPC: " << cumulative_ipc;
        cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << endl;
#endif

        ooo_cpu[i]->next_print_instruction += STAT_PRINTING_PERIOD;

        ooo_cpu[i]->last_sim_instr = ooo_cpu[i]->num_retired;
        ooo_cpu[i]->last_sim_cycle = ooo_cpu[i]->current_cycle;
      }

      // check for warmup
      // warmup complete
      if ((warmup_complete[i] == 0) && (ooo_cpu[i]->num_retired > warmup_instructions))
      {
        warmup_complete[i] = 1;
        all_warmup_complete++;
      }
      if (all_warmup_complete == NUM_CPUS)
      { // this part is called only once
       // when all cores are warmed up
        all_warmup_complete++;
        finish_warmup();
      }

      // simulation complete
      if ((all_warmup_complete > NUM_CPUS) && (simulation_complete[i] == 0)
          && (ooo_cpu[i]->num_retired >= (ooo_cpu[i]->begin_sim_instr + simulation_instructions)))
      {
        simulation_complete[i] = 1;
        ooo_cpu[i]->finish_sim_instr = ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr;
        ooo_cpu[i]->finish_sim_cycle = ooo_cpu[i]->current_cycle - ooo_cpu[i]->begin_sim_cycle;

#if PRINT_STATISTICS_INTO_FILE == ENABLE
        fprintf(outputchampsimstatistics.trace_file, "Finished CPU %ld instructions: %ld cycles: %ld", i, ooo_cpu[i]->finish_sim_instr, ooo_cpu[i]->finish_sim_cycle);
        fprintf(outputchampsimstatistics.trace_file, " cumulative IPC: %f", ((float)ooo_cpu[i]->finish_sim_instr / ooo_cpu[i]->finish_sim_cycle));
        fprintf(outputchampsimstatistics.trace_file, " (Simulation time: %ld hr %ld min %ld sec) \n", elapsed_hour, elapsed_minute, elapsed_second);
#else
        cout << "Finished CPU " << i << " instructions: " << ooo_cpu[i]->finish_sim_instr << " cycles: " << ooo_cpu[i]->finish_sim_cycle;
        cout << " cumulative IPC: " << ((float)ooo_cpu[i]->finish_sim_instr / ooo_cpu[i]->finish_sim_cycle);
        cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << endl;
#endif

        for (auto it = caches.rbegin(); it != caches.rend(); ++it)
          record_roi_stats(i, *it);
      }
    }
  }

  uint64_t elapsed_second = (uint64_t)(time(NULL) - start_time), elapsed_minute = elapsed_second / 60, elapsed_hour = elapsed_minute / 60;
  elapsed_minute -= elapsed_hour * 60;
  elapsed_second -= (elapsed_hour * 3600 + elapsed_minute * 60);

#if PRINT_STATISTICS_INTO_FILE == ENABLE
  fprintf(outputchampsimstatistics.trace_file, "\nChampSim completed all CPUs\n");
  if (NUM_CPUS > 1)
  {
    fprintf(outputchampsimstatistics.trace_file, "\nTotal Simulation Statistics (not including warmup)\n");
    for (uint32_t i = 0; i < NUM_CPUS; i++)
    {
      fprintf(outputchampsimstatistics.trace_file, "\nCPU %d cumulative IPC: %f", i, (float)(ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr) / (ooo_cpu[i]->current_cycle - ooo_cpu[i]->begin_sim_cycle));
      fprintf(outputchampsimstatistics.trace_file, " instructions: %ld cycles: %ld\n", ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr, ooo_cpu[i]->current_cycle - ooo_cpu[i]->begin_sim_cycle);
      for (auto it = caches.rbegin(); it != caches.rend(); ++it)
        print_sim_stats(i, *it);
    }
  }

  fprintf(outputchampsimstatistics.trace_file, "\nRegion of Interest Statistics\n");
  for (uint32_t i = 0; i < NUM_CPUS; i++)
  {
    fprintf(outputchampsimstatistics.trace_file, "\nCPU %d cumulative IPC: %f", i, ((float)ooo_cpu[i]->finish_sim_instr / ooo_cpu[i]->finish_sim_cycle));
    fprintf(outputchampsimstatistics.trace_file, " instructions: %ld cycles: %ld\n", ooo_cpu[i]->finish_sim_instr, ooo_cpu[i]->finish_sim_cycle);
    for (auto it = caches.rbegin(); it != caches.rend(); ++it)
      print_roi_stats(i, *it);
  }
#else
  cout << endl << "ChampSim completed all CPUs" << endl;
  if (NUM_CPUS > 1)
  {
    cout << endl << "Total Simulation Statistics (not including warmup)" << endl;
    for (uint32_t i = 0; i < NUM_CPUS; i++)
    {
      cout << endl
        << "CPU " << i
        << " cumulative IPC: " << (float)(ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr) / (ooo_cpu[i]->current_cycle - ooo_cpu[i]->begin_sim_cycle);
      cout << " instructions: " << ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr
        << " cycles: " << ooo_cpu[i]->current_cycle - ooo_cpu[i]->begin_sim_cycle << endl;
      for (auto it = caches.rbegin(); it != caches.rend(); ++it)
        print_sim_stats(i, *it);
    }
  }

  cout << endl << "Region of Interest Statistics" << endl;
  for (uint32_t i = 0; i < NUM_CPUS; i++)
  {
    cout << endl << "CPU " << i << " cumulative IPC: " << ((float)ooo_cpu[i]->finish_sim_instr / ooo_cpu[i]->finish_sim_cycle);
    cout << " instructions: " << ooo_cpu[i]->finish_sim_instr << " cycles: " << ooo_cpu[i]->finish_sim_cycle << endl;
    for (auto it = caches.rbegin(); it != caches.rend(); ++it)
      print_roi_stats(i, *it);
  }
#endif

  for (auto it = caches.rbegin(); it != caches.rend(); ++it)
    (*it)->impl_prefetcher_final_stats();

  for (auto it = caches.rbegin(); it != caches.rend(); ++it)
    (*it)->impl_replacement_final_stats();

#ifndef CRC2_COMPILE
#if USER_CODES == ENABLE
  print_memory_stats();
#else
  print_dram_stats();
#endif

  print_branch_stats();
#endif

#if PRINT_STATISTICS_INTO_FILE == ENABLE
  output_champsim_statistics_deinitialization(outputchampsimstatistics);
#endif

  return 0;
}

uint64_t champsim::deprecated_clock_cycle::operator[](std::size_t cpu_idx)
{
  static bool deprecate_printed = false;
  if (!deprecate_printed)
  {
    std::cout << "WARNING: The use of 'current_core_cycle[cpu]' is deprecated." << std::endl;
    std::cout << "WARNING: Use 'this->current_cycle' instead." << std::endl;
    deprecate_printed = true;
  }
  return ooo_cpu[cpu_idx]->current_cycle;
}

void record_roi_stats(uint32_t cpu, CACHE* cache)
{
  for (uint32_t i = 0; i < NUM_TYPES; i++)
  {
    cache->roi_access[cpu][i] = cache->sim_access[cpu][i];
    cache->roi_hit[cpu][i] = cache->sim_hit[cpu][i];
    cache->roi_miss[cpu][i] = cache->sim_miss[cpu][i];
  }
}

void print_roi_stats(uint32_t cpu, CACHE* cache)
{
  uint64_t TOTAL_ACCESS = 0, TOTAL_HIT = 0, TOTAL_MISS = 0;

  for (uint32_t i = 0; i < NUM_TYPES; i++)
  {
    TOTAL_ACCESS += cache->roi_access[cpu][i];
    TOTAL_HIT += cache->roi_hit[cpu][i];
    TOTAL_MISS += cache->roi_miss[cpu][i];
  }

  if (TOTAL_ACCESS > 0)
  {
#if PRINT_STATISTICS_INTO_FILE == ENABLE
    fprintf(outputchampsimstatistics.trace_file, "%s TOTAL     ACCESS: %10ld  HIT: %10ld  MISS: %10ld\n", cache->NAME.c_str(), TOTAL_ACCESS, TOTAL_HIT, TOTAL_MISS);

    fprintf(outputchampsimstatistics.trace_file, "%s LOAD      ACCESS: %10ld  HIT: %10ld  MISS: %10ld\n", cache->NAME.c_str(), cache->roi_access[cpu][0], cache->roi_hit[cpu][0], cache->roi_miss[cpu][0]);

    fprintf(outputchampsimstatistics.trace_file, "%s RFO       ACCESS: %10ld  HIT: %10ld  MISS: %10ld\n", cache->NAME.c_str(), cache->roi_access[cpu][1], cache->roi_hit[cpu][1], cache->roi_miss[cpu][1]);

    fprintf(outputchampsimstatistics.trace_file, "%s PREFETCH  ACCESS: %10ld  HIT: %10ld  MISS: %10ld\n", cache->NAME.c_str(), cache->roi_access[cpu][2], cache->roi_hit[cpu][2], cache->roi_miss[cpu][2]);

    fprintf(outputchampsimstatistics.trace_file, "%s WRITEBACK ACCESS: %10ld  HIT: %10ld  MISS: %10ld\n", cache->NAME.c_str(), cache->roi_access[cpu][3], cache->roi_hit[cpu][3], cache->roi_miss[cpu][3]);

    fprintf(outputchampsimstatistics.trace_file, "%s TRANSLATION ACCESS: %10ld  HIT: %10ld  MISS: %10ld\n", cache->NAME.c_str(), cache->roi_access[cpu][4], cache->roi_hit[cpu][4], cache->roi_miss[cpu][4]);

    fprintf(outputchampsimstatistics.trace_file, "%s PREFETCH  REQUESTED: %10ld  ISSUED: %10ld  USEFUL: %10ld  USELESS: %10ld\n", cache->NAME.c_str(), cache->pf_requested, cache->pf_issued, cache->pf_useful, cache->pf_useless);

    fprintf(outputchampsimstatistics.trace_file, "%s AVERAGE MISS LATENCY: %f cycles\n", cache->NAME.c_str(), (1.0 * (cache->total_miss_latency)) / TOTAL_MISS);
#else
    cout << cache->NAME;
    cout << " TOTAL     ACCESS: " << setw(10) << TOTAL_ACCESS << "  HIT: " << setw(10) << TOTAL_HIT << "  MISS: " << setw(10) << TOTAL_MISS << endl;

    cout << cache->NAME;
    cout << " LOAD      ACCESS: " << setw(10) << cache->roi_access[cpu][0] << "  HIT: " << setw(10) << cache->roi_hit[cpu][0] << "  MISS: " << setw(10)
      << cache->roi_miss[cpu][0] << endl;

    cout << cache->NAME;
    cout << " RFO       ACCESS: " << setw(10) << cache->roi_access[cpu][1] << "  HIT: " << setw(10) << cache->roi_hit[cpu][1] << "  MISS: " << setw(10)
      << cache->roi_miss[cpu][1] << endl;

    cout << cache->NAME;
    cout << " PREFETCH  ACCESS: " << setw(10) << cache->roi_access[cpu][2] << "  HIT: " << setw(10) << cache->roi_hit[cpu][2] << "  MISS: " << setw(10)
      << cache->roi_miss[cpu][2] << endl;

    cout << cache->NAME;
    cout << " WRITEBACK ACCESS: " << setw(10) << cache->roi_access[cpu][3] << "  HIT: " << setw(10) << cache->roi_hit[cpu][3] << "  MISS: " << setw(10)
      << cache->roi_miss[cpu][3] << endl;

    cout << cache->NAME;
    cout << " TRANSLATION ACCESS: " << setw(10) << cache->roi_access[cpu][4] << "  HIT: " << setw(10) << cache->roi_hit[cpu][4] << "  MISS: " << setw(10)
      << cache->roi_miss[cpu][4] << endl;

    cout << cache->NAME;
    cout << " PREFETCH  REQUESTED: " << setw(10) << cache->pf_requested << "  ISSUED: " << setw(10) << cache->pf_issued;
    cout << "  USEFUL: " << setw(10) << cache->pf_useful << "  USELESS: " << setw(10) << cache->pf_useless << endl;

    cout << cache->NAME;
    cout << " AVERAGE MISS LATENCY: " << (1.0 * (cache->total_miss_latency)) / TOTAL_MISS << " cycles" << endl;
#endif
    // cout << " AVERAGE MISS LATENCY: " <<
    // (cache->total_miss_latency)/TOTAL_MISS << " cycles " <<
    // cache->total_miss_latency << "/" << TOTAL_MISS<< endl;
  }
}

void print_sim_stats(uint32_t cpu, CACHE* cache)
{
  uint64_t TOTAL_ACCESS = 0, TOTAL_HIT = 0, TOTAL_MISS = 0;

  for (uint32_t i = 0; i < NUM_TYPES; i++)
  {
    TOTAL_ACCESS += cache->sim_access[cpu][i];
    TOTAL_HIT += cache->sim_hit[cpu][i];
    TOTAL_MISS += cache->sim_miss[cpu][i];
  }

  if (TOTAL_ACCESS > 0)
  {
#if PRINT_STATISTICS_INTO_FILE == ENABLE
    fprintf(outputchampsimstatistics.trace_file, "%s TOTAL     ACCESS: %10ld  HIT: %10ld  MISS: %10ld\n", cache->NAME.c_str(), TOTAL_ACCESS, TOTAL_HIT, TOTAL_MISS);

    fprintf(outputchampsimstatistics.trace_file, "%s LOAD      ACCESS: %10ld  HIT: %10ld  MISS: %10ld\n", cache->NAME.c_str(), cache->sim_access[cpu][0], cache->sim_hit[cpu][0], cache->sim_miss[cpu][0]);

    fprintf(outputchampsimstatistics.trace_file, "%s RFO       ACCESS: %10ld  HIT: %10ld  MISS: %10ld\n", cache->NAME.c_str(), cache->sim_access[cpu][1], cache->sim_hit[cpu][1], cache->sim_miss[cpu][1]);

    fprintf(outputchampsimstatistics.trace_file, "%s PREFETCH  ACCESS: %10ld  HIT: %10ld  MISS: %10ld\n", cache->NAME.c_str(), cache->sim_access[cpu][2], cache->sim_hit[cpu][2], cache->sim_miss[cpu][2]);

    fprintf(outputchampsimstatistics.trace_file, "%s WRITEBACK ACCESS: %10ld  HIT: %10ld  MISS: %10ld\n", cache->NAME.c_str(), cache->sim_access[cpu][3], cache->sim_hit[cpu][3], cache->sim_miss[cpu][3]);
#else
    cout << cache->NAME;
    cout << " TOTAL     ACCESS: " << setw(10) << TOTAL_ACCESS << "  HIT: " << setw(10) << TOTAL_HIT << "  MISS: " << setw(10) << TOTAL_MISS << endl;

    cout << cache->NAME;
    cout << " LOAD      ACCESS: " << setw(10) << cache->sim_access[cpu][0] << "  HIT: " << setw(10) << cache->sim_hit[cpu][0] << "  MISS: " << setw(10)
      << cache->sim_miss[cpu][0] << endl;

    cout << cache->NAME;
    cout << " RFO       ACCESS: " << setw(10) << cache->sim_access[cpu][1] << "  HIT: " << setw(10) << cache->sim_hit[cpu][1] << "  MISS: " << setw(10)
      << cache->sim_miss[cpu][1] << endl;

    cout << cache->NAME;
    cout << " PREFETCH  ACCESS: " << setw(10) << cache->sim_access[cpu][2] << "  HIT: " << setw(10) << cache->sim_hit[cpu][2] << "  MISS: " << setw(10)
      << cache->sim_miss[cpu][2] << endl;

    cout << cache->NAME;
    cout << " WRITEBACK ACCESS: " << setw(10) << cache->sim_access[cpu][3] << "  HIT: " << setw(10) << cache->sim_hit[cpu][3] << "  MISS: " << setw(10)
      << cache->sim_miss[cpu][3] << endl;
#endif
  }
}

void print_branch_stats()
{
  for (uint32_t i = 0; i < NUM_CPUS; i++)
  {
#if PRINT_STATISTICS_INTO_FILE == ENABLE
    fprintf(outputchampsimstatistics.trace_file, "\nCPU %d Branch Prediction Accuracy: %f", i, (100.0 * (ooo_cpu[i]->num_branch - ooo_cpu[i]->branch_mispredictions)) / ooo_cpu[i]->num_branch);
    fprintf(outputchampsimstatistics.trace_file, "%% MPKI: %f", (1000.0 * ooo_cpu[i]->branch_mispredictions) / (ooo_cpu[i]->num_retired - warmup_instructions));
    fprintf(outputchampsimstatistics.trace_file, " Average ROB Occupancy at Mispredict: %f\n", (1.0 * ooo_cpu[i]->total_rob_occupancy_at_branch_mispredict) / ooo_cpu[i]->branch_mispredictions);

    fprintf(outputchampsimstatistics.trace_file, "Branch type MPKI\nBRANCH_DIRECT_JUMP: %f\n", (1000.0 * ooo_cpu[i]->branch_type_misses[1] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)));
    fprintf(outputchampsimstatistics.trace_file, "BRANCH_INDIRECT: %f\n", (1000.0 * ooo_cpu[i]->branch_type_misses[2] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)));
    fprintf(outputchampsimstatistics.trace_file, "BRANCH_CONDITIONAL: %f\n", (1000.0 * ooo_cpu[i]->branch_type_misses[3] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)));
    fprintf(outputchampsimstatistics.trace_file, "BRANCH_DIRECT_CALL: %f\n", (1000.0 * ooo_cpu[i]->branch_type_misses[4] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)));
    fprintf(outputchampsimstatistics.trace_file, "BRANCH_INDIRECT_CALL: %f\n", (1000.0 * ooo_cpu[i]->branch_type_misses[5] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)));
    fprintf(outputchampsimstatistics.trace_file, "BRANCH_RETURN: %f\n\n", (1000.0 * ooo_cpu[i]->branch_type_misses[6] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)));
#else
    cout << endl << "CPU " << i << " Branch Prediction Accuracy: ";
    cout << (100.0 * (ooo_cpu[i]->num_branch - ooo_cpu[i]->branch_mispredictions)) / ooo_cpu[i]->num_branch;
    cout << "% MPKI: " << (1000.0 * ooo_cpu[i]->branch_mispredictions) / (ooo_cpu[i]->num_retired - warmup_instructions);
    cout << " Average ROB Occupancy at Mispredict: " << (1.0 * ooo_cpu[i]->total_rob_occupancy_at_branch_mispredict) / ooo_cpu[i]->branch_mispredictions
      << endl;

    /*
    cout << "Branch types" << endl;
    cout << "NOT_BRANCH: " << ooo_cpu[i]->total_branch_types[0] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[0])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl; cout << "BRANCH_DIRECT_JUMP: "
    << ooo_cpu[i]->total_branch_types[1] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[1])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl; cout << "BRANCH_INDIRECT: " <<
    ooo_cpu[i]->total_branch_types[2] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[2])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl; cout << "BRANCH_CONDITIONAL: "
    << ooo_cpu[i]->total_branch_types[3] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[3])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl; cout << "BRANCH_DIRECT_CALL: "
    << ooo_cpu[i]->total_branch_types[4] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[4])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl; cout << "BRANCH_INDIRECT_CALL:
    " << ooo_cpu[i]->total_branch_types[5] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[5])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl; cout << "BRANCH_RETURN: " <<
    ooo_cpu[i]->total_branch_types[6] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[6])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl; cout << "BRANCH_OTHER: " <<
    ooo_cpu[i]->total_branch_types[7] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[7])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl << endl;
    */

    cout << "Branch type MPKI" << endl;
    cout << "BRANCH_DIRECT_JUMP: " << (1000.0 * ooo_cpu[i]->branch_type_misses[1] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << endl;
    cout << "BRANCH_INDIRECT: " << (1000.0 * ooo_cpu[i]->branch_type_misses[2] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << endl;
    cout << "BRANCH_CONDITIONAL: " << (1000.0 * ooo_cpu[i]->branch_type_misses[3] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << endl;
    cout << "BRANCH_DIRECT_CALL: " << (1000.0 * ooo_cpu[i]->branch_type_misses[4] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << endl;
    cout << "BRANCH_INDIRECT_CALL: " << (1000.0 * ooo_cpu[i]->branch_type_misses[5] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << endl;
    cout << "BRANCH_RETURN: " << (1000.0 * ooo_cpu[i]->branch_type_misses[6] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << endl << endl;
#endif
  }
}

#if USER_CODES == ENABLE
void print_memory_stats()
{
  uint64_t total_congested_cycle = 0;
  uint64_t total_congested_count = 0;

  // For DDR Statistics
#if PRINT_STATISTICS_INTO_FILE == ENABLE
  fprintf(outputchampsimstatistics.trace_file, "\nDRAM Statistics\n");
#else
  std::cout << std::endl << "DRAM Statistics" << std::endl;
#endif
  for (uint32_t i = 0; i < DDR_CHANNELS; i++)
  {

    auto& channel = memory.ddr_channels[i];
#if PRINT_STATISTICS_INTO_FILE == ENABLE
    fprintf(outputchampsimstatistics.trace_file, " CHANNEL %d\n", i);
    fprintf(outputchampsimstatistics.trace_file, " RQ ROW_BUFFER_HIT: %10d  ROW_BUFFER_MISS: %10d\n", channel.RQ_ROW_BUFFER_HIT, channel.RQ_ROW_BUFFER_MISS);

    fprintf(outputchampsimstatistics.trace_file, " DBUS AVG_CONGESTED_CYCLE: ");
    if (channel.dbus_count_congested)
      fprintf(outputchampsimstatistics.trace_file, "%10f\n", ((double)channel.dbus_cycle_congested / channel.dbus_count_congested));
    else
      fprintf(outputchampsimstatistics.trace_file, "-\n");

    fprintf(outputchampsimstatistics.trace_file, " WQ ROW_BUFFER_HIT: %10d  ROW_BUFFER_MISS: %10d  FULL: %10d\n\n", channel.WQ_ROW_BUFFER_HIT, channel.WQ_ROW_BUFFER_MISS, channel.WQ_FULL);
#else
    std::cout << " CHANNEL " << i << std::endl;
    std::cout << " RQ ROW_BUFFER_HIT: " << std::setw(10) << channel.RQ_ROW_BUFFER_HIT << " ";
    std::cout << " ROW_BUFFER_MISS: " << std::setw(10) << channel.RQ_ROW_BUFFER_MISS;
    std::cout << std::endl;

    std::cout << " DBUS AVG_CONGESTED_CYCLE: ";
    if (channel.dbus_count_congested)
      std::cout << std::setw(10) << ((double)channel.dbus_cycle_congested / channel.dbus_count_congested);
    else
      std::cout << "-";
    std::cout << std::endl;

    std::cout << " WQ ROW_BUFFER_HIT: " << std::setw(10) << channel.WQ_ROW_BUFFER_HIT << " ";
    std::cout << " ROW_BUFFER_MISS: " << std::setw(10) << channel.WQ_ROW_BUFFER_MISS << " ";
    std::cout << " FULL: " << std::setw(10) << channel.WQ_FULL;
    std::cout << std::endl;

    std::cout << std::endl;
#endif

    total_congested_cycle += channel.dbus_cycle_congested;
    total_congested_count += channel.dbus_count_congested;
  }

  if (DDR_CHANNELS > 1)
  {
#if PRINT_STATISTICS_INTO_FILE == ENABLE
    fprintf(outputchampsimstatistics.trace_file, " DBUS AVG_CONGESTED_CYCLE: ");
    if (total_congested_count)
      fprintf(outputchampsimstatistics.trace_file, "%10f\n", ((double)total_congested_cycle / total_congested_count));
    else
      fprintf(outputchampsimstatistics.trace_file, "-\n");
#else
    std::cout << " DBUS AVG_CONGESTED_CYCLE: ";
    if (total_congested_count)
      std::cout << std::setw(10) << ((double)total_congested_cycle / total_congested_count);
    else
      std::cout << "-";

    std::cout << std::endl;
#endif
  }

  total_congested_cycle = 0;
  total_congested_count = 0;

  // For HBM Statistics
#if PRINT_STATISTICS_INTO_FILE == ENABLE
  fprintf(outputchampsimstatistics.trace_file, "\nHBM Statistics\n");
#else
  std::cout << std::endl << "HBM Statistics" << std::endl;
#endif
  for (uint32_t i = 0; i < HBM_CHANNELS; i++)
  {
    auto& channel = memory.hbm_channels[i];
#if PRINT_STATISTICS_INTO_FILE == ENABLE
    fprintf(outputchampsimstatistics.trace_file, " CHANNEL %d\n", i);
    fprintf(outputchampsimstatistics.trace_file, " RQ ROW_BUFFER_HIT: %10d  ROW_BUFFER_MISS: %10d\n", channel.RQ_ROW_BUFFER_HIT, channel.RQ_ROW_BUFFER_MISS);

    fprintf(outputchampsimstatistics.trace_file, " DBUS AVG_CONGESTED_CYCLE: ");
    if (channel.dbus_count_congested)
      fprintf(outputchampsimstatistics.trace_file, "%10f\n", ((double)channel.dbus_cycle_congested / channel.dbus_count_congested));
    else
      fprintf(outputchampsimstatistics.trace_file, "-\n");

    fprintf(outputchampsimstatistics.trace_file, " WQ ROW_BUFFER_HIT: %10d  ROW_BUFFER_MISS: %10d  FULL: %10d\n\n", channel.WQ_ROW_BUFFER_HIT, channel.WQ_ROW_BUFFER_MISS, channel.WQ_FULL);
#else
    std::cout << " CHANNEL " << i << std::endl;
    std::cout << " RQ ROW_BUFFER_HIT: " << std::setw(10) << channel.RQ_ROW_BUFFER_HIT << " ";
    std::cout << " ROW_BUFFER_MISS: " << std::setw(10) << channel.RQ_ROW_BUFFER_MISS;
    std::cout << std::endl;

    std::cout << " DBUS AVG_CONGESTED_CYCLE: ";
    if (channel.dbus_count_congested)
      std::cout << std::setw(10) << ((double)channel.dbus_cycle_congested / channel.dbus_count_congested);
    else
      std::cout << "-";
    std::cout << std::endl;

    std::cout << " WQ ROW_BUFFER_HIT: " << std::setw(10) << channel.WQ_ROW_BUFFER_HIT << " ";
    std::cout << " ROW_BUFFER_MISS: " << std::setw(10) << channel.WQ_ROW_BUFFER_MISS << " ";
    std::cout << " FULL: " << std::setw(10) << channel.WQ_FULL;
    std::cout << std::endl;

    std::cout << std::endl;
#endif

    total_congested_cycle += channel.dbus_cycle_congested;
    total_congested_count += channel.dbus_count_congested;
  }

  if (HBM_CHANNELS > 1)
  {
#if PRINT_STATISTICS_INTO_FILE == ENABLE
    fprintf(outputchampsimstatistics.trace_file, " DBUS AVG_CONGESTED_CYCLE: ");
    if (total_congested_count)
      fprintf(outputchampsimstatistics.trace_file, "%10f\n", ((double)total_congested_cycle / total_congested_count));
    else
      fprintf(outputchampsimstatistics.trace_file, "-\n");
#else
    std::cout << " DBUS AVG_CONGESTED_CYCLE: ";
    if (total_congested_count)
      std::cout << std::setw(10) << ((double)total_congested_cycle / total_congested_count);
    else
      std::cout << "-";

    std::cout << std::endl;
#endif
  }

  // Output memory statistics
#if PRINT_STATISTICS_INTO_FILE == ENABLE
  fprintf(outputchampsimstatistics.trace_file, "\nAverage memory access time: %10f cycle.\n", memory.get_average_memory_access_time());
#else
  std::cout << "Average memory access time: " << std::setw(10) << memory.get_average_memory_access_time() << " cycle. " << std::endl;
#endif

  output_memory_trace_deinitialization(outputmemorytrace_one);

}

void print_configuration_details()
{
#if PRINT_STATISTICS_INTO_FILE == ENABLE
  fprintf(outputchampsimstatistics.trace_file, "Warmup Instructions: %ld\n", warmup_instructions);
  fprintf(outputchampsimstatistics.trace_file, "Simulation Instructions: %ld\n", simulation_instructions);
  fprintf(outputchampsimstatistics.trace_file, "Number of CPUs: %ld\n", NUM_CPUS);

  uint64_t ddr_address_space = DDR_CHANNELS * DDR_RANKS * DDR_BANKS * DDR_ROWS * DDR_COLUMNS * BLOCK_SIZE / 1024 / 1024; // in MB
  uint64_t ddr_capacity = DDR_CAPACITY / 1024 / 1024; // in MB

  fprintf(outputchampsimstatistics.trace_file, "Off-chip DDR Address Space: ");

  if (ddr_address_space >= 1024)
    fprintf(outputchampsimstatistics.trace_file, "%ld GB", ddr_address_space / 1024);
  else
    fprintf(outputchampsimstatistics.trace_file, "%ld MB", ddr_address_space);

  fprintf(outputchampsimstatistics.trace_file, ", Capacity: ");
  if (ddr_capacity >= 1024)
    fprintf(outputchampsimstatistics.trace_file, "%ld GB", ddr_capacity / 1024);
  else
    fprintf(outputchampsimstatistics.trace_file, "%ld MB", ddr_capacity);

  fprintf(outputchampsimstatistics.trace_file, ", Channels: %ld, Width: %ld-bit, Data Rate: %ld MT/s\n", DDR_CHANNELS, 8 * DRAM_CHANNEL_WIDTH, DRAM_IO_FREQ);

  uint64_t hbm_address_space = HBM_CHANNELS * HBM_BANKS * HBM_ROWS * HBM_COLUMNS * BLOCK_SIZE / 1024 / 1024; // in MB
  uint64_t hbm_capacity = HBM_CAPACITY / 1024 / 1024; // in MB
  fprintf(outputchampsimstatistics.trace_file, "On-chip HBM Address Space: ");
  if (hbm_address_space >= 1024)
    fprintf(outputchampsimstatistics.trace_file, "%ld GB", hbm_address_space / 1024);
  else
    fprintf(outputchampsimstatistics.trace_file, "%ld MB", hbm_address_space);

  fprintf(outputchampsimstatistics.trace_file, ", Capacity: ");
  if (hbm_capacity >= 1024)
    fprintf(outputchampsimstatistics.trace_file, "%ld GB", hbm_capacity / 1024);
  else
    fprintf(outputchampsimstatistics.trace_file, "%ld MB", hbm_capacity);

  fprintf(outputchampsimstatistics.trace_file, ", Channels: %ld, Width: %ld-bit, Data Rate: %ld MT/s\n", HBM_CHANNELS, 8 * DRAM_CHANNEL_WIDTH, DRAM_IO_FREQ);

  fprintf(outputchampsimstatistics.trace_file, "\nVirtualMemory physical capacity: %ld", std::size(vmem.ppage_free_list) * vmem.page_size);
  fprintf(outputchampsimstatistics.trace_file, ", num_ppages: %ld\n", std::size(vmem.ppage_free_list));
  fprintf(outputchampsimstatistics.trace_file, "VirtualMemory page size: %ld, log2_page_size: %d\n\n", PAGE_SIZE, LOG2_PAGE_SIZE);
#else
  std::cout << "Warmup Instructions: " << warmup_instructions << std::endl;
  std::cout << "Simulation Instructions: " << simulation_instructions << std::endl;
  std::cout << "Number of CPUs: " << NUM_CPUS << std::endl;

  uint64_t ddr_address_space = DDR_CHANNELS * DDR_RANKS * DDR_BANKS * DDR_ROWS * DDR_COLUMNS * BLOCK_SIZE / 1024 / 1024; // in MB
  uint64_t ddr_capacity = DDR_CAPACITY / 1024 / 1024; // in MB

  std::cout << "Off-chip DDR Address Space: ";

  if (ddr_address_space >= 1024)
    std::cout << ddr_address_space / 1024 << " GB";
  else
    std::cout << ddr_address_space << " MB";

  std::cout << ", Capacity: ";
  if (ddr_capacity >= 1024)
    std::cout << ddr_capacity / 1024 << " GB";
  else
    std::cout << ddr_capacity << " MB";
  std::cout << ", Channels: " << DDR_CHANNELS << ", Width: " << 8 * DRAM_CHANNEL_WIDTH << "-bit, Data Rate: " << DRAM_IO_FREQ << " MT/s" << std::endl;

  uint64_t hbm_address_space = HBM_CHANNELS * HBM_BANKS * HBM_ROWS * HBM_COLUMNS * BLOCK_SIZE / 1024 / 1024; // in MB
  uint64_t hbm_capacity = HBM_CAPACITY / 1024 / 1024; // in MB
  std::cout << "On-chip HBM Address Space: ";
  if (hbm_address_space >= 1024)
    std::cout << hbm_address_space / 1024 << " GB";
  else
    std::cout << hbm_address_space << " MB";

  std::cout << ", Capacity: ";
  if (hbm_capacity >= 1024)
    std::cout << hbm_capacity / 1024 << " GB";
  else
    std::cout << hbm_capacity << " MB";
  std::cout << ", Channels: " << HBM_CHANNELS << ", Width: " << 8 * DRAM_CHANNEL_WIDTH << "-bit, Data Rate: " << DRAM_IO_FREQ << " MT/s" << std::endl;

  std::cout << std::endl;
  std::cout << "VirtualMemory physical capacity: " << std::size(vmem.ppage_free_list) * vmem.page_size;
  std::cout << ", num_ppages: " << std::size(vmem.ppage_free_list) << std::endl;
  std::cout << "VirtualMemory page size: " << PAGE_SIZE << ", log2_page_size: " << LOG2_PAGE_SIZE << std::endl;

  std::cout << std::endl;
#endif
}

#else
void print_dram_stats()
{
  uint64_t total_congested_cycle = 0;
  uint64_t total_congested_count = 0;

  std::cout << std::endl;
  std::cout << "DRAM Statistics" << std::endl;
  for (uint32_t i = 0; i < DRAM_CHANNELS; i++)
  {
    std::cout << " CHANNEL " << i << std::endl;

    auto& channel = memory.dram_channels[i];
    std::cout << " RQ ROW_BUFFER_HIT: " << std::setw(10) << channel.RQ_ROW_BUFFER_HIT << " ";
    std::cout << " ROW_BUFFER_MISS: " << std::setw(10) << channel.RQ_ROW_BUFFER_MISS;
    std::cout << std::endl;

    std::cout << " DBUS AVG_CONGESTED_CYCLE: ";
    if (channel.dbus_count_congested)
      std::cout << std::setw(10) << ((double)channel.dbus_cycle_congested / channel.dbus_count_congested);
    else
      std::cout << "-";
    std::cout << std::endl;

    std::cout << " WQ ROW_BUFFER_HIT: " << std::setw(10) << channel.WQ_ROW_BUFFER_HIT << " ";
    std::cout << " ROW_BUFFER_MISS: " << std::setw(10) << channel.WQ_ROW_BUFFER_MISS << " ";
    std::cout << " FULL: " << std::setw(10) << channel.WQ_FULL;
    std::cout << std::endl;

    std::cout << std::endl;

    total_congested_cycle += channel.dbus_cycle_congested;
    total_congested_count += channel.dbus_count_congested;
  }



  if (DRAM_CHANNELS > 1)
  {
    std::cout << " DBUS AVG_CONGESTED_CYCLE: ";
    if (total_congested_count)
      std::cout << std::setw(10) << ((double)total_congested_cycle / total_congested_count);
    else
      std::cout << "-";

    std::cout << std::endl;
  }
}
#endif

void reset_cache_stats(uint32_t cpu, CACHE* cache)
{
  for (uint32_t i = 0; i < NUM_TYPES; i++)
  {
    cache->sim_access[cpu][i] = 0;
    cache->sim_hit[cpu][i] = 0;
    cache->sim_miss[cpu][i] = 0;
  }

  cache->pf_requested = 0;
  cache->pf_issued = 0;
  cache->pf_useful = 0;
  cache->pf_useless = 0;
  cache->pf_fill = 0;

  cache->total_miss_latency = 0;

  cache->RQ_ACCESS = 0;
  cache->RQ_MERGED = 0;
  cache->RQ_TO_CACHE = 0;

  cache->WQ_ACCESS = 0;
  cache->WQ_MERGED = 0;
  cache->WQ_TO_CACHE = 0;
  cache->WQ_FORWARD = 0;
  cache->WQ_FULL = 0;
}

void finish_warmup()
{
  uint64_t elapsed_second = (uint64_t)(time(NULL) - start_time), elapsed_minute = elapsed_second / 60, elapsed_hour = elapsed_minute / 60;
  elapsed_minute -= elapsed_hour * 60;
  elapsed_second -= (elapsed_hour * 3600 + elapsed_minute * 60);

  // reset core latency
  // note: since re-ordering he function calls in the main simulation loop, it's
  // no longer necessary to add
  //       extra latency for scheduling and execution, unless you want these
  //       steps to take longer than 1 cycle.
  // PAGE_TABLE_LATENCY = 100;
  // SWAP_LATENCY = 100000;

#if PRINT_STATISTICS_INTO_FILE == ENABLE
  fprintf(outputchampsimstatistics.trace_file, "\n");
#else
  cout << endl;
#endif
  for (uint32_t i = 0; i < NUM_CPUS; i++)
  {
#if PRINT_STATISTICS_INTO_FILE == ENABLE
    fprintf(outputchampsimstatistics.trace_file, "Warmup complete CPU %d instructions: %ld cycles: %ld", i, ooo_cpu[i]->num_retired, ooo_cpu[i]->current_cycle);
    fprintf(outputchampsimstatistics.trace_file, " (Simulation time: %ld hr %ld min %ld sec) \n", elapsed_hour, elapsed_minute, elapsed_second);
#else
    cout << "Warmup complete CPU " << i << " instructions: " << ooo_cpu[i]->num_retired << " cycles: " << ooo_cpu[i]->current_cycle;
    cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << endl;
#endif

    ooo_cpu[i]->begin_sim_cycle = ooo_cpu[i]->current_cycle;
    ooo_cpu[i]->begin_sim_instr = ooo_cpu[i]->num_retired;

    // reset branch stats
    ooo_cpu[i]->num_branch = 0;
    ooo_cpu[i]->branch_mispredictions = 0;
    ooo_cpu[i]->total_rob_occupancy_at_branch_mispredict = 0;

    for (uint32_t j = 0; j < 8; j++)
    {
      ooo_cpu[i]->total_branch_types[j] = 0;
      ooo_cpu[i]->branch_type_misses[j] = 0;
    }

    for (auto it = caches.rbegin(); it != caches.rend(); ++it)
      reset_cache_stats(i, *it);
  }
#if PRINT_STATISTICS_INTO_FILE == ENABLE
  fprintf(outputchampsimstatistics.trace_file, "\n");
#else
  cout << endl;
#endif


#if USER_CODES == ENABLE
  // reset DDR stats
  for (uint32_t i = 0; i < DDR_CHANNELS; i++)
  {
    memory.ddr_channels[i].WQ_ROW_BUFFER_HIT = 0;
    memory.ddr_channels[i].WQ_ROW_BUFFER_MISS = 0;
    memory.ddr_channels[i].RQ_ROW_BUFFER_HIT = 0;
    memory.ddr_channels[i].RQ_ROW_BUFFER_MISS = 0;
  }
  // reset HBM stats
  for (uint32_t i = 0; i < HBM_CHANNELS; i++)
  {
    memory.hbm_channels[i].WQ_ROW_BUFFER_HIT = 0;
    memory.hbm_channels[i].WQ_ROW_BUFFER_MISS = 0;
    memory.hbm_channels[i].RQ_ROW_BUFFER_HIT = 0;
    memory.hbm_channels[i].RQ_ROW_BUFFER_MISS = 0;
  }
#else
  // reset DRAM stats
  for (uint32_t i = 0; i < DRAM_CHANNELS; i++)
  {
    memory.dram_channels[i].WQ_ROW_BUFFER_HIT = 0;
    memory.dram_channels[i].WQ_ROW_BUFFER_MISS = 0;
    memory.dram_channels[i].RQ_ROW_BUFFER_HIT = 0;
    memory.dram_channels[i].RQ_ROW_BUFFER_MISS = 0;
  }

#endif
}

void signal_handler(int signal)
{
  cout << "Caught signal: " << signal << endl;
  exit(1);
}
