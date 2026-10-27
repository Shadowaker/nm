#include <string.h>
#include "ft_nm.h"


const void  *elf_ptr(const t_elf *elf, uint64_t off, uint64_t len)
{
    if (off > elf->size || len > elf->size - off)
        return (NULL);
    return (elf->buf + off);
}

const void  *elf_table(const t_elf *elf, uint64_t off, uint64_t count, uint64_t entsize)
{
    if (entsize == 0 || count > elf->size / entsize)
        return (NULL);
    return (elf_ptr(elf, off, count * entsize));
}

/*
    It returns 0 on invalid
*/
int check_ident(const t_elf *elf)
{
    const unsigned char *id;

    id = elf_ptr(elf, 0, EI_NIDENT);
    if (id == NULL || memcmp(id, ELFMAG, SELFMAG) != 0)
        return (0);
    if (id[EI_CLASS] != ELFCLASS32 && id[EI_CLASS] != ELFCLASS64)
        return (0);
    if (id[EI_DATA] != ELFDATA2LSB || id[EI_VERSION] != EV_CURRENT)
        return (0);
    return (id[EI_CLASS]);
}
