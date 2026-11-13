// Task scheduler (original 0x412770..0x412a90).
//
// The original keeps 1024 task records of 0x220 bytes in a static pool
// (0x4205d8), a ring buffer of free slots (0x4a85d8) and a doubly linked list
// sorted by ascending priority between two sentinels (head prio -1, tail
// prio 1000000).  Every frame each live task's `wait` counter is decremented
// (if > 0) and its callback is invoked, then tasks flagged dead are unlinked.
//
// A new task is created by filling a descriptor (a whole Task record) and
// passing it to task_create(), which copies all 0x220 bytes into a pool slot
// and links it before the first task with a greater priority.
//
// Task layout (offsets of the original 32-bit record):
//   +0x00 prio     sort key, also used as a bit mask by task_*_mask()
//   +0x04 f04
//   +0x08 wait     counted down by the scheduler
//   +0x0c parent   owning task (pointer)
//   +0x10 prev, +0x14 next  (list links, set by task_create)
//   +0x18 dead     non-zero => removed after this frame
//   +0x1c fn       per-frame callback
//   +0x20 .. +0x220 user data (accessed with TI32/TF32/TU8/... by offset)
//
// Pointers do not fit in 32-bit slots on x64, so pointer/function values that
// the original stored inside the user data are kept in a parallel table
// indexed by offset (TPTR / TFN).  Plain data stays byte-for-byte compatible.
#pragma once

#include <cstdint>
#include <cstring>

struct Task;
typedef void (*TaskFn)(Task*);

enum { TASK_DATA_BEGIN = 0x20, TASK_SIZE = 0x220, TASK_POOL = 1024 };

struct Task {
    int32_t prio;     // +0x00
    int32_t f04;      // +0x04
    int32_t wait;     // +0x08
    Task*   parent;   // +0x0c
    Task*   prev;     // +0x10
    Task*   next;     // +0x14
    int32_t dead;     // +0x18
    TaskFn  fn;       // +0x1c
    alignas(8) uint8_t data[TASK_SIZE - TASK_DATA_BEGIN]; // +0x20..
    void*   ptr[(TASK_SIZE - TASK_DATA_BEGIN) / 4];        // pointer side table
    TaskFn  fnp[(TASK_SIZE - TASK_DATA_BEGIN) / 4];        // function pointer side table
};

// Zero a descriptor before filling it (the original used uninitialised stack
// memory for fields it did not set).
inline void task_desc(Task* d) { std::memset(d, 0, sizeof(Task)); }

// Field access by ORIGINAL offset (0x20 <= off < 0x220).
inline int32_t&  TI32(Task* t, int off) { return *reinterpret_cast<int32_t*>(&t->data[off - TASK_DATA_BEGIN]); }
inline uint32_t& TU32(Task* t, int off) { return *reinterpret_cast<uint32_t*>(&t->data[off - TASK_DATA_BEGIN]); }
inline float&    TF32(Task* t, int off) { return *reinterpret_cast<float*>(&t->data[off - TASK_DATA_BEGIN]); }
inline int16_t&  TI16(Task* t, int off) { return *reinterpret_cast<int16_t*>(&t->data[off - TASK_DATA_BEGIN]); }
inline uint16_t& TU16(Task* t, int off) { return *reinterpret_cast<uint16_t*>(&t->data[off - TASK_DATA_BEGIN]); }
inline uint8_t&  TU8(Task* t, int off)  { return t->data[off - TASK_DATA_BEGIN]; }
inline int8_t&   TI8(Task* t, int off)  { return *reinterpret_cast<int8_t*>(&t->data[off - TASK_DATA_BEGIN]); }
inline void*&    TPTR(Task* t, int off) { return t->ptr[(off - TASK_DATA_BEGIN) / 4]; }
inline Task*&    TTASK(Task* t, int off) { return *reinterpret_cast<Task**>(&t->ptr[(off - TASK_DATA_BEGIN) / 4]); }
inline TaskFn&   TFN(Task* t, int off)  { return t->fnp[(off - TASK_DATA_BEGIN) / 4]; }

void  task_system_init();                 // 0x412770
int   task_run_all();                     // 0x412870 (also reaps dead tasks)
Task* task_create(const Task* desc);      // 0x4128c0 (nullptr if the pool is full)
void  task_kill_all();                    // 0x412950 mark every task dead
void  task_set_wait_mask(int32_t mask, int32_t wait); // 0x412970
void  task_pause_mask(int32_t mask);      // 0x4129b0 wait = 999999999
void  task_resume_mask(int32_t mask);     // 0x4129f0 wait = 0
void  task_reap();                        // 0x412a30 unlink dead tasks
Task* task_head();                        // sentinel 0 (g_4a95e0); first task = task_head()->next
Task* task_tail();                        // sentinel 1 (g_4a95e4)
