#include "block.h"
#include "string.h"

/*
 * "Disco Virtual"
 * ---------------
 * Simulado como uma area estatica em memoria (.bss). Ela fica alocada
 * bem abaixo do heap do kmalloc (0x80400000), entao nao ha conflito
 * entre o disco virtual do TreeFS e o alocador de memoria do kernel.
 */
static uint8_t virtual_disk[TOTAL_BLOCKS][BLOCK_SIZE];

/* ---- Bitmap de blocos de dados (cache em memoria, espelha o disco) ---- */
static uint8_t  block_bitmap[TOTAL_BLOCKS / 8];
static uint32_t g_bitmap_block;
static uint32_t g_data_start;
static uint32_t g_data_count;

/* ===================== Driver de Blocos (E/S bruta) ===================== */

void block_init(void)
{
    memset(virtual_disk, 0, sizeof(virtual_disk));
}

int block_read(uint32_t block_num, void *buf)
{
    if (block_num >= TOTAL_BLOCKS || !buf)
        return -1;

    memcpy(buf, virtual_disk[block_num], BLOCK_SIZE);
    return 0;
}

int block_write(uint32_t block_num, const void *buf)
{
    if (block_num >= TOTAL_BLOCKS || !buf)
        return -1;

    memcpy(virtual_disk[block_num], buf, BLOCK_SIZE);
    return 0;
}

uint32_t block_total(void)
{
    return TOTAL_BLOCKS;
}

/* ================= Gerenciamento de Blocos de Dados (bitmap) ============ */

static void block_sync_bitmap(void)
{
    uint8_t tmp[BLOCK_SIZE];
    memset(tmp, 0, BLOCK_SIZE);
    memcpy(tmp, block_bitmap, sizeof(block_bitmap));
    block_write(g_bitmap_block, tmp);
}

void block_manager_init(uint32_t bitmap_block,
                         uint32_t data_start_block,
                         uint32_t data_block_count)
{
    g_bitmap_block = bitmap_block;
    g_data_start   = data_start_block;
    g_data_count   = data_block_count;

    memset(block_bitmap, 0, sizeof(block_bitmap));
    block_sync_bitmap();
}

/* localiza um bloco livre, marca como ocupado e devolve seu numero */
int block_alloc(void)
{
    for (uint32_t i = 0; i < g_data_count; i++)
    {
        uint32_t byte = i / 8;
        uint32_t bit  = i % 8;

        if (!(block_bitmap[byte] & (1u << bit)))
        {
            block_bitmap[byte] |= (1u << bit);
            block_sync_bitmap();

            uint32_t blk = g_data_start + i;

            /* zera o bloco recem-alocado antes de entrega-lo */
            uint8_t zero[BLOCK_SIZE];
            memset(zero, 0, BLOCK_SIZE);
            block_write(blk, zero);

            return (int)blk;
        }
    }
    return -1; /* disco cheio */
}

/* libera um bloco de dados previamente alocado */
void block_free(uint32_t block_num)
{
    if (block_num < g_data_start)
        return;

    uint32_t i = block_num - g_data_start;
    if (i >= g_data_count)
        return;

    uint32_t byte = i / 8;
    uint32_t bit  = i % 8;

    block_bitmap[byte] &= ~(1u << bit);
    block_sync_bitmap();
}
