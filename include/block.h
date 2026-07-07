#pragma once
#include <stdint.h>

/*
 * Driver de Blocos (camada mais baixa da arquitetura):
 *
 *   Aplicacoes -> API de Arquivos -> TreeFS -> Driver de Blocos -> Disco Virtual
 *
 * Este modulo tem duas responsabilidades:
 *
 *   1) Acesso bruto ao "disco virtual" (um vetor estatico em memoria que
 *      simula um dispositivo de bloco, ja que o microkernel ainda nao
 *      possui um driver de disco real / virtio-blk).
 *
 *   2) Gerenciamento dos blocos de DADOS atraves de um bitmap (localizar,
 *      reservar e liberar blocos livres), conforme pedido na secao
 *      "Gerenciamento de Blocos" da atividade.
 */

#define BLOCK_SIZE    512u
#define TOTAL_BLOCKS  512u   /* 512 * 512B = 256 KB de disco virtual */

/* ---- Disco virtual / E/S bruta de blocos ---- */
void     block_init(void);
int      block_read(uint32_t block_num, void *buf);
int      block_write(uint32_t block_num, const void *buf);
uint32_t block_total(void);

/* ---- Gerenciamento de blocos de dados (bitmap) ---- */
void     block_manager_init(uint32_t bitmap_block,
                             uint32_t data_start_block,
                             uint32_t data_block_count);
int      block_alloc(void);              /* retorna numero do bloco alocado, ou -1 se cheio */
void     block_free(uint32_t block_num);
