#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "ft_nm.h"

int body(char *path)
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
    print_status(path, status);
    free(elf.buf);

    return (status != NM_OK && status != NM_NO_SYMBOLS);
}

int main(int argc, char **argv)
{

    if (argc == 1)
        return (body("a.out"));
    else
    {
        for (int i = 1; i < argc; i++)
        {
            if (body(argv[i]))
                return (1);
        }
    }

    return (0);
}