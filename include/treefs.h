#pragma once
#include <stdint.h>
#include "inode.h"

#define TREEFS_MAGIC     0x54524546u  /* "TREF" */
#define TREEFS_MAX_NAME  28           /* tamanho maximo de um nome (arquivo/dir) */
#define TREEFS_MAX_PATH  128          /* tamanho maximo de um caminho absoluto   */
#define MAX_OPEN_FILES   16           /* tamanho da tabela de arquivos abertos   */
#define ROOT_INODE_NUM   0            /* numero do inode da raiz "/"            */

/* Entrada de diretorio: nome + inode associado (conforme pedido no PDF) */
typedef struct
{
    char     name[TREEFS_MAX_NAME];
    uint32_t inode;
} dirent_t;

/* Superblock: informacoes globais do sistema de arquivos */
typedef struct
{
    uint32_t magic;
    uint32_t total_blocks;
    uint32_t total_inodes;
    uint32_t block_size;
    uint32_t inode_bitmap_block;
    uint32_t block_bitmap_block;
    uint32_t inode_table_block;
    uint32_t inode_table_blocks;
    uint32_t data_start_block;
} superblock_t;

/* ================= API de Arquivos (obrigatoria pelo enunciado) ================= */
int      fs_init(void);
inode_t *path_lookup(const char *path);
int      mkdir(const char *path);
int      create(const char *path);
int      unlink(const char *path);
int      ls(const char *path);
int      read(int fd, void *buf, uint32_t size);
int      write(int fd, const void *buf, uint32_t size);

/* ================= Extra (nao exigido pelo enunciado) =================
 * Reposiciona o cursor de leitura/escrita de um descritor de arquivo.
 * Foi adicionado porque a especificacao nao define open()/lseek(), mas
 * sem isso nao seria possivel reler um arquivo logo apos escreve-lo
 * usando o mesmo fd (o cursor ficaria no final do arquivo). */
int      fs_seek(int fd, uint32_t offset);
