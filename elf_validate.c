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
    Returns string index of a string table, NULL when the table
    is outside the file or not '\0'-terminated.
    Index 0 is always "", even if no table is present.
*/
const char  *elf_str(const t_elf *elf, const Elf64_Shdr *strtab, uint64_t idx)
{
    const char  *str;

    if (idx == 0)
        return ("");
    if (strtab->sh_type != SHT_STRTAB && strtab->sh_type < SHT_LOOS)
        return (NULL);
    
    str = elf_ptr(elf, strtab->sh_offset, strtab->sh_size);
    if (str == NULL || idx >= strtab->sh_size
        || str[strtab->sh_size - 1] != '\0')
        return (NULL);
    
    return (str + idx);
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

/*
    sh_link must name an existing section, and so must sh_info when it is
    an index
*/
static int  links_are_valid(const t_elf *elf, const Elf64_Shdr *sh)
{
    if (sh->sh_link >= elf->shnum)
        return (0);
    if (sh->sh_type != SHT_REL && sh->sh_type != SHT_RELA
        && !(sh->sh_flags & SHF_INFO_LINK))
        return (1);
    return (sh->sh_info < elf->shnum);
}

/*
    A bad link is invalid, a section outside the file is only a warning,
    section 0 included.
    It returns 0 on invalid.
*/
int check_sections(t_elf *elf)
{
    Elf64_Shdr  sh;
    uint64_t    i;

    i = 0;
    while (i < elf->shnum)
    {
        if (!read_shdr(elf, i, &sh) || (i > 0 && !links_are_valid(elf, &sh)))
            return (0);
        if (sh.sh_type != SHT_NOBITS && !elf_ptr(elf, sh.sh_offset, sh.sh_size))
            elf->warnings |= W_PAST_EOF;
        i++;
    }
    return (1);
}

static size_t   sym_size(const t_elf *elf)
{
    if (elf->is_64)
        return (sizeof(Elf64_Sym));
    return (sizeof(Elf32_Sym));
}

/*
    Every SHT_SYMTAB needs entries of the right size, and sh_info 
    inside it unless it is empty. The first one wins.
    It returns 0 on invalid
*/
static int  pick_symtab(t_elf *elf, const Elf64_Shdr *sh, uint64_t i)
{
    if (sh->sh_entsize != sym_size(elf))
        return (0);
    if (sh->sh_size != 0 && sh->sh_info * sh->sh_entsize > sh->sh_size)
        return (0);
    if (elf->symtab_idx == 0)
        elf->symtab_idx = i;
    else if (elf->ignored_symtab == 0)
    {
        elf->warnings |= W_MULTI_SYMTAB;
        elf->ignored_symtab = i;
    }
    return (1);
}

/*
    It returns 0 on invalid
*/
static int  check_type(const t_elf *elf, const Elf64_Shdr *sh)
{
    if (sh->sh_type == SHT_REL)
        return (sh->sh_entsize == (elf->is_64 ? sizeof(Elf64_Rel) : sizeof(Elf32_Rel)));
    if (sh->sh_type == SHT_RELA)
        return (sh->sh_entsize == (elf->is_64 ? sizeof(Elf64_Rela) : sizeof(Elf32_Rela)));
    if (sh->sh_type == SHT_GNU_versym)
        return (sh->sh_entsize == 2);
    if (sh->sh_type == SHT_DYNSYM)
        return (sh->sh_entsize == sym_size(elf)
            && (sh->sh_size == 0 || sh->sh_info * sh->sh_entsize <= sh->sh_size));
    if (sh->sh_type == SHT_NOTE)
        return (sh->sh_size == 0 || elf_ptr(elf, sh->sh_offset, sh->sh_size) != NULL);
    return (1);
}

/*
    The name must resolve, then the first SHT_SYMTAB wins.
    It returns 0 on invalid
*/
static int  scan_sections(t_elf *elf, const Elf64_Shdr *shstrtab)
{
    Elf64_Shdr  sh;
    uint64_t    i;

    i = 1;
    while (i < elf->shnum)
    {
        if (!read_shdr(elf, i, &sh) || !elf_str(elf, shstrtab, sh.sh_name))
            return (0);
        if (sh.sh_type == SHT_SYMTAB && !pick_symtab(elf, &sh, i))
            return (0);
        if (!check_type(elf, &sh))
            return (0);
        i++;
    }
    return (1);
}

/*
    A bad e_shstrndx is only a warning.
    Index 0 also means no sections, but without the warning
*/
t_status    check_shstrtab(t_elf *elf)
{
    Elf64_Shdr  shstrtab;

    if (elf->shstrndx >= elf->shnum || !read_shdr(elf, elf->shstrndx, &shstrtab)
        || shstrtab.sh_type != SHT_STRTAB)
    {
        elf->warnings |= W_SHSTRNDX;
        return (NM_NO_SYMBOLS);
    }
    if (elf->shstrndx == 0)
        return (NM_NO_SYMBOLS);
    if (!scan_sections(elf, &shstrtab))
        return (NM_FORMAT);
    return (NM_OK);
}

/*
    With no entry it reads nothing, otherwise entries outside 
    the file are fatal
*/
t_status    check_symtab(t_elf *elf)
{
    Elf64_Shdr  sh;
    uint64_t    count;

    if (elf->symtab_idx == 0 || !read_shdr(elf, elf->symtab_idx, &sh))
        return (NM_NO_SYMBOLS);
    count = sh.sh_size / sh.sh_entsize;
    if (count == 0)
        return (NM_NO_SYMBOLS);
    if (!elf_table(elf, sh.sh_offset, count, sh.sh_entsize))
        return (NM_SYMTAB_PAST_EOF);
    if (count == 1)
        return (NM_NO_SYMBOLS);
    return (NM_OK);
}

/*
    Runs every check in order, each one only reads what the previous ones
    proved is inside the file
*/
t_status    validate_elf(t_elf *elf)
{
    int         elf_class;
    t_status    status;

    elf_class = check_ident(elf);
    if (elf_class == 0 || !read_ehdr(elf, elf_class) || !check_shdr_table(elf)
        || !check_sections(elf))
        return (NM_FORMAT);
    status = check_shstrtab(elf);
    if (status != NM_OK)
        return (status);
    return (check_symtab(elf));
}
