#ifndef DOSERROR_H
#define DOSERROR_H

#define MAX_DOSERROR		0x5A

#define DOS_EINVAL	0x01
#define DOS_ENOENT	0x02
#define DOS_EMFILE	0x04
#define DOS_EACCES	0x05
#define DOS_EPERM	EACCES
#define DOS_EBADF	0x06
#define DOS_ENOMEM	0x08
#define DOS_EBUSY	0x15
#define DOS_EGENERAL	0x1F



static char *DOSerrcodes[MAX_DOSERROR+1] = {
  /* the below error list is shamelessly stolen from Ralph Brown's */
  /* 0x00 */  "no error",
#if 0
  /* 0x01 */  "function number invalid",
#else
  /* 0x01 */  "invalid argument",
#endif
  /* 0x02 */  "file not found",
  /* 0x03 */  "path not found",
  /* 0x04 */  "too many open files",
  /* 0x05 */  "access denied",
  /* 0x06 */  "invalid handle",
  /* 0x07 */  "memory control block destroyed",
  /* 0x08 */  "insufficient memory",
  /* 0x09 */  "memory block address invalid",
  /* 0x0A */  "environment invalid",
  /* 0x0B */  "format invalid",
  /* 0x0C */  "access code invalid",
  /* 0x0D */  "data invalid",
  /* 0x0E */  0,
  /* 0x0F */  "invalid drive",
  /* 0x10 */  "attempted to remove current directory",
  /* 0x11 */  "not same device",
  /* 0x12 */  "no more files",
  /* 0x13 */  "disk write-protected",
  /* 0x14 */  "unknown unit",
  /* 0x15 */  "drive not ready",
  /* 0x16 */  "unknown command",
  /* 0x17 */  "data error (CRC)",
  /* 0x18 */  "bad request structure length",
  /* 0x19 */  "seek error",
  /* 0x1A */  "unknown media type",
  /* 0x1B */  "sector not found",
  /* 0x1C */  "printer out of paper",
  /* 0x1D */  "write fault",
  /* 0x1E */  "read fault",
  /* 0x1F */  "general failure",
  /* 0x20 */  "sharing violation",
  /* 0x21 */  "lock violation",
  /* 0x22 */  "disk change invalid",
  /* 0x23 */  "FCB unavailable",
  /* 0x24 */  "sharing buffer overflow",
  /* 0x25 */  "code page mismatch",
  /* 0x26 */  "cannot complete file operation (out of input)",
  /* 0x27 */  "insufficient disk space",
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  /* 0x32 */  "network request not supported",
  /* 0x33 */  "remote computer not listening",
  /* 0x34 */  "duplicate name on network",
  /* 0x35 */  "network name not found",
  /* 0x36 */  "network busy",
  /* 0x37 */  "network device no longer exists",
  /* 0x38 */  "network BIOS command limit exceeded",
  /* 0x39 */  "network adapter hardware error",
  /* 0x3A */  "incorrect response from network",
  /* 0x3B */  "unexpected network error",
  /* 0x3C */  "incompatible remote adapter",
  /* 0x3D */  "print queue full",
  /* 0x3E */  "queue not full",
  /* 0x3F */  "not enough space to print file",
  /* 0x40 */  "network name was deleted",
  /* 0x41 */  "network: Access denied",
  /* 0x42 */  "network device type incorrect",
  /* 0x43 */  "network name not found",
  /* 0x44 */  "network name limit exceeded",
  /* 0x45 */  "network BIOS session limit exceeded",
  /* 0x46 */  "temporarily paused",
  /* 0x47 */  "network request not accepted",
  /* 0x48 */  "network print/disk redirection paused",
  /* 0x49 */  "network software not installed",
  /* 0x4A */  "unexpected adapter close",
  /* 0x4B */  "password expired",
  /* 0x4C */  "login attempt invalid at this time",
  /* 0x4D */  "disk limit exceeded on network node",
  /* 0x4E */  "not logged in to network node",
  /* 0x4F */  0,
  /* 0x50 */  "file exists",
  /* 0x51 */  0,
  /* 0x52 */  "cannot make directory",
  /* 0x53 */  "fail on INT 24h",
  /* 0x54 */  "too many redirections",
  /* 0x55 */  "duplicate redirection",
  /* 0x56 */  "invalid password",
  /* 0x57 */  "invalid parameter",
  /* 0x58 */  "network write fault",
  /* 0x59 */  "function not supported on network",
  /* 0x5A */  "required system component not installed"

#if 0 /* the below won't happen here */
   0, 0, 0, 0, 0, 0, 0, 0, 0,
  /* 0x64 */  "(MSCDEX) unknown error",
  /* 0x65 */  "(MSCDEX) not ready",
  /* 0x66 */  "(MSCDEX) EMS memory no longer valid",
  /* 0x67 */  "(MSCDEX) not High Sierra or ISO-9660 format",
  /* 0x68 */  "(MSCDEX) door open",
#endif
};

static char * decode_DOS_error(unsigned short errcode)
{
    static char *unknown = "unknown error";
    if (errcode > MAX_DOSERROR) return unknown;
    if (!DOSerrcodes[errcode]) return unknown;
    return DOSerrcodes[errcode];
}

#endif /* DOSERROR_H */
