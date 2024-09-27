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
#include <arpa/inet.h>
#include "utilities.h"
#include "builtins.h"
#include "commands.h"
#include "doshelpers.h"

static void emutcp_config(void)
{
  struct REGPACK r = REGPACK_INIT;
  char gw[INET_ADDRSTRLEN];
  in_addr_t gwa;

  r.r_ax = (DOS_HELPER_TCP_HELPER | (DOS_SUBHELPER_TCP_CONFIG << 8)) & 0xffff;
  com_intr(DOS_HELPER_INT, &r);
  if (r.r_flags & 1) {
    com_printf("TCP helper failed\n");
    return;
  }

  com_printf("Enabled: %i\n", r.r_bx);
  com_printf("Iface: %s (%s)\n",
      (char *)LINEAR2UNIX(SEGOFF2LINEAR(r.r_es, r.r_di)),
      r.r_cx ? "fixed" : "default");
  r.r_ax = (DOS_HELPER_TCP_HELPER | (DOS_SUBHELPER_TCP_GETGW << 8)) & 0xffff;
  com_intr(DOS_HELPER_INT, &r);
  if (r.r_flags & 1) {
    com_printf("TCP helper failed\n");
    return;
  }
  gwa = ((unsigned)r.r_cx << 16) | r.r_dx;
  com_printf("Gateway: %s\n", inet_ntop(AF_INET, &gwa, gw, INET_ADDRSTRLEN));
}

static void emutcp_enable(int en)
{
  struct REGPACK r = REGPACK_INIT;

  r.r_ax = (DOS_HELPER_TCP_HELPER | (DOS_SUBHELPER_TCP_ENABLE << 8)) & 0xffff;
  r.r_bx = en;
  com_intr(DOS_HELPER_INT, &r);
  if (r.r_flags & 1) {
    com_printf("TCP helper failed\n");
    return;
  }
}

static void show_help(void)
{
  const char *name = "emutcp";
  com_printf("%s -s\t\t - show current TCP settings\n", name);
  com_printf("%s -e <1|0>\t\t - enable/disable TCP driver\n", name);
  com_printf("%s -g <gw>\t\t - set gateway (0 - reset to default)\n", name);
  com_printf("%s -i <iface>\t - set interface for TCP driver\n", name);
  com_printf("%s -I\t\t - reset to default interface\n", name);
  com_printf("%s -h \t\t - this help\n", name);
}

static void set_iface(const char *iface)
{
  struct REGPACK r = REGPACK_INIT;
  char *iname;

  r.r_ax = (DOS_HELPER_TCP_HELPER | (DOS_SUBHELPER_TCP_SETIF << 8)) & 0xffff;
  r.r_cx = strlen(iface);
  iname = lowmem_alloc(r.r_cx + 1);
  strcpy(iname, iface);
  r.r_es = DOSEMU_LMHEAP_SEG;
  r.r_di = DOSEMU_LMHEAP_OFFS_OF(iname);
  com_intr(DOS_HELPER_INT, &r);
  lowmem_free(iname);
  if (r.r_flags & 1) {
    com_printf("TCP helper failed\n");
    return;
  }
}

static void clear_iface(void)
{
  struct REGPACK r = REGPACK_INIT;

  r.r_ax = (DOS_HELPER_TCP_HELPER | (DOS_SUBHELPER_TCP_SETIF << 8)) & 0xffff;
  r.r_cx = 0;
  com_intr(DOS_HELPER_INT, &r);
  if (r.r_flags & 1) {
    com_printf("TCP helper failed\n");
    return;
  }
}

static void set_gw(const char *gw)
{
  struct REGPACK r = REGPACK_INIT;
  in_addr_t gwa;
  int rc;

  if (strcmp(gw, "0") == 0) {
    gwa = 0;
  } else {
    rc = inet_pton(AF_INET, gw, &gwa);
    if (rc != 1) {
      com_printf("Bad Gateway\n");
      return;
    }
  }
  r.r_ax = (DOS_HELPER_TCP_HELPER | (DOS_SUBHELPER_TCP_SETGW << 8)) & 0xffff;
  r.r_cx = gwa >> 16;
  r.r_dx = gwa & 0xffff;
  com_intr(DOS_HELPER_INT, &r);
  if (r.r_flags & 1) {
    com_printf("TCP helper failed\n");
    return;
  }
}

int emutcp_main(int argc, char **argv)
{
  int c;

  if (argc < 2) {
    show_help();
    return 0;
  }

  GETOPT_RESET();
  while ((c = getopt(argc, argv, "se:hi:Ig:")) != -1) {
    switch (c) {
      case 's':
        emutcp_config();
        break;
      case 'e':
        emutcp_enable(atoi(optarg));
        break;
      case 'g':
        set_gw(optarg);
        break;
      case 'h':
        show_help();
        break;
      case 'i':
        set_iface(optarg);
        break;
      case 'I':
        clear_iface();
        break;
      default:
        com_printf("Unknown option\n");
        return EXIT_FAILURE;
    }
  }
  return 0;
}
