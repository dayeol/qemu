#include "memtrace.h"
#include <string.h>

bool memtrace_is_started = false; /* dynamically change */
bool memtrace_enable = false;
FILE* memtrace_file = NULL;
uint64_t memtrace_region_start=0;
uint64_t memtrace_region_end=(uint64_t)-1;
uint64_t memtrace_ram_base = NULL;

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

inline void memtrace(uint64_t addr, unsigned size, bool is_store)
{
    if(!memtrace_enable || !memtrace_is_started || !memtrace_file)
        return;

    if( addr - memtrace_ram_base < memtrace_region_start || addr - memtrace_ram_base >= memtrace_region_end)
        return;

    if(is_store) {
        memtrace_vprintf("S %#"PRIx64" size %u \n", addr, size);
    }
    else {
        memtrace_vprintf("L %#"PRIx64" size %u \n", addr, size);
    }
    return;
}


