/***********************************************
 * File: DOSDBG.C
 *  Program for Linux DOSDBG
 * Written: 12/15/93 by Tim Bird
 *
 * NOTES:
 *  DOSDBG supports the following commands:
 *  DOSDBG
 *    shows the current state of the debug variables
 *  DOSDBG string
 *    where string contains letters and + and - which allow the
 *    various debug options to be turned on and off
 *  DOSDBG HELP or DOSDBG ?
 *    show usage information for DOSDBG ???
 ***********************************************/
#include <stdio.h>    /* for printf */
#include <dos.h>    /* for geninterrupt and MK_FP */
#include <string.h>

typedef unsigned char uint8;
typedef unsigned int uint16;

#define CARRY_FLAG    1 /* carry bit in flags register */

#define CC_SUCCESS    0

#define MAX_DEBUG_STRING_LENGTH   100

#define DOS_HELPER_INT        0xE6
#define DOS_HELPER_DOSEMU_CHECK   0x00
#define DOS_HELPER_SHOW_REGS    0x01
#define DOS_HELPER_SHOW_INTS    0x02
#define DOS_HELPER_ADJUST_IOPERMS 0x03  /* CY indicates get or set */
#define DOS_HELPER_CONTROL_VIDEO  0x04  /* BL indicates init or release */
#define DOS_HELPER_SHOW_BANNER    0x05
#define DOS_HELPER_MFS_HELPER   0x20
#define DOS_HELPER_EMS_HELPER   0x21
#define DOS_HELPER_EMS_BIOS     0x22
#define DOS_HELPER_GET_DEBUG_STRING 0x10
#define DOS_HELPER_SET_DEBUG_STRING 0x11

void Usage( void )
{
  printf("Usage: DOSDBG [string|HELP]\n" );
  printf("If no string is specified, then DOSDBG will show the current debug settings.\n");
  printf("  If HELP is specified, then this screen is displayed.  Otherwise <string>\n");
  printf("  is parsed and used to change the current debug settings.\n");
  printf("\n");
  printf("<string> can contain letters, and the '+' and '-' characters.\n");
  printf("  Letters denote message classes from the following list:\n");
  printf("\n");
  printf("k  keyboard     d  disk         R  disk Reads   W  disk Writes\n");
  printf("h  hardware     i  port I/O     v  video        w  warnings\n");
  printf("s  serial       m  mouse        p  printer      g  general\n");
  printf("x  XMS          E  EMS          I  IPC          c  configuration\n");
  printf("n  network\n");
  printf("a  all (shorthand for all of the above)\n");
  printf("\n");
  printf("Any classes following a '+', up until the end of string or a '-',\n");
  printf("  will be turned on.  Likewise, any classes following a '-', to the\n");
  printf("  end of string or a '+' will be turned off.\n");
  printf("\n");
  printf("The character 'a' acts like a string of all possible debugging classes,\n");
  printf("  so \"-a\" turns all message off, and \"+a-RW\" would turn all messages\n");
  printf("  on except for disk Read and Write messages.\n");
}
/* returns non-zero major version number if DOSEMU is loaded */
uint16 CheckForDOSEMU( void )
{
  _AL = DOS_HELPER_DOSEMU_CHECK;
  geninterrupt( DOS_HELPER_INT );
  /* check for signature in AX */
  if( _AX==0xaa55 ) {
    return( _BX );
  } else {
    return( 0 );
  }
}

uint16 GetDebugString( char *debugStr )
{
  _ES = FP_SEG(debugStr);
  _DI = FP_OFF(debugStr);
  _AL = DOS_HELPER_GET_DEBUG_STRING;
  geninterrupt( DOS_HELPER_INT );
  return( _AX );
}

uint16 SetDebugString( char *debugStr )
{
  _ES = FP_SEG(debugStr);
  _DI = FP_OFF(debugStr);
  _AL = DOS_HELPER_SET_DEBUG_STRING;
  geninterrupt( DOS_HELPER_INT );
  return( _AX );
}

void printDebugClass( char class, char value )
{
  switch( class ) {
    case 'k': printf("k  keyboard   "); break;
    case 'd': printf("d  disk       "); break;
    case 'D': printf("D  dos        "); break;
    case 'R': printf("R  disk Reads "); break;
    case 'W': printf("W  disk Writes"); break;
    case 'h': printf("h  hardware   "); break;
    case 'i': printf("i  port I/O   "); break;
    case 'v': printf("v  video      "); break;
    case 'w': printf("w  warnings   "); break;
    case 's': printf("s  serial     "); break;
    case 'm': printf("m  mouse      "); break;
    case 'p': printf("p  printer    "); break;
    case 'g': printf("g  general    "); break;
    case 'x': printf("x  XMS        "); break;
    case 'E': printf("E  EMS        "); break;
    case 'I': printf("I  IPC        "); break;
    case 'c': printf("c  config     "); break;
    case 'n': printf("n  network    "); break;
    default: printf("%c  unknown    ", class); break;
  }
  switch( value ) {
    case '-': printf(" OFF"); break;
    case '+': printf(" ON"); break;
    default: printf(" UNKNOWN"); break;
  }
}

void ShowDebugString( void )
{
  uint16  ccode;
  char  debugStr[ MAX_DEBUG_STRING_LENGTH ];
  int   i;
  char  class, value;

  debugStr[0] = 0;
  ccode = GetDebugString( debugStr );
  if( ccode==CC_SUCCESS && strlen(debugStr) ) {
    printf("Current debug message class settings:\n");
    for( i=0; i<strlen(debugStr); i+=2 ) {
      value = debugStr[i];
      class = debugStr[i+1];
      printDebugClass( class, value );
      printf("\n");
    }
  } else {
    if( strlen(debugStr)==0 ) {
      printf("Cannot retrieve debug string from this version of DOSEMU\n");
    } else {
      printf("Error %04X getting debug string.\n", ccode );
    }
  }
}

uint16 ParseAndSetDebugString( char *userDebugStr )
{
  uint16  ccode;
  char debugStr[MAX_DEBUG_STRING_LENGTH];
  int   i, j;
  char  class, value;

  // expand the user string to a canonical form
  // by inserting a value before each class letter, and
  // by replacing 'a' with all values
  j = 0;
  value = '+';    /* assume ON for start of string */
  for( i=0; i<strlen(userDebugStr); i++ ) {
    class = userDebugStr[i];
    /* change sense of value if required */
    if( class=='+' || class=='-' ) {
      value=class;
      continue;
    }
    if( class=='a' )  {
      if( value=='+' ) {
        strcpy( &debugStr[j], "+k+d+R+W+h+i+v+w+s+m+p+g+x+E+I+c+n" );
      } else {
        strcpy( &debugStr[j], "-k-d-R-W-h-i-v-w-s-m-p-g-x-E-I-c-n" );
      }
      j += strlen( "+k+d+R+W+h+i+v+w+s+m+p+g+x+E+I+c+n" );
      continue;
    }
    debugStr[j++] = value;
    debugStr[j++] = class;
  }
  debugStr[j] = 0;
  ccode = SetDebugString( debugStr );
  if( ccode==0 ) {
    printf("Debug settings were adjusted...\n");
    ShowDebugString();
  }
  return( ccode );
}

main( int argc, char **argv )
{
  uint16  ccode;

  ccode = CheckForDOSEMU();
  if( ccode==0 ) {
    printf("DOSEMU is not running.  This program is intended for use\n");
    printf("only with the Linux DOS emulator.\n");
    exit(1);
  }

  /* need to parse the command line */
  /* if no parameters, then just show current mappings */
  if( argc == 1 ) {
    ShowDebugString();
    exit(0);
  }

  if( strcmpi(argv[1], "HELP")==0 || argv[1][0]=='?' ) {
    Usage();
    exit(0);
  }

  if( argc == 2 ) {
    ccode = ParseAndSetDebugString( argv[1] );
  } else {
    // if wrong number of parameters, force a usage screen
    ccode = 1;
  }

  if( ccode ) {
    Usage();
  }


MainExit:
  return( ccode );
}
