// See LICENSE for license details.

#include "cachesim.h"
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <cinttypes>
#include <cstdint>
#include <cstdarg>

bool cachesim_enable = false;
FILE* cachesim_file = NULL;
icache_sim_t* cache_l1i = NULL;
dcache_sim_t* cache_l1d = NULL;
cache_sim_t* cache_l2 = NULL;
cache_sim_t* cache_l3 = NULL;
void (*cache_miss_callback)(uint64_t, uint64_t, unsigned, bool);

static memtracer_list_t tracer;

void register_memtracer(memtracer_t* _tracer)
{
    tracer.hook(_tracer);
}

void init_cache_l1(const char* optstr)
{
    cache_l1i = new icache_sim_t(optstr);
    cache_l1d = new dcache_sim_t(optstr);
}

void init_cache_l2(const char* optstr)
{
    if(cache_l1i && cache_l1d) { 
        cache_l2 = cache_sim_t::construct(optstr, "L2$");
        cache_l1i->set_miss_handler(&*cache_l2);
        cache_l1d->set_miss_handler(&*cache_l2);
    }
    else {
        fprintf(stderr, "Cannot define L2 without L1 cache\n");
        exit(1);
    }
}

void init_cache_l3(const char* optstr)
{
    cache_l3 = cache_sim_t::construct(optstr, "L3$");
    if(cache_l2) {
        cache_l3 = cache_sim_t::construct(optstr, "L3$");
        cache_l2->set_miss_handler(&*cache_l3);
    } else {
        fprintf(stderr, "Cannot define L3 without L2 cache\n");
        exit(1);
    }
}

void cachesim_destroy()
{
    if(cache_l1i && cache_l1d)
    {
        delete cache_l1i;
        delete cache_l1d;
    }
    if(cache_l2)
        delete cache_l2;
    if(cache_l3)
        delete cache_l3;
}

void init_cachesim(const char* filename)
{
    cachesim_enable = true;
    register_memtracer(&*cache_l1i);
    register_memtracer(&*cache_l1d);
		if(filename != NULL) {
			cachesim_file = fopen(filename, "w");
		}	else {
			cachesim_file = stdout;
		}

    if(cache_l3)
    {
				fprintf(cachesim_file, "L3 misses will be traced\n");
        cache_l3->enable_trace_miss();
        return;
    }
    if(cache_l2)
    {
				fprintf(cachesim_file, "L2 misses will be traced\n");
        cache_l2->enable_trace_miss();
        return;
    }
    if(cache_l1i && cache_l1d)
    {
				fprintf(cachesim_file, "L1 misses will be traced\n");
        cache_l1i->enable_trace_miss();
        cache_l1d->enable_trace_miss();
    }
}

void cachesim_ld(uint64_t vaddr, uint64_t paddr, size_t bytes)
{
    tracer.trace(vaddr, paddr, bytes, LOAD);
}
void cachesim_st(uint64_t vaddr, uint64_t paddr, size_t bytes)
{
    tracer.trace(vaddr, paddr, bytes, STORE);
}
void cachesim_fc(uint64_t vaddr, uint64_t paddr, size_t bytes)
{
    tracer.trace(vaddr, paddr, bytes, FETCH);
}

cache_sim_t::cache_sim_t(size_t _sets, size_t _ways, size_t _linesz, const char* _name)
: sets(_sets), ways(_ways), linesz(_linesz), name(_name)
{
    init();
}

static void help()
{
    std::cerr << "Cache configurations must be of the form" << std::endl;
    std::cerr << "  sets:ways:blocksize" << std::endl;
    std::cerr << "where sets, ways, and blocksize are positive integers, with" << std::endl;
  std::cerr << "sets and blocksize both powers of two and blocksize at least 8." << std::endl;
  exit(1);
}

cache_sim_t* cache_sim_t::construct(const char* config, const char* name)
{
  const char* wp = strchr(config, ':');
  if (!wp++) help();
  const char* bp = strchr(wp, ':');
  if (!bp++) help();

  size_t sets = atoi(std::string(config, wp).c_str());
  size_t ways = atoi(std::string(wp, bp).c_str());
  size_t linesz = atoi(bp);
  
  if (ways > 4 /* empirical */ && sets == 1)
    return new fa_cache_sim_t(ways, linesz, name);
  return new cache_sim_t(sets, ways, linesz, name);
}

void cache_sim_t::init()
{
  if(sets == 0 || (sets & (sets-1)))
    help();
  if(linesz < 8 || (linesz & (linesz-1)))
    help();

  idx_shift = 0;
  for (size_t x = linesz; x>1; x >>= 1)
    idx_shift++;

  tags = new uint64_t[sets*ways]();
  srcs = new uint64_t[sets*ways]();
  read_accesses = 0;
  read_misses = 0;
  bytes_read = 0;
  write_accesses = 0;
  write_misses = 0;
  bytes_written = 0;
  writebacks = 0;

  trace_miss = false;
  miss_handler = NULL;

  trace_miss = false;}

cache_sim_t::cache_sim_t(const cache_sim_t& rhs)
 : sets(rhs.sets), ways(rhs.ways), linesz(rhs.linesz),
   idx_shift(rhs.idx_shift), name(rhs.name)
{
  tags = new uint64_t[sets*ways];
  srcs = new uint64_t[sets*ways];
  memcpy(tags, rhs.tags, sets*ways*sizeof(uint64_t));
  memcpy(srcs, rhs.srcs, sets*ways*sizeof(uint64_t));
}

cache_sim_t::~cache_sim_t()
{
  print_stats();
  delete [] tags;
  delete [] srcs;
}

void cache_sim_t::print_stats()
{
  if(read_accesses + write_accesses == 0)
    return;

  float mr = 100.0f*(read_misses+write_misses)/(read_accesses+write_accesses);

	fprintf(cachesim_file, "======== %s ========\n", name.c_str());
	fprintf(cachesim_file, "Bytes Read: %lu\n", bytes_read);
	fprintf(cachesim_file, "Bytes Written: %lu\n", bytes_written);
	fprintf(cachesim_file, "Read Accesses: %lu\n", read_accesses);
	fprintf(cachesim_file, "Write Accesses: %lu\n", write_accesses);
	fprintf(cachesim_file, "Read Misses: %lu\n", read_misses);
	fprintf(cachesim_file, "Write Misses: %lu\n", write_misses);
	fprintf(cachesim_file, "Writebacks: %lu\n", writebacks);
	fprintf(cachesim_file, "Miss Rate: %.3f\n", mr);
	
	/*
  std::cout << std::setprecision(3) << std::fixed;
  std::cout << name << " ";
  std::cout << "Bytes Read:            " << bytes_read << std::endl;
  std::cout << name << " ";
  std::cout << "Bytes Written:         " << bytes_written << std::endl;
  std::cout << name << " ";
  std::cout << "Read Accesses:         " << read_accesses << std::endl;
  std::cout << name << " ";
  std::cout << "Write Accesses:        " << write_accesses << std::endl;
  std::cout << name << " ";
  std::cout << "Read Misses:           " << read_misses << std::endl;
  std::cout << name << " ";
  std::cout << "Write Misses:          " << write_misses << std::endl;
  std::cout << name << " ";
  std::cout << "Writebacks:            " << writebacks << std::endl;
  std::cout << name << " ";
  std::cout << "Miss Rate:             " << mr << '%' << std::endl;
	*/
}

uint64_t* cache_sim_t::check_tag(uint64_t addr)
{
  size_t idx = (addr >> idx_shift) & (sets-1);
  size_t tag = (addr >> idx_shift) | VALID;

  for (size_t i = 0; i < ways; i++)
    if (tag == (tags[idx*ways + i] & ~DIRTY))
      return &tags[idx*ways + i];

  return NULL;
}

uint64_t cache_sim_t::victimize(uint64_t addr, uint64_t src, uint64_t &victim_src)
{
  size_t idx = (addr >> idx_shift) & (sets-1);
  size_t way = lfsr.next() % ways;
  uint64_t victim = tags[idx*ways + way];
  victim_src = srcs[idx*ways + way];
  tags[idx*ways + way] = (addr >> idx_shift) | VALID;
  srcs[idx*ways + way] = src;
  return victim;
}

void cache_sim_t::access(uint64_t vaddr, uint64_t paddr, size_t bytes, bool store)
{
  store ? write_accesses++ : read_accesses++;
  (store ? bytes_written : bytes_read) += bytes;

  uint64_t* hit_way = check_tag(paddr);
  if (likely(hit_way != NULL))
  {
    if (store)
      *hit_way |= DIRTY;
    return;
  }
  /* cache miss occurs */
  if(trace_miss)
  {
      cache_miss_callback(vaddr & ~(linesz-1), paddr & ~(linesz-1), linesz, store);
  }

  store ? write_misses++ : read_misses++;

  uint64_t victim_vaddr;
  uint64_t victim = victimize(paddr, vaddr & ~(linesz-1), victim_vaddr);

  if ((victim & (VALID | DIRTY)) == (VALID | DIRTY))
  {
    uint64_t dirty_addr = (victim & ~(VALID | DIRTY)) << idx_shift;
    if (miss_handler)
      miss_handler->access(victim_vaddr, dirty_addr, linesz, true);
    writebacks++;
  }

  if (miss_handler)
    miss_handler->access(vaddr & ~(linesz-1), paddr & ~(linesz-1), linesz, false);

  if (store)
    *check_tag(paddr) |= DIRTY;
}

fa_cache_sim_t::fa_cache_sim_t(size_t ways, size_t linesz, const char* name)
  : cache_sim_t(1, ways, linesz, name)
{
}

uint64_t* fa_cache_sim_t::check_tag(uint64_t addr)
{
  auto it = tags.find(addr >> idx_shift);
  return it == tags.end() ? NULL : &it->second;
}

uint64_t fa_cache_sim_t::victimize(uint64_t addr)
{
  uint64_t old_tag = 0;
  if (tags.size() == ways)
  {
    auto it = tags.begin();
    std::advance(it, lfsr.next() % ways);
    old_tag = it->second;
    tags.erase(it);
  }
  tags[addr >> idx_shift] = (addr >> idx_shift) | VALID;
  return old_tag;
}
