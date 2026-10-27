#ifndef FT_NM_H
# define FT_NM_H

# include <stddef.h>
# include <stdint.h>
# include <elf.h>


typedef struct s_elf {

    uint8_t		*buf;
    size_t		size;

    Elf64_Ehdr  ehdr;
    int         is_64;
}   t_elf;

/* elf_load.c */
int load_file(char *path, t_elf *elf);

/* elf_validate.c */
const void  *elf_ptr(const t_elf *elf, uint64_t off, uint64_t len);
const void  *elf_table(const t_elf *elf, uint64_t off, uint64_t count, uint64_t entsize);
int         check_ident(const t_elf *elf);
int         read_ehdr(t_elf *elf, int elf_class);

#endif