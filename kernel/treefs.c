#include "treefs.h"
#include "block.h"
#include "inode.h"
#include "string.h"
#include "uart.h"

/* ============================================================
 *  TreeFS - Sistema de Arquivos Hierarquico baseado em Inodes
 *
 *  Arquitetura:
 *    Aplicacoes -> API de Arquivos -> TreeFS -> Driver de Blocos -> Disco Virtual
 *
 *  Este arquivo concentra:
 *    - o Superblock (informacoes globais do sistema de arquivos);
 *    - a resolucao de caminhos (path_lookup);
 *    - a manipulacao de diretorios hierarquicos;
 *    - a tabela de arquivos abertos (fd -> inode + posicao);
 *    - a API publica exigida (fs_init, mkdir, create, unlink, ls,
 *      read, write).
 *
 *  A alocacao de blocos de dados fica no driver (drivers/block.c) e a
 *  alocacao de inodes fica em kernel/inode.c; este arquivo apenas
 *  orquestra os dois para implementar a semantica de arquivos/diretorios.
 * ============================================================ */

static superblock_t sb;

/* Tabela de arquivos abertos (criada por create(), usada por read()/write()) */
typedef struct
{
    int      in_use;
    uint32_t inode_num;
    uint32_t pos;
} ofile_t;

static ofile_t open_files[MAX_OPEN_FILES];

/* ---------------------------------------------------------------
 * E/S generica sobre os dados de um inode (usada tanto por arquivos
 * comuns quanto por diretorios, que sao tratados como "arquivos" cujo
 * conteudo e uma lista de dirent_t).
 * --------------------------------------------------------------- */

static int inode_read_at(inode_t *ino, uint32_t offset, void *buf, uint32_t size)
{
    if (!ino || offset >= ino->size)
        return 0;

    if (offset + size > ino->size)
        size = ino->size - offset;

    uint8_t *out = (uint8_t *)buf;
    uint32_t done = 0;
    uint8_t tmp[BLOCK_SIZE];

    while (done < size)
    {
        uint32_t blk_index = (offset + done) / BLOCK_SIZE;
        uint32_t blk_off   = (offset + done) % BLOCK_SIZE;

        if (blk_index >= ino->nblocks)
            break;

        block_read(ino->blocks[blk_index], tmp);

        uint32_t chunk = BLOCK_SIZE - blk_off;
        if (chunk > size - done)
            chunk = size - done;

        memcpy(out + done, tmp + blk_off, chunk);
        done += chunk;
    }

    return (int)done;
}

static int inode_write_at(inode_t *ino, uint32_t offset, const void *buf, uint32_t size)
{
    if (!ino)
        return -1;

    const uint8_t *in = (const uint8_t *)buf;
    uint32_t done = 0;
    uint8_t tmp[BLOCK_SIZE];

    while (done < size)
    {
        uint32_t blk_index = (offset + done) / BLOCK_SIZE;
        uint32_t blk_off   = (offset + done) % BLOCK_SIZE;

        if (blk_index >= INODE_MAX_BLOCKS)
            break; /* tamanho maximo de arquivo suportado atingido */

        if (blk_index >= ino->nblocks)
        {
            int nb = block_alloc();
            if (nb < 0)
                break; /* disco cheio */
            ino->blocks[ino->nblocks++] = (uint32_t)nb;
        }

        block_read(ino->blocks[blk_index], tmp);

        uint32_t chunk = BLOCK_SIZE - blk_off;
        if (chunk > size - done)
            chunk = size - done;

        memcpy(tmp + blk_off, in + done, chunk);
        block_write(ino->blocks[blk_index], tmp);

        done += chunk;
    }

    if (offset + done > ino->size)
        ino->size = offset + done;

    return (int)done;
}

/* ---------------------------------------------------------------
 * Operacoes sobre diretorios (um diretorio e um inode do tipo DIR cujo
 * conteudo e uma sequencia de dirent_t).
 * --------------------------------------------------------------- */

static int dir_find_entry(inode_t *dir, const char *name, uint32_t *out_inode)
{
    dirent_t entry;
    uint32_t offset = 0;

    while (offset < dir->size)
    {
        inode_read_at(dir, offset, &entry, sizeof(entry));
        if (entry.name[0] != 0 && strcmp(entry.name, name) == 0)
        {
            *out_inode = entry.inode;
            return 0;
        }
        offset += sizeof(entry);
    }
    return -1;
}

static int dir_add_entry(inode_t *dir, const char *name, uint32_t inode_num)
{
    dirent_t entry;
    uint32_t offset = 0;

    /* tenta reaproveitar uma entrada vazia (deixada por um unlink anterior) */
    while (offset < dir->size)
    {
        inode_read_at(dir, offset, &entry, sizeof(entry));
        if (entry.name[0] == 0)
        {
            strcpy(entry.name, name);
            entry.inode = inode_num;
            inode_write_at(dir, offset, &entry, sizeof(entry));
            return 0;
        }
        offset += sizeof(entry);
    }

    /* nao havia entrada vazia: acrescenta ao final do diretorio */
    strcpy(entry.name, name);
    entry.inode = inode_num;
    int n = inode_write_at(dir, dir->size, &entry, sizeof(entry));
    return (n == (int)sizeof(entry)) ? 0 : -1;
}

static int dir_remove_entry(inode_t *dir, const char *name)
{
    dirent_t entry;
    uint32_t offset = 0;

    while (offset < dir->size)
    {
        inode_read_at(dir, offset, &entry, sizeof(entry));
        if (entry.name[0] != 0 && strcmp(entry.name, name) == 0)
        {
            memset(&entry, 0, sizeof(entry));
            inode_write_at(dir, offset, &entry, sizeof(entry));
            return 0;
        }
        offset += sizeof(entry);
    }
    return -1;
}

/* ---------------------------------------------------------------
 * Resolucao de caminhos
 * --------------------------------------------------------------- */

/* Extrai o proximo componente do caminho `*p`, avancando o ponteiro.
 * Retorna 0 quando nao ha mais componentes. */
static int next_component(const char **p, char *out)
{
    while (**p == '/')
        (*p)++;

    if (**p == 0)
        return 0;

    int i = 0;
    while (**p && **p != '/' && i < TREEFS_MAX_NAME - 1)
        out[i++] = *(*p)++;
    out[i] = 0;

    return 1;
}

inode_t *path_lookup(const char *path)
{
    if (!path || path[0] != '/')
        return 0; /* apenas caminhos absolutos sao suportados */

    uint32_t cur = ROOT_INODE_NUM;
    const char *p = path;
    char comp[TREEFS_MAX_NAME];

    while (next_component(&p, comp))
    {
        inode_t *dir = inode_get(cur);
        if (!dir || !dir->used || dir->type != TREEFS_TYPE_DIR)
            return 0;

        uint32_t next;
        if (dir_find_entry(dir, comp, &next) != 0)
            return 0; /* componente do caminho nao encontrado */

        cur = next;
    }

    return inode_get(cur);
}

/* Separa "path" em diretorio pai (`parent`) e nome final (`name`).
 * Ex.: "/home/aluno/notas.txt" -> parent="/home/aluno", name="notas.txt" */
static int split_path(const char *path, char *parent, char *name)
{
    if (!path || path[0] != '/')
        return -1;

    int len = (int)strlen(path);
    if (len == 0)
        return -1;

    int last_slash = -1;
    for (int i = 0; i < len; i++)
        if (path[i] == '/')
            last_slash = i;

    if (last_slash == len - 1)
        return -1; /* caminho termina em "/", sem nome de arquivo/dir */

    strcpy(name, path + last_slash + 1);

    if (last_slash == 0)
        strcpy(parent, "/");
    else
    {
        for (int i = 0; i < last_slash; i++)
            parent[i] = path[i];
        parent[last_slash] = 0;
    }

    return 0;
}

/* ---------------------------------------------------------------
 * Tabela de arquivos abertos
 * --------------------------------------------------------------- */

static int fd_open(uint32_t inode_num)
{
    for (int i = 0; i < MAX_OPEN_FILES; i++)
    {
        if (!open_files[i].in_use)
        {
            open_files[i].in_use    = 1;
            open_files[i].inode_num = inode_num;
            open_files[i].pos       = 0;
            return i;
        }
    }
    return -1; /* tabela de arquivos abertos cheia */
}

int fs_seek(int fd, uint32_t offset)
{
    if (fd < 0 || fd >= MAX_OPEN_FILES || !open_files[fd].in_use)
        return -1;

    open_files[fd].pos = offset;
    return 0;
}

/* ---------------------------------------------------------------
 * API publica
 * --------------------------------------------------------------- */

int fs_init(void)
{
    block_init();

    /* ---- calcula o layout do disco ---- */
    uint32_t inode_table_bytes  = TOTAL_INODES * sizeof(inode_t);
    uint32_t inode_table_blocks = (inode_table_bytes + BLOCK_SIZE - 1) / BLOCK_SIZE;

    uint32_t superblock_block   = 0;
    uint32_t inode_bitmap_block = 1;
    uint32_t block_bitmap_block = 2;
    uint32_t inode_table_block  = 3;
    uint32_t data_start_block   = inode_table_block + inode_table_blocks;
    uint32_t data_block_count   = TOTAL_BLOCKS - data_start_block;

    sb.magic              = TREEFS_MAGIC;
    sb.total_blocks       = TOTAL_BLOCKS;
    sb.total_inodes       = TOTAL_INODES;
    sb.block_size         = BLOCK_SIZE;
    sb.inode_bitmap_block = inode_bitmap_block;
    sb.block_bitmap_block = block_bitmap_block;
    sb.inode_table_block  = inode_table_block;
    sb.inode_table_blocks = inode_table_blocks;
    sb.data_start_block   = data_start_block;

    /* persiste o superblock no bloco 0 */
    uint8_t tmp[BLOCK_SIZE];
    memset(tmp, 0, BLOCK_SIZE);
    memcpy(tmp, &sb, sizeof(sb));
    block_write(superblock_block, tmp);

    /* inicializa os gerenciadores de inodes e de blocos de dados */
    inode_manager_init(inode_bitmap_block, inode_table_block, inode_table_blocks);
    block_manager_init(block_bitmap_block, data_start_block, data_block_count);

    /* zera a tabela de arquivos abertos */
    memset(open_files, 0, sizeof(open_files));

    /* ---- cria a raiz "/" manualmente (nao ha pai para chamar mkdir) ---- */
    inode_t *root = inode_alloc();
    if (!root)
        return -1;
    uint32_t root_idx = inode_index(root); /* deve ser 0 == ROOT_INODE_NUM */

    root->type   = TREEFS_TYPE_DIR;
    root->nlinks = 1;

    dir_add_entry(root, ".", root_idx);
    dir_add_entry(root, "..", root_idx); /* raiz e pai de si mesma */
    inode_sync(root_idx);

    /* ---- estrutura inicial obrigatoria: /home, /tmp, /bin ---- */
    mkdir("/home");
    mkdir("/tmp");
    mkdir("/bin");

    return 0;
}

int mkdir(const char *path)
{
    char parent[TREEFS_MAX_PATH];
    char name[TREEFS_MAX_NAME];

    if (split_path(path, parent, name) != 0)
        return -1;

    inode_t *pdir = path_lookup(parent);
    if (!pdir || pdir->type != TREEFS_TYPE_DIR)
        return -1; /* diretorio pai nao existe */

    uint32_t existing;
    if (dir_find_entry(pdir, name, &existing) == 0)
        return -1; /* ja existe (duplicidade) */

    inode_t *new_dir = inode_alloc();
    if (!new_dir)
        return -1; /* sem inodes livres */
    uint32_t new_idx = inode_index(new_dir);

    new_dir->type   = TREEFS_TYPE_DIR;
    new_dir->nlinks = 1;

    if (dir_add_entry(new_dir, ".", new_idx) != 0 ||
        dir_add_entry(new_dir, "..", inode_index(pdir)) != 0)
    {
        inode_free(new_idx);
        return -1;
    }
    inode_sync(new_idx);

    if (dir_add_entry(pdir, name, new_idx) != 0)
    {
        inode_free(new_idx);
        return -1;
    }
    inode_sync(inode_index(pdir));

    return 0;
}

int create(const char *path)
{
    char parent[TREEFS_MAX_PATH];
    char name[TREEFS_MAX_NAME];

    if (split_path(path, parent, name) != 0)
        return -1;

    inode_t *pdir = path_lookup(parent);
    if (!pdir || pdir->type != TREEFS_TYPE_DIR)
        return -1;

    uint32_t existing;
    if (dir_find_entry(pdir, name, &existing) == 0)
        return -1; /* ja existe (duplicidade) */

    inode_t *file = inode_alloc();
    if (!file)
        return -1;
    uint32_t idx = inode_index(file);

    file->type   = TREEFS_TYPE_FILE;
    file->nlinks = 1;
    inode_sync(idx);

    if (dir_add_entry(pdir, name, idx) != 0)
    {
        inode_free(idx);
        return -1;
    }
    inode_sync(inode_index(pdir));

    /* abre o arquivo recem-criado e devolve o descritor (fd) */
    return fd_open(idx);
}

int unlink(const char *path)
{
    char parent[TREEFS_MAX_PATH];
    char name[TREEFS_MAX_NAME];

    if (split_path(path, parent, name) != 0)
        return -1;

    inode_t *pdir = path_lookup(parent);
    if (!pdir || pdir->type != TREEFS_TYPE_DIR)
        return -1;

    uint32_t idx;
    if (dir_find_entry(pdir, name, &idx) != 0)
        return -1; /* nao encontrado */

    inode_t *ino = inode_get(idx);
    if (!ino || ino->type != TREEFS_TYPE_FILE)
        return -1; /* unlink so remove arquivos */

    for (uint32_t i = 0; i < ino->nblocks; i++)
        block_free(ino->blocks[i]);

    inode_free(idx);

    dir_remove_entry(pdir, name);
    inode_sync(inode_index(pdir));

    /* fecha quaisquer descritores abertos apontando para esse inode */
    for (int i = 0; i < MAX_OPEN_FILES; i++)
        if (open_files[i].in_use && open_files[i].inode_num == idx)
            open_files[i].in_use = 0;

    return 0;
}

int ls(const char *path)
{
    inode_t *dir = path_lookup(path);
    if (!dir || dir->type != TREEFS_TYPE_DIR)
        return -1;

    dirent_t entry;
    uint32_t offset = 0;

    while (offset < dir->size)
    {
        inode_read_at(dir, offset, &entry, sizeof(entry));

        if (entry.name[0] != 0 &&
            strcmp(entry.name, ".") != 0 &&
            strcmp(entry.name, "..") != 0)
        {
            uart_print(entry.name);
            uart_print("\n");
        }

        offset += sizeof(entry);
    }

    return 0;
}

int read(int fd, void *buf, uint32_t size)
{
    if (fd < 0 || fd >= MAX_OPEN_FILES || !open_files[fd].in_use)
        return -1;

    ofile_t *f = &open_files[fd];
    inode_t *ino = inode_get(f->inode_num);
    if (!ino)
        return -1;

    int n = inode_read_at(ino, f->pos, buf, size);
    if (n > 0)
        f->pos += n;

    return n;
}

int write(int fd, const void *buf, uint32_t size)
{
    if (fd < 0 || fd >= MAX_OPEN_FILES || !open_files[fd].in_use)
        return -1;

    ofile_t *f = &open_files[fd];
    inode_t *ino = inode_get(f->inode_num);
    if (!ino)
        return -1;

    int n = inode_write_at(ino, f->pos, buf, size);
    if (n > 0)
    {
        f->pos += n;
        inode_sync(f->inode_num);
    }

    return n;
}
