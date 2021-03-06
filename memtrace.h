#ifndef __MEMTRACE__
#define __MEMTRACE__

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <inttypes.h>
#include "cachesim.h"

extern bool memtrace_is_started;
extern FILE* memtrace_file;
extern bool memtrace_enable;
extern bool memtrace_code;
extern uint64_t memtrace_region_start;
extern uint64_t memtrace_region_end;
extern void (*cache_miss_callback)(uint64_t, uint64_t, unsigned, bool);

void memtrace_set_region(const char* region);
void memtrace_set_ram_base(uint8_t* addr,uint64_t size);
//void memtrace(uint64_t addr, unsigned size, bool is_store);

static inline void memtrace_vprintf(const char* fmt, ...)
{
    if(memtrace_file) {
        va_list ap;
        va_start(ap, fmt);
        vfprintf(memtrace_file, fmt, ap);
        va_end(ap);
    }
}
void log_filtered_trace(uint64_t vaddr, uint64_t paddr, unsigned size, bool is_store);

#if 0
static inline void memtrace_ld(uint64_t addr, unsigned size)
{
    memtrace(addr,size,false);
}

static inline void memtrace_st(uint64_t addr, unsigned size)
{
    memtrace(addr,size,true);
}
#endif

static inline void memtrace_mark_location(void)
{
    if(memtrace_file)
        fprintf(memtrace_file,"===FIRST===\n");
}

static inline void memtrace_mark_location2(void)
{
    if(memtrace_file)
        fprintf(memtrace_file,"===SECOND===\n");
}

static inline void memtrace_mark_location3(void)
{
    if(memtrace_file)
        fprintf(memtrace_file,"===THIRD===\n");
}
#endif
