#include "inode.h"
#include "block.h"
#include "string.h"

/*
 * Area de Inodes (cache em memoria, espelhada no disco virtual a cada
 * alteracao) + Bitmap de Inodes (controla quais inodes estao livres /
 * ocupados).
 */
static inode_t inode_table[TOTAL_INODES];
static uint8_t inode_bitmap[(TOTAL_INODES + 7) / 8];

static uint32_t g_inode_bitmap_block;
static uint32_t g_inode_table_block;
static uint32_t g_inode_table_blocks; /* nao usado diretamente, mantido para depuracao */

/* Escreve `size` bytes de `data` na regiao do disco que comeca no bloco
 * `start_block`, no deslocamento `region_offset` (em bytes) dentro dessa
 * regiao. Faz leitura-modificacao-escrita bloco a bloco, permitindo que
 * a area escrita atravesse mais de um bloco quando necessario. */
static void region_write(uint32_t start_block, uint32_t region_offset,
                          const void *data, uint32_t size)
{
    const uint8_t *src = (const uint8_t *)data;
    uint32_t done = 0;
    uint8_t tmp[BLOCK_SIZE];

    while (done < size)
    {
        uint32_t abs_off = region_offset + done;
        uint32_t blk      = start_block + abs_off / BLOCK_SIZE;
        uint32_t blk_off  = abs_off % BLOCK_SIZE;
        uint32_t chunk    = BLOCK_SIZE - blk_off;

        if (chunk > size - done)
            chunk = size - done;

        block_read(blk, tmp);
        memcpy(tmp + blk_off, src + done, chunk);
        block_write(blk, tmp);

        done += chunk;
    }
}

static void inode_sync_bitmap(void)
{
    uint8_t tmp[BLOCK_SIZE];
    memset(tmp, 0, BLOCK_SIZE);
    memcpy(tmp, inode_bitmap, sizeof(inode_bitmap));
    block_write(g_inode_bitmap_block, tmp);
}

void inode_manager_init(uint32_t inode_bitmap_block,
                         uint32_t inode_table_block,
                         uint32_t inode_table_blocks)
{
    g_inode_bitmap_block = inode_bitmap_block;
    g_inode_table_block  = inode_table_block;
    g_inode_table_blocks = inode_table_blocks;

    memset(inode_bitmap, 0, sizeof(inode_bitmap));
    memset(inode_table, 0, sizeof(inode_table));

    inode_sync_bitmap();

    /* persiste a tabela de inodes zerada no disco virtual */
    for (uint32_t i = 0; i < TOTAL_INODES; i++)
        inode_sync(i);
}

inode_t *inode_alloc(void)
{
    for (uint32_t i = 0; i < TOTAL_INODES; i++)
    {
        uint32_t byte = i / 8;
        uint32_t bit  = i % 8;

        if (!(inode_bitmap[byte] & (1u << bit)))
        {
            inode_bitmap[byte] |= (1u << bit);
            inode_sync_bitmap();

            inode_t *ino = &inode_table[i];
            memset(ino, 0, sizeof(inode_t));
            ino->used = 1;

            inode_sync(i);
            return ino;
        }
    }
    return 0; /* nenhum inode livre */
}

void inode_free(uint32_t idx)
{
    if (idx >= TOTAL_INODES)
        return;

    uint32_t byte = idx / 8;
    uint32_t bit  = idx % 8;

    inode_bitmap[byte] &= ~(1u << bit);
    inode_sync_bitmap();

    memset(&inode_table[idx], 0, sizeof(inode_t));
    inode_sync(idx);
}

inode_t *inode_get(uint32_t idx)
{
    if (idx >= TOTAL_INODES)
        return 0;
    return &inode_table[idx];
}

uint32_t inode_index(inode_t *ino)
{
    return (uint32_t)(ino - inode_table);
}

void inode_sync(uint32_t idx)
{
    if (idx >= TOTAL_INODES)
        return;

    region_write(g_inode_table_block, idx * sizeof(inode_t),
                 &inode_table[idx], sizeof(inode_t));
}
