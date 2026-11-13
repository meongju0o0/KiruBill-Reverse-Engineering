#include "mem.h"

alignas(16) uint8_t g_mem[MEM_END - MEM_BASE];
void*   g_mem_ptr[(MEM_END - MEM_BASE) / 4];
SceneFn g_mem_fn[(MEM_END - MEM_BASE) / 4];

// Initialised game variables of the original .data section.
void mem_init()
{
    std::memset(g_mem, 0, sizeof(g_mem));
    std::memset(g_mem_ptr, 0, sizeof(g_mem_ptr));
    for (auto& f : g_mem_fn)
        f = nullptr;
    MI32(0x41e050) = 1;
    MI32(0x41e054) = 0x72;
    MI32(0x41e058) = 1;
}
