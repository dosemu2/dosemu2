#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <linux/cdrom.h>

#include "cpu.h"
#include "emu.h"

int cdrom_fd = -1;

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

void cdrom_reset()
{
  /* after a disk change a new read access will 
     return in error. In order to unlock this condition
     the drive must be reopend.
     Does some one knows a better way?                   */
    pd_printf("cdrom reset\n");
  close (cdrom_fd); 
  priv_off();
  cdrom_fd = open (_PATH_CDROM, O_RDONLY);
#ifdef __NetBSD__
  if (cdrom_fd >= 0) ioctl(cdrom_fd, CDIOCALLOW, 0);
#endif
  priv_on();
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
   int n,ioctlin;
   
                /* error ("call function %d !\n", HI(ax));

                ioctlin = 0;
                req_buf = SEG_ADR((char *), es, di);
                error ("\ndebug cdrom: \n");
                for (n = 1; n <= req_buf[0]; ++n) {
                   error ("  %3x", req_buf[n-1]);
                   if ((n % 8) == 0)
                     error ("\n");
                };
                error ("\n");
                if (req_buf[2] == 3) {
                  ioctlin = 1;
                  error ("Ein ioctlin request : ");
                  req_buf = SEG_ADR((char *), ds, si);
                  for (n = 0; n <= 9; ++n)
                     error ("  %3x", req_buf[n]);
                  error ("\n");
                }
                error ("\n");
                */
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
#ifdef __NetBSD__
		if (cdrom_fd >= 0) ioctl(cdrom_fd, CDIOCALLOW, 0);
#endif
		priv_on();
                
                if (cdrom_fd < 0) 
                  LO(ax) = 1;
                 else LO(ax) = 0;
                break;
     case 0x02: /* read long */
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
                lseek (cdrom_fd, Sector*2048, SEEK_SET);
                if (read (cdrom_fd, transfer_buf, *CALC_PTR(req_buf,MSCD_READ_NUMSECTORS,u_short)*2048) < 0)
                  LO(ax) = 1;
                 else LO(ax) = 0;
                break; 
     case 0x03: /* seek */
                req_buf = SEG_ADR((char *), es, di);
                lseek (cdrom_fd, *CALC_PTR(req_buf,MSCD_SEEK_STARTSECTOR,u_long)*2048, SEEK_SET);
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
                if ((audio_status.media_changed) ||
                      ioctl (cdrom_fd, CDROMSUBCHNL, &cdrom_subchnl)) {
                  audio_status.media_changed = 0;
                  LO(bx) = 1; /* media has been changed */
                  if (! ioctl (cdrom_fd, CDROMSUBCHNL, &cdrom_subchnl)) {
                    /* new disk inserted */
                    cdrom_reset();
                  }
                 }
                 else /* media has not changed, check audio status */
                      if (cdrom_subchnl.cdsc_audiostatus == CDROM_AUDIO_PLAY)
                        HI(ax) = 1; /* audio playing in progress */
                break;
     case 0x0A: /* device status */
                HI(ax) = 0; LO(ax) = 0;
                if (ioctl (cdrom_fd, CDROMSUBCHNL, &cdrom_subchnl))
                  if (ioctl (cdrom_fd, CDROMSUBCHNL, &cdrom_subchnl))
                    { /* no disk in drive */
                      LWORD(ebx) = audio_status.status | 0x800;
                      break;
                    }
                   else { /* disk has been changed; new disk in drive ! */
                          cdrom_reset();
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
                if (audio_status.status & 0x02) /* drive unlocked ? */
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
                  error ("Fatal cdrom error(audio disk info); read toc header succeeded but following read entry didn't\n");
                  LO(ax) = 1;
                  break;
                }
                *CALC_PTR(req_buf,MSCD_DISKINFO_LEADOUT+3,u_char) = 0;
                *CALC_PTR(req_buf,MSCD_DISKINFO_LEADOUT+2,u_char) = cdrom_tocentry.cdte_addr.msf.minute;
                *CALC_PTR(req_buf,MSCD_DISKINFO_LEADOUT+1,u_char) = cdrom_tocentry.cdte_addr.msf.second;
                *CALC_PTR(req_buf,MSCD_DISKINFO_LEADOUT+0,u_char) = cdrom_tocentry.cdte_addr.msf.frame;
                break;                
     case 0x11: /* track info */
                req_buf = SEG_ADR((char *), ds, si);
                cdrom_tocentry.cdte_track = *CALC_PTR(req_buf,MSCD_TRACKINFO_TRACKNUM,u_char);
                cdrom_tocentry.cdte_format = CDROM_MSF;
                if (ioctl (cdrom_fd, CDROMREADTOCENTRY, &cdrom_tocentry)) {
                  audio_status.media_changed = 1;
                  if (ioctl (cdrom_fd, CDROMREADTOCENTRY, &cdrom_tocentry)) {
                    /* no disk in drive */
                    LO(ax) = 1;
                    break;
                  }
                }
                *CALC_PTR(req_buf,MSCD_TRACKINFO_TRACKPOS+3,u_char) = 0;
                *CALC_PTR(req_buf,MSCD_TRACKINFO_TRACKPOS+2,u_char) = cdrom_tocentry.cdte_addr.msf.minute;
                *CALC_PTR(req_buf,MSCD_TRACKINFO_TRACKPOS+1,u_char) = cdrom_tocentry.cdte_addr.msf.second;
                *CALC_PTR(req_buf,MSCD_TRACKINFO_TRACKPOS+0,u_char) = cdrom_tocentry.cdte_addr.msf.frame;                  
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
                *CALC_PTR(req_buf,MSCD_GETVOLUMESIZE_SIZE,int) = cdrom_tocentry.cdte_addr.msf.minute*60*75
                                                                    +cdrom_tocentry.cdte_addr.msf.second*60
                                                                    +cdrom_tocentry.cdte_addr.msf.frame;
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
                *CALC_PTR(req_buf,MSCD_QCHAN_MIN,u_char)  = cdrom_subchnl.cdsc_reladdr.msf.minute;
                *CALC_PTR(req_buf,MSCD_QCHAN_SEC,u_char)  = cdrom_subchnl.cdsc_reladdr.msf.second;
                *CALC_PTR(req_buf,MSCD_QCHAN_FRM,u_char)  = cdrom_subchnl.cdsc_reladdr.msf.frame;
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
     default: error ("CDROM: unknown request !\n");
   }

                /* if (ioctlin) {
                  error ("            return  : ");
                  req_buf = SEG_ADR((char *), ds, si);
                  for (n = 0; n <= 9; ++n)
                     error ("  %3x", req_buf[n]);
                  error ("\n\n");
                 }
                else ("\n");

                error ("Leave cdrom request with return status %d !\n\n", LWORD(eax));
                */
   return ;
}
