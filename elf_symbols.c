#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ft_nm.h"

static void sym_from_32(Elf64_Sym *out, const Elf32_Sym *s)
{
    out->st_name = s->st_name;
    out->st_info = s->st_info;
    out->st_other = s->st_other;
    out->st_shndx = s->st_shndx;
    out->st_value = s->st_value;
    out->st_size = s->st_size;
}

/*
    Copies symbol i into out, widening a 32-bit one.
    It returns 0 on invalid.
*/
static int  read_sym(const t_elf *elf, const Elf64_Shdr *symtab, uint64_t i, Elf64_Sym *out)
{
    const void  *p;
    Elf32_Sym   s32;

    p = elf_ptr(elf, symtab->sh_offset + i * symtab->sh_entsize, symtab->sh_entsize);
    if (p == NULL)
        return (0);
    if (elf->is_64)
        memcpy(out, p, sizeof(*out));
    else
    {
        memcpy(&s32, p, sizeof(s32));
        sym_from_32(out, &s32);
    }
    return (1);
}

/*
    Relocations that apply to another section through the symbol table
*/
static int  applies_relocs(const t_elf *elf, const Elf64_Shdr *sh)
{
    Elf64_Shdr  target;

    if (sh->sh_link != elf->symtab_idx || sh->sh_info == 0
        || !read_shdr(elf, sh->sh_info, &target))
        return (0);
    if ((elf->ehdr.e_type == ET_EXEC || elf->ehdr.e_type == ET_DYN)
        && (sh->sh_flags & SHF_ALLOC))
        return (0);
    return (target.sh_type != SHT_REL && target.sh_type != SHT_RELA);
}

static int  has_own_section(const t_elf *elf, const t_tables *t, uint64_t idx,
    const Elf64_Shdr *sh)
{
    uint32_t    type;

    type = sh->sh_type;
    if (type == SHT_STRTAB)
        return (idx != elf->shstrndx && idx != t->symtab.sh_link);
    if (type == SHT_REL || type == SHT_RELA)
        return (!applies_relocs(elf, sh));
    if (type == SHT_X86_64_UNWIND && elf->ehdr.e_machine == EM_X86_64)
        return (1);
    if (type >= SHT_LOUSER)
        return (!(sh->sh_flags & SHF_ALLOC));
    if (type >= SHT_LOOS && type <= SHT_HIOS)
        return (1);
    return (type == SHT_PROGBITS || type == SHT_NOBITS || type == SHT_HASH
        || type == SHT_DYNAMIC || type == SHT_NOTE || type == SHT_DYNSYM
        || type == SHT_INIT_ARRAY || type == SHT_FINI_ARRAY
        || type == SHT_PREINIT_ARRAY || type == SHT_GROUP);
}

static int  is_debug_name(const char *name)
{
    return (strncmp(name, ".debug", 6) == 0 || strncmp(name, ".zdebug", 7) == 0
        || strncmp(name, ".gnu.debuglto_.debug_", 21) == 0
        || strncmp(name, ".gnu.linkonce.wi.", 17) == 0
        || strncmp(name, ".line", 5) == 0 || strncmp(name, ".stab", 5) == 0
        || strcmp(name, ".gdb_index") == 0);
}

/*
    From nm's order: code, loaded data, no contents, debugging, read-only
*/
static char section_letter(const Elf64_Shdr *sh, const char *name)
{
    if (sh->sh_flags & SHF_EXECINSTR)
        return ('t');
    if ((sh->sh_flags & SHF_ALLOC) && sh->sh_type != SHT_NOBITS)
        return ((sh->sh_flags & SHF_WRITE) ? 'd' : 'r');
    if (sh->sh_type == SHT_NOBITS)
        return ('b');
    if (is_debug_name(name))
        return ('N');
    if (!(sh->sh_flags & SHF_WRITE))
        return ('n');
    return ('?');
}

/*
    The lowercase letter of a defined symbol's section, 'a' when absolute.
    In a relocatable file the value is relative, so the section address
    is added
*/
static char section_class(const t_elf *elf, const t_tables *t, const Elf64_Sym *sym,
    uint64_t *value)
{
    Elf64_Shdr  sh;
    const char  *name;

    if (sym->st_shndx >= SHN_LORESERVE || sym->st_shndx >= elf->shnum
        || !read_shdr(elf, sym->st_shndx, &sh) || !has_own_section(elf, t, sym->st_shndx, &sh))
        return ('a');
    if (elf->ehdr.e_type != ET_EXEC && elf->ehdr.e_type != ET_DYN)
        *value += sh.sh_addr;
    name = elf_str(elf, &t->shstrtab, sh.sh_name);
    if (name == NULL)
        name = "";
    return (section_letter(&sh, name));
}

/*
    From nm's order: common, undefined, ifunc, weak, unique, then the
    section's letter, uppercase for globals
*/
static char sym_letter(const t_elf *elf, const t_tables *t, const Elf64_Sym *sym,
    uint64_t *value)
{
    unsigned char   bind;
    int             object;
    char            c;

    bind = ELF64_ST_BIND(sym->st_info);
    object = (ELF64_ST_TYPE(sym->st_info) == STT_OBJECT
        || ELF64_ST_TYPE(sym->st_info) == STT_COMMON);
    if (sym->st_shndx == SHN_COMMON || (sym->st_shndx == SHN_X86_64_LCOMMON
        && elf->ehdr.e_machine == EM_X86_64))
    {
        *value = sym->st_size;
        return ('C');
    }
    if (sym->st_shndx == SHN_UNDEF && bind == STB_WEAK)
        return (object ? 'v' : 'w');
    if (sym->st_shndx == SHN_UNDEF)
        return ('U');
    c = section_class(elf, t, sym, value);
    if (ELF64_ST_TYPE(sym->st_info) == STT_GNU_IFUNC)
        return ('i');
    if (bind == STB_WEAK)
        return (object ? 'V' : 'W');
    if (bind == STB_GNU_UNIQUE)
        return ('u');
    if (bind != STB_LOCAL && bind != STB_GLOBAL)
        return ('?');
    if (bind == STB_GLOBAL)
        c = toupper(c);
    return (c);
}

/*
    Section and file symbols are debugging symbols. Nm hides them.
    It returns 0 for a symbol to skip.
*/
static int  fill_sym(const t_elf *elf, const t_tables *t, const Elf64_Sym *sym, t_sym *out)
{
    unsigned char   type;

    type = ELF64_ST_TYPE(sym->st_info);
    if (type == STT_SECTION || type == STT_FILE)
        return (0);
    out->name = elf_str(elf, &t->strtab, sym->st_name);
    if (out->name == NULL)
        out->name = "(null)";
    out->value = sym->st_value;
    out->type = sym_letter(elf, t, sym, &out->value);
    return (1);
}

/*
    Like nm by name in the user's locale, then byte by byte. 
    Equal names are left to qsort.
*/
static int  compare_syms(const void *a, const void *b)
{
    const t_sym *x;
    const t_sym *y;
    int         r;

    x = a;
    y = b;
    r = strcoll(x->name, y->name);
    if (r != 0)
        return (r);
    return (strcmp(x->name, y->name));
}

static void print_sym(const t_elf *elf, const t_sym *s)
{
    int width;

    width = 8;
    if (elf->is_64)
        width = 16;
    if (s->type == 'U' || s->type == 'w' || s->type == 'v')
        printf("%*s %c %s\n", width, "", s->type, s->name);
    else
        printf("%0*llx %c %s\n", width, (unsigned long long)s->value, s->type, s->name);
}

/*
    Entry 0 is always null.
    It returns -1 when out of memory
*/
int print_symbols(const char *path, const t_elf *elf)
{
    t_tables    t;
    t_sym       *syms;
    Elf64_Sym   sym;
    uint64_t    count;
    uint64_t    n;
    uint64_t    i;

    if (!read_shdr(elf, elf->symtab_idx, &t.symtab) || !read_shdr(elf, t.symtab.sh_link, &t.strtab)
        || !read_shdr(elf, elf->shstrndx, &t.shstrtab))
        return (0);
    count = t.symtab.sh_size / t.symtab.sh_entsize;
    syms = malloc((count - 1) * sizeof(*syms));
    if (syms == NULL)
    {
        fflush(stdout);
        fprintf(stderr, "ft_nm: %s: %s\n", path, strerror(ENOMEM));
        return (-1);
    }
    n = 0;
    i = 1;
    while (i < count)
    {
        if (read_sym(elf, &t.symtab, i, &sym) && fill_sym(elf, &t, &sym, &syms[n]))
            n++;
        i++;
    }
    qsort(syms, n, sizeof(*syms), compare_syms);
    i = 0;
    while (i < n)
        print_sym(elf, &syms[i++]);
    free(syms);
    return (0);
}
