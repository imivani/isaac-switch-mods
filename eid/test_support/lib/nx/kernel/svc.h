#pragma once
#include <stdint.h>

// Host-only replacement for the memory-query boundary used by feature_api.hpp.
// The test harness permits only the explicit fake-game allocations it owns.
typedef struct MemoryInfo {
    uint64_t addr;
    uint64_t size;
    uint32_t perm;
} MemoryInfo;
enum { Perm_R = 1 };
uint32_t svcQueryMemory(MemoryInfo* info, uint32_t* page, uint64_t address);
