#include <string.h>
#include <stdlib.h>
#include "ft_nm.h"

int body(char *path)
{
    t_elf   elf;

    if (load_file(path, &elf))
    {
        return (1);
    }

    return (0);
}

int main(int argc, char **argv)
{
    if (argc == 1)
        return (body("a.out"), 1);
    else
    {
        for (int i = 0; i < argc; i++)
        {
            return (body(argv[i]), 1);
        }
    }

    return (0);
}