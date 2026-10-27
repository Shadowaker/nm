#ifndef FT_NM_H
# define FT_NM_H

# include <stddef.h>
# include <stdint.h>
# include <elf.h>


typedef struct s_elf {

    uint8_t		*buf;
    size_t		size;
	size_t		capacity;
	size_t		orig_size;


    Elf64_Ehdr  *ehdr;
    Elf64_Phdr  *phdr;

    int         is_pie;
}   t_elf;

int load_file(char *path, t_elf *elf);

#endif