#include "memory.h"
#include <stdint.h>

/* Configuração do heap */

#define HEAP_START  0x80400000UL
#define HEAP_SIZE   (64 * 1024)        /* 64 KB */

/* Cabeçalho de bloc  */

typedef struct block
{
    uint64_t      size;    /* bytes de dados (sem contar o header)  */
    int           free;    /* 1 = livre, 0 = ocupado                */
    struct block *next;    /* próximo bloco na lista encadeada      */
} block_t;

#define HEADER_SIZE  sizeof(block_t)   /* 24 bytes em RV64 */

/* Raiz da lista */

static block_t *heap_list = (block_t *)HEAP_START;

/* memory_init */

void memory_init(void)
{
    /* Heap começa como um único bloco livre cobrindo tudo */
    heap_list->size = HEAP_SIZE - HEADER_SIZE;
    heap_list->free = 1;
    heap_list->next = 0;
}

/* kmalloc — First Fit */

void *kmalloc(uint64_t size)
{
    if (size == 0)
        return 0;

    /* Alinhamento de 8 bytes */
    size = (size + 7) & ~(uint64_t)7;

    for (block_t *cur = heap_list; cur; cur = cur->next)
    {
        if (!cur->free || cur->size < size)
            continue;

        /* Divisão: só divide se sobrar espaço para header + mínimo de 8 bytes */
        if (cur->size >= size + HEADER_SIZE + 8)
        {
            block_t *split  = (block_t *)((uint8_t *)cur + HEADER_SIZE + size);
            split->size     = cur->size - size - HEADER_SIZE;
            split->free     = 1;
            split->next     = cur->next;

            cur->size = size;
            cur->next = split;
        }

        cur->free = 0;
        return (void *)((uint8_t *)cur + HEADER_SIZE);
    }

    return 0;   /* out of memory */
}

/*  kfree — com coalescência  */

void kfree(void *ptr)
{
    if (!ptr)
        return;

    /* Recupera o cabeçalho a partir do ponteiro de dados */
    block_t *blk = (block_t *)((uint8_t *)ptr - HEADER_SIZE);
    blk->free = 1;

    /* Coalescência: une blocos livres adjacentes */
    for (block_t *cur = heap_list; cur; )
    {
        if (cur->free && cur->next && cur->next->free)
        {
            /* Absorve o próximo bloco (header + dados) */
            cur->size += HEADER_SIZE + cur->next->size;
            cur->next  = cur->next->next;
            /* Não avança: pode coalescer de novo com o novo next */
        }
        else
        {
            cur = cur->next;
        }
    }
}

/*  Estatísticas  */

uint64_t memory_used(void)
{
    uint64_t used = 0;
    for (block_t *c = heap_list; c; c = c->next)
        if (!c->free)
            used += c->size;
    return used;
}

uint64_t memory_free(void)
{
    /* Complemento: total - usado (inclui overhead de headers como "disponível") */
    return HEAP_SIZE - memory_used();
}

uint64_t memory_total(void)
{
    return HEAP_SIZE;
}