/*
 * $Id: netbsd_cdio.h,v 1.2 1995/07/13 02:09:59 jtk Exp $
 */

#ifndef _NETBSD_CDIO_H_
#define  _NETBSD_CDIO_H_

#include <sys/ioctl.h>
#include <sys/cdio.h>

#define cdrom_msf ioc_play_msf
#define cdmsf_min0	start_m
#define cdmsf_sec0	start_s
#define cdmsf_frame0	start_f
#define cdmsf_min1	end_m
#define cdmsf_sec1	end_s
#define cdmsf_frame1	end_f
#define CDROMPLAYMSF	CDIOCPLAYMSF

#define cdrom_subchnl ioc_read_subchannel
#define cdsc_audiostatus	data->header.audio_status
#define CDROM_AUDIO_PLAY	CD_AS_PLAY_IN_PROGRESS
#define CDROMSUBCHNL	CDIOCREADSUBCHANNEL
#define CDROMPAUSE	CDIOCPAUSE
#define CDROMRESUME	CDIOCRESUME
#define CDROMEJECT	CDIOCEJECT
#define	cdsc_absaddr	data->what
#define		msf	position
#define			minute	absaddr[1]
#define			second	absaddr[2]
#define			frame	absaddr[3]
#define	cdsc_adr	data->what.position.addr_type
#define	cdsc_trk	data->what.position.track_number
#define	cdsc_ind	data->what.position.index_number
#define	cdsc_ind	data->what.position.index_number
#define	cdsc_ctrl	data->what.position.control
#define	cdsc_format	address_format

#define CDROMVOLCTRL	CDIOCSETVOL
#define cdrom_volctrl	ioc_vol
#define 	channel0	vol[0]
#define 	channel1	vol[1]
#define 	channel2	vol[2]
#define 	channel3	vol[3]

#define	CDROMREADTOCHDR	CDIOREADTOCHEADER
#define	cdrom_tochdr	ioc_toc_header
#define		cdth_trk0	starting_track
#define		cdth_trk1	ending_track

#define	cdrom_tocentry	ioc_read_toc_entry
#define	CDROMREADTOCENTRY	CDIOREADTOCENTRYS
#define	cdte_addr	data->addr
#define	cdte_track	starting_track
#define	cdte_format	address_format
#define	cdte_ctrl	data->control
#define	CDROM_MSF	CD_MSF_FORMAT

#define	CDROM_LEADOUT	0xAA

#endif
