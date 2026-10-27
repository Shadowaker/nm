#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "ft_nm.h"

int body(char *path)
{
    t_elf   elf;
    int     elf_class;

    if (load_file(path, &elf))
    {
        free(elf.buf);
        return (1);
    }

    elf_class = check_ident(&elf);
    if (elf_class == 0 || !read_ehdr(&elf, elf_class))
    {
        fprintf(stderr, "ft_nm: %s: file format not recognized\n", path);
        free(elf.buf);
        return (1);
    }
    
    free(elf.buf);

    return (0);
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