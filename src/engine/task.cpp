// Task scheduler – port of 0x412770..0x412a30.
#include "task.h"

namespace {
Task  g_pool[TASK_POOL];      // 0x4205d8 (1024 * 0x220)
Task* g_free[TASK_POOL];      // 0x4a85d8 ring of free slots
int   g_alloc_idx;            // 0x4a95d8
int   g_free_idx;             // 0x4a95dc
Task* g_head;                 // 0x4a95e0
Task* g_tail;                 // 0x4a95e4

inline int ring_next(int i) { return (i + 1) & (TASK_POOL - 1); }
}

Task* task_head() { return g_head; }
Task* task_tail() { return g_tail; }

// 0x412770
void task_system_init()
{
    for (int i = 0; i < TASK_POOL; ++i)
        g_free[i] = &g_pool[i];
    g_alloc_idx = 2;
    g_free_idx = 0;
    g_head = &g_pool[0];
    g_tail = &g_pool[1];

    g_head->fn = nullptr;
    g_head->prio = -1;
    g_head->f04 = 0;
    g_head->dead = 0;
    g_head->parent = nullptr;
    g_head->next = g_tail;
    g_head->prev = nullptr;
    g_head->data[0] = 0;  // +0x20 cleared (dword) in the original
    TI32(g_head, 0x20) = 0;

    g_tail->fn = nullptr;
    g_tail->prio = 1000000;
    g_tail->f04 = 0;
    g_tail->dead = 0;
    g_tail->parent = nullptr;
    g_tail->next = nullptr;
    g_tail->prev = g_head;
    TI32(g_tail, 0x20) = 0;
}

// 0x412870
int task_run_all()
{
    int n = 0;
    Task* t = g_head->next;
    while (t != g_tail) {
        if (t->wait > 0)
            t->wait--;
        if (t->fn)
            t->fn(t);
        t = t->next;
        ++n;
    }
    task_reap();
    return n;
}

// 0x4128c0
Task* task_create(const Task* desc)
{
    Task* it = g_head->next;
    if (g_alloc_idx == g_free_idx)
        return nullptr;
    Task* node = g_free[g_alloc_idx];
    *node = *desc;  // copies the full 0x220 byte record
    // find the first task with a greater priority (stable insertion)
    while (node->prio >= it->prio) {
        if (it == g_tail)
            return nullptr;
        it = it->next;
    }
    it->prev->next = node;
    node->prev = it->prev;
    node->next = it;
    it->prev = node;
    node->dead = 0;
    g_alloc_idx = ring_next(g_alloc_idx);
    return node;
}

// 0x412950
void task_kill_all()
{
    for (Task* t = g_head->next; t != g_tail; t = t->next)
        t->dead = 1;
}

// 0x412970: tasks are sorted by priority, so the walk stops at prio > mask.
void task_set_wait_mask(int32_t mask, int32_t wait)
{
    for (Task* t = g_head->next; t != g_tail; t = t->next) {
        if (t->prio > mask)
            return;
        if (t->prio & mask)
            t->wait = wait;
    }
}

// 0x4129b0
void task_pause_mask(int32_t mask) { task_set_wait_mask(mask, 999999999); }

// 0x4129f0
void task_resume_mask(int32_t mask) { task_set_wait_mask(mask, 0); }

// 0x412a30
void task_reap()
{
    Task* t = g_head->next;
    while (t != g_tail) {
        Task* cur = t;
        t = cur->next;
        if (cur->dead) {
            cur->prev->next = cur->next;
            cur->next->prev = cur->prev;
            g_free[g_free_idx] = cur;
            g_free_idx = ring_next(g_free_idx);
        }
    }
}
