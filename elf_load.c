#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "ft_nm.h"


static void print_sys_error(char *path)
{
    int err;

    err = errno;
    fputs("ft_nm: ", stderr);
    errno = err;
    perror(path);
}

/*
    It returns 1 on invalid
*/
static int  check_file(const char *path)
{
    struct stat st;

    if (stat(path, &st) < 0)
    {
        if (errno == ENOENT)
            fprintf(stderr, "ft_nm: '%s': No such file\n", path);
        else
            fprintf(stderr, "ft_nm: Warning: could not locate '%s'.  reason: %s\n",
                path, strerror(errno));
        return (1);
    }
    if (S_ISDIR(st.st_mode))
        fprintf(stderr, "ft_nm: Warning: '%s' is a directory\n", path);
    else if (!S_ISREG(st.st_mode))
        fprintf(stderr, "ft_nm: Warning: '%s' is not an ordinary file\n", path);
    return (!S_ISREG(st.st_mode) || st.st_size == 0);
}

int load_file(char *path, t_elf *elf)
{
    int     fd;
    off_t   size;
    ssize_t r;
    size_t  off;

    memset(elf, 0, sizeof(*elf));
    fflush(stdout);     // the previous files' output comes before any error below
    if (check_file(path))
        return (1);
    fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        print_sys_error(path);
        return (1);
    }

    size = lseek(fd, 0, SEEK_END);
    if (size < 0 || lseek(fd, 0, SEEK_SET) < 0)
    {
        print_sys_error(path);
        close(fd);
        return (1);
    }

    elf->size = (size_t) size;
    elf->buf = malloc(elf->size);
    if (!elf->buf)
    {
        print_sys_error(path);
        close(fd);
        return (1);
    }

    off = 0;
    while (off < elf->size)
    {
        r = read(fd, elf->buf + off, elf->size - off);
        if (r < 0)
        {
            print_sys_error(path);
            close(fd);
            free(elf->buf);
            elf->buf = NULL;
            return (1);
        }
        if (r == 0)
            break ;
        off += (size_t) r;
    }

    close(fd);
    if (off != elf->size)
    {
        free(elf->buf);
        elf->buf = NULL;
        return (fprintf(stderr, "ft_nm: unexpected end of file.\n"), 1);
    }
    return (0);
}