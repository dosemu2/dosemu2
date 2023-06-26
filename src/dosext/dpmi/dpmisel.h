/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef DPMISEL_H
#define DPMISEL_H

extern char _binary_dpmisel_o_bin_end[] asm("_binary_dpmisel_o_bin_end");
extern char _binary_dpmisel_o_bin_start[] asm("_binary_dpmisel_o_bin_start");

#define DPMI_SEL_OFF(x) (x-DPMI_sel_code_start)

#include "dpmisel_offsets.h"

#endif
