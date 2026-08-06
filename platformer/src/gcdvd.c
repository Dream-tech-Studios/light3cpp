/*
 * gcdvd.c - PLATFORM_GC only. See gcdvd.h.
 *
 * Background: a GameCube disc's data (everything after boot.bin/bi2.bin/
 * the apploader) is described by an "FST" - a flat array of 12-byte
 * entries (first one is always the root directory) followed by a string
 * table, whose on-disc location and size are themselves given by two
 * fields in the very first 0x440 bytes of the disc ("boot.bin"). Each
 * entry is either:
 *   - a file:      { flags=0, name_offset:24, disc_byte_offset:32, byte_length:32 }
 *   - a directory: { flags=1, name_offset:24, parent_index:32,     next_index:32   }
 * where a directory's "next_index" is the index of the entry right after
 * its own subtree, letting a linear scan skip whole subdirectories without
 * recursion. All multi-byte fields are big-endian, which conveniently
 * needs no byte-swapping on this big-endian PowerPC target.
 *
 * The GameCube's DVD hardware only ever transfers whole 32-byte-aligned
 * chunks (both the destination buffer and the disc offset/length), so
 * gcdvd_read_raw() below always reads into a 32-byte-aligned scratch
 * buffer sized to the enclosing aligned range and copies out just the
 * bytes the caller actually asked for.
 */
#ifdef PLATFORM_GC

#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <sys/iosupport.h>
#include <sys/stat.h>
#include <gccore.h>
#include "gcdvd.h"

#define GC_BOOT_HEADER_SIZE 0x440u /* 1088 bytes; already a multiple of 32 */
#define FST_ENTRY_SIZE      12u

typedef struct {
    u64 offset; /* disc byte offset where this file's data begins */
    u32 length; /* file size in bytes */
    u32 pos;    /* current stream position, 0..length */
} GcdvdFile;

static unsigned char *g_fst          = NULL; /* entries[] followed by the string table */
static u32             g_entry_count = 0;
static const char     *g_strings     = NULL;
static int             g_mounted     = 0;

static u32 read_be32(const unsigned char *p) {
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
}

/* Reads an arbitrary byte range [offset, offset+len) from the disc into
 * `dst` (which need not itself be aligned), respecting the DVD hardware's
 * 32-byte alignment requirement on the buffer/offset/length it's actually
 * given. */
static int gcdvd_read_raw(void *dst, u32 len, u64 offset) {
    u64 aligned_start = offset & ~(u64)31;
    u64 aligned_end   = (offset + len + 31) & ~(u64)31;
    u32 aligned_len   = (u32)(aligned_end - aligned_start);
    unsigned char *scratch;
    dvdcmdblk block;
    s32 ret;

    if (len == 0) return 1;

    scratch = (unsigned char *)memalign(32, aligned_len);
    if (!scratch) return 0;

    ret = DVD_ReadPrio(&block, scratch, aligned_len, (s64)aligned_start, 2);
    if (ret <= 0) {
        free(scratch);
        return 0;
    }

    memcpy(dst, scratch + (offset - aligned_start), len);
    free(scratch);
    return 1;
}

static int      fst_is_dir(u32 i)  { return g_fst[i * FST_ENTRY_SIZE] != 0; }
static u32      fst_name_off(u32 i) {
    const unsigned char *p = g_fst + i * FST_ENTRY_SIZE;
    return ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
}
static u32      fst_field2(u32 i)  { return read_be32(g_fst + i * FST_ENTRY_SIZE + 4); }
static u32      fst_field3(u32 i)  { return read_be32(g_fst + i * FST_ENTRY_SIZE + 8); }
static const char *fst_name(u32 i) { return (i == 0) ? "" : (g_strings + fst_name_off(i)); }

/* Walks the FST tree looking up `path` (device prefix, e.g. "gcdvd:",
 * already stripped by the caller). On success returns 1 and fills in the
 * matching file's disc offset/length. */
static int fst_find(const char *path, u64 *out_offset, u32 *out_length) {
    u32 range_start, range_end;
    const char *p = path;

    if (!g_fst || g_entry_count == 0) return 0;
    if (*p == '/') ++p;
    range_start = 1;
    range_end   = fst_field3(0); /* root's "next" == total entry count */

    for (;;) {
        char component[256];
        size_t clen = 0;
        int is_last;
        int found = -1;
        u32 i;

        while (p[clen] != '\0' && p[clen] != '/') ++clen;
        if (clen == 0 || clen >= sizeof(component)) return 0;
        memcpy(component, p, clen);
        component[clen] = '\0';
        is_last = (p[clen] == '\0');

        i = range_start;
        while (i < range_end) {
            int   dir     = fst_is_dir(i);
            u32   skip_to = dir ? fst_field3(i) : (i + 1);
            if (strcmp(fst_name(i), component) == 0) { found = (int)i; break; }
            i = skip_to;
        }
        if (found < 0) return 0;

        if (is_last) {
            if (fst_is_dir((u32)found)) return 0; /* asked for a file, found a directory */
            *out_offset = fst_field2((u32)found);
            *out_length = fst_field3((u32)found);
            return 1;
        }
        if (!fst_is_dir((u32)found)) return 0; /* mid-path component isn't a directory */
        range_start = (u32)found + 1;
        range_end   = fst_field3((u32)found);
        p += clen + 1;
    }
}

static const char *strip_device_prefix(const char *path) {
    const char *colon = strchr(path, ':');
    return colon ? colon + 1 : path;
}

static int gcdvd_open_r(struct _reent *r, void *fileStruct, const char *path, int flags, int mode) {
    GcdvdFile *f = (GcdvdFile *)fileStruct;
    u64 offset;
    u32 length;
    (void)mode;

    if ((flags & O_ACCMODE) != O_RDONLY) {
        r->_errno = EROFS;
        return -1;
    }
    if (!fst_find(strip_device_prefix(path), &offset, &length)) {
        r->_errno = ENOENT;
        return -1;
    }
    f->offset = offset;
    f->length = length;
    f->pos    = 0;
    return 0;
}

static int gcdvd_close_r(struct _reent *r, void *fd) {
    (void)r; (void)fd;
    return 0;
}

static ssize_t gcdvd_read_r(struct _reent *r, void *fd, char *ptr, size_t len) {
    GcdvdFile *f = (GcdvdFile *)fd;
    u32 remaining = f->length - f->pos;
    u32 n = (len < remaining) ? (u32)len : remaining;

    if (n == 0) return 0;
    if (!gcdvd_read_raw(ptr, n, f->offset + f->pos)) {
        r->_errno = EIO;
        return -1;
    }
    f->pos += n;
    return (ssize_t)n;
}

static off_t gcdvd_seek_r(struct _reent *r, void *fd, off_t pos, int dir) {
    GcdvdFile *f = (GcdvdFile *)fd;
    off_t base;

    switch (dir) {
    case SEEK_SET: base = 0; break;
    case SEEK_CUR: base = (off_t)f->pos; break;
    case SEEK_END: base = (off_t)f->length; break;
    default: r->_errno = EINVAL; return -1;
    }
    if (base + pos < 0 || base + pos > (off_t)f->length) {
        r->_errno = EINVAL;
        return -1;
    }
    f->pos = (u32)(base + pos);
    return (off_t)f->pos;
}

static int gcdvd_fstat_r(struct _reent *r, void *fd, struct stat *st) {
    GcdvdFile *f = (GcdvdFile *)fd;
    (void)r;
    memset(st, 0, sizeof(*st));
    st->st_size = (off_t)f->length;
    st->st_mode = S_IFREG;
    return 0;
}

static const devoptab_t gcdvd_devoptab = {
    .name       = "gcdvd",
    .structSize = sizeof(GcdvdFile),
    .open_r     = gcdvd_open_r,
    .close_r    = gcdvd_close_r,
    .read_r     = gcdvd_read_r,
    .seek_r     = gcdvd_seek_r,
    .fstat_r    = gcdvd_fstat_r,
};

int gcdvd_mount(void) {
    unsigned char *header;
    u32 fst_offset, fst_size, fst_alloc;
    dvdcmdblk block;
    s32 ret;

    if (g_mounted) return 1;

    DVD_Init();
    if (DVD_Mount() < 0) return 0;

    header = (unsigned char *)memalign(32, GC_BOOT_HEADER_SIZE);
    if (!header) return 0;

    ret = DVD_ReadPrio(&block, header, GC_BOOT_HEADER_SIZE, 0, 2);
    if (ret <= 0) { free(header); return 0; }

    fst_offset = read_be32(header + 0x424);
    fst_size   = read_be32(header + 0x428);
    free(header);
    if (fst_size == 0) return 0;

    fst_alloc = (fst_size + 31u) & ~31u;
    g_fst = (unsigned char *)memalign(32, fst_alloc);
    if (!g_fst) return 0;

    if (!gcdvd_read_raw(g_fst, fst_size, fst_offset)) {
        free(g_fst);
        g_fst = NULL;
        return 0;
    }

    g_entry_count = fst_field3(0);
    g_strings     = (const char *)(g_fst + (size_t)g_entry_count * FST_ENTRY_SIZE);

    if (AddDevice(&gcdvd_devoptab) < 0) {
        free(g_fst);
        g_fst = NULL;
        return 0;
    }

    g_mounted = 1;
    return 1;
}

#endif /* PLATFORM_GC */
