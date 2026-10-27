#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "ft_nm.h"


int load_file(char *path, t_elf *elf)
{
    int     fd;
    off_t   size;
    ssize_t r;
    size_t  off;

    memset(elf, 0, sizeof(*elf));
    fd = open(path, O_RDONLY);
    if (fd < 0)
        return (fprintf(stderr, "ft_nm: %s: file format not recognized\n", path), 1);

    size = lseek(fd, 0, SEEK_END);
    if (size < 0 || lseek(fd, 0, SEEK_SET) < 0)
    {
        close(fd);
        return (fprintf(stderr, "ft_nm: lseek error\n"), 1);
    }

    elf->orig_size = (size_t) size;
    elf->size = (size_t) size;
    elf->capacity = (size_t) size + 2 * 4096 + 0x10000; // MAGIC NUMBERS GO BRRRRRRR
    elf->buf = malloc(elf->capacity);
    if (!elf->buf)
    {
        close(fd);
        return (fprintf(stderr, "ft_nm: malloc error\n"), 1);
    }

    off = 0;
    while (off < elf->size)
    {
        r = read(fd, elf->buf + off, elf->size - off);
        if (r < 0)
        {
            close(fd);
            free(elf->buf);
            return (fprintf(stderr, "ft_nm: read error\n"), 1);
        }
        if (r == 0)
            break ;
        off += (size_t) r;
    }

    close(fd);
    if (off != elf->size)
    {
        free(elf->buf);
        return (fprintf(stderr, "ft_nm: unexpected end of file.\n"), 1);
    }
    return (0);
}