/*
 * crtcemu.c 
 *
 * VGA crt controller emulator for VGAemu
 *
 * (c) 1997, sw
 *
 * This emulates a very basic CRT controller. Just
 * the start address registers (0xc, 0xd).
 *
 * DANG_BEGIN_MODULE
 *
 * The VGA crt controller for VGAemu.
 *
 * DAND_END_MODULE
 *
 */

/*
 * Defines to debug the crtc
 */
/* #define DEBUG_CRTC */

#include "config.h"
#include "emu.h"
#include "vgaemu.h"
#include "vgaemu_inside.h"

#define CRTC_MAX_INDEX 24

static indexed_register CRTC_data[CRTC_MAX_INDEX + 1];
static unsigned CRTC_index = 0;

unsigned CRTC_start_addr = 0;

void CRTC_init()
{
  unsigned i;
  indexed_register ir = {0, 0, reg_read_write, False};

#ifdef DEBUG_CRTC
  v_printf("VGAemu: CRTC_init\n");
#endif

  for(i = 0; i <= CRTC_MAX_INDEX; i++) CRTC_data[i] = ir;
  CRTC_index = 0;
}


void CRTC_set_index(unsigned char data)
{
#ifdef DEBUG_CRTC
  v_printf("VGAemu: CRTC_set_index: index = 0x%02x\n", (unsigned) data);
#endif

  CRTC_index = data;
}


unsigned char CRTC_get_index()
{
#ifdef DEBUG_CRTC
  v_printf("VGAemu: CRTC_get_index: index = 0x%02x\n", CRTC_index);
#endif

  return CRTC_index;
}


void CRTC_write_value(unsigned char data)
{
  unsigned u;

#ifdef DEBUG_CRTC
  v_printf("VGAemu: CRTC_write_value: CRTC[0x%02x] = 0x%02x\n", CRTC_index, (unsigned) data);
#endif

  switch(CRTC_index) {

    case 0x0c:
      CRTC_data[CRTC_index].write = CRTC_data[CRTC_index].read = data;
      u = data;
      CRTC_start_addr = (CRTC_start_addr & 0xff) | (u << 8);
#ifdef DEBUG_CRTC
      v_printf("VGAemu: CRTC_write_value: start addr = 0x%04x\n", CRTC_start_addr);
#endif
      break;

    case 0x0d:
      CRTC_data[CRTC_index].write = CRTC_data[CRTC_index].read = data;
      u = data;
      CRTC_start_addr = (CRTC_start_addr & 0xff00) | u;
#ifdef DEBUG_CRTC
      v_printf("VGAemu: CRTC_write_value: start addr = 0x%04x\n", CRTC_start_addr);
#endif
      break;

    default:
      if(CRTC_index <= CRTC_MAX_INDEX)
        CRTC_data[CRTC_index].write = CRTC_data[CRTC_index].read = data;
  }
}


unsigned char CRTC_read_value()
{
  unsigned char uc = 0xff;

  switch(CRTC_index) {

    default:
      if(CRTC_index <= CRTC_MAX_INDEX) uc = CRTC_data[CRTC_index].read;
  }

#ifdef DEBUG_CRTC
  v_printf("VGAemu: CRTC_read_value: CRTC[0x%02x] = 0x%02x\n", CRTC_index, (unsigned) uc);
#endif

  return uc;
}
