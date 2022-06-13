<p align="center">
  <h1 align="center"> ChampSim </h1>
  <p> ChampSim is a trace-based simulator for a microarchitecture study. You can sign up to the public mailing list by sending an empty mail to champsim+subscribe@googlegroups.com. If you have questions about how to use ChampSim, you can often receive a quicker response on the mailing list. Please reserve GitHub Issues for bugs. <p>
</p>

# Visual Studio Code
This branch is based on the master branch of ChampSim and is modified for comfortable coding experience in Visual Studio Code.

# Modificaton of the master branch of ChampSim

After building the ChampSim from this link (http://github.com/yousei-github/ChampSim/tree/master), you need first copy and replace several files from this branch into your ChampSim directory.
```
$ cp -r this_branch/new_files/ champsim_directory/
$ cp champsim_directory/new_files/champsim.h champsim_directory/inc/
$ cp champsim_directory/new_files/cache.h champsim_directory/inc/
$ cp champsim_directory/new_files/ooo_cpu.h champsim_directory/inc/
$ cp champsim_directory/new_files/Configuration.h champsim_directory/inc/
$ cp champsim_directory/new_files/bimodal.cc champsim_directory/branch/bimodal/
$ cp champsim_directory/new_files/lru.cc champsim_directory/replacement/lru/
$ cp champsim_directory/new_files/basic_btb.cc champsim_directory/btb/basic_btb/
$ cp champsim_directory/new_files/no/no.cc champsim_directory/prefetcher/no/
$ cp champsim_directory/new_files/no_instr/no.cc champsim_directory/prefetcher/no_instr/
```

Next, in order to compile and debug the ChampSim project using Visual Studio Code's method, you need to create two files, `tasks.json` and `launch.json`, by referring to the official documentation at here (https://code.visualstudio.com/docs/cpp/config-linux). Note now, you might not be able to compile and debug the source codes with those files.

When you have those two files, you need to replace their contents with those in the same name at this branch's `vscode` directory. Note that you may need to modify the contents on your own, such as the compiler's path.

# Compile and debug

Following the above instructions, you should be able to compile and debug the ChampSim project using Visual Studio Code's method.

## Build
* Click `Run Build Task` in the `Terminal` tab.
* Input command in Terminal, for example,
```
$ ./bin/champsim --warmup_instructions 1000000 --simulation_instructions 50000000 ../path_to_traces/600.perlbench_s-210B.champsimtrace.xz
```

## Debug
Click `Start Debugging` in the `Run` tab.

Please be aware this branch only uses bimodal for branch predictor, lru for replacement, no and no_instr for prefetcher. If you want to try other modules, you can modify their source files by referring to this branch's source code. 

# Miscellaneous

If IntelliSense still doesn't work properly, it might be because of the version of the C++ language standard used. To solve this problem, you need to open Visual Studio Code, click `View` -> `Command Palette`, and in the center where a terminal is popped out, input or select `C/C++: Edit Configurations (UI)`. A new file called `c_cpp_properties.json` should be created, and its UI is opened. After you modify `C++ standard` to `gnu++17` (or higher) in that file. IntelliSense should work properly now.
