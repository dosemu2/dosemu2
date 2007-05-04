/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* 
 * DANG_BEGIN_MODULE
 * REMARK
 * VERB
 * drivers/aspi.c - emulate the DOS ASPI programming interface using 
 *		    the Linux generic SCSI driver (/dev/sg?)
 * basic aspi code shamelessly stolen from the wine
 * project aspi.c written by  Bruce Milner <Bruce.Milner@genetics.utah.edu>
 *
 * hacked by         Karl Kiniger <ki@kretz.co.at>
 * further hacked by Hans Lermen <lermen@dosemu.org>
 *
 * For now there are several limitations I am aware of:
 *
 * - Residual byte length reporting not handled
 * - Only Linux supported so far
 * - SG_BIG_BUFF may need to be adjusted and the sg driver therefore be recompiled
 * - No posting routine possible - reentrancy needed for posting, since the
 *   posting routine may call ASPI services
 * - Blocking read/write to/from /dev/sg? used
 * - Too few sense bytes returned (16 instead of 18). This is a limitiation
 *     from the sg driver.
 * - SCSI timeout may need to be increased - formatting a DAT tape may take some time
 *     set to 5 minutes for now.
 * - 'Direction' table very incomplete (see below)
 * - Debug output not well organized/formatted
 * - no access control for now
 * - Look at the various FIXME's
 * - all hostadapters are mapped to _one_ single seen by DOS,
 *   but the targetID can be mapped differently.
 * - LUNs have to be equal for real and DOS seen ones, because we
 *   don't translate the LUNs in the CDB.
 * /VERB
 * /REMARK
 * DANG_END_MODULE
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <memory.h>
#include <unistd.h>

#include <scsi/sg.h>

#include "emu.h"
#include "aspi.h"


#define SCSI_OFF sizeof(struct sg_header)

#define ASPI_POSTING(prb) (prb->SRB_Flags & 0x1)

#define HOST_TO_TARGET(prb) (((prb->SRB_Flags>>3) & 0x3) == 0x2)
#define TARGET_TO_HOST(prb) (((prb->SRB_Flags>>3) & 0x3) == 0x1)
#define NO_DATA_TRANSFERED(prb) (((prb->SRB_Flags>>3) & 0x3) == 0x3)

#define U_HOST_TO_TARGET(prb) (sc_d[prb->CDBByte[0]]=='O')
#define U_TARGET_TO_HOST(prb) (sc_d[prb->CDBByte[0]]=='I')
#define U_NO_DATA_TRANSFERED(prb) (sc_d[prb->CDBByte[0]]=='N')

/* FIXME - the following table is NOT CHECKED for completeness */
/* it is used if the corresponding bits in SRB_Flags are not set */

static unsigned char sc_d[256] = {  /* transfer direction: In,Out,None */

'N','N','N','N','N','I','N','O', 'I','N','O','N','N','N','N','N',
'N','N','I','N','N','O','N','N', 'N','N','I','N','I','O','N','N',
'N','N','N','N','O','I','N','N', 'I','I','O','N','N','I','O','N',
'N','N','N','N','I','N','N','I', 'N','N','N','O','I','N','I','O',

'N','N','N','I','N','N','N','N', 'N','N','N','N','O','I','N','N',
'N','N','N','N','N','O','N','N', 'N','N','I','N','N','N','N','N',
'N','N','N','N','N','N','N','N', 'N','N','N','N','N','N','N','N',
'N','N','N','N','N','N','N','N', 'N','N','N','N','N','N','N','N',


'N','N','N','N','N','N','N','N', 'N','N','N','N','N','N','N','N',
'N','N','N','N','N','N','N','N', 'N','N','N','N','N','N','N','N',
'N','N','N','N','N','N','N','N', 'I','N','O','N','N','N','N','N',
'N','N','N','N','N','N','N','N', 'N','N','N','N','N','N','N','N',

'N','N','N','N','N','N','N','N', 'N','N','N','N','N','N','N','N',
'N','N','N','N','N','N','N','N', 'I','N','N','N','N','N','N','N',
'N','N','N','N','N','N','N','N', 'N','N','N','N','N','N','N','N',
'N','N','N','N','N','N','N','N', 'N','N','N','N','N','N','N','N'

};

#define SRB_ENABLE_RESIDUAL_COUNT 0x4

#define INQUIRY_VENDOR		8

#define CMD_TEST_UNIT_READY 0x00
#define CMD_REQUEST_SENSE 0x03
#define CMD_INQUIRY 0x12


#define INQURIY_CMDLEN 6
#define INQURIY_REPLY_LEN 96
#define INQUIRY_VENDOR 8

#define SENSE_BUFFER(prb) (&prb->CDBByte[prb->SRB_CDBLen])


/* -------------------------------------------------------- */

struct scsi_device_info {
  int fd;
  int sgminor;
  int hostId;
  int channel;
  int target;
  int lun;
  int dos_seen_target;
  int devtype;
  int ansirev;
  char *vendor;
  char *model;
  char *modelrev;
};

struct CDB_inquiry {
  char cmd;
  unsigned char EVPD:1;
  unsigned char byte1res:4;
  unsigned char lun:3;
  char pagecode;
  char reserved;
  unsigned char alloclen;
  unsigned char byte5res:6;
  unsigned char vendorunique:2;
} __attribute__ ((packed));

struct standard_inquiry_data {
  unsigned char peripheral_device_type:5; /* 00 */
  unsigned char peripheral_qualifier:3;

  unsigned char device_type_modifier:7;	/* 01 */
  unsigned char removable_media:1;

  unsigned char ansi_version:3;		/* 02 */
  unsigned char ECMA_version:3;
  unsigned char ISO_version:2;

  unsigned char response_data_format:4;	/* 03 */
  unsigned char byte3res:2;
  unsigned char TrmIOP:1;
  unsigned char AENC:1;

  unsigned char additional_length;	/* 04 */
  unsigned char byte5res;		/* 05 */
  unsigned char byte6res;		/* 06 */

  unsigned char SftReset:1;		/* 07 */
  unsigned char CmdQueue:1;
  unsigned char byte7res:1;
  unsigned char linked:1;
  unsigned char sync:1;
  unsigned char WBus16:1;
  unsigned char WBus32:1;
  unsigned char RelAdr:1;

  char vendor_identification[8];	/* 08 */
  char product_identification[16];	/* 16 */
  char product_revision[4];		/* 32 */
#if 0
  char vendor_specific[20];		/* 36 */
  char reserved[40];			/* 56 */
  char unit_serial_number[10];		/* 96 */
					/*106 */
#endif
} __attribute__ ((packed));

static char *scsi_device_types[] = {
  "Direct-Access",
  "Sequential-Access",
  "Printer",
  "Processor",
  "WORM",
  "CD-ROM",
  "Scanner",
  "Optical Device",
  "Medium Changer",
  "Communications",
  0
};
#define NUMKNOWNTYPES ((sizeof(scsi_device_types)/sizeof(char *))-1)


static char *scsiprocfile = "/proc/scsi/scsi";
static struct scsi_device_info *sg_devices = 0;
static int num_sg_devices = 0;
static struct scsi_device_info **configured_devices = 0;
static int num_configured_devices = 0;
static struct scsi_device_info *current_device = 0;

static int query_device_type(char *devname)
{
  int fd;
  int in_len, out_len, status, ret;
  struct CDB_inquiry *cdb;
  struct sg_header *sg_hd;
  struct sg_header *sg_reply_hdr;
  int sg_off = sizeof(struct sg_header);
  struct standard_inquiry_data *si;
  
  fd = open(devname, O_RDWR);
  if (fd < 0) return -1;

  in_len = sg_off + sizeof(struct CDB_inquiry);
  sg_hd = (struct sg_header *) malloc(in_len);
  memset(sg_hd, 0, in_len);
  cdb = (struct CDB_inquiry *)(sg_hd + 1);
  cdb->cmd = CMD_INQUIRY /*0x12*/;
  cdb->alloclen = sizeof(struct standard_inquiry_data);

  out_len = sg_off + cdb->alloclen;
  sg_reply_hdr = (struct sg_header *) malloc(out_len);
  memset(sg_reply_hdr, 0, SCSI_OFF);
  sg_hd->reply_len = out_len;

  status = write(fd, sg_hd, in_len);
  if (status < 0 || status != in_len) return -1;

  do {
    status = read(fd, sg_reply_hdr, out_len);
  } while (status == -1 && errno == EINTR);

  si = (struct standard_inquiry_data *)(sg_reply_hdr+1);
  ret = si->peripheral_device_type;
  free(sg_hd);
  free(sg_reply_hdr);
  close(fd);
  return ret;
}

static char *strbetween(char *s, char **next, char *pre, char *post)
{
   char *p, *p2 = 0;
   p = strstr(s, pre);
   if (!p) return 0;
   p += strlen(pre);
   while (p[0] && isspace(p[0])) p++;
   if (post[0]) p2 = strstr(p, post);
   if (!p2) p2 = p + strlen(p);
   *next = p2;
   while ((p2 != p) && isspace(p2[-1])) *(--p2) = 0;
   return p;
}

static int decode_device_type(char *name)
{
  int i;
  for (i=0; scsi_device_types[i]; i++) {
    if (!strcmp(name, scsi_device_types[i])) return i;
  }
  if (isdigit(name[0])) return strtoul(name,0,10);
  return -1;
}

static int init_sg_device_list(void) {
  struct scsi_device_info *devs;
  int maxdevs = 32;
  int dev = 0;
  FILE *f;
  char buf[1024];
  char *p, *s;
  static char* attached = "Attached devices:";

  if (sg_devices) return 1;

  f = fopen(scsiprocfile, "r");
  if (!f) return 0;

  devs = malloc(sizeof(struct scsi_device_info) * (maxdevs+1));
  if (!devs) {
    fclose(f);
    return 0;
  }
  while (!feof(f) && dev < maxdevs) {
    if (!fgets(buf, sizeof(buf), f)) break;
    if (!strncmp(buf, attached, sizeof(attached))) {
      if (strstr(buf, "none")) break;
      fgets(buf, sizeof(buf), f);
    }
    devs[dev].fd = -1;
    devs[dev].sgminor = dev;
    p = buf;
    s = strbetween(p,&p, "Host:", "Channel:");
    devs[dev].hostId = s[strlen(s)-1] -'0';
    s = strbetween(p,&p, "Channel:", "Id:");
    devs[dev].channel = strtoul(s,0,10);
    s = strbetween(p,&p, "Id:", "Lun:");
    devs[dev].target = strtoul(s,0,10);
    devs[dev].dos_seen_target = devs[dev].target;
    s = strbetween(p,&p, "Lun:", "");
    devs[dev].lun = strtoul(s,0,10);
    fgets(buf, sizeof(buf), f);
    p = buf;
    s = strbetween(p,&p, "Vendor:", "Model:");
    devs[dev].vendor = strdup(s);
    s = strbetween(p,&p, "Model:", "Rev:");
    devs[dev].model = strdup(s);
    s = strbetween(p,&p, "Rev:", "");
    devs[dev].modelrev = strdup(s);
    fgets(buf, sizeof(buf), f);
    p = buf;
    s = strbetween(p,&p, "Type:", "ANSI SCSI revision:");
    devs[dev].devtype = decode_device_type(s);
    if (devs[dev].devtype == -1) {
      char devname[20];
      sprintf(devname, "/dev/sg%d", dev);
      devs[dev].devtype = query_device_type(devname);
    }
    s = strbetween(p,&p, "ANSI SCSI revision:", "");
    if (s[2]) devs[dev].ansirev = 1;
    else devs[dev].ansirev = strtoul(s,0,16);
    dev++;
  }
  fclose(f);
  if (dev) {
    devs = realloc(devs, sizeof(struct scsi_device_info) * (dev+1));
    memset(&devs[dev], 0, sizeof(struct scsi_device_info));
    sg_devices = devs;
    num_sg_devices = dev;
    return 1;
  }
  free(devs);
  return 0;
}

static int search_for_sg(int hostId, int channel, int target, int lun)
{
  int i, sgminor;
  for (i=0, sgminor=-1; i <num_sg_devices; i++) {
    if (   (hostId == sg_devices[i].hostId)
        && (channel == sg_devices[i].channel)
        && (target == sg_devices[i].target)
        && (lun == sg_devices[i].lun) ) {
      return sg_devices[i].sgminor;
    }
  }
  return -1;
}

char *aspi_add_device(char *name, char *devtype, int target)
{
  int sgminor, i;
  char *s;
  struct scsi_device_info *si;

  if (!init_sg_device_list()) return 0;
  if (!name || !name[0]) return 0;
  if (isdigit(name[0]) && isdigit(name[2]) && isdigit(name[4]) && isdigit(name[6])
      && (name[1] == '/') && (name[3] == '/') && (name[5] == '/')) {
    sgminor = search_for_sg(name[0] -'0', name[2] -'0', name[4] -'0', name[6] -'0');
    if (sgminor <0) return 0;
  }
  else {
    if (strncmp(name, "sg", 2)) return 0;
    if (name[2] > '9') {
      if (strlen(name) >3) return 0;
      sgminor = name[2] - 'a';
    }
    else {
      sgminor = strtoul(name+2, 0, 10);
    }
  }
  if (sgminor < 0 || sgminor >= num_sg_devices) return 0;
  if (!devtype || !devtype[0]) {
    if (!sg_devices[sgminor].devtype) return 0; /* don't allow disks unchecked */
  }
  else {
    if (sg_devices[sgminor].devtype != decode_device_type(devtype)) {
      if (sg_devices[sgminor].devtype < NUMKNOWNTYPES) return 0;
    }
  }
  si = &sg_devices[sgminor];
  if (configured_devices) {
    for (i=0; i < num_configured_devices; i++) {
      if (configured_devices[i]->sgminor == sgminor) return 0;
    }
    configured_devices[num_configured_devices] = si;
    configured_devices = realloc(configured_devices, sizeof(void *) * (num_configured_devices+2));
    configured_devices[num_configured_devices+1] = 0;
  }
  else {
    configured_devices = malloc(sizeof(void *) * 2);
    configured_devices[0] = si;
    configured_devices[1] = 0;
  }
  if (target >= 0) si->dos_seen_target = target;
  num_configured_devices++;
  s = malloc(1024);
  sprintf(s, "/dev/sg%d, scsi%d(0), chan%d, ID=%d(%d), LUN=%d, type=%d, %s %s %s",
     sgminor, si->hostId, si->channel,
     si->target, si->dos_seen_target, si->lun,
     si->devtype, si->vendor, si->model, si->modelrev
  );
  return s;
}

/* -------------------------------------------------------- */



static int ASPI_OpenDevice16(SRB_ExecSCSICmd16 *prb)
{
    int	fd;
    int i, sgminor;
    int tout = 5 * 60 * sysconf(_SC_CLK_TCK);
    static char	devname[50];
    int hostId, target, lun;

    if (!num_configured_devices) return -1;

    /* search list of devices to see if we've opened it already.
     * There is not an explicit open/close in ASPI land, so hopefully
     * keeping a device open won't be a problem.
     *
     * Comment: sure there is 'open' and 'close', but only for the
     *          whole ASPI driver: DOS-open/close on 'SCSIMGR$'
     *          We need to use this in order to avoid blocking
     *          the devices longer as needed. -- Hans
     */

    hostId = prb->SRB_HaId;
    target = prb->SRB_Target;
    lun = prb->SRB_Lun;
    for (i=0, sgminor=-1; i <num_configured_devices; i++) {
      if (   (hostId == configured_devices[i]->hostId)
          && (target == configured_devices[i]->dos_seen_target)
          && (lun == configured_devices[i]->lun) ) {
        if (configured_devices[i]->fd != -1)
          return configured_devices[i]->fd;
        sgminor = configured_devices[i]->sgminor;
        break;
      }
    }
    if (sgminor == -1) return -1;

    /* device wasn't cached, go ahead and open it */

    A_printf("ASPI: ASPI_OpenDevice16: opening /dev/sg%d\n", sgminor);
    sprintf(devname, "/dev/sg%d", sgminor);
    fd = open(devname, O_RDWR);
    if (fd == -1) {
	A_printf("ASPI: No device could be opened for host%d:ID%d:LUN%d\n",
          hostId, target, lun);
	return -1;
    }
    if (ioctl(fd,SG_SET_TIMEOUT,&tout))
        A_printf("ASPI: SG_SET_TIMEOUT failure\n");

    /* device is now open */
    current_device = &sg_devices[sgminor];
    current_device->fd = fd;
    return fd;
}

static void ASPI_DebugPrintCmd16(SRB_ExecSCSICmd16 *prb)
{
  Bit8u	cmd;
  int	i;
  Bit8u *cdb;
  Bit8u *lpBuf;

  lpBuf = (Bit8u *)(uintptr_t) FARPTR16_TO_LIN(prb->SRB_BufPointer);
  switch (prb->CDBByte[0]) {
  case CMD_INQUIRY:
    A_printf("ASPI: {\n");
    A_printf("\tEVPD: %d\n", prb->CDBByte[1] & 1);
    A_printf("\tLUN: %d\n", (prb->CDBByte[1] & 0xc) >> 1);
    A_printf("\tPAGE CODE: %d\n", prb->CDBByte[2]);
    A_printf("\tALLOCATION LENGTH: %d\n", prb->CDBByte[4]);
    A_printf("\tCONTROL: %d\n", prb->CDBByte[5]);
    A_printf("}\n");
    break;
  default:
    A_printf("ASPI: Transfer Length: %d\n", prb->CDBByte[4]);
    break;
  }

  A_printf("ASPI: Host Adapter: %d\n", prb->SRB_HaId);
  A_printf("ASPI: Flags: %d\n", prb->SRB_Flags);
  if (TARGET_TO_HOST(prb)) {
    A_printf("\tData transfer: Target to host. Length checked.\n");
  }
  else if (HOST_TO_TARGET(prb)) {
    A_printf("\tData transfer: Host to target. Length checked.\n");
  }
  else if (NO_DATA_TRANSFERED(prb)) {
    A_printf("\tData transfer: none\n");
  }
  else {
    if (U_HOST_TO_TARGET(prb))
        A_printf("\tTransfer by scsi cmd set to Host->Target. Length not checked\n");
    else if (U_TARGET_TO_HOST(prb))
        A_printf("\tTransfer by scsi cmd set to Target->Host. Length not checked\n");
    else 
        A_printf("\tTransfer by scsi cmd set to NONE. Length not checked\n");
  }

  A_printf("\tResidual byte length reporting %s\n", prb->SRB_Flags & 0x4 ? "enabled" : "disabled");
  A_printf("\tLinking %s\n", prb->SRB_Flags & 0x2 ? "enabled" : "disabled");
  A_printf("\tPosting %s\n", prb->SRB_Flags & 0x1 ? "enabled" : "disabled");
  A_printf("ASPI: Target: %d\n", prb->SRB_Target);
  A_printf("ASPI: Lun: %d\n", prb->SRB_Lun);
  A_printf("ASPI: BufLen: %d\n", prb->SRB_BufLen);
  A_printf("ASPI: SenseLen: %d\n", prb->SRB_SenseLen);
  A_printf("ASPI: BufPtr: %x (%p)\n", prb->SRB_BufPointer, lpBuf);
  A_printf("ASPI: LinkPointer %x\n", prb->SRB_Rsvd1);
  A_printf("ASPI: CDB Length: %d\n", prb->SRB_CDBLen);
  A_printf("ASPI: POST Proc: %x\n", (Bit32u) prb->SRB_PostProc);
  cdb = &prb->CDBByte[0];
  A_printf("ASPI: CDB buffer[");
  cmd = prb->CDBByte[0];
  for (i = 0; i < prb->SRB_CDBLen; i++) {
    if (i != 0)
      A_printf(",");
    A_printf("%02x", *cdb++);
  }
  A_printf("]\n");
}

static void PrintSenseArea16(SRB_ExecSCSICmd16 *prb)
{
  int	i;
  Bit8u *cdb;

  cdb = &prb->CDBByte[0];
  A_printf("ASPI: SenseArea[");
  for (i = 0; i < prb->SRB_SenseLen; i++) {
    if (i)
      A_printf(",");
    A_printf("%02x", *cdb++);
  }
  A_printf("]\n");
}

static void ASPI_DebugPrintResult16(SRB_ExecSCSICmd16 *prb)
{
  Bit8u *lpBuf;

  lpBuf = (Bit8u *)(uintptr_t) FARPTR16_TO_LIN(prb->SRB_BufPointer);

  switch (prb->CDBByte[0]) {
  case CMD_INQUIRY:
    A_printf("ASPI: Vendor: %s\n", lpBuf + INQUIRY_VENDOR);
    break;
  case CMD_TEST_UNIT_READY:
    PrintSenseArea16(prb);
    break;
  }
}

static Bit16u ASPI_ExecScsiCmd16(SRB_ExecSCSICmd16 *prb, FARPTR16 segptr_prb)
{
  struct sg_header *sg_hd=0, *sg_reply_hdr=0;
  int	status;
  Bit8u *lpBuf;
  int	in_len, out_len;
  int	error_code = 0;
  int	fd;
#define D_UNDF 0
#define D_T_H  1
#define D_H_T  2
#define D_NONE 3

  int   direction = D_UNDF;

  ASPI_DebugPrintCmd16(prb);

  fd = ASPI_OpenDevice16(prb);
  if (fd == -1) {
      prb->SRB_Status = SS_ERR;
      return SS_ERR;
  }

  sg_hd = NULL;
  sg_reply_hdr = NULL;

  prb->SRB_Status = SS_PENDING;
  lpBuf = (Bit8u *)(uintptr_t) FARPTR16_TO_LIN(prb->SRB_BufPointer);

  if (!prb->SRB_CDBLen) {
      prb->SRB_Status = SS_ERR;
      return SS_ERR;
  }

  if (HOST_TO_TARGET(prb))
	direction = D_H_T;
  else if (TARGET_TO_HOST(prb))
	direction = D_T_H;
  else if (NO_DATA_TRANSFERED(prb))
	direction = D_NONE;

  if (direction == D_UNDF) {       /* look in our table which is incomplete */
	if (U_HOST_TO_TARGET(prb))
	    direction = D_H_T;
	else if (U_TARGET_TO_HOST(prb))	
	    direction = D_T_H;
  }	
  A_printf("ASPI: direction = %d\n",direction);
  /* build up sg_header + scsi cmd */
  if (direction == D_H_T) {
    /* send header, command, and then data */
    in_len = SCSI_OFF + prb->SRB_CDBLen + prb->SRB_BufLen;
    sg_hd = (struct sg_header *) malloc(in_len);
    memset(sg_hd, 0, SCSI_OFF);
    memcpy(sg_hd + 1, &prb->CDBByte[0], prb->SRB_CDBLen);
    if (prb->SRB_BufLen) {
      memcpy(((Bit8u *) sg_hd) + SCSI_OFF + prb->SRB_CDBLen, lpBuf, prb->SRB_BufLen);
    }
  }
  else {
    /* send header and command - no data */
    in_len = SCSI_OFF + prb->SRB_CDBLen;
    sg_hd = (struct sg_header *) malloc(in_len);
    memset(sg_hd, 0, SCSI_OFF);
    memcpy(sg_hd + 1, &prb->CDBByte[0], prb->SRB_CDBLen);
  }

  if (direction == D_T_H) {
    out_len = SCSI_OFF + prb->SRB_BufLen;
    sg_reply_hdr = (struct sg_header *) malloc(out_len);
    memset(sg_reply_hdr, 0, SCSI_OFF);
    sg_hd->reply_len = out_len;
  }
  else {
    out_len = SCSI_OFF;
    sg_reply_hdr = (struct sg_header *) malloc(out_len);
    memset(sg_reply_hdr, 0, SCSI_OFF);
    sg_hd->reply_len = out_len;
  }
  if (prb->SRB_CDBLen == 12) {
	sg_hd->twelve_byte = 1;
	A_printf("ASPI: twelve_byte set to 1\n");
  }

  status = write(fd, sg_hd, in_len);
  if (status < 0 || status != in_len) {
      int myerror = errno;

    A_printf("ASPI: not enough bytes written to scsi device bytes=%d .. %d\n", in_len, status);
    if (status < 0) {
	if (myerror == ENOMEM) {
	    A_printf("ASPI: Linux generic scsi driver\n  You probably need to re-compile your kernel with a larger SG_BIG_BUFF value (sg.h)\n  Suggest 130560\n");
	}
	A_printf("ASPI: errno: = %d\n", myerror);
    }
    goto error_exit;
  }

again:
  status = read(fd, sg_reply_hdr, out_len);
  if (status == -1 && errno == EINTR)
	goto again;

  A_printf("ASPI: after read: status=%d result=%d\n", status, sg_reply_hdr->result);

  { int i; /* FIXME - no selective enable */
    Bit8u *p = &sg_reply_hdr->sense_buffer[0];
    A_printf("ASPI: SenseBuffer[");
    for (i = 0; i < 16; i++) {
      if (i)
        A_printf(",");
      A_printf("%02x", *p++);
    }
    A_printf("]\n");
  }

  if (status < 0 || status != out_len) {
    A_printf("ASPI: not enough bytes read from scsi device%d\n", status);
    goto error_exit;
  }

  if (sg_reply_hdr->result != 0) {
    error_code = sg_reply_hdr->result;
    A_printf("ASPI: reply header error (%d)\n", sg_reply_hdr->result);
    goto error_exit;
  }

  if ((direction == D_T_H) && prb->SRB_BufLen) {
    memcpy(lpBuf, sg_reply_hdr + 1, prb->SRB_BufLen);
    A_printf("ASPI: copied %d bytes\n",(unsigned)prb->SRB_BufLen);
  }

  /* copy in sense buffer to amount that is available in client */
  prb->SRB_TargStat = STATUS_GOOD;
  prb->SRB_Status = SS_COMP;

  if (prb->SRB_SenseLen) {
    int sense_len = prb->SRB_SenseLen;
    if (prb->SRB_SenseLen > 16)
      sense_len = 16;
    memcpy(SENSE_BUFFER(prb), &sg_reply_hdr->sense_buffer[0], sense_len);
    if (sg_reply_hdr->sense_buffer[0]) {
	A_printf("ASPI: setting STATUS_CHKCOND\n");
	prb->SRB_TargStat = STATUS_CHKCOND;
        prb->SRB_Status = SS_ERR;
    }
  }

  prb->SRB_HaStat = HASTAT_OK;

  /* now do  posting */

  if (ASPI_POSTING(prb) && prb->SRB_PostProc)
    A_printf("ASPI: Post Routine calling NOT implemented\n"); /* FIXME */
  if (sg_reply_hdr) free(sg_reply_hdr);
  if (sg_hd) free(sg_hd);
  ASPI_DebugPrintResult16(prb);
  return SS_COMP;
  
error_exit:
  if (error_code == EBUSY) {
      prb->SRB_Status = SS_ASPI_IS_BUSY;
      A_printf("ASPI: Device busy\n");
  }
  else {
      A_printf("ASPI: ASPI_GenericHandleScsiCmd failed\n");
      prb->SRB_Status = SS_ERR;
  }

  /* I'm not sure exactly error codes work here
   * We probably should set prb->SRB_TargStat, SRB_HaStat ?
   */
  A_printf("ASPI: ASPI_GenericHandleScsiCmd: error_exit\n");
  if (sg_reply_hdr) free(sg_reply_hdr);
  if (sg_hd) free(sg_hd);
  prb->SRB_TargStat = STATUS_CHKCOND;
  return prb->SRB_Status;
}

static Bit16u ASPI_GetDeviceType(SRB_GDEVBlock16 *prb, FARPTR16 segptr_prb)
{
  int	fd,Type;

  A_printf("ASPI: Host Adapter: %d, Flags: %d, Target: %d, Lun: %d\n",
      prb->SRB_HaId, prb->SRB_Flags,  prb->SRB_Target, prb->SRB_Lun);

  fd = ASPI_OpenDevice16((SRB_ExecSCSICmd16 *)prb);
  if (fd == -1) {
      prb->SRB_Status = SS_ERR;
       A_printf("ASPI: device not available\n");
      return SS_ERR;
  }
  Type = current_device->devtype;
  A_printf("ASPI: Device type returned: %d\n",Type);

  prb->SRB_Status = SS_COMP;
  prb->SRB_DeviceType = Type;
  return SS_COMP;
}

static Bit16u ASPI_Inquiry(SRB_GDEVBlock16 *prb, FARPTR16 segptr_prb)
{
  SRB_HaInquiry16 *p = (SRB_HaInquiry16 *)prb;

#if 0 /* We currently behave as an _old_ ASPI interface
       * and need not to flag (inverse 0xAA55) to indicate
       * we are a new style ASPI driver.
       * Once we do, we need to verify wether SRB_ExtBufferSize
       * contains the _whole_ buffer size or only the size of
       * the extension (which currently is only 2 bytes opposite
       * to what the ADAPTEC aspi_dos.txtx says :-(
       * So, _what_ is right ???     --Hans
       */
  if (p->SRB_55AASignature == 0xAA55) {
    /* we got an Extended Host Adapter Inquiry command */
    p->SRB_55AASignature = 0x55AA; /* reverse the order to indicate we are
    				    * we've set the extention */
    if (p->SRB_ExtBufferSize < 2) {
       /* oops, that shouldn't happen */
       p->SRB_ExtBufferSize = 0;
    }
    else {
       p->SRB_ExtBufferSize = 2;
       p->HA_Extensions = 0;      /* have do no 'extended' stuff */
    }
  }
#endif

  p->HA_Count = 1;
  p->HA_SCSI_ID = 7;
  strncpy(p->HA_ManagerId, "DOSEMU ASPI     ",16);
  strncpy(p->HA_Identifier,"DUMMY SCSI ADAPT",16);
  memset(p->HA_Unique,0,16);
    
#if 0 /* this is wrong here, the srb is _no_ SRB_ExecSCSICmd16 */
  ASPI_DebugPrintResult16((SRB_ExecSCSICmd16 *)prb);
#endif

  return SS_COMP;
}

/***********************************************************************
 *             GetASPISupportInfo16   (WINASPI.1)
 */

#if 0 /* seems not to be used */

static Bit16u GetASPISupportInfo16()
{
    A_printf("ASPI: GETASPISupportInfo\n");
    /* high byte SS_COMP - low byte number of host adapters.
     * FIXME!!! The number of host adapters is incorrect.
     * I'm not sure how to determine this under linux etc.
     */
    return ((SS_COMP << 8) | 0x1);
}

#endif

/***********************************************************************
 *             SendASPICommand16   (WINASPI.2)
 */

static Bit16u SendASPICommand16(FARPTR16 segptr_srb)
{
  LPSRB16 lpSRB = (LPSRB16)(uintptr_t)segptr_srb;

  switch (lpSRB->common.SRB_Cmd) {
  case SC_HA_INQUIRY:
    A_printf("ASPI: ASPI CMD: HA_INQUIRY\n");
    return ASPI_Inquiry((SRB_GDEVBlock16 *)&lpSRB->cmd, segptr_srb);
    break;
  case SC_GET_DEV_TYPE:
    A_printf("ASPI: ASPI CMD: GET_DEV_TYPE\n");
    return ASPI_GetDeviceType((SRB_GDEVBlock16 *)&lpSRB->cmd, segptr_srb);
    break;
  case SC_EXEC_SCSI_CMD:
    A_printf("ASPI: ASPI CMD: EXEC_SCSI\n");
    return ASPI_ExecScsiCmd16(&lpSRB->cmd, segptr_srb);
    break;
  case SC_RESET_DEV:
    A_printf("ASPI: Not implemented SC_RESET_DEV\n");
    break;
  default:
    A_printf("ASPI: Unknown command %d\n", lpSRB->common.SRB_Cmd);
  }
  return SS_INVALID_SRB;
}

void aspi_helper(int mode)
{
   Bit16u arg_s, arg_o, ret;
   Bit16u *spp;
   SRB16 *srb;

   if (mode) {
     /* installation check */
     A_printf("ASPI: installation check, configured devices = %d\n", num_configured_devices);
     LWORD(eax) = num_configured_devices;
     return;
   }

   spp = SEG_ADR((unsigned short *),ss,sp);
   spp += 2; /* 2 x unsigned short */
   arg_o = *spp;
   arg_s = *(spp+1);
   A_printf("    request at  %#x:%#x\n",arg_o,arg_s);
   srb = (SRB16 *) SEGOFF2LINEAR(arg_s, arg_o);

   srb->common.SRB_Status = 255;
   ret = SendASPICommand16((FARPTR16)(uintptr_t)srb);
   if ( srb->common.SRB_Status == 255 )
     srb->common.SRB_Status = ret; /* set status from return code */

   if (srb->common.SRB_Status == SS_PENDING)
     /* FIXME - hack: set command complete if someone forgot to set this
      * ( ... need to be removed, once 'command posting' (call backs)
      *   is implemented  --Hans )
      */
     srb->common.SRB_Status = 1;
   LO(ax) = 0; /* FIXME - is this needed ? */

#if 0
{
   Bit8u *p = (Bit8u *) srb;
   for (n=0; n<64; ++n) {
	A_printf("%02x ",*p++);
	if ((n%16)==15) A_printf("\n");
   }	
}
#endif

   return;
}
