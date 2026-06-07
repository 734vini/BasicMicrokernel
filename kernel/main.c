#include "task.h"
#include "scheduler.h"
#include "memory.h"

#include "uart.h"

static void print_heap_stats(void)
{
    uart_print("  Total : "); uart_print_uint(memory_total()); uart_print(" bytes\n");
    uart_print("  Usado : "); uart_print_uint(memory_used());  uart_print(" bytes\n");
    uart_print("  Livre : "); uart_print_uint(memory_free());  uart_print(" bytes\n\n");
}

/*   Tasks   */

void task1()
{
    int i = 0;
    while (1)
    {
        if (i < 3) {
            uart_print("[Task 1] rodando\n");
            print_heap_stats();
            i++;
        }
        yield();
    }
}

void task2()
{
    int i = 0;
    while (1)
    {
        if (i < 3) {
            uart_print("[Task 2] rodando\n");
            print_heap_stats();
            i++;
        }
        yield();
    }
}

/*   Kernel   */

void kernel_main()
{
    memory_init();   // OBRIGATÓRIO

    uart_print("\n=== Microkernel - Free List Allocator ===\n\n");

    uart_print("[1] Estado inicial\n");
    print_heap_stats();

    uart_print("[2] Multiplas alocacoes: 256 + 512 + 128\n");
    void *p1 = kmalloc(256);
    void *p2 = kmalloc(512);
    void *p3 = kmalloc(128);
    print_heap_stats();

    uart_print("[3] Liberando p1 + p2 -> coalescencia\n");
    kfree(p1); kfree(p2);
    print_heap_stats();

    uart_print("[4] Reutilizacao: kmalloc(300)\n");
    void *p4 = kmalloc(300);
    print_heap_stats();

    uart_print("[5] Divisao: libera tudo, aloca 64\n");
    kfree(p3); kfree(p4);
    void *p5 = kmalloc(64);
    print_heap_stats();

    uart_print("[6] Liberacao total\n");
    kfree(p5);
    print_heap_stats();

    uart_print("[7] xTaskCreate demo\n");
    xTaskCreate(task1, 2048, 1);
    print_heap_stats();

    uart_print("[7b] xTaskDestroy demo\n");
    xTaskDestroy(0);
    print_heap_stats();

    uart_print("[8] Criando tasks do scheduler\n");
    xTaskCreate(task1, 2048, 1);
    xTaskCreate(task2, 2048, 1);
    print_heap_stats();

    uart_print("=== Iniciando Scheduler ===\n\n");
    scheduler_start();

    while (1);
}