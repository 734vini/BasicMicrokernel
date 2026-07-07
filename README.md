# BasicMicrokernel
A simple microkernel to use in OS subject and run in CodeSpace.

## TreeFS — Sistema de Arquivos Hierárquico (Avaliação M3)

Este fork adiciona ao microkernel um sistema de arquivos hierárquico baseado
em inodes, chamado **TreeFS**, seguindo a arquitetura:

```
Aplicações -> API de Arquivos -> TreeFS -> Driver de Blocos -> Disco Virtual
```

### Arquivos novos

| Arquivo               | Responsabilidade                                                        |
|------------------------|--------------------------------------------------------------------------|
| `include/block.h` / `drivers/block.c` | Driver de blocos: E/S bruta sobre o "disco virtual" (RAM) + bitmap de blocos de dados (`block_alloc`/`block_free`) |
| `include/inode.h` / `kernel/inode.c`  | Área e bitmap de inodes (`inode_alloc`/`inode_free`/`inode_sync`)         |
| `include/treefs.h` / `kernel/treefs.c`| Superblock, diretórios hierárquicos, resolução de caminhos e a API pública (`fs_init`, `path_lookup`, `mkdir`, `create`, `unlink`, `ls`, `read`, `write`) |

### Arquivos modificados

- `kernel/string.c`: implementadas `memcpy`, `strcmp`, `strcpy` e `strlen` (as assinaturas já existiam em `include/string.h`, mas faltava a implementação, necessária ao TreeFS).
- `kernel/main.c`: adicionada `treefs_demo()`, que formata o disco e executa todos os cenários de validação do TreeFS antes de iniciar o escalonador.
- `Makefile`: adicionadas as regras de build para `block.o`, `inode.o` e `treefs.o`.

### Como compilar e executar

```bash
sudo apt install gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf qemu-system-misc
make
make qemu
```
