/* drivers/cdrom.c
 *
 * Author: Karsten Rucker (rucker@astro.uni-bonn.de)
 *
 * See dosemu/doc/README.CDROM for further information
 *
 * History:
 *   May 25, 95, Karsten Rucker (rucker@astro.uni-bonn.de)
 *   May 30, 95, Werner Zimmermann (zimmerma@rz.fht-esslingen.de)
 *   - Works with Mitsumi and Aztech/Orchid/Okano/Wearnes CDROM drives
 *   - Minor modifications in comments
 *   Dec 3, 95, lee
 *   - Debugging output changed from *printf()/error() to C_printf()
 *   - Minor editing, no real changes
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#ifdef __linux__
#include <linux/cdrom.h>
#endif
#ifdef __NetBSD__
#include "netbsd_cdio.h"
#include <string.h>
#endif

#include "emu.h"

extern int errno;

int cdrom_fd = -1;
int cdu33a = 0;

/* eject_allowed = 0
 * For Mitsumi and Aztech/Orchid/Okano/Wearnes drives, because eject.com may
 * hang thes system.
 *
 * eject_allowed = 1
 * Allows to eject the cdrom using eject.com.
 */

int eject_allowed = 1;

unsigned int device_status;
struct audio_status { unsigned int status;
		      unsigned char media_changed;
                      unsigned char paused_bit;
                      unsigned int last_StartSector,last_EndSector;
                      unsigned char outchan0,outchan1,outchan2,outchan3;
                      unsigned char volume0,volume1,volume2,volume3;
                    } audio_status;


#define CALC_PTR(PTR,OFFSET,RESULT_TYPE) ((RESULT_TYPE *)(PTR+OFFSET))


#define MSCD_GETVOLUMESIZE_SIZE    1

#define MSCD_READ_ADRESSING       13
#define MSCD_READ_STARTSECTOR     20
#define MSCD_READ_NUMSECTORS      18

#define MSCD_SEEK_STARTSECTOR     20

#define MSCD_PLAY_ADRESSING       13
#define MSCD_PLAY_STARTSECTOR     14
#define MSCD_PLAY_NUMSECTORS      18

#define MSCD_LOCH_ADRESSING        1
#define MSCD_LOCH_LOCATION         2

#define MSCD_DISKINFO_LTN          1
#define MSCD_DISKINFO_HTN          2
#define MSCD_DISKINFO_LEADOUT      3

#define MSCD_TRACKINFO_TRACKNUM    1
#define MSCD_TRACKINFO_TRACKPOS    2
#define MSCD_TRACKINFO_CTRL        6

#define MSCD_QCHAN_CTRL            1
#define MSCD_QCHAN_TNO             2
#define MSCD_QCHAN_IND             3
#define MSCD_QCHAN_MIN             4
#define MSCD_QCHAN_SEC             5
#define MSCD_QCHAN_FRM             6
#define MSCD_QCHAN_ZERO            7
#define MSCD_QCHAN_AMIN            8
#define MSCD_QCHAN_ASEC            9
#define MSCD_QCHAN_AFRM           10

#define MSCD_AUDSTAT_PAUSED        1
#define MSCD_AUDSTAT_START         3
#define MSCD_AUDSTAT_END           7

#define MSCD_CTRL_VOLUME0          2
#define MSCD_CTRL_VOLUME1          4
#define MSCD_CTRL_VOLUME2          6
#define MSCD_CTRL_VOLUME3          8

#ifdef __linux__
#define _PATH_CDROM "/dev/cdrom"
#endif
#ifdef __NetBSD__
#define _PATH_CDROM "/dev/rcd0a"
#endif

#ifdef CDROM_DEBUG
int logging_ioctl(int fd, unsigned int cmd, void *arg)
{
    int rval;
    int err;
    C_printf("ioctl 0x%x\n", cmd);
    rval = ioctl(fd, cmd, arg);
    err = errno;

    if (rval == -1) {
	C_printf(" --> %s\n", strerror(errno));
	errno = err;
    }
    return rval;
}

#define ioctl logging_ioctl
#endif

void cdrom_reset()
{
  /* after a disk change a new read access will
     return in error. In order to unlock this condition
     the drive must be reopend.
     Does some one knows a better way?                   */
    C_printf("cdrom reset\n");
   close (cdrom_fd);
   priv_off();
   cdrom_fd = open (_PATH_CDROM, O_RDONLY);
#ifdef __NetBSD__
   if (cdrom_fd >= 0) ioctl(cdrom_fd, CDIOCALLOW, 0);
#endif
   priv_default();
}

#define MSCD_AUDCHAN_VOLUME0       2
#define MSCD_AUDCHAN_VOLUME1       4
#define MSCD_AUDCHAN_VOLUME2       6
#define MSCD_AUDCHAN_VOLUME3       8

void cdrom_helper(void)
{
   unsigned char *req_buf,*transfer_buf;
   unsigned int Sector_plus_150,Sector;
   struct cdrom_msf cdrom_msf;
   struct cdrom_subchnl cdrom_subchnl;
   struct cdrom_tochdr cdrom_tochdr;
   struct cdrom_tocentry cdrom_tocentry;
   struct cdrom_volctrl cdrom_volctrl;
   int n, error;
#ifdef CDROM_DEBUG
   int ioctlin,;
#endif

#ifdef __NetBSD__
   struct cd_toc_entry toc_entry;
   struct cd_sub_channel_info scinfo;

   memset(&cdrom_subchnl, 0, sizeof(cdrom_subchnl));
   memset(&scinfo, 0, sizeof(scinfo));

   memset(&cdrom_tocentry, 0, sizeof(cdrom_tocentry));
   memset(&toc_entry, 0, sizeof(toc_entry));

   cdrom_subchnl.data = &scinfo;
   cdrom_subchnl.data_len = sizeof(scinfo);
   cdrom_tocentry.data =  &toc_entry;
   cdrom_tocentry.data_len =  sizeof(toc_entry);
#endif
   cdrom_subchnl.cdsc_format = CDROM_MSF;

   if ((cdu33a) && (cdrom_fd < 0)) {
        priv_off();
        cdrom_fd = open (_PATH_CDROM, O_RDONLY);
#ifdef __NetBSD__
        if (cdrom_fd >= 0) ioctl(cdrom_fd, CDIOCALLOW, 0);
#endif
        priv_default();

        if (cdrom_fd < 0) {
          LO(bx) = 1; /* media changed (for media changed request) */
          LWORD(ebx) = audio_status.status | 0x800; /* no disc (for device status request) */

          if ((HI(ax) == 0x09) || (HI(ax) == 0x0A))
            LO(ax) = 0;
           else LO(ax) = 1; /* for other requests return with error */
          return ;
        }
   }

#ifdef __NetBSD__
   cdrom_subchnl.address_format = CD_MSF_FORMAT;
   cdrom_subchnl.data_format = CD_CURRENT_POSITION;
   cdrom_subchnl.track = 0;
#endif

   switch (HI(ax)) {
     case 0x01: audio_status.status = 0x00000310;
                audio_status.paused_bit = 0;
                audio_status.media_changed = 0;
                audio_status.volume0 = 0xFF;
                audio_status.volume1 = 0xFF;
                audio_status.volume2 = 0;
                audio_status.volume3 = 0;
                audio_status.outchan0 = 0;
                audio_status.outchan1 = 1;
                audio_status.outchan2 = 2;
                audio_status.outchan3 = 3;

                priv_off();
                cdrom_fd = open (_PATH_CDROM, O_RDONLY);
		error = errno;
#ifdef __NetBSD__
                if (cdrom_fd >= 0) ioctl(cdrom_fd, CDIOCALLOW, 0);
#endif
                priv_default();

                if (cdrom_fd < 0) {
		  C_printf("cdrom open (" _PATH_CDROM ") failed: %s\n",
			    strerror(error));
                  if (error == EIO) {
                    /* drive which cannot be opened if no
                       disc is inserted!                   */
                    LO(ax) = 0;
                    cdu33a = 1;
                   }
                  else LO(ax) = 1; /* no cdrom drive installed */
                  if (! eject_allowed)
                    LO(ax) = 1; /* no disk in drive */
                 }
                else {
                       LO(ax) = 0;
                       if (! eject_allowed) {
                         if (ioctl (cdrom_fd, CDROMREADTOCHDR, &cdrom_tochdr))
                           if (ioctl (cdrom_fd, CDROMREADTOCHDR, &cdrom_tochdr))
                             LO(ax) = 1;
                       }
                     }
                break;
     case 0x02: /* read long */
                if (eject_allowed && ioctl (cdrom_fd, CDROMSUBCHNL, &cdrom_subchnl)) {
                  audio_status.media_changed = 1;
                  if (ioctl (cdrom_fd, CDROMSUBCHNL, &cdrom_subchnl)) {
                    /* no disc in drive */
                    LO(ax) = 1;
                    break;
                   }
                  else { /* disc in drive */
                       }
                }

                req_buf = SEG_ADR((char *), es, di);
                transfer_buf = SEG_ADR((char *), ds, si);

                if (*CALC_PTR(req_buf,MSCD_READ_ADRESSING,u_char) == 1) {
                  cdrom_msf.cdmsf_min0   = *CALC_PTR(req_buf,MSCD_READ_STARTSECTOR+2,u_char);
                  cdrom_msf.cdmsf_sec0   = *CALC_PTR(req_buf,MSCD_READ_STARTSECTOR+1,u_char);
                  cdrom_msf.cdmsf_frame0 = *CALC_PTR(req_buf,MSCD_READ_STARTSECTOR+0,u_char);
                  Sector = cdrom_msf.cdmsf_min0*60*75+cdrom_msf.cdmsf_sec0*75
                            +cdrom_msf.cdmsf_frame0-150;
                 }
                 else { Sector = *CALC_PTR(req_buf,MSCD_READ_STARTSECTOR,u_long);
                      }

		C_printf("reading sector %x (fmt %d)\n", Sector,
			  *CALC_PTR(req_buf,MSCD_READ_ADRESSING,u_char));
                if ((off_t) -1 == lseek (cdrom_fd, Sector*2048, SEEK_SET)) {
		    HI(ax) = (errno == EINVAL ? 0x08 : 0x0F);
		    C_printf("lseek failed: %s\n", strerror(errno));
		    LO(ax) = 1;
		} else {
		    if ( (n = read (cdrom_fd, transfer_buf, *CALC_PTR(req_buf,MSCD_READ_NUMSECTORS,u_short)*2048)) < 0) {
			/* cd must be in drive, reset drive and try again */
			cdrom_reset();
			if ((off_t) -1 == lseek (cdrom_fd, Sector*2048, SEEK_SET)) {
			    HI(ax) = (errno == EINVAL ? 0x08 : 0x0F);
			    C_printf("lseek failed: %s\n", strerror(errno));
			    LO(ax) = 1;
			} else
			    if ( (n = read (cdrom_fd, transfer_buf, *CALC_PTR(req_buf,MSCD_READ_NUMSECTORS,u_short)*2048)) < 0) {
				HI(ax) = (errno == EFAULT ? 0x0A : 0x0F);
				C_printf("sector read (to %p, len %x) failed: %s\n",
					  transfer_buf, *CALC_PTR(req_buf,MSCD_READ_NUMSECTORS,u_short)*2048, strerror(errno));
				LO(ax) = 1;
			    } else LO(ax) = 0;
		    }
		    if (n != *CALC_PTR(req_buf,MSCD_READ_NUMSECTORS,u_short)*2048) {
			C_printf("sector read len %x got %x\n",
				  *CALC_PTR(req_buf,MSCD_READ_NUMSECTORS,u_short)*2048, n);
			LO(ax) = 1;
			HI(ax) = 0x0F;
		    } else {
			LO(ax) = 0;
			if (dbg_fd) {
			    fwrite(transfer_buf, 1, n, dbg_fd);
			    fflush(dbg_fd);
			}
		    }
		}
                break;
     case 0x03: /* seek */
                req_buf = SEG_ADR((char *), es, di);
                if ((off_t)-1 == lseek (cdrom_fd, *CALC_PTR(req_buf,MSCD_SEEK_STARTSECTOR,u_long)*2048, SEEK_SET)) {
		    C_printf("lseek failed: %s\n", strerror(errno));
		    LO(ax) = 1;
		}
                break;
     case 0x04: /* play */
                req_buf = SEG_ADR((char *), es, di);
                if (*CALC_PTR(req_buf,MSCD_PLAY_ADRESSING,u_char) == 1) {
                  cdrom_msf.cdmsf_min0   = *CALC_PTR(req_buf,MSCD_PLAY_STARTSECTOR+2,u_char);
                  cdrom_msf.cdmsf_sec0   = *CALC_PTR(req_buf,MSCD_PLAY_STARTSECTOR+1,u_char);
                  cdrom_msf.cdmsf_frame0 = *CALC_PTR(req_buf,MSCD_PLAY_STARTSECTOR+0,u_char);
                  Sector_plus_150 = cdrom_msf.cdmsf_min0*60*75+cdrom_msf.cdmsf_sec0*75
                                      +cdrom_msf.cdmsf_frame0;
                  audio_status.last_StartSector = Sector_plus_150;
                 }
                 else { Sector_plus_150 = *CALC_PTR(req_buf,MSCD_PLAY_STARTSECTOR,u_long) + 150;
                        cdrom_msf.cdmsf_min0   = (Sector_plus_150 / (60*75));
                        cdrom_msf.cdmsf_sec0   = (Sector_plus_150 % (60*75)) / 75;
                        cdrom_msf.cdmsf_frame0 = (Sector_plus_150 % (60*75)) % 75;
                        audio_status.last_StartSector = Sector_plus_150;
                      }
                Sector_plus_150 += *CALC_PTR(req_buf,MSCD_PLAY_NUMSECTORS,u_long);
                cdrom_msf.cdmsf_min1   = (Sector_plus_150 / (60*75));
                cdrom_msf.cdmsf_sec1   = (Sector_plus_150 % (60*75)) / 75;
                cdrom_msf.cdmsf_frame1 = (Sector_plus_150 % (60*75)) % 75;

                audio_status.last_EndSector = Sector_plus_150;
                audio_status.paused_bit = 0;
                if (ioctl (cdrom_fd, CDROMPLAYMSF, &cdrom_msf)) {
                  audio_status.media_changed = 1;
                  if (ioctl (cdrom_fd, CDROMPLAYMSF, &cdrom_msf)) {
                    /* no disk in drive */
                    LO(ax) = 1;
                    break;
                  }
                }
                LO(ax) = 0;
                break;
     case 0x05: /* pause (stop) audio */
                LO(ax) = 0;
                if (ioctl (cdrom_fd, CDROMSUBCHNL, &cdrom_subchnl) == 0) {
                  if (cdrom_subchnl.cdsc_audiostatus == CDROM_AUDIO_PLAY) {
                    audio_status.last_StartSector =
                                cdrom_subchnl.cdsc_absaddr.msf.minute*60*75
                                +cdrom_subchnl.cdsc_absaddr.msf.second*75
                                +cdrom_subchnl.cdsc_absaddr.msf.frame;
                    ioctl (cdrom_fd, CDROMPAUSE, NULL);
                    audio_status.paused_bit = 1;
                   }
                  else { audio_status.last_StartSector = 0;
                         audio_status.last_EndSector = 0;
                         audio_status.paused_bit = 0;
                       }
                 }
                 else { audio_status.last_StartSector = 0;
                        audio_status.last_EndSector = 0;
                        audio_status.paused_bit = 0;
                        audio_status.media_changed = 1;
                      }
                break;
     case 0x06: /* resume audio */
                LO(ax) = 0;
                if (audio_status.paused_bit) {
                  if (ioctl (cdrom_fd, CDROMRESUME, NULL) == 0) {
                    audio_status.paused_bit = 0;
                    HI(ax) = 1;
                  }
                 }
                else LO(ax) = 1;
                break;
     case 0x07: /* location of head */
                LWORD(eax) = 0;
                if (ioctl (cdrom_fd, CDROMSUBCHNL, &cdrom_subchnl)) {
                  audio_status.media_changed = 1;
                  if (ioctl (cdrom_fd, CDROMSUBCHNL, &cdrom_subchnl)) {
                    /* no disk in drive */
                    LO(ax) = 1;
                    break;
                  }
                }
                if (cdrom_subchnl.cdsc_audiostatus == CDROM_AUDIO_PLAY)
                  HI(ax) = 1;

                req_buf = SEG_ADR((char *), ds, si);
                if (*CALC_PTR(req_buf,MSCD_LOCH_ADRESSING,u_char) == 0) {
                  *CALC_PTR(req_buf,MSCD_LOCH_LOCATION,u_long)
                     = cdrom_subchnl.cdsc_absaddr.msf.minute*60*75
                            +cdrom_subchnl.cdsc_absaddr.msf.second*75
                             +cdrom_subchnl.cdsc_absaddr.msf.frame-150;
                 }
                 else {/* red book adressing */
                       *CALC_PTR(req_buf,MSCD_LOCH_LOCATION+3,u_char) = 0;
                       *CALC_PTR(req_buf,MSCD_LOCH_LOCATION+2,u_char) = cdrom_subchnl.cdsc_absaddr.msf.minute;
                       *CALC_PTR(req_buf,MSCD_LOCH_LOCATION+1,u_char) = cdrom_subchnl.cdsc_absaddr.msf.second;
                       *CALC_PTR(req_buf,MSCD_LOCH_LOCATION+0,u_char) = cdrom_subchnl.cdsc_absaddr.msf.frame;
                      }
                break;
     case 0x08: /* return sectorsize */
                LO(ax) = 0;
                LWORD(ebx) = 2048;
                break;
     case 0x09: /* media changed */
                /* this function will be called from MSCDEX before
                   each new disk access !                         */
                HI(ax) = 0; LO(ax) = 0; LO(bx) = 0;
		C_printf("media changed?  %x\n", audio_status.media_changed);
		errno = 0;
		if (eject_allowed) {
                  if ((audio_status.media_changed) ||
                        ioctl (cdrom_fd, CDROMSUBCHNL, &cdrom_subchnl)) {
		    if (errno == EIO)
			cdrom_reset();
                    audio_status.media_changed = 0;
                    LO(bx) = 1; /* media has been changed */
		    C_printf("media changed?  yes\n");
                    ioctl (cdrom_fd, CDROMSUBCHNL, &cdrom_subchnl);
                    if (! ioctl (cdrom_fd, CDROMSUBCHNL, &cdrom_subchnl))
                      cdrom_reset(); /* disc in drive */
                   }
                   else /* media has not changed, check audio status */
                        if (cdrom_subchnl.cdsc_audiostatus == CDROM_AUDIO_PLAY)
                          HI(ax) = 1; /* audio playing in progress */
                }
                break;
     case 0x0A: /* device status */
                HI(ax) = 0; LO(ax) = 0;
                if (eject_allowed) {
                  if (ioctl (cdrom_fd, CDROMSUBCHNL, &cdrom_subchnl))
                    if (ioctl (cdrom_fd, CDROMSUBCHNL, &cdrom_subchnl))
                      { /* no disk in drive */
                        LWORD(ebx) = audio_status.status | 0x800;
			C_printf("subch failed: %s\n", strerror(errno));
                        break;
                      }
                     else cdrom_reset();
                }
                /* disk in drive */
                LWORD(ebx) = audio_status.status;
                if (cdrom_subchnl.cdsc_audiostatus == CDROM_AUDIO_PLAY)
                  HI(ax) = 1;
                break;
     case 0x0B: /* drive reset */
                LO(ax) = 0;
                break;
     case 0x0C: /* lock/unlock door */
                if (LO(bx) == 1)
                  audio_status.status &= 0xFFFFFFFD;
                 else audio_status.status |= 0x2;
                LO(ax) = 0;
                break;
     case 0x0D: /* eject */
                LO(ax) = 0;
                if ((eject_allowed) && (audio_status.status & 0x02)) /* drive unlocked ? */
                  if (ioctl (cdrom_fd, CDROMEJECT, NULL))
                    LO(ax) = 1;
                break;
     case 0x0E: /* close tray */
                LO(ax) = 0;
                break;
     case 0x0F: /* audio channel control */
                LWORD(eax) = 0;
                if (ioctl (cdrom_fd, CDROMSUBCHNL, &cdrom_subchnl)) {
                  audio_status.media_changed = 1;
                  if (ioctl (cdrom_fd, CDROMSUBCHNL, &cdrom_subchnl)) {
                    /* no disk in drive */
                    LO(ax) = 1;
                    break;
                  }
                }
                if (cdrom_subchnl.cdsc_audiostatus == CDROM_AUDIO_PLAY)
                  HI(ax) = 1;

                req_buf = SEG_ADR((char *), ds, si);
                cdrom_volctrl.channel0 = *CALC_PTR(req_buf, MSCD_CTRL_VOLUME0, u_char);
                cdrom_volctrl.channel1 = *CALC_PTR(req_buf, MSCD_CTRL_VOLUME1, u_char);
                cdrom_volctrl.channel2 = *CALC_PTR(req_buf, MSCD_CTRL_VOLUME2, u_char);
                cdrom_volctrl.channel3 = *CALC_PTR(req_buf, MSCD_CTRL_VOLUME3, u_char);
                audio_status.volume0 = cdrom_volctrl.channel0;
                audio_status.volume1 = cdrom_volctrl.channel1;
                audio_status.volume2 = cdrom_volctrl.channel2;
                audio_status.volume3 = cdrom_volctrl.channel3;
                audio_status.outchan0 = *CALC_PTR(req_buf, MSCD_CTRL_VOLUME0-1, u_char);
                audio_status.outchan1 = *CALC_PTR(req_buf, MSCD_CTRL_VOLUME1-1, u_char);
                audio_status.outchan2 = *CALC_PTR(req_buf, MSCD_CTRL_VOLUME2-1, u_char);
                audio_status.outchan3 = *CALC_PTR(req_buf, MSCD_CTRL_VOLUME3-1, u_char);
                ioctl (cdrom_fd, CDROMVOLCTRL, &cdrom_volctrl);
                break;
     case 0x10: /* audio disk info */
                LWORD(eax) = 0;
                if (ioctl (cdrom_fd, CDROMREADTOCHDR, &cdrom_tochdr)) {
                  audio_status.media_changed = 1;
                  if (ioctl (cdrom_fd, CDROMREADTOCHDR, &cdrom_tochdr)) {
                    /* no disk in drive */
                    LO(ax) = 1;
                    break;
                  }
                }

                req_buf = SEG_ADR((char *), ds, si);
                *CALC_PTR(req_buf,MSCD_DISKINFO_LTN,u_char) = cdrom_tochdr.cdth_trk0;
                *CALC_PTR(req_buf,MSCD_DISKINFO_HTN,u_char) = cdrom_tochdr.cdth_trk1;
                cdrom_tocentry.cdte_track = CDROM_LEADOUT;
                cdrom_tocentry.cdte_format = CDROM_MSF;
                if (ioctl (cdrom_fd, CDROMREADTOCENTRY, &cdrom_tocentry)) {
                  C_printf ("Fatal cdrom error(audio disk info); read toc header succeeded but following read entry didn't\n");
                  LO(ax) = 1;
                  break;
                }
#ifdef __NetBSD__
                *CALC_PTR(req_buf,MSCD_DISKINFO_LEADOUT+3,u_char) = 0;
                *CALC_PTR(req_buf,MSCD_DISKINFO_LEADOUT+2,u_char) = cdrom_tocentry.data->addr[1];
                *CALC_PTR(req_buf,MSCD_DISKINFO_LEADOUT+1,u_char) = cdrom_tocentry.data->addr[2];
                *CALC_PTR(req_buf,MSCD_DISKINFO_LEADOUT+0,u_char) = cdrom_tocentry.data->addr[3];
#endif
#ifdef __linux__
                *CALC_PTR(req_buf,MSCD_DISKINFO_LEADOUT+3,u_char) = 0;
                *CALC_PTR(req_buf,MSCD_DISKINFO_LEADOUT+2,u_char) = cdrom_tocentry.cdte_addr.msf.minute;
                *CALC_PTR(req_buf,MSCD_DISKINFO_LEADOUT+1,u_char) = cdrom_tocentry.cdte_addr.msf.second;
                *CALC_PTR(req_buf,MSCD_DISKINFO_LEADOUT+0,u_char) = cdrom_tocentry.cdte_addr.msf.frame;
#endif
                break;
     case 0x11: /* track info */
                req_buf = SEG_ADR((char *), ds, si);
                cdrom_tocentry.cdte_track = *CALC_PTR(req_buf,MSCD_TRACKINFO_TRACKNUM,u_char);
                cdrom_tocentry.cdte_format = CDROM_MSF;
		C_printf("track info, track %d\n", cdrom_tocentry.cdte_track);
                if (ioctl (cdrom_fd, CDROMREADTOCENTRY, &cdrom_tocentry)) {
		    /* XXX MSCDEX reads beyond the end of existing tracks.  Sigh. */
		    if (errno != EINVAL)
			audio_status.media_changed = 1;
                  if (ioctl (cdrom_fd, CDROMREADTOCENTRY, &cdrom_tocentry)) {
		      if (errno == EIO) {
			  audio_status.media_changed = 1;
			  /* no disk in drive */
		      }
		      LO(ax) = 1;
                    break;
                  }
                }
#ifdef __NetBSD__
                *CALC_PTR(req_buf,MSCD_TRACKINFO_TRACKPOS+3,u_char) = 0;
                *CALC_PTR(req_buf,MSCD_TRACKINFO_TRACKPOS+2,u_char) = cdrom_tocentry.data->addr[1];
                *CALC_PTR(req_buf,MSCD_TRACKINFO_TRACKPOS+1,u_char) = cdrom_tocentry.data->addr[2];
                *CALC_PTR(req_buf,MSCD_TRACKINFO_TRACKPOS+0,u_char) = cdrom_tocentry.data->addr[3];
#endif
#ifdef __linux__
                *CALC_PTR(req_buf,MSCD_TRACKINFO_TRACKPOS+3,u_char) = 0;
                *CALC_PTR(req_buf,MSCD_TRACKINFO_TRACKPOS+2,u_char) = cdrom_tocentry.cdte_addr.msf.minute;
                *CALC_PTR(req_buf,MSCD_TRACKINFO_TRACKPOS+1,u_char) = cdrom_tocentry.cdte_addr.msf.second;
                *CALC_PTR(req_buf,MSCD_TRACKINFO_TRACKPOS+0,u_char) = cdrom_tocentry.cdte_addr.msf.frame;
#endif
                *CALC_PTR(req_buf,MSCD_TRACKINFO_CTRL,u_char) = cdrom_tocentry.cdte_ctrl << 4 | 0x20;
                LO(ax) = 0;
                break;
     case 0x12: /* volume size */
                cdrom_tocentry.cdte_track = CDROM_LEADOUT;
                cdrom_tocentry.cdte_format = CDROM_MSF;
                if (ioctl (cdrom_fd, CDROMREADTOCENTRY, &cdrom_tocentry)) {
                  audio_status.media_changed = 1;
                  if (ioctl (cdrom_fd, CDROMREADTOCENTRY, &cdrom_tocentry)) {
                    /* no disk in drive */
                    LO(ax) = 1;
                    break;
                  }
                }
                req_buf = SEG_ADR((char *), ds, si);
#ifdef __NetBSD__
                *CALC_PTR(req_buf,MSCD_GETVOLUMESIZE_SIZE,int) =
		     cdrom_tocentry.data->addr[1]*60*75
		     +cdrom_tocentry.data->addr[2]*60
		     +cdrom_tocentry.data->addr[3];
#endif
#ifdef __linux__
                *CALC_PTR(req_buf,MSCD_GETVOLUMESIZE_SIZE,int) = cdrom_tocentry.cdte_addr.msf.minute*60*75
                                                                    +cdrom_tocentry.cdte_addr.msf.second*60
                                                                    +cdrom_tocentry.cdte_addr.msf.frame;
#endif
                LO(ax) = 0;
                break;
     case 0x13: /* q channel */
                LWORD(eax) = 0;
                if (ioctl (cdrom_fd, CDROMSUBCHNL, &cdrom_subchnl)) {
                  audio_status.media_changed = 1;
                  if (ioctl (cdrom_fd, CDROMSUBCHNL, &cdrom_subchnl)) {
                    /* no disk in drive */
                    LO(ax) = 1;
                    break;
                  }
                }
                if (cdrom_subchnl.cdsc_audiostatus == CDROM_AUDIO_PLAY)
                  HI(ax) = 1;

                req_buf = SEG_ADR((char *), ds, si);
                *CALC_PTR(req_buf,MSCD_QCHAN_CTRL,u_char) = (cdrom_subchnl.cdsc_adr << 4) + (cdrom_subchnl.cdsc_ctrl);
                *CALC_PTR(req_buf,MSCD_QCHAN_TNO,u_char)  = cdrom_subchnl.cdsc_trk;
                *CALC_PTR(req_buf,MSCD_QCHAN_IND,u_char)  = cdrom_subchnl.cdsc_ind;
#ifdef __NetBSD__
                *CALC_PTR(req_buf,MSCD_QCHAN_MIN,u_char)  = cdrom_subchnl.data->what.position.reladdr[1];
                *CALC_PTR(req_buf,MSCD_QCHAN_SEC,u_char)  = cdrom_subchnl.data->what.position.reladdr[2];
                *CALC_PTR(req_buf,MSCD_QCHAN_FRM,u_char)  = cdrom_subchnl.data->what.position.reladdr[3];
#endif
#ifdef __linux__
                *CALC_PTR(req_buf,MSCD_QCHAN_MIN,u_char)  = cdrom_subchnl.cdsc_reladdr.msf.minute;
                *CALC_PTR(req_buf,MSCD_QCHAN_SEC,u_char)  = cdrom_subchnl.cdsc_reladdr.msf.second;
                *CALC_PTR(req_buf,MSCD_QCHAN_FRM,u_char)  = cdrom_subchnl.cdsc_reladdr.msf.frame;
#endif
                *CALC_PTR(req_buf,MSCD_QCHAN_ZERO,u_char) = 0;
                *CALC_PTR(req_buf,MSCD_QCHAN_AMIN,u_char) = cdrom_subchnl.cdsc_absaddr.msf.minute;
                *CALC_PTR(req_buf,MSCD_QCHAN_ASEC,u_char) = cdrom_subchnl.cdsc_absaddr.msf.second;
                *CALC_PTR(req_buf,MSCD_QCHAN_AFRM,u_char) = cdrom_subchnl.cdsc_absaddr.msf.frame;
                break;
     case 0x14: /* audio status */
                LWORD(eax) = 0;
                if (ioctl (cdrom_fd, CDROMSUBCHNL, &cdrom_subchnl)) {
                  audio_status.media_changed = 1;
                  if (ioctl (cdrom_fd, CDROMSUBCHNL, &cdrom_subchnl)) {
                    /* no disk in drive */
                    LO(ax) = 1;
                    break;
                  }
                }
                if (cdrom_subchnl.cdsc_audiostatus == CDROM_AUDIO_PLAY)
                  HI(ax) = 1;

                req_buf = SEG_ADR((char *), ds, si);
                *CALC_PTR(req_buf,MSCD_AUDSTAT_PAUSED,u_short)= audio_status.paused_bit;
                *CALC_PTR(req_buf,MSCD_AUDSTAT_START ,u_long) = audio_status.last_StartSector;
                *CALC_PTR(req_buf,MSCD_AUDSTAT_END   ,u_long) = audio_status.last_EndSector;
                break;
     case 0x15: /* get audio channel information */
                LWORD(eax) = 0;
                if (ioctl (cdrom_fd, CDROMSUBCHNL, &cdrom_subchnl)) {
                  audio_status.media_changed = 1;
                  if (ioctl (cdrom_fd, CDROMSUBCHNL, &cdrom_subchnl)) {
                    /* no disk in drive */
                    LO(ax) = 1;
                    break;
                  }
                }
                if (cdrom_subchnl.cdsc_audiostatus == CDROM_AUDIO_PLAY)
                  HI(ax) = 1;

                req_buf = SEG_ADR((char *), ds, si);
                *CALC_PTR(req_buf,MSCD_AUDCHAN_VOLUME0,u_char) = audio_status.volume0;
                *CALC_PTR(req_buf,MSCD_AUDCHAN_VOLUME1,u_char) = audio_status.volume1;
                *CALC_PTR(req_buf,MSCD_AUDCHAN_VOLUME2,u_char) = audio_status.volume2;
                *CALC_PTR(req_buf,MSCD_AUDCHAN_VOLUME3,u_char) = audio_status.volume3;
                *CALC_PTR(req_buf,MSCD_AUDCHAN_VOLUME0-1,u_char) = audio_status.outchan0;
                *CALC_PTR(req_buf,MSCD_AUDCHAN_VOLUME1-1,u_char) = audio_status.outchan1;
                *CALC_PTR(req_buf,MSCD_AUDCHAN_VOLUME2-1,u_char) = audio_status.outchan2;
                *CALC_PTR(req_buf,MSCD_AUDCHAN_VOLUME3-1,u_char) = audio_status.outchan3;
                break;
     default: C_printf ("CDROM: unknown request !\n");
   }

#ifdef CDROM_DEBUG
                if (ioctlin) {
                  C_printf ("            return  : ");
                  req_buf = SEG_ADR((char *), ds, si);
                  for (n = 0; n <= 9; ++n)
                     C_printf ("  %3x", req_buf[n]);
                  C_printf ("\n");
                 }
                else ("\n");

                C_printf ("Leave cdrom request with return status %d.\n", LWORD(eax));
#endif
   return ;
}
