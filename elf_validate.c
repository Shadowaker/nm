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
    Check if the class identity is right.
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

static void ehdr_from_32(Elf64_Ehdr *out, const Elf32_Ehdr *eh)
{
    memcpy(out->e_ident, eh->e_ident, EI_NIDENT);
    out->e_type = eh->e_type;
    out->e_machine = eh->e_machine;
    out->e_version = eh->e_version;
    out->e_entry = eh->e_entry;
    out->e_phoff = eh->e_phoff;
    out->e_shoff = eh->e_shoff;
    out->e_flags = eh->e_flags;
    out->e_ehsize = eh->e_ehsize;
    out->e_phentsize = eh->e_phentsize;
    out->e_phnum = eh->e_phnum;
    out->e_shentsize = eh->e_shentsize;
    out->e_shnum = eh->e_shnum;
    out->e_shstrndx = eh->e_shstrndx;
}

/*
    Copies the ELF header into elf->ehdr, widening a 32-bit one.
    It returns 0 on invalid
*/
int read_ehdr(t_elf *elf, int elf_class)
{
    const void  *p;
    Elf32_Ehdr  eh32;
    size_t      size;

    elf->is_64 = (elf_class == ELFCLASS64);
    size = sizeof(Elf32_Ehdr);
    if (elf->is_64)
        size = sizeof(Elf64_Ehdr);
    p = elf_ptr(elf, 0, size);
    if (p == NULL)
        return (0);
    if (elf->is_64)
        memcpy(&elf->ehdr, p, sizeof(elf->ehdr));
    else
    {
        memcpy(&eh32, p, sizeof(eh32));
        ehdr_from_32(&elf->ehdr, &eh32);
    }
    if (elf->ehdr.e_type == ET_CORE || elf->ehdr.e_shoff < size)
        return (0);
    return (1);
}
