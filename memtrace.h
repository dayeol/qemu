#ifndef __MEMTRACE__
#define __MEMTRACE__

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <inttypes.h>

extern bool memtrace_is_started;
extern FILE* memtrace_file;
extern bool memtrace_enable;
extern uint64_t memtrace_region_start;
extern uint64_t memtrace_region_end;

void memtrace_set_region(const char* region);
void memtrace_set_ram_base(uint8_t* addr,uint64_t size);
inline void memtrace(uint64_t addr, unsigned size, bool is_store);

static inline void memtrace_vprintf(const char* fmt, ...)
{
    if(memtrace_file) {
        va_list ap;
        va_start(ap, fmt);
        vfprintf(memtrace_file, fmt, ap);
        va_end(ap);
    }
}

static inline void memtrace_ld(uint64_t addr, unsigned size)
{
    memtrace(addr,size,false);
}

static inline void memtrace_st(uint64_t addr, unsigned size)
{
    memtrace(addr,size,true);
}

static inline void memtrace_mark_location(void)
{
    fprintf(memtrace_file,"===UCBTRACE===");
}

#endif
