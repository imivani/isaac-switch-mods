/* Host test substitute: only the read-only memory-query contract is mocked. */
#pragma once
#include <stdint.h>
typedef struct {
    uint64_t addr, size;
    uint32_t type, attr, perm, device_refcount, ipc_refcount, padding;
} MemoryInfo;
enum { Perm_R = 1, Perm_W = 2 };
uint32_t svcQueryMemory(MemoryInfo*, uint32_t*, uint64_t);
