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
      d.X = value;
      break;
#endif
    case 'k':
      d.keyb = value;
      break;
    case '?':
      d.debug = value;
      break;
    case 'i':
      d.io = value;
      break;
    case 's':
      d.serial = value;
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
    case 'w':
      d.warning = value;
      break;
    case 'h':
      d.hardware = value;
      break;
    case 'x':
      d.xms = value;
      break;
    case 'm':
      d.mouse = value;
      break;
    case 'I':
      d.IPC = value;
      break;
    case 'E':
      d.EMS = value;
      break;
    case 'c':
      d.config = value;
      break;
    case 'n':
      d.network = value;
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
  if (d.disk)
    debugStr[i++] = '+';
  else
    debugStr[i++] = '-';
  debugStr[i++] = 'd';
  if (d.read)
    debugStr[i++] = '+';
  else
    debugStr[i++] = '-';
  debugStr[i++] = 'R';
  if (d.write)
    debugStr[i++] = '+';
  else
    debugStr[i++] = '-';
  debugStr[i++] = 'W';
  if (d.dos)
    debugStr[i++] = '+';
  else
    debugStr[i++] = '-';
  debugStr[i++] = 'D';
  if (d.video)
    debugStr[i++] = '+';
  else
    debugStr[i++] = '-';
  debugStr[i++] = 'v';
#ifdef X_SUPPORT
  if (d.X)
    debugStr[i++] = '+';
  else
    debugStr[i++] = '-';
  debugStr[i++] = 'X';
#endif
  if (d.keyb)
    debugStr[i++] = '+';
  else
    debugStr[i++] = '-';
  debugStr[i++] = 'k';
  if (d.debug)
    debugStr[i++] = '+';
  else
    debugStr[i++] = '-';
  debugStr[i++] = '?';
  if (d.io)
    debugStr[i++] = '+';
  else
    debugStr[i++] = '-';
  debugStr[i++] = 'i';
  if (d.serial)
    debugStr[i++] = '+';
  else
    debugStr[i++] = '-';
  debugStr[i++] = 's';
  if (d.defint)
    debugStr[i++] = '+';
  else
    debugStr[i++] = '-';
  debugStr[i++] = '#';
  if (d.printer)
    debugStr[i++] = '+';
  else
    debugStr[i++] = '-';
  debugStr[i++] = 'p';
  if (d.general)
    debugStr[i++] = '+';
  else
    debugStr[i++] = '-';
  debugStr[i++] = 'g';
  if (d.warning)
    debugStr[i++] = '+';
  else
    debugStr[i++] = '-';
  debugStr[i++] = 'w';
  if (d.hardware)
    debugStr[i++] = '+';
  else
    debugStr[i++] = '-';
  debugStr[i++] = 'h';
  if (d.xms)
    debugStr[i++] = '+';
  else
    debugStr[i++] = '-';
  debugStr[i++] = 'x';
  if (d.mouse)
    debugStr[i++] = '+';
  else
    debugStr[i++] = '-';
  debugStr[i++] = 'm';
  if (d.IPC)
    debugStr[i++] = '+';
  else
    debugStr[i++] = '-';
  debugStr[i++] = 'I';
  if (d.EMS)
    debugStr[i++] = '+';
  else
    debugStr[i++] = '-';
  debugStr[i++] = 'E';
  if (d.config)
    debugStr[i++] = '+';
  else
    debugStr[i++] = '-';
  debugStr[i++] = 'c';
  if (d.network)
    debugStr[i++] = '+';
  else
    debugStr[i++] = '-';
  debugStr[i++] = 'n';
  debugStr[i] = 0;
  dbug_printf("debugStr is %s\n", debugStr);

  return (0);
}
