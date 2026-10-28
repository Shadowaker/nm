#include <stdio.h>
#include "ft_nm.h"

/*
    stdout is flushed first so both streams come out in order when they go
    to the same place
*/
void    print_warnings(const char *path, const t_elf *elf)
{
    fflush(stdout);
    if (elf->warnings & W_PAST_EOF)
        fprintf(stderr, "ft_nm: warning: %s has a section extending past end of file\n", path);
    if (elf->warnings & W_SHSTRNDX)
        fprintf(stderr, "ft_nm: warning: %s has a corrupt string table index - ignoring\n", path);
    if (elf->warnings & W_MULTI_SYMTAB)
        fprintf(stderr, "ft_nm: %s: warning: multiple symbol tables detected"
            " - ignoring the table in section %llu\n", path, (unsigned long long)elf->ignored_symtab);
}

void    print_status(const char *path, t_status status)
{
    fflush(stdout);
    if (status == NM_FORMAT)
        fprintf(stderr, "ft_nm: %s: file format not recognized\n", path);
    else if (status == NM_NO_SYMBOLS || status == NM_SYMTAB_PAST_EOF)
        fprintf(stderr, "ft_nm: %s: no symbols\n", path);
    else if (status == NM_MALFORMED)
        fprintf(stderr, "ft_nm: %s: malformed archive\n", path);
}
