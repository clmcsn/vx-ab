// Copyright © 2019-2023
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <fstream>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include "processor.h"
#include "mem.h"
#include "constants.h"
#include <util.h>
#include "core.h"

using namespace vortex;

static void show_usage() {
   std::cout << "Usage: [-c <cores>] [-w <warps>] [-t <threads>] [-r: riscv-test] [-s: stats] [-h: help] <program>" << std::endl;
}

uint32_t num_threads = NUM_THREADS;
uint32_t num_warps = NUM_WARPS;
uint32_t num_cores = NUM_CORES;
uint32_t num_clusters = NUM_CLUSTERS;
bool showStats = false;;
bool riscv_test = false;
const char* program = nullptr;

static void parse_args(int argc, char **argv) {
  	int c;
  	while ((c = getopt(argc, argv, "t:w:c:g:rsh?")) != -1) {
    	switch (c) {
      case 't':
        num_threads = atoi(optarg);
        break;
      case 'w':
        num_warps = atoi(optarg);
        break;
		  case 'c':
        num_cores = atoi(optarg);
        break;
		  case 'g':
        num_clusters = atoi(optarg);
        break;
      case 'r':
        riscv_test = true;
        break;
      case 's':
        showStats = true;
        break;
    	case 'h':
    	case '?':
      		show_usage();
      		exit(0);
    		break;
    	default:
      		show_usage();
      		exit(-1);
    	}
	}

	if (optind < argc) {
		program = argv[optind];
    std::cout << "Running " << program << "..." << std::endl;
	} else {
		show_usage();
    exit(-1);
	}
}

int main(int argc, char **argv) {
  int exitcode = 0;

  parse_args(argc, argv);

  {
    // create processor configuation
    Arch arch(num_threads, num_warps, num_cores, num_clusters);

    // create memory module
    RAM ram(RAM_PAGE_SIZE);

    // create processor
    Processor processor(arch);
  
    // attach memory module
    processor.attach_ram(&ram); 

	  // setup base DCRs
    const uint64_t startup_addr(STARTUP_ADDR);
    processor.write_dcr(VX_DCR_BASE_STARTUP_ADDR0, startup_addr & 0xffffffff);
  #if (XLEN == 64)
    processor.write_dcr(VX_DCR_BASE_STARTUP_ADDR1, startup_addr >> 32);
  #endif
	  processor.write_dcr(VX_DCR_BASE_MPM_CLASS, 0);

    // load program
    {      
      std::string program_ext(fileExtension(program));
      if (program_ext == "bin") {
        ram.loadBinImage(program, startup_addr);
      } else if (program_ext == "hex") {
        ram.loadHexImage(program);
      } else {
        std::cout << "*** error: only *.bin or *.hex images supported." << std::endl;
        return -1;
      }
    }

    // run simulation
    exitcode = processor.run(riscv_test);
  }   

  if (exitcode != 0) {
    std::cout << "*** error: exitcode=" << exitcode << std::endl;
  } 

  return exitcode;
}
