/* 
 * (C) Copyright 1992, ..., 2000 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* dynamic debug handlers - by Tim Bird */
/* modified to support debug levels -- peak */
#include "config.h"
#include "emu.h"

extern int parse_debugflags(const char *s, unsigned char flag);

int
SetDebugFlagsHelper(char *debugStr)
{
  return parse_debugflags(debugStr, 0);
}

static char
DebugFlag(int f)
{
  if (f == 0)
    return '-';
  else if (f >= 2 && f <= 9)
    return f + '0';
  else
    return '+';
}

int
GetDebugFlagsHelper(char *debugStr, int print)
{
  int i;

  if (print) dbug_printf("GetDebugFlagsHelper\n");
  if (print) dbug_printf("debugStr at %x\n", (int)debugStr);
  i = 0;

  debugStr[i++] = DebugFlag(d.disk);     debugStr[i++] = 'd';
  debugStr[i++] = DebugFlag(d.read);     debugStr[i++] = 'R';
  debugStr[i++] = DebugFlag(d.write);    debugStr[i++] = 'W';
  debugStr[i++] = DebugFlag(d.dos);      debugStr[i++] = 'D';
  debugStr[i++] = DebugFlag(d.cdrom);    debugStr[i++] = 'C';
  debugStr[i++] = DebugFlag(d.video);    debugStr[i++] = 'v';
#ifdef X_SUPPORT
  debugStr[i++] = DebugFlag(d.X);        debugStr[i++] = 'X';
#endif
  debugStr[i++] = DebugFlag(d.keyb);     debugStr[i++] = 'k';
  debugStr[i++] = DebugFlag(d.io);       debugStr[i++] = 'i';
  debugStr[i++] = DebugFlag(d.io_trace); debugStr[i++] = 'T';
  debugStr[i++] = DebugFlag(d.serial);   debugStr[i++] = 's';
  debugStr[i++] = DebugFlag(d.mouse);    debugStr[i++] = 'm';
  debugStr[i++] = DebugFlag(d.defint);   debugStr[i++] = '#';
  debugStr[i++] = DebugFlag(d.printer);  debugStr[i++] = 'p';
  debugStr[i++] = DebugFlag(d.general);  debugStr[i++] = 'g';
  debugStr[i++] = DebugFlag(d.config);   debugStr[i++] = 'c';
  debugStr[i++] = DebugFlag(d.warning);  debugStr[i++] = 'w';
  debugStr[i++] = DebugFlag(d.hardware); debugStr[i++] = 'h';
  debugStr[i++] = DebugFlag(d.IPC);      debugStr[i++] = 'I';
  debugStr[i++] = DebugFlag(d.EMS);      debugStr[i++] = 'E';
  debugStr[i++] = DebugFlag(d.xms);      debugStr[i++] = 'x';
  debugStr[i++] = DebugFlag(d.dpmi);     debugStr[i++] = 'M';
  debugStr[i++] = DebugFlag(d.network);  debugStr[i++] = 'n';
  debugStr[i++] = DebugFlag(d.pd);       debugStr[i++] = 'P';
  debugStr[i++] = DebugFlag(d.request);  debugStr[i++] = 'r';
  debugStr[i++] = DebugFlag(d.sound);    debugStr[i++] = 'S';
  debugStr[i++] = DebugFlag(d.aspi);     debugStr[i++] = 'A';
  debugStr[i++] = DebugFlag(d.mapping);  debugStr[i++] = 'Q';
#ifdef X86_EMULATOR
  debugStr[i++] = DebugFlag(d.emu);      debugStr[i++] = 'e';
#endif
#ifdef TRACE_DPMI
  debugStr[i++] = DebugFlag(d.dpmit);    debugStr[i++] = 't';
#endif

  debugStr[i] = 0;
  if (print) dbug_printf("debugStr is %s\n", debugStr);

  return (0);
}
