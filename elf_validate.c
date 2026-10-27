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

static size_t   shdr_size(const t_elf *elf)
{
    if (elf->is_64)
        return (sizeof(Elf64_Shdr));
    return (sizeof(Elf32_Shdr));
}

static void shdr_from_32(Elf64_Shdr *out, const Elf32_Shdr *sh)
{
    out->sh_name = sh->sh_name;
    out->sh_type = sh->sh_type;
    out->sh_flags = sh->sh_flags;
    out->sh_addr = sh->sh_addr;
    out->sh_offset = sh->sh_offset;
    out->sh_size = sh->sh_size;
    out->sh_link = sh->sh_link;
    out->sh_info = sh->sh_info;
    out->sh_addralign = sh->sh_addralign;
    out->sh_entsize = sh->sh_entsize;
}

/*
    Copies section header index into out, widening a 32-bit one
    It returns 0 on invalid
*/
int read_shdr(const t_elf *elf, uint64_t i, Elf64_Shdr *out)
{
    const uint8_t   *table;
    Elf32_Shdr      sh32;
    size_t          size;

    size = shdr_size(elf);
    if (i >= elf->size / size)
        return (0);
    table = elf_ptr(elf, elf->ehdr.e_shoff, (i + 1) * size);
    if (table == NULL)
        return (0);
    if (elf->is_64)
        memcpy(out, table + i * size, sizeof(*out));
    else
    {
        memcpy(&sh32, table + i * size, sizeof(sh32));
        shdr_from_32(out, &sh32);
    }
    return (1);
}

/*
    With 0xff00 sections or more, the real counts live in section header 0
*/
static int  resolve_extended(t_elf *elf)
{
    Elf64_Shdr  sh0;

    if (!read_shdr(elf, 0, &sh0))
        return (0);
    elf->shnum = elf->ehdr.e_shnum;
    if (elf->shnum == 0)
        elf->shnum = sh0.sh_size;
    elf->shstrndx = elf->ehdr.e_shstrndx;
    if (elf->shstrndx == SHN_XINDEX)
        elf->shstrndx = sh0.sh_link;
    elf->phnum = elf->ehdr.e_phnum;
    if (elf->phnum == PN_XNUM && sh0.sh_info != 0)
        elf->phnum = sh0.sh_info;
    return (1);
}

/*
    No sections or a wrong e_shentsize is invalid, and both header 
    tables must fit in the file 
    It returns 0 on invalid
*/
int check_shdr_table(t_elf *elf)
{
    size_t  phdr_size;

    if (!resolve_extended(elf))
        return (0);
    if (elf->shnum == 0 || elf->ehdr.e_shentsize != shdr_size(elf))
        return (0);
    if (!elf_table(elf, elf->ehdr.e_shoff, elf->shnum, shdr_size(elf)))
        return (0);
    phdr_size = sizeof(Elf32_Phdr);
    if (elf->is_64)
        phdr_size = sizeof(Elf64_Phdr);
    if (elf->phnum != 0 && !elf_table(elf, elf->ehdr.e_phoff, elf->phnum, phdr_size))
        return (0);
    return (1);
}
