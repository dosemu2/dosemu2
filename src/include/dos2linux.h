/* dos2linux.h
 *
 * Function prototypes for the DOSEMU/LINUX interface
 *
 */

#ifndef DOS2LINUX_H
#define DOS2LINUX_H

#include "cpu.h"

struct MCB {
	char id;			/* 0 */
	unsigned short owner_psp;	/* 1 */
	unsigned short size;		/* 3 */
	char align8[3];			/* 5 */
	char name[8];			/* 8 */
} __attribute__((packed));

struct PSP {
	unsigned short	opint20;	/* 0x00 */
	unsigned short	memend_frame;	/* 0x02 */
	unsigned char	dos_reserved4;	/* 0x04 */
	unsigned char	cpm_function_entry[0xa-0x5];	/* 0x05 */
	FAR_PTR		int22_copy;	/* 0x0a */
	FAR_PTR		int23_copy;	/* 0x0e */
	FAR_PTR		int24_copy;	/* 0x12 */
	unsigned short	parent_psp;	/* 0x16 */
	unsigned char	file_handles[20];	/* 0x18 */
	unsigned short	envir_frame;	/* 0x2c */
	FAR_PTR		system_stack;	/* 0x2e */
	unsigned short	max_open_files;	/* 0x32 */
	FAR_PTR		file_handles_ptr;	/* 0x34 */
	unsigned char	dos_reserved38[0x50-0x38];	/* 0x38 */
	unsigned char	high_language_dos_call[0x53-0x50];	/* 0x50 */
	unsigned char	dos_reserved53[0x5c-0x53];	/* 0x53 */
	unsigned char	FCB1[0x6c-0x5c];	/* 0x5c */
	unsigned char	FCB2[0x80-0x6c];	/* 0x6c */
	unsigned char	cmdline_len;		/* 0x80 */
	unsigned char	cmdline[0x100-0x81];	/* 0x81 */
} __attribute__((packed));

struct DPB {
	unsigned char drv_num;
	unsigned char unit_num;
	unsigned short bytes_per_sect;
	unsigned char last_sec_in_clust;
	unsigned char sec_shift;
	unsigned short reserv_secs;
	unsigned char num_fats;
	unsigned short root_ents;
	unsigned short data_start;
	unsigned short max_clu;
	unsigned short sects_per_fat;
	unsigned short first_dir_off;
	far_t ddh_ptr;
	unsigned char media_id;
	unsigned char accessed;
	far_t next_DPB;
	unsigned short first_free_clu;
	unsigned short fre_clusts;
} __attribute__((packed));

struct DINFO {
	unsigned short level;
	unsigned int serial;
	unsigned char label[11];
	unsigned char fs_type[8];
} __attribute__((packed));

struct param4a {
    unsigned short envframe;
    far_t cmdline;
    far_t fcb1;
    far_t fcb2;
} __attribute__((packed));

struct lowstring {
	unsigned char len;
	char s[0];
} __attribute__((packed));

typedef u_char *sdb_t;

#define sdb_drive_letter(sdb)	(*(u_char  *)&sdb[sdb_drive_letter_off])
#define sdb_template_name(sdb)	((char     *)&sdb[sdb_template_name_off])
#define sdb_template_ext(sdb)	((char     *)&sdb[sdb_template_ext_off])
#define	sdb_attribute(sdb)	(*(u_char  *)&sdb[sdb_attribute_off])
#define sdb_dir_entry(sdb)	(*(u_short *)&sdb[sdb_dir_entry_off])
#define sdb_p_cluster(sdb)	(*(u_short *)&sdb[sdb_p_cluster_off])
#define	sdb_file_name(sdb)	((char     *)&sdb[sdb_file_name_off])
#define	sdb_file_ext(sdb)	((char     *)&sdb[sdb_file_ext_off])
#define	sdb_file_attr(sdb)	(*(u_char  *)&sdb[sdb_file_attr_off])
#define	sdb_file_time(sdb)	(*(u_short *)&sdb[sdb_file_time_off])
#define	sdb_file_date(sdb)	(*(u_short *)&sdb[sdb_file_date_off])
#define sdb_file_st_cluster(sdb)(*(u_short *)&sdb[sdb_file_st_cluster_off])
#define sdb_file_size(sdb)	(*(u_int   *)&sdb[sdb_file_size_off])

typedef u_char *sft_t;

#define sft_handle_cnt(sft) 	(*(u_short *)&sft[sft_handle_cnt_off])
#define sft_open_mode(sft)  	(*(u_short *)&sft[sft_open_mode_off])
#define sft_attribute_byte(sft) (*(u_char  *)&sft[sft_attribute_byte_off])
#define sft_device_info(sft)  	(*(u_short *)&sft[sft_device_info_off])
#define	sft_dev_drive_ptr(sft)	(*(u_int   *)&sft[sft_dev_drive_ptr_off])
#define	sft_start_cluster(sft)	(*(u_short *)&sft[sft_start_cluster_off])
#define	sft_time(sft)		(*(u_short *)&sft[sft_time_off])
#define	sft_date(sft)		(*(u_short *)&sft[sft_date_off])
#define	sft_size(sft)		(*(u_int   *)&sft[sft_size_off])
#define	sft_position(sft)	(*(u_int   *)&sft[sft_position_off])
#define sft_rel_cluster(sft)	(*(u_short *)&sft[sft_rel_cluster_off])
#define sft_abs_cluster(sft)	(*(u_short *)&sft[sft_abs_cluster_off])
#define	sft_directory_sector(sft) (*(u_short *)&sft[sft_directory_sector_off])
#define	sft_directory_entry(sft)  (*(u_char  *)&sft[sft_directory_entry_off])
#define	sft_name(sft)		( (char    *)&sft[sft_name_off])
#define	sft_ext(sft)		( (char    *)&sft[sft_ext_off])

#define	sft_fd(sft)		(*(u_char *)&sft[sft_fd_off])

typedef u_char *cds_t;
extern cds_t cds_base;
extern cds_t cds;
extern int cds_current_path_off;
extern int cds_rootlen_off;
extern int cds_record_size;


#define	cds_current_path(cds)	((char	   *)&cds[cds_current_path_off])
#define	cds_flags(cds)		(*(u_short *)&cds[cds_flags_off])
#define cds_DBP_pointer(cds)	(*(far_t *)&cds[cds_DBP_pointer_off])
#define cds_cur_cluster(cds)	(*(u_short *)&cds[cds_cur_cluster_off])
#define	cds_rootlen(cds)	(*(u_short *)&cds[cds_rootlen_off])
#define drive_cds(dd) ((cds_t)(((char *)cds_base)+(cds_record_size*(dd))))

#define CDS_FLAG_NOTNET 0x0080
#define CDS_FLAG_SUBST  0x1000
#define CDS_FLAG_JOIN   0x2000
#define CDS_FLAG_READY  0x4000
#define CDS_FLAG_REMOTE 0x8000

#define CDS_DEFAULT_ROOT_LEN	2

typedef u_char *sda_t;
extern sda_t sda;
extern int sda_cur_drive_off;

#define	sda_current_dta(sda)	(FARADDR((far_t *)&sda[sda_current_dta_off]))
#define	sda_error_code(sda)		(*(u_short *)&sda[4])
#define sda_cur_psp(sda)		(*(u_short *)&sda[sda_cur_psp_off])
#define sda_cur_drive(sda)		(*(u_char *)&sda[sda_cur_drive_off])
#define sda_filename1(sda)		((char  *)&sda[sda_filename1_off])
#define	sda_filename2(sda)		((char  *)&sda[sda_filename2_off])
#define sda_sdb(sda)			((sdb_t    )&sda[sda_sdb_off])
#define	sda_cds(sda)		((cds_t)(uintptr_t)(FARPTR((far_t *)&sda[sda_cds_off])))
#define sda_search_attribute(sda)	(*(u_char *)&sda[sda_search_attribute_off])
#define sda_open_mode(sda)		(*(u_char *)&sda[sda_open_mode_off])
#define sda_rename_source(sda)		((sdb_t    )&sda[sda_rename_source_off])
#define sda_user_stack(sda)		((char *)(uintptr_t)(FARPTR((far_t *)&sda[sda_user_stack_off])))

/*
 *  Data for extended open/create operations, DOS 4 or greater:
 */
#define sda_ext_act(sda)		(*(u_short *)&sda[sda_ext_act_off])
#define sda_ext_attr(sda)		(*(u_short *)&sda[sda_ext_attr_off])
#define sda_ext_mode(sda)		(*(u_short *)&sda[sda_ext_mode_off])

#define psp_parent_psp(psp)		(*(u_short *)&psp[0x16])
#define psp_handles(psp)		((char *)(uintptr_t)(FARPTR((far_t *)&psp[0x34])))

#define lol_dpbfarptr(lol)		(rFAR_FARt(READ_DWORD((lol)+lol_dpbfarptr_off)))
#define lol_cdsfarptr(lol)		(rFAR_FARt(READ_DWORD((lol)+lol_cdsfarptr_off)))
#define lol_last_drive(lol)		(READ_BYTE((lol)+lol_last_drive_off))
#define lol_nuldev(lol)		        ((lol)+lol_nuldev_off)
#define lol_njoined(lol)		((lol)+lol_njoined_off)

extern int sdb_drive_letter_off;
extern int sdb_template_name_off;
extern int sdb_template_ext_off;
extern int sdb_attribute_off;
extern int sdb_dir_entry_off;
extern int sdb_p_cluster_off;
extern int sdb_file_name_off;
extern int sdb_file_ext_off;
extern int sdb_file_attr_off;
extern int sdb_file_time_off;
extern int sdb_file_date_off;
extern int sdb_file_st_cluster_off;
extern int sdb_file_size_off;

extern int sft_handle_cnt_off;
extern int sft_open_mode_off;
extern int sft_attribute_byte_off;
extern int sft_device_info_off;
extern int sft_dev_drive_ptr_off;
extern int sft_fd_off;
extern int sft_start_cluster_off;
extern int sft_time_off;
extern int sft_date_off;
extern int sft_size_off;
extern int sft_position_off;
extern int sft_rel_cluster_off;
extern int sft_abs_cluster_off;
extern int sft_directory_sector_off;
extern int sft_directory_entry_off;
extern int sft_name_off;
extern int sft_ext_off;
extern int sft_record_size;

extern int cds_record_size;
extern int cds_current_path_off;
extern int cds_flags_off;
extern int cds_DBP_pointer_off;
extern int cds_cur_cluster_off;
extern int cds_rootlen_off;

extern int sda_current_dta_off;
extern int sda_cur_psp_off;
extern int sda_cur_drive_off;
extern int sda_filename1_off;
extern int sda_filename2_off;
extern int sda_sdb_off;
extern int sda_cds_off;
extern int sda_search_attribute_off;
extern int sda_open_mode_off;
extern int sda_rename_source_off;
extern int sda_user_stack_off;

extern int lol_dpbfarptr_off;
extern int lol_cdsfarptr_off;
extern int lol_last_drive_off;
extern int lol_nuldev_off;
extern int lol_njoined_off;

/*
 * These offsets only meaningful for DOS 4 or greater:
 */
extern int sda_ext_act_off;
extern int sda_ext_attr_off;
extern int sda_ext_mode_off;

typedef unsigned lol_t;
extern lol_t lol;
extern int lol_nuldev_off;

extern int com_errno;
extern int unix_e_welcome;

extern int misc_e6_envvar (char *str);

extern int misc_e6_commandline (char *str, int *is_ux_path);
extern char *misc_e6_options (void);
extern void misc_e6_store_command (char *str, int ux_path);
extern int misc_e6_need_terminate(void);

extern int find_drive (char **linux_path_resolved);
extern int find_free_drive(void);

extern int run_unix_command (char *buffer);
extern int change_config(unsigned item, void *buf, int grab_active, int kbd_grab_active);

void show_welcome_screen(void);
void memcpy_2unix(void *dest, unsigned src, size_t n);
void memcpy_2dos(unsigned dest, const void *src, size_t n);
void memmove_dos2dos(unsigned dest, unsigned src, size_t n);
void memcpy_dos2dos(unsigned dest, unsigned src, size_t n);


int unix_read(int fd, void *data, int cnt);
int dos_read(int fd, unsigned data, int cnt);
int unix_write(int fd, const void *data, int cnt);
int dos_write(int fd, unsigned data, int cnt);
int com_vsprintf(char *str, const char *format, va_list ap);
int com_vsnprintf(char *str, size_t size, const char *format, va_list ap);
int com_sprintf(char *str, char *format, ...) FORMAT(printf, 2, 3);
int com_vfprintf(int dosfilefd, char *format, va_list ap);
int com_vprintf(char *format, va_list ap);
int com_fprintf(int dosfilefd, char *format, ...) FORMAT(printf, 2, 3);
int com_printf(char *format, ...) FORMAT(printf, 1, 2);
int com_puts(char *s);
char *skip_white_and_delim(char *s, int delim);
void pre_msdos(void);
void call_msdos(void);
void post_msdos(void);
int com_doswrite(int dosfilefd, char *buf32, u_short size);
int com_dosread(int dosfilefd, char *buf32, u_short size);
int com_dosreadcon(char *buf32, u_short size);
int com_doswritecon(char *buf32, u_short size);
int com_dosprint(char *buf32);
int com_biosgetch(void);
int com_bioscheckkey(void);
int com_biosread(char *buf32, u_short size);
int com_setcbreak(int on);

static inline u_short dos_get_psp(void)
{
  return sda_cur_psp(sda);
}

void init_all_DOS_tables(void);
extern unsigned char upperDOS_table[0x100];
extern unsigned char lowerDOS_table[0x100];
extern unsigned short dos_to_unicode_table[0x100];
#define toupperDOS(c) (upperDOS_table[(unsigned char)(c)])
#define tolowerDOS(c) (lowerDOS_table[(unsigned char)(c)])
char *strupperDOS(char *s);
char *strlowerDOS(char *s);
#define iscntrlDOS(c) (((unsigned char)(c)) < 0x20)
int strequalDOS(const char *s1, const char *s2);
char *strstrDOS(char *haystack, const char *upneedle);
int name_ufs_to_dos(char *dest, const char *src);

int dos2tty_init(void);
void dos2tty_done(void);

#endif /* DOS2LINUX_H */
