#include "qemu/osdep.h"
#include "cpu.h"
#include "exec/helper-proto.h"
#include "exec/cpu_ldst.h"
#include "memtrace.h"

#include <string.h>

bool memtrace_is_started = false; /* if this is false, do not simulate cache, nor trace mem access */
bool memtrace_enable = false; /* is trace is enabled? */
FILE* memtrace_file = NULL; /* trace log file */
uint64_t memtrace_region_start=0; /* filter start (physical addr) */
uint64_t memtrace_region_end=(uint64_t)-1; /* filter end (physical addr) */
uint64_t memtrace_ram_base = 0; /* host virtual address of guest DRAM */

/* this is the callback for cache sim */
static void cache_miss_trace(uint64_t addr, unsigned size, bool is_store)
{
    log_filtered_trace(addr, size, is_store);
}

void memtrace_set_region(const char* region){
    char start[50];
    char end[50];

    if(region == NULL)
        return;

    char* idx = strchr(region, ':');
    if( idx == NULL )
    {
        goto exit_error;
    }

    strncpy(start, region, idx-region);
    strncpy(end, idx+1, strlen(region));

    sscanf(start, "%" PRIx64, &memtrace_region_start);
    sscanf(end, "%" PRIx64, &memtrace_region_end);

    fprintf(stderr, "region_start: %"PRIx64 "\n", memtrace_region_start);
    fprintf(stderr, "region_end: %"PRIx64 "\n", memtrace_region_end);

    cache_miss_callback = cache_miss_trace;

    return;

exit_error:
    fprintf(stderr, "Usage: -memtrace <start>:<end>\n"
                        "       (e.g., -memtrace 0x80000:0x90000)\n");
    exit(1);
} 

void memtrace_set_ram_base(uint8_t* addr,uint64_t size)
{
    /* only if this is the first call, because the first one is for the system memory. */
    if(memtrace_ram_base == 0 && memtrace_file) {
        memtrace_ram_base = (uint64_t) addr;
        fprintf(memtrace_file, "RAM base: %" PRIx64 ", size:%"PRIx64"\n", memtrace_ram_base, size);
    }
}

void log_filtered_trace(uint64_t addr, unsigned size, bool is_store)
{
    if(!memtrace_enable || !memtrace_file)
        return;

    if(addr < memtrace_region_start || addr >= memtrace_region_end)
        return;

    if(is_store) {
        memtrace_vprintf("S %#"PRIx64" size %u \n", addr, size);
    }
    else {
        memtrace_vprintf("L %#"PRIx64" size %u \n", addr, size);
    }
}

static void memtrace(CPUX86State *env, uint64_t vaddr, uint32_t size,
                     bool is_store)
{
    CPUState *cs = CPU(x86_env_get_cpu(env));
    uint64_t paddr;

    if(!memtrace_is_started)
        return;

    if((size-1)&vaddr && (vaddr&0xfff)+size >= 0x1000) {
        memtrace(env, vaddr,          size/2, is_store);
        memtrace(env, vaddr + size/2, size/2, is_store);
    }

    paddr = cpu_get_phys_page_debug(cs, (vaddr&~(0x1000-1))) + (vaddr&(0x1000-1));

    /* if cache exists, trace last-level cache miss instead */
    if(cachesim_enable)
    {
        if(is_store)
            cachesim_st(paddr, size);
        else
            cachesim_ld(paddr, size);
        return;
    }

    log_filtered_trace(paddr, size, is_store);
    return;
}

void helper_memtrace_ld(CPUX86State *env, target_ulong vaddr, uint32_t size)
{
    memtrace(env, vaddr, size, false);
}

void helper_memtrace_st(CPUX86State *env, target_ulong vaddr, uint32_t size)
{
    memtrace(env, vaddr, size, true);
}
