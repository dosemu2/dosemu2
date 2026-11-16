/*
 * redbook.h - Red Book CD addressing in minutes, seconds and frames (MSF).
 *
 * There are 75 frames to the second and 60 seconds to the minute.  MSF
 * time starts with the 2-second pregap of track 1, so logical block
 * address (LBA) 0 is MSF 00:02:00.
 */
#ifndef REDBOOK_H
#define REDBOOK_H

#include <stdint.h>

/* The same name and value as in <linux/cdrom.h>. */
#ifndef CD_MSF_OFFSET
#define CD_MSF_OFFSET 150       /* LBA 0 == MSF 00:02:00 */
#endif

static inline uint32_t cd_msf_to_frames(unsigned m, unsigned s, unsigned f)
{
  return (m * 60 + s) * 75 + f;
}

static inline void cd_frames_to_msf(uint32_t frames, uint8_t *m,
                                    uint8_t *s, uint8_t *f)
{
  *m = frames / (60 * 75);
  *s = (frames / 75) % 60;
  *f = frames % 75;
}

/* An absolute MSF as an LBA; addresses inside the pregap give 0. */
static inline uint32_t cd_msf_to_lba(unsigned m, unsigned s, unsigned f)
{
  uint32_t n = cd_msf_to_frames(m, s, f);

  return n < CD_MSF_OFFSET ? 0 : n - CD_MSF_OFFSET;
}

static inline void cd_lba_to_msf(uint32_t lba, uint8_t *m, uint8_t *s,
                                 uint8_t *f)
{
  cd_frames_to_msf(lba + CD_MSF_OFFSET, m, s, f);
}

#endif                          /* REDBOOK_H */
