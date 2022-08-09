<p align="center">
  <h1 align="center"> ChampSim </h1>
  <p> ChampSim is a trace-based simulator for a microarchitecture study. You can sign up to the public mailing list by sending an empty mail to champsim+subscribe@googlegroups.com. If you have questions about how to use ChampSim, you can often receive a quicker response on the mailing list. Please reserve GitHub Issues for bugs. Traces for the 3rd Data Prefetching Championship (DPC-3) can be found from here (https://dpc3.compas.cs.stonybrook.edu/?SW_IS). <p>
</p>

# Hybrid Memory Systems
This branch is based on the master branch of ChampSim and is modified to simulate hybrid memory systems. The parameters about hybrid memory systems are defined in the `champsim_constants.h` file. To enable the functionality of hybrid memory systems, set the preprocessor `MEMORY_USE_HYBRID` to `ENABLE` in the `ProjectConfiguration.h` file.

# Download DPC-3 trace

Professor Daniel Jimenez at Texas A&M University kindly provided traces for DPC-3. Use the following script to download these traces (~20GB size and max simpoint only).
```
$ cd scripts

$ ./download_dpc3_traces.sh
```

# Build and debug

Before starting to build or debug this project, you might need to be familiar with [the visual studio code tutorial](https://code.visualstudio.com/docs/cpp/config-linux).

## Build
Build methods are explained below.

### 1. Visual Studio Code-based method.
- You may need to modify the compiler's path in the `tasks.json` file in the `.vscode` directory.
- Click `Run Build Task` in the `Terminal` tab.

### 2. Command line-based method.
By referring the contents of `tasks.json` file in the `vscode` directory, input the below command,
```
$ g++ -fdiagnostics-color=always -g -fopenmp -std=c++17 -I champsim_directory/inc/ 
champsim_directory/branch/bimodal/*.c* champsim_directory/prefetcher/no/*.c*" champsim_directory/prefetcher/no_instr/*.c* champsim_directory/replacement/lru/*.c* champsim_directory/btb/basic_btb/*.c* champsim_directory/src/*.c* 
-o champsim_directory/bin/champsim
```

## Debug
Debug methods are explained below.

### Visual Studio Code-based method.
- You may need to modify the debugger's path in the `launch.json` file in the `.vscode` directory.
- Click `Start Debugging` in the `Run` tab.

### 2. Command line-based method.
By referring the contents of `launch.json` file in the `vscode` directory, input the below command,
```
(waiting for updating)
```

Please be aware this branch only uses bimodal for branch predictor, lru for replacement, no and no_instr for prefetcher. If you want to try other modules, you can modify their source files by referring to this branch's source code. 

# Run simulation
Execute the binary directly,
```
$ ./bin/champsim --warmup_instructions [N_WARM] --simulation_instructions [N_SIM] [TRACE]
```
where [N_WARM] is the number of instructions for warmup (1 million), [N_SIM] is the number of instructinos for detailed simulation (10 million),
[TRACE] is the trace name (619.lbm_s-4268B.champsimtrace.xz). For example,
```
$ ./bin/champsim --warmup_instructions 1000000 --simulation_instructions 2000000 path_to_traces/619.lbm_s-4268B.champsimtrace.xz
```

The number of warmup and simulation instructions given will be the number of instructions retired. Note that the statistics printed at the end of the
simulation include only the simulation phase.

# Miscellaneous

If IntelliSense still doesn't work properly, it might be because of the version of the C++ language standard used. To solve this problem, you need to open Visual Studio Code, click `View` -> `Command Palette`, and in the center where a terminal is popped out, input or select `C/C++: Edit Configurations (UI)`. A new file called `c_cpp_properties.json` should be created, and its UI is opened. After you modify `C++ standard` to `gnu++17` (or higher) in that file. IntelliSense should work properly now.
