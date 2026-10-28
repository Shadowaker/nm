#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "ft_nm.h"

int is_archive(const t_elf *file)
{
    const char  *magic;

    magic = elf_ptr(file, 0, AR_MAGIC_SIZE);
    return (magic != NULL && (memcmp(magic, "!<arch>\n", AR_MAGIC_SIZE) == 0
        || memcmp(magic, "!<thin>\n", AR_MAGIC_SIZE) == 0));
}

static int  out_of_memory(const char *path)
{
    fflush(stdout);
    fprintf(stderr, "ft_nm: %s: %s\n", path, strerror(ENOMEM));
    return (-1);
}

/*
    Copies the header at off, '\0'-terminated so the size can be read with
    sscanf.
    Fewer than 60 bytes left is the end of the archive
*/
static t_next   read_header(const t_archive *ar, uint64_t off, t_member *m)
{
    const char  *p;
    char        size[11];

    p = elf_ptr(ar->file, off, AR_HDR_SIZE);
    if (p == NULL)
        return (AR_END);
    memcpy(m->hdr, p, AR_HDR_SIZE);
    m->hdr[AR_HDR_SIZE] = '\0';
    memcpy(size, m->hdr + 48, 10);
    size[10] = '\0';
    if (memcmp(m->hdr + 58, "`\n", 2) != 0 || sscanf(size, "%" SCNu64, &m->size) != 1)
        return (AR_MALFORMED);
    m->data = off + AR_HDR_SIZE;
    m->name = NULL;
    return (AR_MEMBER);
}

/*
    "/N" is offset N in the "//" table, read with strtol like nm
*/
static t_next   long_name(const t_archive *ar, t_member *m)
{
    unsigned long   idx;

    errno = 0;
    idx = strtol(m->hdr + 1, NULL, 10);
    if (errno != 0 || idx >= ar->names_size)
        return (AR_MALFORMED);
    m->name = strdup(ar->names + idx);
    if (m->name == NULL)
        return (AR_NOMEM);
    return (AR_MEMBER);
}

/*
    "#1/N": the name is the first N bytes of the data. A negative N wraps
    around and is too big.
*/
static t_next   bsd_name(const t_archive *ar, t_member *m)
{
    const char  *p;
    uint64_t    len;

    len = atoi(m->hdr + 3);
    if (len > m->size || len > ar->file->size)
        return (AR_MALFORMED);
    p = elf_ptr(ar->file, m->data, len);
    if (p == NULL)
        return (AR_END);
    m->name = strndup(p, len);
    if (m->name == NULL)
        return (AR_NOMEM);
    m->data += len;
    m->size -= len;
    return (AR_MEMBER);
}

static t_next   read_member(const t_archive *ar, uint64_t off, t_member *m)
{
    const char  *h;
    const char  *end;
    t_next      r;

    r = read_header(ar, off, m);
    if (r != AR_MEMBER)
        return (r);
    h = m->hdr;
    if ((h[0] == '/' || (h[0] == ' ' && !memchr(h, '/', AR_NAME_MAX)))
        && ar->names != NULL)
        return (long_name(ar, m));
    if (h[0] == '#' && h[1] == '1' && h[2] == '/' && isdigit((unsigned char)h[3]))
        return (bsd_name(ar, m));
    end = memchr(h, '\0', AR_NAME_MAX);
    if (end == NULL)
        end = memchr(h, '/', AR_NAME_MAX);
    if (end == NULL)
        end = memchr(h, ' ', AR_NAME_MAX);
    if (end == NULL)
        end = h + AR_NAME_MAX;
    m->name = strndup(h, end - h);
    if (m->name == NULL)
        return (AR_NOMEM);
    return (AR_MEMBER);
}

static uint64_t read_uint(const uint8_t *p, int size, int big_endian)
{
    uint64_t    v;
    int         i;

    v = 0;
    i = 0;
    while (i < size)
    {
        if (big_endian)
            v = (v << 8) | p[i];
        else
            v |= (uint64_t)p[i] << (8 * i);
        i++;
    }
    return (v);
}

/*
    GNU symbol index with 4-byte ("/") or 8-byte ("/SYM64/") big-endian
    words: a count, the offsets, then the names, all inside the file. nm
    also skips a second "/" index right after a 4-byte one.
    It returns 0 on invalid.
*/
static int  skip_gnu_index(t_archive *ar, int word)
{
    t_member        m;
    t_member        second;
    const uint8_t   *p;
    uint64_t        end;

    if (read_header(ar, AR_MAGIC_SIZE, &m) != AR_MEMBER)
        return (0);
    p = elf_ptr(ar->file, m.data, word);
    if (p == NULL || m.size < (uint64_t)word || m.size > ar->file->size
        || read_uint(p, word, 1) > (m.size - word) / word
        || !elf_ptr(ar->file, m.data, m.size))
        return (0);
    end = m.data + m.size;
    ar->first = end + end % 2;
    if (word == 4 && read_header(ar, end, &second) == AR_MEMBER
        && second.hdr[0] == '/' && second.hdr[1] == ' ')
        ar->first += (second.size + AR_HDR_SIZE + 1) & ~(uint64_t)1;
    return (1);
}

/*
    BSD symbol index ("__.SYMDEF"): little-endian sizes, and every name
    offset inside its string table.
    It returns 0 on invalid.
*/
static int  skip_bsd_index(t_archive *ar)
{
    t_member        m;
    const uint8_t   *p;
    uint64_t        bytes;
    uint64_t        i;

    if (read_header(ar, AR_MAGIC_SIZE, &m) != AR_MEMBER || m.size < 8)
        return (0);
    p = elf_ptr(ar->file, m.data, m.size);
    if (p == NULL)
        return (0);
    bytes = read_uint(p, 4, 0);
    if (bytes > m.size - 8 || bytes % 8 != 0)
        return (0);
    i = 0;
    while (i < bytes / 8)
    {
        if (read_uint(p + 4 + i * 8, 4, 0) >= m.size - 8 - bytes)
            return (0);
        i++;
    }
    ar->first = m.data + m.size;
    ar->first += ar->first % 2;
    return (1);
}

/*
    A broken symbol index, or a partial name right after
    the magic, makes the whole file unrecognized.
    It returns 0 on invalid.
*/
static int  skip_symbol_index(t_archive *ar)
{
    const char  *name;

    ar->first = AR_MAGIC_SIZE;
    name = elf_ptr(ar->file, AR_MAGIC_SIZE, 16);
    if (name == NULL)
        return (ar->file->size == AR_MAGIC_SIZE);
    if (memcmp(name, "/               ", 16) == 0)
        return (skip_gnu_index(ar, 4));
    if (memcmp(name, "/SYM64/         ", 16) == 0)
        return (skip_gnu_index(ar, 8));
    if (memcmp(name, "__.SYMDEF       ", 16) == 0
        || memcmp(name, "__.SYMDEF/      ", 16) == 0)
        return (skip_bsd_index(ar));
    return (1);
}

/*
    It returns 0 on invalid, -1 when out of memory.
*/
static int  load_long_names(t_archive *ar)
{
    const char  *p;
    t_member    m;
    uint64_t    i;

    p = elf_ptr(ar->file, ar->first, 16);
    if (p == NULL || (memcmp(p, "//              ", 16) != 0
        && memcmp(p, "ARFILENAMES/    ", 16) != 0))
        return (1);
    if (read_header(ar, ar->first, &m) != AR_MEMBER || m.size > ar->file->size)
        return (0);
    p = elf_ptr(ar->file, m.data, m.size);
    if (p == NULL)
        return (0);
    ar->names = malloc(m.size + 1);
    if (ar->names == NULL)
        return (-1);
    memcpy(ar->names, p, m.size);
    i = 0;
    while (i < m.size)
    {
        if (ar->names[i] == '\n')
            ar->names[(i > 0 && ar->names[i - 1] == '/') ? i - 1 : i] = '\0';
        if (ar->names[i] == '\\')
            ar->names[i] = '/';
        i++;
    }
    ar->names[m.size] = '\0';
    ar->names_size = m.size;
    ar->first = m.data + m.size;
    ar->first += ar->first % 2;
    return (1);
}

/*
    A thin archive only stores paths, relative to the archive's directory
*/
static char *thin_path(const char *archive, const char *name)
{
    const char  *slash;
    size_t      dir;
    char        *path;

    slash = strrchr(archive, '/');
    if (name[0] == '/' || slash == NULL)
        return (strdup(name));
    dir = slash - archive + 1;
    path = malloc(dir + strlen(name) + 1);
    if (path == NULL)
        return (NULL);
    memcpy(path, archive, dir);
    strcpy(path + dir, name);
    return (path);
}

/*
    Reads a thin archive's member file whole.
    It returns 0 when it can't be opened,
    -1 when it can't be read.
*/
static int  read_member_file(const char *path, t_elf *elf)
{
    struct stat st;
    ssize_t     r;
    int         fd;

    memset(elf, 0, sizeof(*elf));
    fd = open(path, O_RDONLY);
    if (fd < 0)
        return (errno);
    r = -1;
    if (fstat(fd, &st) == 0)
    {
        elf->buf = malloc(st.st_size + 1);
        r = 1;
    }
    if (r > 0 && elf->buf == NULL)
    {
        close(fd);
        return (ENOMEM);
    }
    while (r > 0 && elf->size < (size_t)st.st_size)
    {
        r = read(fd, elf->buf + elf->size, st.st_size - elf->size);
        if (r > 0)
            elf->size += r;
    }
    close(fd);
    if (r < 0)
        return (-1);
    return (0);
}

/*
    It returns 0 or -1 when the file can't be opened or 
    read stops everything.
*/
static int  show_thin_member(const t_archive *ar, const t_member *m)
{
    t_elf   elf;
    char    *path;
    int     err;
    int     r;

    path = thin_path(ar->path, m->name);
    if (path == NULL)
        return (out_of_memory(ar->path));
    err = read_member_file(path, &elf);
    if (err > 0)
    {
        fflush(stdout);
        fprintf(stderr, "ft_nm: %s: %s\n", ar->path, strerror(err));
    }
    else if (err < 0)
        print_status(ar->path, NM_FORMAT);
    r = -1;
    if (err == 0 && display_elf(&elf, path, path, path) >= 0)
        r = 0;
    free(elf.buf);
    free(path);
    return (r);
}

/*
    It returns 0, or -1 when nm can't display
*/
static int  show_member(const t_archive *ar, const t_member *m)
{
    t_elf   elf;
    char    *warn_name;
    int     r;

    if (ar->thin)
        return (show_thin_member(ar, m));
    memset(&elf, 0, sizeof(elf));
    elf.buf = ar->file->buf + m->data;
    elf.size = ar->file->size - m->data;
    if (m->size < elf.size)
        elf.size = m->size;
    warn_name = malloc(strlen(ar->path) + strlen(m->name) + 3);
    if (warn_name == NULL)
        return (out_of_memory(ar->path));
    sprintf(warn_name, "%s(%s)", ar->path, m->name);
    r = display_elf(&elf, m->name, warn_name, m->name);
    free(warn_name);
    if (r < 0)
        return (-1);
    return (0);
}

/*
    It returns 0 on invalid
*/
static int  next_offset(const t_archive *ar, const t_member *m, uint64_t *off)
{
    *off = m->data;
    if (ar->thin)
        return (1);
    *off += m->size;
    *off += *off % 2;
    return (*off >= m->data);
}

/*
    It returns 0, or -1 when a broken header is malformed.
*/
static int  walk_members(const t_archive *ar)
{
    t_member    m;
    t_next      next;
    uint64_t    off;
    int         r;

    next = read_member(ar, ar->first, &m);
    while (next == AR_MEMBER)
    {
        r = show_member(ar, &m);
        free(m.name);
        if (r != 0)
            return (r);
        next = AR_MALFORMED;
        if (next_offset(ar, &m, &off))
            next = read_member(ar, off, &m);
    }
    if (next == AR_NOMEM)
        return (out_of_memory(ar->path));
    if (next == AR_MALFORMED)
    {
        print_status(ar->path, NM_MALFORMED);
        return (-1);
    }
    return (0);
}

/*
    It returns 0 on success, 1 on failure, -1 when walk_members returns -1
*/
int display_archive(const char *path, t_elf *file, int multi)
{
    t_archive   ar;
    int         r;

    memset(&ar, 0, sizeof(ar));
    ar.path = path;
    ar.file = file;
    ar.thin = (memcmp(file->buf, "!<thin>\n", AR_MAGIC_SIZE) == 0);
    r = 0;
    if (skip_symbol_index(&ar))
        r = load_long_names(&ar);
    if (r == 0)
    {
        print_status(path, NM_FORMAT);
        return (1);
    }
    if (r < 0)
        return (out_of_memory(path));
    if (multi)
        printf("\n%s:\n", path);
    r = walk_members(&ar);
    free(ar.names);
    return (r);
}
