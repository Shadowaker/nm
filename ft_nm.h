#ifndef FT_NM_H
# define FT_NM_H

# include <stddef.h>
# include <stdint.h>
# include <elf.h>

/*
    Bitmask, always power of 2 to not overwrite themselves.
    All of them together are 111
*/
# define W_PAST_EOF         1   // 001
# define W_SHSTRNDX         2   // 010
# define W_MULTI_SYMTAB     4   // 100

typedef enum e_status
{
    NM_OK,
    NM_FORMAT,
    NM_NO_SYMBOLS,
    NM_SYMTAB_PAST_EOF      // nm handle this as a fatal
}   t_status;


typedef struct s_elf {

    uint8_t		*buf;
    size_t		size;

    Elf64_Ehdr  ehdr;
    int         is_64;
    uint64_t    shnum;
    uint64_t    shstrndx;
    uint64_t    phnum;

    int         warnings;
    uint64_t    ignored_symtab;
}   t_elf;

/* elf_load.c */
int load_file(char *path, t_elf *elf);

/* elf_validate.c */
const void  *elf_ptr(const t_elf *elf, uint64_t off, uint64_t len);
const void  *elf_table(const t_elf *elf, uint64_t off, uint64_t count, uint64_t entsize);
int         check_ident(const t_elf *elf);
int         read_ehdr(t_elf *elf, int elf_class);
int         read_shdr(const t_elf *elf, uint64_t idx, Elf64_Shdr *out);
int         check_shdr_table(t_elf *elf);
t_status    validate_elf(t_elf *elf);

/* elf_report.c */
void        print_warnings(const char *path, const t_elf *elf);
void        print_status(const char *path, t_status status);

#endif