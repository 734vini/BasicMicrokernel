#include "task.h"
#include "memory.h"
#include <stdint.h>

TCB tasks[MAX_TASKS];
int task_count = 0;

void xTaskCreate(void (*task)(void),
                 uint32_t stack_size,
                 int priority)
{
    int slot = -1;
    for (int i = 0; i < task_count; i++)
    {
        if (!tasks[i].active) { slot = i; break; }
    }
    if (slot == -1)
    {
        if (task_count >= MAX_TASKS) return;
        slot = task_count++;
    }

    if (stack_size == 0)
        stack_size = DEFAULT_STACK_SIZE;

    TCB *t = &tasks[slot];

    t->stack = (uint8_t*)kmalloc(stack_size);
    if (!t->stack) return;

    uint64_t *sp = (uint64_t*)(t->stack + stack_size);

    /* Configurar contexto inicial */

    t->regs[0] = (uint64_t)task;   // ra
    t->regs[1] = (uint64_t)sp;     // sp

    t->entry = task;
    t->priority = priority;
    t->active = 1;
}

void xTaskDestroy(int task_id)
{
    if (task_id < 0 || task_id >= task_count)
        return;

    TCB *t = &tasks[task_id];
    if (!t->active)
        return;

    kfree(t->stack);
    t->stack  = 0;
    t->active = 0;
}