/* dynamic debug handlers - by Tim Bird */
#include "emu.h"

int
SetDebugFlagsHelper(char *debugStr)
{
  int i;
  char value;

  for (i = 0; i < strlen(debugStr); i = i + 2) {
    value = (debugStr[i] == '-') ? 0 : 1;
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
    default:
      /* unknown flag, return failure */
      return (1);
    }
  }
  return (0);
}

int
GetDebugFlagsHelper(char *debugStr)
{
  int i;

  dbug_printf("GetDebugFlagsHelper\n");
  dbug_printf("debugStr at %x\n", (int)debugStr);
  i = 0;

  debugStr[i++] = (d.disk)     ? '+' : '-'; debugStr[i++] = 'd';
  debugStr[i++] = (d.read)     ? '+' : '-'; debugStr[i++] = 'R';
  debugStr[i++] = (d.write)    ? '+' : '-'; debugStr[i++] = 'W';
  debugStr[i++] = (d.dos)      ? '+' : '-'; debugStr[i++] = 'D';
  debugStr[i++] = (d.video)    ? '+' : '-'; debugStr[i++] = 'v';
#ifdef X_SUPPORT
  debugStr[i++] = (d.X)        ? '+' : '-'; debugStr[i++] = 'X';
#endif
  debugStr[i++] = (d.keyb)     ? '+' : '-'; debugStr[i++] = 'k';
  debugStr[i++] = (d.io)       ? '+' : '-'; debugStr[i++] = 'i';
  debugStr[i++] = (d.serial)   ? '+' : '-'; debugStr[i++] = 's';
  debugStr[i++] = (d.mouse)    ? '+' : '-'; debugStr[i++] = 'm';
  debugStr[i++] = (d.defint)   ? '+' : '-'; debugStr[i++] = '#';
  debugStr[i++] = (d.printer)  ? '+' : '-'; debugStr[i++] = 'p';
  debugStr[i++] = (d.general)  ? '+' : '-'; debugStr[i++] = 'g';
  debugStr[i++] = (d.config)   ? '+' : '-'; debugStr[i++] = 'c';
  debugStr[i++] = (d.warning)  ? '+' : '-'; debugStr[i++] = 'w';
  debugStr[i++] = (d.hardware) ? '+' : '-'; debugStr[i++] = 'h';
  debugStr[i++] = (d.IPC)      ? '+' : '-'; debugStr[i++] = 'I';
  debugStr[i++] = (d.EMS)      ? '+' : '-'; debugStr[i++] = 'E';
  debugStr[i++] = (d.xms)      ? '+' : '-'; debugStr[i++] = 'x';
  debugStr[i++] = (d.dpmi)     ? '+' : '-'; debugStr[i++] = 'M';
  debugStr[i++] = (d.network)  ? '+' : '-'; debugStr[i++] = 'n';
  debugStr[i++] = (d.pd)       ? '+' : '-'; debugStr[i++] = 'P';

  debugStr[i] = 0;
  dbug_printf("debugStr is %s\n", debugStr);

  return (0);
}
