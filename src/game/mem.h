// Game global memory, kept at the ORIGINAL addresses.
//
// The game's own global variables live in the original .data/.bss between
// 0x41e050 and 0x41f878 (the "game state" struct returned by 0x40b5e0 starts
// at 0x41f688).  The translated code accesses them by original address so it
// can be checked against the disassembly one to one:
//
//     MI32(0x41f6dc)          // int32 at 0x41f6dc
//     MF32(0x41f688 + 0x24)   // float field of the game state struct
//     MI32(0x41f468 + 4 * i)  // array element
//
// Pointer and function pointer values (8 bytes on x64) cannot live in a 4
// byte slot, so they are stored in parallel side tables indexed by address:
//
//     MTASK(0x41f7bc)  -> Task*&      MFN(0x41f79c) -> SceneFn&
//     MPTR(addr)       -> void*&
#pragma once

#include <cstdint>
#include <cstring>
#include "../engine/task.h"

typedef void (*SceneFn)();

enum : uint32_t { MEM_BASE = 0x41e000, MEM_END = 0x41f878 };

extern uint8_t  g_mem[MEM_END - MEM_BASE];
extern void*    g_mem_ptr[(MEM_END - MEM_BASE) / 4];
extern SceneFn  g_mem_fn[(MEM_END - MEM_BASE) / 4];

inline uint8_t*  MADDR(uint32_t a) { return &g_mem[a - MEM_BASE]; }
inline int32_t&  MI32(uint32_t a) { return *reinterpret_cast<int32_t*>(MADDR(a)); }
inline uint32_t& MU32(uint32_t a) { return *reinterpret_cast<uint32_t*>(MADDR(a)); }
inline float&    MF32(uint32_t a) { return *reinterpret_cast<float*>(MADDR(a)); }
inline double&   MF64(uint32_t a) { return *reinterpret_cast<double*>(MADDR(a)); }
inline int16_t&  MI16(uint32_t a) { return *reinterpret_cast<int16_t*>(MADDR(a)); }
inline uint16_t& MU16(uint32_t a) { return *reinterpret_cast<uint16_t*>(MADDR(a)); }
inline uint8_t&  MU8(uint32_t a)  { return *MADDR(a); }
inline int8_t&   MI8(uint32_t a)  { return *reinterpret_cast<int8_t*>(MADDR(a)); }
inline void*&    MPTR(uint32_t a) { return g_mem_ptr[(a - MEM_BASE) / 4]; }
inline Task*&    MTASK(uint32_t a) { return *reinterpret_cast<Task**>(&g_mem_ptr[(a - MEM_BASE) / 4]); }
inline SceneFn&  MFN(uint32_t a)  { return g_mem_fn[(a - MEM_BASE) / 4]; }

// 0x40b5e0 returns the address of the game state struct.
enum : uint32_t { GS = 0x41f688 };

// Initialise g_mem with the original initialised values.
void mem_init();
