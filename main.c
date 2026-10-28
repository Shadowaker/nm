#include <locale.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "ft_nm.h"

/*
    It returns 0 on success, 1 on failure, -1 when nm would stop there
*/
int display_elf(t_elf *elf, const char *name, const char *warn_name,
    const char *header)
{
    t_status    status;
    int         r;

    status = validate_elf(elf);
    print_warnings(warn_name, elf);
    if (header != NULL && status != NM_FORMAT)
        printf("\n%s:\n", header);
    print_status(name, status);
    r = 0;
    if (status == NM_OK)
        r = print_symbols(name, elf);

    if (status == NM_SYMTAB_PAST_EOF || r < 0)
        return (-1);
    return (status == NM_FORMAT);
}

/*
    It returns 0 on success, 1 on failure, -1 when nm would stop there
*/
int body(char *path, int multi)
{
    t_elf   elf;
    int     r;

    if (load_file(path, &elf))
    {
        free(elf.buf);
        return (1);
    }

    if (is_archive(&elf))
        r = display_archive(path, &elf, multi);
    else
        r = display_elf(&elf, path, path, multi ? path : NULL);
    free(elf.buf);

    return (r);
}

/*
    The exit code is the number of files that failed, and a fatal
    error stops everything with 1
*/
int main(int argc, char **argv)
{
    int failures;
    int r;

    setlocale(LC_COLLATE, "");      // sorts names in the user's locale
    setlocale(LC_CTYPE, "");
    if (argc == 1)
        return (body("a.out", 0) != 0);
    failures = 0;
    for (int i = 1; i < argc; i++)
    {
        r = body(argv[i], argc > 2);
        if (r < 0)
            return (1);
        failures += r;
    }

    return (failures);
}