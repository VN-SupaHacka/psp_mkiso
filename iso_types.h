#ifndef ISO_TYPES_H
#define ISO_TYPES_H

#include <inttypes.h>

#define BIT(X) (1UL << X)
#define DIR_DATE_YEAR(X) (X - 1990) /* Convert year from years since 1990 (as used by ISO standard) */
#define DIR_DATE_GMT(X) (X * 4)     /* Convert hours to 15min units (as used by ISO standard) */
#define DIR_DATE_LITERAL(Y,M,D,H,MM,S,GMT) (struct dir_date){DIR_DATE_YEAR(Y), M, D, H, MM, S, DIR_DATE_GMT(GMT)}

#define ISO_DATE_INIT(Y,M,D,H,MM,S,SS,GMT) {Y M D H MM S SS, DIR_DATE_GMT(GMT)}

#define ISO_U32_INIT(X) {X,cpu_to_be32(X)}
#define ISO_U16_INIT(X) {X,cpu_to_be16(X)}

#define ISO_U16_LITERAL(X) (iso_u16){X, cpu_to_be16(X)}
#define ISO_U32_LITERAL(X) (iso_u32){X, cpu_to_be32(X)}

#define ISO_SECTOR_SIZE 2048

enum {
    ISO_PRIMARY_VOL_TYPE    = 1,
    ISO_SUPPLEMENT_VOL_TYPE = 2,
    ISO_PARTITION_VOL_TYPE  = 3,
    ISO_ENDSET_VOL_TYPE     = 255,

    ISO_ATTR_VISIBLE = BIT(0), /* Is a file visible to the user */
    ISO_ATTR_DIR     = BIT(1), /* Is a directory */
    ISO_ATTR_ASSOC   = BIT(2), /* Is a associated file */
    ISO_ATTR_RECORD  = BIT(3), /* Has a record format */
    ISO_ATTR_PROT    = BIT(4), /* Has group/user permission */
    ISO_ATTR_MEXTENT = BIT(7), /* If this not the final dir record for this file */
};

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

typedef struct {
    u16 le;
    u16 be;
} iso_u16;

typedef struct {
    u32 le;
    u32 be;
} iso_u32;

#pragma pack(1)
struct iso_uxa {
    u32 r1;
    char uxa[4]; //0x8D "UXA"
    char r2[6];
};

#pragma pack(1)
struct dir_date {
    u8 years_1990;
    u8 month;
    u8 day;
    u8 hour;
    u8 minute;
    u8 second;
    u8 gmt_off;
};

#pragma pack(1)
struct iso_date {
    char str[16];
    u8 gmt_off;
};


struct iso_vol_desc {
    u8 type;
    char std_id[5]; //CD001
    u8 version;
};

struct iso_path_table {
    u8 name_len;   /* Length of name  */
    u8 attr_ex;    /**/
    u32 lba;       /* */
    u16 parent_id; /* Index of the  */
    char name[];   /* Name of this directory */
};

struct iso_dir_record{
    u8 len;               /* Length of this field */
    u8 attr_ex;           /* Extra attribute flags */
    iso_u32 extent_off;   /* Extent offset */
    iso_u32 dlen;         /* Data length  */
    struct dir_date date; /* Record date and time */
    u8 flags;             /* ISO_ATTR flags */
    u8 unit_sz;           /* File unit size if the file is interleaved, otherwise 0 */
    u8 gap_sz;            /* */
    iso_u16 seq_num;
    u8 id_len;
    char id[];
    /* ID, System use */
};

struct iso_psp_app_data { 
    char psp_code[141];
    char std_id[115];
    char reserved[256];
};

/* rx are "reserved" fields which should been zeroed */
struct iso_primary_vol_desc {
    u8 type;        /* = ISO_PRIMARY_VOL_TYPE */
    char std_id[5]; /* CD001 */
    u8 version;
    u8 r1;
    char sysid[32];
    char volid[32];
    char r2[8];
    iso_u32 space_sz;
    char r3[32];
    iso_u16 set_sz;
    iso_u16 seq_num;
    iso_u16 blk_sz;
    iso_u32 tbl_sz;
    u32 l_path_tbl_off;
    u32 opt_l_path_tbl_off;
    u32 m_path_tbl_off;
    u32 opt_m_path_tbl_off;
    struct iso_dir_record root_dir;
    u8 pad; /* Padding for root dir record */
    char vol_set_id[128];
    char publisher_id[128];
    char preparer_id[128];
    char app_id[128];
    char copyright[37];
    char abstract_fileid[37];
    char bibliographic_fileid[37];
    struct iso_date cdate;  /* Creation date */
    struct iso_date mdate;  /* Modification date */
    struct iso_date exdate; /* Expiration date */
    struct iso_date edate;  /* Effective date */
    u8 fver; /* File version */
    u8 r4;
    char app_use[512]; /* Reserved for app usage */
};

struct iso_supplement_vol_desc {
    u8 type;        /* = ISO_SUPPLEMENT_VOL_TYPE */
    char std_id[5]; /* CD001 */
    u8 version;
    u8 vol_flags;
    char sysid[32];
    char volid[32];
    char r1[8];
    iso_u32 space_sz;
    char esc_seq[32];
    iso_u16 set_sz;
    iso_u16 seq_num;
    iso_u16 blk_sz;
    iso_u32 tbl_sz;
    u32 l_path_tbl_off;
    u32 opt_l_path_tbl_off;
    u32 m_path_tbl_off;
    u32 opt_m_path_tbl_off;
    struct iso_dir_record root_dir;
    char vol_set_id[128];
    char publisher_id[128];
    char preparer_id[128];
    char app_id[128];
    char copyright[37];
    char abstract_fileid[37];
    char bibliographic_fileid[37];
    struct iso_date cdate;  /* Creation date */
    struct iso_date mdate;  /* Modification date */
    struct iso_date exdate; /* Expiration date */
    struct iso_date edate;  /* Effective date */
    u8 fver;
    u8 r3;
    char app_use[512];
};

static u16 cpu_to_be16(u16 value)
{
    u16 result;
    u8 *data = (u8*)&result;
    data[0] = (value >> 8) & 0xFF;
    data[1] = value & 0xFF;

    return result;
}

static u32 cpu_to_be32(u32 value) 
{
    u32 result;
    u8 *data = (u8*)&result;
    data[0] = value >> 24;
    data[1] = (value >> 16) & 0xFF;
    data[2] = (value >> 8) & 0xFF;
    data[3] = value & 0xFF;

    return result;
}

#endif
