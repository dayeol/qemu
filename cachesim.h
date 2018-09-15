// See LICENSE for license details.

#ifndef _RISCV_CACHE_SIM_H
#define _RISCV_CACHE_SIM_H

extern bool cachesim_enable;
#ifdef __cplusplus

#define   likely(x) __builtin_expect(x, 1)
#define unlikely(x) __builtin_expect(x, 0)
#include <string>
#include <map>
#include <cstdint>
#include <cstring>
#include "memtracer.h"

class lfsr_t
{
 public:
  lfsr_t() : reg(1) {}
  lfsr_t(const lfsr_t& lfsr) : reg(lfsr.reg) {}
  uint32_t next() { return reg = (reg>>1)^(-(reg&1) & 0xd0000001); }
 private:
  uint32_t reg;
};

class cache_sim_t
{
 public:
  cache_sim_t(size_t sets, size_t ways, size_t linesz, const char* name);
  cache_sim_t(const cache_sim_t& rhs);
  virtual ~cache_sim_t();

  void access(uint64_t vaddr, uint64_t paddr, size_t bytes, bool store);
  void print_stats();
  void set_miss_handler(cache_sim_t* mh) { miss_handler = mh; }

  static cache_sim_t* construct(const char* config, const char* name);
  
  void enable_trace_miss(){ trace_miss = true; }
 protected:
  bool trace_miss;
  static const uint64_t VALID = 1ULL << 63;
  static const uint64_t DIRTY = 1ULL << 62;

  virtual uint64_t* check_tag(uint64_t addr);
  virtual uint64_t victimize(uint64_t addr, uint64_t src, uint64_t &victim_src);

  lfsr_t lfsr;
  cache_sim_t* miss_handler;

  size_t sets;
  size_t ways;
  size_t linesz;
  size_t idx_shift;

  uint64_t* tags;
  uint64_t* srcs;

  uint64_t read_accesses;
  uint64_t read_misses;
  uint64_t bytes_read;
  uint64_t write_accesses;
  uint64_t write_misses;
  uint64_t bytes_written;
  uint64_t writebacks;

  std::string name;

  void init();
};

class fa_cache_sim_t : public cache_sim_t
{
 public:
  fa_cache_sim_t(size_t ways, size_t linesz, const char* name);
  uint64_t* check_tag(uint64_t addr);
  uint64_t victimize(uint64_t addr);
 private:
  static bool cmp(uint64_t a, uint64_t b);
  std::map<uint64_t, uint64_t> tags;
};

class cache_memtracer_t : public memtracer_t
{
 public:
  cache_memtracer_t(const char* config, const char* name)
  {
    cache = cache_sim_t::construct(config, name);
  }
  ~cache_memtracer_t()
  {
    delete cache;
  }
  void set_miss_handler(cache_sim_t* mh)
  {
    cache->set_miss_handler(mh);
  }

 protected:
  cache_sim_t* cache;
};

class icache_sim_t : public cache_memtracer_t
{
 public:
  void enable_trace_miss() {cache->enable_trace_miss();}
  icache_sim_t(const char* config) : cache_memtracer_t(config, "I$") {}
  bool interested_in_range(uint64_t begin, uint64_t end, access_type type)
  {
    return type == FETCH;
  }
  void trace(uint64_t vaddr, uint64_t paddr, size_t bytes, access_type type)
  {
    if (type == FETCH) cache->access(vaddr, paddr, bytes, false);
  }
};

class dcache_sim_t : public cache_memtracer_t
{
 public:
  void enable_trace_miss() {cache->enable_trace_miss();}
  dcache_sim_t(const char* config) : cache_memtracer_t(config, "D$") {}
  bool interested_in_range(uint64_t begin, uint64_t end, access_type type)
  {
    return type == LOAD || type == STORE;
  }
  void trace(uint64_t vaddr, uint64_t paddr, size_t bytes, access_type type)
  {
    if (type == LOAD || type == STORE) cache->access(vaddr, paddr, bytes, type == STORE);
  }
};
#endif /* ifdef __cplusplus */


#ifdef __cplusplus
extern "C" {
#endif
    void init_cache_l1(const char* optstr);
    void init_cache_l2(const char* optstr);
    void init_cache_l3(const char* optstr);
    void cachesim_ld(uint64_t vaddr, uint64_t paddr, size_t bytes);
    void cachesim_st(uint64_t vaddr, uint64_t paddr, size_t bytes);
    void cachesim_fc(uint64_t vaddr, uint64_t paddr, size_t bytes);
    void init_cachesim(const char* file);
    void cachesim_destroy(void);
#ifdef __cplusplus
}
#endif

#endif /* _RISCV_CACHE_SIM_H */
