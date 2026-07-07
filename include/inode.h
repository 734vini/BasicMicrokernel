#pragma once
#include <stdint.h>

#define INODE_MAX_BLOCKS  12   /* blocos diretos por inode (sem blocos indiretos) */
#define TOTAL_INODES      64   /* quantidade de inodes suportados pelo TreeFS */

#define TREEFS_TYPE_FREE  0
#define TREEFS_TYPE_FILE  1
#define TREEFS_TYPE_DIR   2

/*
 * Inode: guarda os METADADOS de um arquivo ou diretorio (nunca os dados
 * em si, que ficam nos blocos apontados por `blocks[]`).
 */
typedef struct
{
    uint32_t used;                     /* 1 = inode alocado, 0 = livre        */
    uint32_t type;                     /* TREEFS_TYPE_FILE ou TREEFS_TYPE_DIR */
    uint32_t size;                     /* tamanho em bytes                     */
    uint32_t nlinks;                   /* quantidade de referencias            */
    uint32_t nblocks;                  /* quantos blocos de `blocks[]` estao em uso */
    uint32_t blocks[INODE_MAX_BLOCKS]; /* lista de blocos de dados (diretos)   */
} inode_t;

/* Inicializa a Area de Inodes + Bitmap de Inodes a partir do layout calculado por fs_init() */
void      inode_manager_init(uint32_t inode_bitmap_block,
                              uint32_t inode_table_block,
                              uint32_t inode_table_blocks);

/* Localiza um inode livre, reserva e devolve o ponteiro (cache em memoria) */
inode_t  *inode_alloc(void);

/* Libera um inode removido (limpa metadados e marca como livre no bitmap) */
void      inode_free(uint32_t idx);

/* Acesso a um inode pelo indice (com checagem de limites) */
inode_t  *inode_get(uint32_t idx);

/* Converte um ponteiro de inode (dentro da tabela em cache) de volta para seu indice */
uint32_t  inode_index(inode_t *ino);

/* Persiste um inode (metadados) da cache em memoria para o disco virtual */
void      inode_sync(uint32_t idx);
