/* dynamic debug handlers - by Tim Bird */
/* modified to support debug levels -- peak */
#include "emu.h"

int
SetDebugFlagsHelper(char *debugStr)
{
  int i;
  char value;

  for (i = 0; i < strlen(debugStr); i = i + 2) {
    if (debugStr[i] == '+')
      value = 1;
    else if (debugStr[i] == '-')
      value = 0;
    else if (debugStr[i] >= '0' && debugStr[i] <= '9')
      value = debugStr[i] - '0';
    else
      return (1); /* invalid value */

    switch (debugStr[i + 1]) {
    case 'd':
      d.disk = value;
      break;
    case 'R':
      d.read = value;
      break;
    case 'W':
      d.write = value;
      break;
    case 'D':
      d.dos = value;
      break;
    case 'C':
      d.cdrom = value;
      break;
    case 'v':
      d.video = value;
      break;
#ifdef X_SUPPORT
    case 'X':
      d.X = value;
      break;
#endif
    case 'k':
      d.keyb = value;
      break;
    case 'i':
      d.io = value;
      break;
    case 's':
      d.serial = value;
      break;
    case 'm':
      d.mouse = value;
      break;
    case '#':
      d.defint = value;
      break;
    case 'p':
      d.printer = value;
      break;
    case 'g':
      d.general = value;
      break;
    case 'c':
      d.config = value;
      break;
    case 'w':
      d.warning = value;
      break;
    case 'h':
      d.hardware = value;
      break;
    case 'I':
      d.IPC = value;
      break;
    case 'E':
      d.EMS = value;
      break;
    case 'x':
      d.xms = value;
      break;
    case 'M':
      d.dpmi = value;
      break;
    case 'n':
      d.network = value;
      break;
    case 'P':
      d.pd = value;
      break;
    case 'r':
      d.request = value;
      break;
    default:
      /* unknown flag, return failure */
      return (1);
    }
  }
  return (0);
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
GetDebugFlagsHelper(char *debugStr)
{
  int i;

  dbug_printf("GetDebugFlagsHelper\n");
  dbug_printf("debugStr at %x\n", (int)debugStr);
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

  debugStr[i] = 0;
  dbug_printf("debugStr is %s\n", debugStr);

  return (0);
}
