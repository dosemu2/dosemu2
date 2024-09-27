/*
 *  Copyright (C) 2024  @stsp
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include "utilities.h"
#include "builtins.h"
#include "commands.h"
#include "doshelpers.h"

static void show_help(void)
{
  const char *name = "emuipx";
  com_printf("%s -s\t\t - show current IPX settings\n", name);
  com_printf("%s -c <host:port>\t - connect to IPX->UDP relay server\n", name);
  com_printf("%s -d\t\t - disconnect from relay server\n", name);
  com_printf("%s -h \t\t - this help\n", name);
}

static int ipx_connect(const char *dest)
{
  struct REGPACK r = REGPACK_INIT;
  char *s = strdup(dest);
  char *p = strchr(s, ':');
  u_short port = 0;
  char *host = NULL;
  int len;

  if (p)
    *p++ = '\0';
  else
    p = s;
  port = atoi(p);
  if (!port) {
    com_printf("port not set\n");
    free(s);
    return -1;
  }
  r.r_bx = port;
  *p++ = '\0';
  len = strlen(s);
  if (len >= 128) {
    com_printf("host name too long\n");
    free(s);
    return -1;
  }
  r.r_cx = len;
  if (len) {
    host = lowmem_alloc(len + 1);
    strcpy(host, s);
    free(s);
    r.r_es = DOSEMU_LMHEAP_SEG;
    r.r_di = DOSEMU_LMHEAP_OFFS_OF(host);
  }
  r.r_ax = (DOS_HELPER_IPX_HELPER | (DOS_SUBHELPER_IPX_CONNECT << 8)) & 0xffff;
  com_intr(DOS_HELPER_INT, &r);
  if (host)
    lowmem_free(host);
  if (r.r_flags & 1) {
    com_printf("UDP connect failed\n");
    return -1;
  }
  return 0;
}

static int ipx_disconnect(void)
{
  struct REGPACK r = REGPACK_INIT;

  r.r_ax = (DOS_HELPER_IPX_HELPER | (DOS_SUBHELPER_IPX_DISCONNECT << 8)) & 0xffff;
  com_intr(DOS_HELPER_INT, &r);
  if (r.r_flags & 1) {
    com_printf("UDP not connected\n");
    return -1;
  }
  return 0;
}

static int show_config(void)
{
  struct REGPACK r = REGPACK_INIT;
  r.r_ax = (DOS_HELPER_IPX_HELPER | (DOS_SUBHELPER_IPX_CONFIG << 8)) & 0xffff;
  com_intr(DOS_HELPER_INT, &r);
  if (r.r_flags & 1) {
    com_printf("IPX helper failed\n");
    return -1;
  }
  com_printf("IPX Enabled: %i\n", r.r_bx & 1);
  com_printf("IPX->UDP Relay: %s\n", (r.r_bx & 2) ? "on" : "off");
  com_printf("Connected: %s\n", (r.r_bx & 4) ? "yes" : "no");
  return 0;
}

int emuipx_main(int argc, char **argv)
{
  int c;

  if (argc < 2) {
    show_help();
    return 0;
  }

  GETOPT_RESET();
  while ((c = getopt(argc, argv, "sc:dh")) != -1) {
    switch (c) {
      case 's':
        show_config();
        break;
      case 'c':
        ipx_connect(optarg);
        break;
      case 'd':
        ipx_disconnect();
        break;
      case 'h':
        show_help();
        break;
      default:
        com_printf("Unknown option\n");
        return EXIT_FAILURE;
    }
  }
  return 0;
}
