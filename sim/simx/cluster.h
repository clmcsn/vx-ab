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

#pragma once

#include <simobject.h>
#include "dcrs.h"
#include "arch.h"
#include "cache_cluster.h"
#include "shared_mem.h"
#include "core.h"
#include "constants.h"

namespace vortex {

class ProcessorImpl;

class Cluster : public SimObject<Cluster> {
public:
  struct PerfStats {
    CacheSim::PerfStats   icache;
    CacheSim::PerfStats   dcache;
    SharedMem::PerfStats  sharedmem;
    CacheSim::PerfStats   l2cache;

    PerfStats& operator+=(const PerfStats& rhs) {
      this->icache      += rhs.icache;
      this->dcache      += rhs.dcache;
      this->sharedmem   += rhs.sharedmem;
      this->l2cache     += rhs.l2cache;
      return *this;
    }
  };

  SimPort<MemReq> mem_req_port;
  SimPort<MemRsp> mem_rsp_port;

  Cluster(const SimContext& ctx, 
          uint32_t cluster_id,
          ProcessorImpl* processor, 
          const Arch &arch, 
          const DCRS &dcrs);

  ~Cluster();

  void reset();

  void tick();

  void attach_ram(RAM* ram);

  bool running() const;

  bool check_exit(Word* exitcode, bool riscv_test) const;  

  void barrier(uint32_t bar_id, uint32_t count, uint32_t core_id);

  ProcessorImpl* processor() const;

  Cluster::PerfStats perf_stats() const;
  
private:
  uint32_t                     cluster_id_;  
  std::vector<Core::Ptr>       cores_;  
  std::vector<CoreMask>        barriers_;
  CacheSim::Ptr                l2cache_;
  CacheCluster::Ptr            icaches_;
  CacheCluster::Ptr            dcaches_;
  std::vector<SharedMem::Ptr>  sharedmems_;
  CacheCluster::Ptr            tcaches_;
  CacheCluster::Ptr            ocaches_;
  CacheCluster::Ptr            rcaches_;
  ProcessorImpl*               processor_;
};

} // namespace vortex