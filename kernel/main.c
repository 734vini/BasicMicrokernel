#include "task.h"
#include "scheduler.h"
#include "memory.h"
#include "treefs.h"
#include "string.h"

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

/*   TreeFS - demonstracao dos cenarios de validacao (M3)   */

static void treefs_demo(void)
{
    uart_print("\n=== TreeFS - Sistema de Arquivos Hierarquico ===\n\n");

    fs_init();

    uart_print("[Cenario 1] ls(\"/\")\n");
    ls("/");

    uart_print("\n[Extra] mkdir(\"/home\") -- deve falhar (ja existe)\n");
    uart_print(mkdir("/home") == 0 ? "  ERRO: nao deveria ter criado\n"
                                    : "  OK: duplicidade detectada corretamente\n");

    uart_print("\n[Cenario 2] mkdir(\"/home/aluno\")\n");
    uart_print(mkdir("/home/aluno") == 0 ? "  diretorio criado com sucesso\n"
                                          : "  ERRO ao criar diretorio\n");

    uart_print("\n[Cenario 3] create(\"/home/aluno/notas.txt\")\n");
    int fd = create("/home/aluno/notas.txt");
    if (fd >= 0) { uart_print("  arquivo criado, fd = "); uart_print_uint((uint64_t)fd); uart_print("\n"); }
    else          uart_print("  ERRO ao criar arquivo\n");

    uart_print("\n[Cenario 4] write(fd, \"Sistemas Operacionais\", 22)\n");
    int wr = write(fd, "Sistemas Operacionais", 22);
    uart_print("  bytes escritos: "); uart_print_uint((uint64_t)wr); uart_print("\n");

    uart_print("\n[Cenario 5] read(fd, buffer, 22)\n");
    /* reposiciona o cursor no inicio do arquivo antes de ler (ver fs_seek em treefs.h) */
    fs_seek(fd, 0);
    char buffer[32];
    memset(buffer, 0, sizeof(buffer));
    int rd = read(fd, buffer, 22);
    uart_print("  bytes lidos: "); uart_print_uint((uint64_t)rd); uart_print("\n");
    uart_print("  conteudo lido: "); uart_print(buffer); uart_print("\n");

    uart_print("\n[Cenario 7] ls(\"/home\") -- navegacao hierarquica\n");
    ls("/home");

    uart_print("\n[Cenario 6] unlink(\"/home/aluno/notas.txt\")\n");
    uart_print(unlink("/home/aluno/notas.txt") == 0 ? "  arquivo removido com sucesso\n"
                                                      : "  ERRO ao remover arquivo\n");

    uart_print("\n[Cenario 8] Reutilizacao do inode/bloco liberado\n");
    int fd2 = create("/home/aluno/novo.txt");
    if (fd2 >= 0)
    {
        uart_print("  novo arquivo criado (reaproveitando inode/bloco), fd = ");
        uart_print_uint((uint64_t)fd2); uart_print("\n");
        write(fd2, "TreeFS reaproveitado!", 21);
        uart_print("  ls(\"/home/aluno\") apos recriacao:\n");
        ls("/home/aluno");
    }
    else
        uart_print("  ERRO ao recriar arquivo\n");

    uart_print("\n=== Fim da demonstracao do TreeFS ===\n");
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

    treefs_demo();

    uart_print("\n=== Iniciando Scheduler ===\n\n");
    scheduler_start();

    while (1);
}