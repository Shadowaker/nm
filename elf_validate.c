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
