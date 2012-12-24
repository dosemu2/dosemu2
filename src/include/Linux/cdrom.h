/*
 * -- <linux/cdrom.h>
 * general (not only SCSI) header library for linux CDROM drivers
 * (C) 1992         David Giller rafetmad@oxy.edu
 *     1994, 1995   Eberhard Moenkeberg emoenke@gwdg.de
 *
 */

#ifndef	_LINUX_CDROM_H
#define	_LINUX_CDROM_H

/*
 * some fix numbers
 */
#define CD_MINS             74 /* max. minutes per CD, not really a limit */
#define CD_SECS             60 /* seconds per minute */
#define CD_FRAMES           75 /* frames per second */

#define CD_SYNC_SIZE        12 /* 12 sync bytes per raw data frame, not transfered by the drive */
#define CD_HEAD_SIZE         4 /* header (address) bytes per raw data frame */
#define CD_SUBHEAD_SIZE      8 /* subheader bytes per raw XA data frame */
#define CD_XA_HEAD        (CD_HEAD_SIZE+CD_SUBHEAD_SIZE) /* "before data" part of raw XA frame */
#define CD_XA_SYNC_HEAD   (CD_SYNC_SIZE+CD_XA_HEAD)/* sync bytes + header of XA frame */

#define CD_FRAMESIZE      2048 /* bytes per frame, "cooked" mode */
#define CD_FRAMESIZE_RAW  2352 /* bytes per frame, "raw" mode */
/* most drives don't deliver everything: */
#define CD_FRAMESIZE_RAW1 (CD_FRAMESIZE_RAW-CD_SYNC_SIZE) /* 2340 */
#define CD_FRAMESIZE_RAW0 (CD_FRAMESIZE_RAW-CD_SYNC_SIZE-CD_HEAD_SIZE) /* 2336 */
/* Optics drive also has a 'read all' mode: */
#define CD_FRAMESIZE_RAWER 2646 /* bytes per frame */

#define CD_EDC_SIZE         4 /* bytes EDC per most raw data frame types */
#define CD_ZERO_SIZE        8 /* bytes zero per yellow book mode 1 frame */
#define CD_ECC_SIZE       276 /* bytes ECC per most raw data frame types */
#define CD_XA_TAIL        (CD_EDC_SIZE+CD_ECC_SIZE) /* "after data" part of raw XA frame */

#define CD_FRAMESIZE_SUB    96 /* subchannel data "frame" size */
#define CD_MSF_OFFSET      150 /* MSF numbering offset of first frame */

#define CD_CHUNK_SIZE       24 /* lowest-level "data bytes piece" */
#define CD_NUM_OF_CHUNKS    98 /* chunks per frame */

#define CD_FRAMESIZE_XA CD_FRAMESIZE_RAW1 /* obsolete name */
#define CD_BLOCK_OFFSET    CD_MSF_OFFSET /* obsolete name */

/*
 * the raw frame layout:
 *
 * - audio (red):                  | audio_sample_bytes |
 *                                 |        2352        |
 *
 * - data (yellow, mode1):         | sync - head - data - EDC - zero - ECC |
 *                                 |  12  -   4  - 2048 -  4  -   8  - 276 |
 *
 * - data (yellow, mode2):         | sync - head - data |
 *                                 |  12  -   4  - 2336 |
 *
 * - XA data (green, mode2 form1): | sync - head - sub - data - EDC - ECC |
 *                                 |  12  -   4  -  8  - 2048 -  4  - 276 |
 *
 * - XA data (green, mode2 form2): | sync - head - sub - data - EDC |
 *                                 |  12  -   4  -  8  - 2324 -  4  |
 */

/*
 * CDROM IOCTL structures
 */

struct cdrom_blk
{
	unsigned from;
	unsigned short len;
};


struct cdrom_msf
{
	u_char	cdmsf_min0;	/* start minute */
	u_char	cdmsf_sec0;	/* start second */
	u_char	cdmsf_frame0;	/* start frame */
	u_char	cdmsf_min1;	/* end minute */
	u_char	cdmsf_sec1;	/* end second */
	u_char	cdmsf_frame1;	/* end frame */
};

struct cdrom_ti
{
	u_char	cdti_trk0;	/* start track */
	u_char	cdti_ind0;	/* start index */
	u_char	cdti_trk1;	/* end track */
	u_char	cdti_ind1;	/* end index */
};

struct cdrom_tochdr
{
	u_char	cdth_trk0;	/* start track */
	u_char	cdth_trk1;	/* end track */
};

struct cdrom_msf0		/* address in MSF format */
{
	u_char	minute;
	u_char	second;
	u_char	frame;
};

union cdrom_addr		/* address in either MSF or logical format */
{
	struct cdrom_msf0	msf;
	int			lba;
};

struct cdrom_tocentry
{
	u_char	cdte_track;
	u_char	cdte_adr	:4;
	u_char	cdte_ctrl	:4;
	u_char	cdte_format;
	union cdrom_addr cdte_addr;
	u_char	cdte_datamode;
};

/*
 * CD-ROM address types (cdrom_tocentry.cdte_format)
 */
#define	CDROM_LBA 0x01 /* "logical block": first frame is #0 */
#define	CDROM_MSF 0x02 /* "minute-second-frame": binary, not bcd here! */

/*
 * bit to tell whether track is data or audio (cdrom_tocentry.cdte_ctrl)
 */
#define	CDROM_DATA_TRACK	0x04

/*
 * The leadout track is always 0xAA, regardless of # of tracks on disc
 */
#define	CDROM_LEADOUT	0xAA

struct cdrom_subchnl
{
	u_char	cdsc_format;
	u_char	cdsc_audiostatus;
	u_char	cdsc_adr:	4;
	u_char	cdsc_ctrl:	4;
	u_char	cdsc_trk;
	u_char	cdsc_ind;
	union cdrom_addr cdsc_absaddr;
	union cdrom_addr cdsc_reladdr;
};

struct cdrom_mcn {
  u_char medium_catalog_number[14]; /* 13 ASCII digits, null-terminated */
};

/*
 * audio states (from SCSI-2, but seen with other drives, too)
 */
#define	CDROM_AUDIO_INVALID	0x00	/* audio status not supported */
#define	CDROM_AUDIO_PLAY	0x11	/* audio play operation in progress */
#define	CDROM_AUDIO_PAUSED	0x12	/* audio play operation paused */
#define	CDROM_AUDIO_COMPLETED	0x13	/* audio play successfully completed */
#define	CDROM_AUDIO_ERROR	0x14	/* audio play stopped due to error */
#define	CDROM_AUDIO_NO_STATUS	0x15	/* no current audio status to return */

struct cdrom_volctrl
{
	u_char	channel0;
	u_char	channel1;
	u_char	channel2;
	u_char	channel3;
};

struct cdrom_read
{
	int	cdread_lba;
	caddr_t	cdread_bufaddr;
	int	cdread_buflen;
};

/*
 * extensions for transferring audio frames
 * currently used by sbpcd.c, cdu31a.c, ide-cd.c
 */
struct cdrom_read_audio
{
	union cdrom_addr addr; /* frame address */
	u_char addr_format; /* CDROM_LBA or CDROM_MSF */
	int nframes; /* number of 2352-byte-frames to read at once, limited by the drivers */
	u_char *buf; /* frame buffer (size: nframes*2352 bytes) */
};

/*
 * this has to be the "arg" of the CDROMMULTISESSION ioctl
 * for obtaining multi session info.
 * The returned "addr" is valid only if "xa_flag" is true.
 */
struct cdrom_multisession
{
	union cdrom_addr addr; /* frame address: start-of-last-session (not the new "frame 16"!)*/
	u_char xa_flag; /* 1: "is XA disk" */
	u_char addr_format; /* CDROM_LBA or CDROM_MSF */
};

#ifdef FIVETWELVE
#define	CDROM_MODE1_SIZE	512
#else
#define	CDROM_MODE1_SIZE	2048
#endif /* FIVETWELVE */
#define	CDROM_MODE2_SIZE	2336

/*
 * CD-ROM IOCTL commands
 * For IOCTL calls, we will commandeer byte 0x53, or 'S'.
 */

#define CDROMPAUSE		0x5301
#define CDROMRESUME		0x5302
#define CDROMPLAYMSF		0x5303	/* (struct cdrom_msf) */
#define CDROMPLAYTRKIND		0x5304	/* (struct cdrom_ti) */

#define CDROMREADTOCHDR		0x5305	/* (struct cdrom_tochdr) */
#define CDROMREADTOCENTRY	0x5306	/* (struct cdrom_tocentry) */

#define CDROMSTOP		0x5307	/* stop the drive motor	*/
#define CDROMSTART		0x5308	/* turn the motor on */

#define CDROMEJECT		0x5309	/* eject CD-ROM media */

#define CDROMVOLCTRL		0x530a	/* (struct cdrom_volctrl) */

#define CDROMSUBCHNL		0x530b	/* (struct cdrom_subchnl) */

#define CDROMREADMODE2		0x530c	/* (struct cdrom_read) */
                                          /* read type-2 data */

#define CDROMREADMODE1		0x530d	/* (struct cdrom_read) */
                                          /* read type-1 data */

#define CDROMREADAUDIO		0x530e	/* (struct cdrom_read_audio) */

/*
 * enable (1) / disable (0) auto-ejecting
 */
#define CDROMEJECT_SW		0x530f	/* arg: 0 or 1 */

/*
 * obtain the start-of-last-session address of multi session disks
 */
#define CDROMMULTISESSION	0x5310	/* (struct cdrom_multisession) */

/*
 * obtain the "universal product code" number
 * (only some data disks have it coded)
 */
#define CDROM_GET_UPC		0x5311	/* 8 bytes returned */

#define CDROMRESET		0x5312	/* hard-reset the drive */
#define CDROMVOLREAD		0x5313	/* let the drive tell its volume setting */
					/* (struct cdrom_volctrl) */

/*
 * these ioctls are used in aztcd.c and optcd.c
 */
#define CDROMREADRAW		0x5314	/* read data in raw mode */
#define CDROMREADCOOKED		0x5315	/* read data in cooked mode */
#define CDROMSEEK		0x5316  /* seek msf address */

/*
 * for playing audio in logical block addressing mode
 */
#define CDROMPLAYBLK		0x5317	/* (struct cdrom_blk) */

/*
 * these ioctls are used in optcd.c
 */
#define CDROMREADALL		0x5318	/* read all 2646 bytes */
#define CDROMCLOSETRAY		0x5319	/* pendant of CDROMEJECT */


/*
 * For controlling a changer.  (Used by ATAPI driver.)
 * This ioctl is depreciated in favor of CDROM_SELECT_DISC from
 * ucdrom.h.  It will probably be deleted during the 2.1 kernel series.
 */
#define CDROMLOADFROMSLOT	0x531a	/* LOAD disk from slot*/


/*
 * CD-ROM-specific SCSI command opcodes
 */

/*
 * Group 2 (10-byte).  All of these are called 'optional' by SCSI-II.
 */
#define SCMD_READ_TOC		0x43	/* read table of contents */
#define SCMD_PLAYAUDIO_MSF	0x47	/* play data at time offset */
#define SCMD_PLAYAUDIO_TI	0x48	/* play data at track/index */
#define SCMD_PAUSE_RESUME	0x4B	/* pause/resume audio */
#define SCMD_READ_SUBCHANNEL	0x42	/* read SC info on playing disc */
#define SCMD_PLAYAUDIO10	0x45	/* play data at logical block */
#define SCMD_READ_HEADER	0x44	/* read TOC header */

/*
 * Group 5
 */
#define SCMD_PLAYAUDIO12	0xA5 	/* play data at logical block */
#define SCMD_PLAYTRACK_REL12	0xA9	/* play track at relative offset */

/*
 * Group 6 Commands
 */
#define SCMD_CD_PLAYBACK_CONTROL 0xC9	/* Sony vendor-specific audio */
#define SCMD_CD_PLAYBACK_STATUS 0xC4	/* control opcodes */

/*
 * CD-ROM capacity structure.
 */
struct scsi_capacity
{
	u_long	capacity;
	u_long	lbasize;
};

/*
 * CD-ROM MODE_SENSE/MODE_SELECT parameters
 */
#define ERR_RECOVERY_PARMS	0x01
#define DISCO_RECO_PARMS	0x02
#define FORMAT_PARMS		0x03
#define GEOMETRY_PARMS		0x04
#define CERTIFICATION_PARMS	0x06
#define CACHE_PARMS		0x38

/*
 * standard mode-select header prepended to all mode-select commands
 */
struct ccs_modesel_head
{
	u_char	_r1;	/* reserved */
	u_char	medium;	/* device-specific medium type */
	u_char 	_r2;	/* reserved */
	u_char	block_desc_length; /* block descriptor length */
	u_char	density; /* device-specific density code */
	u_char	number_blocks_hi; /* number of blocks in this block desc */
	u_char	number_blocks_med;
	u_char	number_blocks_lo;
	u_char	_r3;
	u_char	block_length_hi; /* block length for blocks in this desc */
	u_short	block_length;
};

/*
 * error recovery parameters
 */
struct ccs_err_recovery
{
	u_char	_r1 : 2;	/* reserved */
	u_char	page_code : 6;	/* page code */
	u_char	page_length;	/* page length */
	u_char	awre	: 1;	/* auto write realloc enabled */
	u_char	arre	: 1;	/* auto read realloc enabled */
	u_char	tb	: 1;	/* transfer block */
	u_char 	rc	: 1;	/* read continuous */
	u_char	eec	: 1;	/* enable early correction */
	u_char	per	: 1;	/* post error */
	u_char	dte	: 1;	/* disable transfer on error */
	u_char	dcr	: 1;	/* disable correction */
	u_char	retry_count;	/* error retry count */
	u_char	correction_span; /* largest recov. to be attempted, bits */
	u_char	head_offset_count; /* head offset (2's C) for each retry */
	u_char	strobe_offset_count; /* data strobe */
	u_char	recovery_time_limit; /* time limit on recovery attempts	*/
};

/*
 * disco/reco parameters
 */
struct ccs_disco_reco
{
	u_char	_r1	: 2;	/* reserved */
	u_char	page_code : 6;	/* page code */
	u_char	page_length;	/* page length */
	u_char	buffer_full_ratio; /* write buffer reconnect threshold */
	u_char	buffer_empty_ratio; /* read */
	u_short	bus_inactivity_limit; /* limit on bus inactivity time */
	u_short	disconnect_time_limit; /* minimum disconnect time */
	u_short	connect_time_limit; /* minimum connect time */
	u_short	_r2;		/* reserved */
};

/*
 * drive geometry parameters
 */
struct ccs_geometry
{
	u_char	_r1	: 2;	/* reserved */
	u_char	page_code : 6;	/* page code */
	u_char	page_length;	/* page length */
	u_char	cyl_ub;		/* #cyls */
	u_char	cyl_mb;
	u_char	cyl_lb;
	u_char	heads;		/* #heads */
	u_char	precomp_cyl_ub;	/* precomp start */
	u_char	precomp_cyl_mb;
	u_char	precomp_cyl_lb;
	u_char	current_cyl_ub;	/* reduced current start */
	u_char	current_cyl_mb;
	u_char	current_cyl_lb;
	u_short	step_rate;	/* stepping motor rate */
	u_char	landing_cyl_ub;	/* landing zone */
	u_char	landing_cyl_mb;
	u_char	landing_cyl_lb;
	u_char  _r2;
	u_char	_r3;
	u_char	_r4;
};

/*
 * cache parameters
 */
struct ccs_cache
{
	u_char	_r1	: 2;	/* reserved */
	u_char	page_code : 6;	/* page code */
	u_char	page_length;	/* page length */
	u_char	mode;		/* cache control byte */
	u_char	threshold;	/* prefetch threshold */
	u_char	max_prefetch;	/* maximum prefetch size */
	u_char	max_multiplier;	/* maximum prefetch multiplier */
	u_char	min_prefetch;	/* minimum prefetch size */
	u_char	min_multiplier;	/* minimum prefetch multiplier */
	u_char	_r2[8];
};

#endif  /* _LINUX_CDROM_H */
/*==========================================================================*/
/*
 * Overrides for Emacs so that we follow Linus's tabbing style.
 * Emacs will notice this stuff at the end of the file and automatically
 * adjust the settings for this buffer only.  This must remain at the end
 * of the file.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-indent-level: 8
 * c-brace-imaginary-offset: 0
 * c-brace-offset: -8
 * c-argdecl-indent: 8
 * c-label-offset: -8
 * c-continued-statement-offset: 8
 * c-continued-brace-offset: 0
 * End:
 */
