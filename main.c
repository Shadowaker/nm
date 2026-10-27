#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "ft_nm.h"

/*
    It returns 0 on success, 1 on failure, -1 when nm would stop there
*/
int body(char *path, int multi)
{
    t_elf       elf;
    t_status    status;

    if (load_file(path, &elf))
    {
        free(elf.buf);
        return (1);
    }

    status = validate_elf(&elf);
    print_warnings(path, &elf);
    if (multi && status != NM_FORMAT)
        printf("\n%s:\n", path);
    print_status(path, status);
    free(elf.buf);

    if (status == NM_SYMTAB_PAST_EOF)
        return (-1);
    return (status == NM_FORMAT);
}

/*
    The exit code is the number of files that failed, and a fatal
    error stops everything with 1
*/
int main(int argc, char **argv)
{
    int failures;
    int r;

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