#include "scheduler.h"
#include "task.h"

extern void context_switch(void*, void*);

static int current = 0;

/*   Round-Robin padrão   */

static int round_robin(void)
{
    int next = (current + 1) % task_count;
    int tries = 0;
    while (!tasks[next].active && tries < task_count)
    {
        next = (next + 1) % task_count;
        tries++;
    }
    return next;
}

/*   Algoritmo atual   */

static sched_algo_t current_algo = round_robin;

void scheduler_set_algorithm(sched_algo_t algo)
{
    if (algo)
        current_algo = algo;
}

/*   Yield   */

void yield()
{
    int prev = current;
    int next = current_algo();

    if (prev == next) return;
    current = next;
    context_switch(tasks[prev].regs, tasks[next].regs);
}
 
/*   Início   */

void scheduler_start()
{
    if (task_count == 0)
        return;

    for (int i = 0; i < task_count; i++)
    {
        if (tasks[i].active)
        {
            current = i;
            tasks[i].entry();
            return;
        }
    }
}