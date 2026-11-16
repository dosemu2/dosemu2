/*
 * cdrom_priv.h - state and MSCDEX request-buffer layout used by cdrom.c.
 */
#ifndef CDROM_PRIV_H
#define CDROM_PRIV_H

struct audio_status {
    unsigned int status;
    unsigned char media_changed;
    unsigned char paused_bit;
    unsigned int last_StartSector, last_EndSector;
    unsigned char outchan0, outchan1, outchan2, outchan3;
    unsigned char volume0, volume1, volume2, volume3;
};
extern struct audio_status audio_status;

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

/* Drive state as the init request leaves it */
static inline void cdrom_status_init(void)
{
    audio_status.status = 0x00000710;   /* see the device status request */
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
}

/*
 * Track control information for the track info request: the track's
 * CONTROL bits in the high nibble, with 0x20 ("digital copy permitted")
 * set.
 */
static inline unsigned char cdrom_track_ctrl(unsigned int ctrl)
{
    return ctrl << 4 | 0x20;
}

#endif /* CDROM_PRIV_H */
