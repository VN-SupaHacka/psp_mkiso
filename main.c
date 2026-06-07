#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "iso_types.h"

#define fwrite fwrite_unlocked
#define putchar putchar_unlocked

#define ARRAY_SIZE(X) (sizeof(X)/sizeof(X[0]))
#define iso_strcpy(DST, SRC) _iso_strcpy(DST, SRC, ARRAY_SIZE(DST))

static char zerobuf[16 * ISO_SECTOR_SIZE] = {0}; //Used for simple zero pad sectors

static char outbuf[64 * 1024] = {0}; //64KB buffer (used for partial writes)

static void _iso_strcpy(char *dst, const char *src, size_t dstlen)
{
    size_t len = strlen(src);
    if (len > dstlen)
        len = dstlen;
    memcpy(dst, src, len);
    if(dstlen > len)
        memset(dst + len, ' ', dstlen - len);
}

struct iso_dirent {
    char *name;
    DIR *dirp;
    int fd;
    int level;
    int parent_idx;
    u32 id;
    u32 parent_id;
    int : 32; //Padding
};

struct path_table_context {
    struct iso_dirent *entry;
    char *namebuf;
};

static int tbl_cmp(const void *p1, const void *p2) {
    const struct iso_dirent *e1 = p1;
    const struct iso_dirent *e2 = p2;
    if (e1->parent_idx != e2->parent_idx)
        return e1->parent_idx - e2->parent_idx;
    return strcmp(e1->name, e2->name);
}

/* Path table sort by hierarchy level */
static int tbl_level_cmp(const void *p1, const void *p2)
{
    const struct iso_dirent *e1 = p1;
    const struct iso_dirent *e2 = p2;
    return e1->level - e2->level;
}

static void build_path_table_recursive(int dirfd, u32 level, u32 parent_id, struct path_table_context *ctx)
{
    u32 cnt = 0;
    u32 id = 0;
    
    DIR *dirp = fdopendir(dirfd);
    struct dirent *entry;

    ctx->entry[-1].dirp = dirp; //Ugly hack for now
    while (entry = readdir(dirp)) { 
        if (!(entry->d_type & DT_DIR) || 
            !memcmp(entry->d_name,".", 2) || 
            !memcmp(entry->d_name, "..", 3)) 
            continue;

        if (level == 0 && id == 0)
            id = 1;
        else 
            id = ctx->entry[-1].id + 1;

        int newdirfd = openat(dirfd, entry->d_name, O_RDONLY);
        ctx->entry->name      = ctx->namebuf;
        ctx->entry->fd        = newdirfd;
        ctx->entry->id        = id;
        ctx->entry->level     = level;
        ctx->entry->parent_id = parent_id;
        ctx->entry++;

        size_t namelen = strlen(entry->d_name);
        memcpy(ctx->namebuf, entry->d_name, namelen + 1);
        ctx->namebuf += namelen + 1; 

        build_path_table_recursive(newdirfd, level+1, id, ctx);
    }
}

static struct iso_dirent *build_path_table(const char *content_dir, u32 *amount)
{
    static char entry_names[255 * 500];
    static struct iso_dirent entries[255];
    struct path_table_context ctx = {
        .entry   = entries + 1,
        .namebuf = entry_names + 1
    };
    entry_names[0] = '\0';

    entries[0].fd         = open(content_dir, O_RDONLY);
    entries[0].id         = 1;
    entries[0].level      = 0;
    entries[0].name       = entry_names;
    entries[0].parent_idx = 1;
    entries[0].parent_id  = 0;

    build_path_table_recursive(entries[0].fd, 0, 1, &ctx);

    size_t count = ctx.entry - entries;
    qsort(entries+1, count-1, sizeof(*entries), tbl_level_cmp);

    int max_level = entries[count - 1].level;
    struct iso_dirent *back_entry = NULL;
    struct iso_dirent *next_entry = entries + 1;
    for(int level = 0; level <= max_level; level++) {
        size_t cnt = 0;

        if (back_entry != NULL) {
            for(struct iso_dirent *entry = next_entry; entry->level == level; entry++) {
                for(size_t i = 0; i < (next_entry - back_entry); i++) {
                    if(back_entry[i].id == entry->parent_id) {
                        entry->parent_idx = (back_entry - entries) + i + 1;
                        break;
                    }
                }
                cnt++;
            }
        } else { 
            for(struct iso_dirent *entry = next_entry; entry->level == level; entry++) {
                entry->parent_idx = 1;
                cnt++;
            }
        }

        qsort(next_entry, cnt, sizeof(*next_entry), tbl_cmp);

        back_entry = next_entry;
        next_entry += cnt;
    }

    *amount = count;
    
    return entries;
}

static u32 write_path_table(struct iso_dirent *entries, size_t count, u32 *sector_idx, FILE *iso)
{
    char secbuf[2048];
    char *secpos = secbuf;
    struct iso_path_table *path_table;
    for(size_t i = 0; i < count; i++) {
        path_table = (struct iso_path_table*)secpos;
        path_table->name_len  = (i > 0) ? strlen(entries[i].name) : 1;
        path_table->attr_ex   = 0;
        path_table->lba       = *sector_idx;
        path_table->parent_id = entries[i].parent_idx; 
        memcpy(path_table->name, entries[i].name, path_table->name_len);
        if (path_table->name_len & 1) {
            path_table->name[path_table->name_len] = '\0';
            secpos++;
        }
        *sector_idx += 1;
        secpos += sizeof(*path_table) + path_table->name_len;
    }

    size_t size = secpos - secbuf;

    fwrite(secbuf, size, 1, iso); 
    fwrite(zerobuf, ISO_SECTOR_SIZE - size, 1, iso);

    fwrite(secbuf, size, 1, iso);
    fwrite(zerobuf, ISO_SECTOR_SIZE - size, 1, iso);

    /* Convert L path table to M Path table (LE -> BE) */
    char *pos = secbuf;
    char *end = secbuf + size;
    while (pos < end) {
        struct iso_path_table *path_tbl = (struct iso_path_table*)pos;
        path_tbl->lba       = cpu_to_be32(path_tbl->lba);
        path_tbl->parent_id = cpu_to_be16(path_tbl->parent_id);
        pos += sizeof(*path_tbl) + path_tbl->name_len + (path_tbl->name_len & 1);
    }
    fwrite(secbuf, size, 1, iso);
    fwrite(zerobuf, ISO_SECTOR_SIZE - size, 1, iso);
    fwrite(secbuf, size, 1, iso);
    fwrite(zerobuf, ISO_SECTOR_SIZE - size, 1, iso);

    return (u32)size;
}

u32 get_iso_file_size(struct iso_dirent *iso_entry, struct dirent *entry)
{
    if (entry->d_type == DT_DIR)
        return ISO_SECTOR_SIZE; //A directory is always 1 sector sized
    struct stat statbuf;
    fstatat(iso_entry->fd, entry->d_name, &statbuf, 0);
    return (u32)statbuf.st_size;
}

u32 write_file_entire(int dirfd, const char *name, size_t size, FILE *out)
{
    int fd    = openat(dirfd, name, O_RDONLY);
    void *map = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    madvise(map, size, MADV_SEQUENTIAL);
    fwrite(map, size, 1, out);
    fwrite(zerobuf, size & (ISO_SECTOR_SIZE-1), 1, out);
 
    munmap(map, size);
    close(fd);

    return (size + ISO_SECTOR_SIZE - 1) / ISO_SECTOR_SIZE;
}


void write_dir_headers(struct iso_dirent *entries, u32 count, u32 sector, FILE *iso)
{

    char secbuf[2048];
    
    for (u32 i = 0; i < count; i++) {
        char *secpos = secbuf;

        struct dirent *entry;
        rewinddir(entries[i].dirp);
        while ((entry = readdir(entries[i].dirp)) != NULL) {

            struct iso_dir_record *rec = (struct iso_dir_record*)secpos;
            u32 fsize = get_iso_file_size(entries + i, entry);
            u32 entry_sec;

            if (memcmp(entry->d_name, ".", 2) == 0) {
                rec->id_len = 1;
                rec->id[0]  = '\x00';
                entry_sec   = 22 + i;

            } else if (!memcmp(entry->d_name, "..", 3)) {
                rec->id_len = 1;
                rec->id[0]  = '\x01';
                entry_sec   = 22 + entries[i].parent_idx - 1;
            } else {
                rec->id_len = strlen(entry->d_name);
                memcpy(rec->id, entry->d_name, rec->id_len);
                if (entry->d_type == DT_DIR) {
                    for (u32 j = 1; j < count; j++) {
                        if (!strcmp(entries[j].name, entry->d_name)) {
                            entry_sec = 22 + j;
                            break;
                        }
                    }
                } else {
                    entry_sec = sector;
                }
            }
            rec->date       = DIR_DATE_LITERAL(2009, 2, 1, 0, 0, 0, +9);
            rec->attr_ex    = 0;
            rec->unit_sz    = 0;
            rec->gap_sz     = 0;
            rec->seq_num    = ISO_U16_LITERAL(1);
            rec->dlen       = ISO_U32_LITERAL(fsize);
            rec->extent_off = ISO_U32_LITERAL(entry_sec);        
            rec->flags      = ISO_ATTR_DIR * (entry->d_type == DT_DIR);
            
            u32 uxa_off = 0;
            if ((sizeof(struct iso_dir_record) + rec->id_len) & 1) {
                rec->id[rec->id_len] = '\0';
                uxa_off = 1;
            }

            struct iso_uxa *uxa = (struct iso_uxa*)(rec->id + rec->id_len + uxa_off);
            uxa->r1 = 0;
            memset(uxa->r2, 0, sizeof(uxa->r2));
            memcpy(uxa->uxa, "\x8D" "UXA", 4);
            rec->len = sizeof(*uxa) + sizeof(*rec) + rec->id_len + uxa_off;
            secpos += rec->len;

            if(entry->d_type == DT_DIR)
                continue;

            long fpos = ftell(iso);
            fseek(iso, ISO_SECTOR_SIZE * entry_sec, SEEK_SET);
            sector += write_file_entire(entries[i].fd, entry->d_name, fsize, iso);
            fseek(iso, fpos, SEEK_SET);
        }
        size_t outsize = secpos - secbuf;
        fwrite(secbuf, outsize, 1, iso);
        fwrite(zerobuf, ISO_SECTOR_SIZE - outsize, 1, iso);
    }
}

//argv[1] -> Base directory
//argv[2] -> Output ISO
int main(int argc, char **argv)
{
    if (argc < 3) {
        printf("usage: %s {base_directory} {output_iso}\n", argv[0]);
        return EXIT_FAILURE;
    }

    FILE *iso = fopen(argv[2], "wb");

    //Sectors 0-15 are blank
    fwrite(zerobuf, 1, 16 * ISO_SECTOR_SIZE, iso);  

    //This is the place for the primary sector, but we don't have enough information yet (Sector 16)
    fwrite(zerobuf, 1, ISO_SECTOR_SIZE, iso); 

    //Sector 17 is used to mark the start of path tables
    struct iso_vol_desc endvol = {ISO_ENDSET_VOL_TYPE, "CD001", 1};
    fwrite(&endvol, 1, sizeof(endvol), iso);
    fwrite(zerobuf, 1, ISO_SECTOR_SIZE - sizeof(endvol), iso);

    //Sectors 18-19 are L path tables and 20-21 are M path tables (L = le, M = be)
    u32 sector_idx = 22; 
    u32 entry_cnt;

    //Writing the L and M path tables, and advancing the sector index to the first file content
    char path_tbl_sec[ISO_SECTOR_SIZE];
    struct iso_dirent *entries = build_path_table(argv[1], &entry_cnt);
    u32 path_tbl_size          = write_path_table(entries, entry_cnt, &sector_idx, iso);
    write_dir_headers(entries, entry_cnt, sector_idx, iso);

    struct iso_primary_vol_desc pvol = {
        .type     = ISO_PRIMARY_VOL_TYPE,
        .std_id   = "CD001",
        .version  = 1,
        .blk_sz   = ISO_U16_INIT(ISO_SECTOR_SIZE),
        .tbl_sz   = ISO_U32_INIT(path_tbl_size),
        .root_dir = {
            .len  = sizeof(struct iso_dir_record) + 1,
            .date = { /* Remember11 Japanese PSP release */
                .years_1990 = DIR_DATE_YEAR(2009),
                .month      = 2,
                .day        = 1,
                .hour       = 0,
                .gmt_off    = DIR_DATE_GMT(+9),
            },
            .extent_off = ISO_U32_INIT(22),
            .dlen       = ISO_U32_INIT(ISO_SECTOR_SIZE),
            .flags      = ISO_ATTR_DIR,
            .seq_num    = ISO_U32_INIT(1),
            .id_len     = 1
        },
        .l_path_tbl_off     = 18,
        .opt_l_path_tbl_off = 19,
        .m_path_tbl_off     = 20,
        .opt_m_path_tbl_off = 21,
        .cdate  = ISO_DATE_INIT("2009", "05", "01", "00", "00", "00", "00", +9),
        .mdate  = ISO_DATE_INIT("0000", "00", "00", "00", "00", "00", "00", +0),
        .exdate = ISO_DATE_INIT("0000", "00", "00", "00", "00", "00", "00", +0),
        .edate  = ISO_DATE_INIT("0000", "00", "00", "00", "00", "00", "00", +0),
        .fver   = 2,
    };
    iso_strcpy(pvol.sysid, "PSP GAME");
    iso_strcpy(pvol.volid, "");
    iso_strcpy(pvol.vol_set_id, "");
    iso_strcpy(pvol.publisher_id, "");
    iso_strcpy(pvol.preparer_id, "");
    iso_strcpy(pvol.app_id, "PSP GAME");
    iso_strcpy(pvol.copyright, "");

    struct iso_psp_app_data *psp_usage = (struct iso_psp_app_data*) pvol.app_use;
    iso_strcpy(psp_usage->psp_code, "ULJM-05444|0F3200A5EAED86E6|0001"); /* Game specific code */
    iso_strcpy(psp_usage->std_id, "CD-XA001"); 

    fseek(iso, 0, SEEK_END);
    size_t secpos = ftell(iso) / ISO_SECTOR_SIZE;
    pvol.space_sz = ISO_U32_LITERAL(secpos);
    pvol.set_sz   = ISO_U16_LITERAL(1);
    pvol.seq_num  = ISO_U16_LITERAL(1);

    fseek(iso, 16 * ISO_SECTOR_SIZE, SEEK_SET);
    fwrite(&pvol, sizeof(pvol), 1, iso);
}
