/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * DANG_BEGIN_MODULE
 *
 * Description: 8042 Keyboard controller chip emulation for DOSEMU.
 *
 * Exports: keyb_8042_init(void), keyb_8042_reset(void)
 *
 * Maintainers: Scott Buchholz, Rainer Zimmermann
 * 
 * REMARK
 * This code provides truly rudimentary 8042 controller emulation.
 * Not having any documentation on the 8042 makes it hard to improve. :)
 *
 * /REMARK
 * DANG_END_MODULE
 *
 */

#include "emu.h"
#include "iodev.h"
#include "int.h"
#include "port.h"
#include "memory.h"
#include "keyboard.h"
#include "keyb_server.h"
#include "keyb_clients.h"
#include "speaker.h"
#include "hma.h"


/* accurate emulation of special 8042 and keyboard commands - currently untested...
*/
#define KEYB_CMD 1

Bit8u port60_buffer = 0;
Boolean port60_ready = 0;

#if KEYB_CMD

/* variable indicating the command status of the keyboard/8042.
 * if non-zero, e.g. a parameter byte to a command is expected.
 */
int wstate = 0;
int rstate = 0;

static int     keyb_ctrl_scanmap    = 1;
static int     keyb_ctrl_typematic  = 0x23;
static int     keyb_ctrl_enable     = 1;
static int     keyb_ctrl_isdata     = 0;
static Bit8u   keyb_ctrl_command    = 0x01;

static inline void keyb_ctrl_clearbuf(void)
{
/* this probably ought to do something :) */
}

/* write byte to the 8042's output buffer */
void output_byte_8042(Bit8u value) {
   port60_buffer=value;
   port60_ready=1;
   if (keyb_ctrl_command & 0x01) {    /* if interrupt enabled */
      k_printf("8042: scheduling IRQ1\n");
      pic_request(PIC_IRQ1);
   }
   else
      k_printf("8042: interrupt flag OFF!\n");
}

static void ack(void) {
   output_byte_8042(0xfa);
}

static void write_port60(Bit8u value)
{
  switch (wstate) {
    case 0x00:
      switch (value) {
	case 0xed:    /* set mode indicators */
	  h_printf("8042: write port 0x60 set mode indicators\n");
	  ack();
	  wstate=0xed;
	  break;
        case 0xee:    /* port check */
	  h_printf("8042: write port 0x60 test mode 0xee\n");
	  output_byte_8042(0xee);
	  break;
	case 0xf0:    /* set keyb scan byte */
	  h_printf("8042: write port 0x60 set keyb scan type\n");
	  ack();
	  wstate=0xf0;
	  break;
	case 0xf2:    /* get keyb type */
	  h_printf("8042: write port 0x60 get keyb type\n");
	  ack();
	  rstate=0xf2;
	  break;
	case 0xf3:    /* set typematic speed */
	  h_printf("8042: write port 0x60 set typematic speed\n");
	  ack();
	  wstate=0xf3;
	  break;
	case 0xf4:    /* clear buffer */
	  h_printf("8042: write port 0x60 clear buffer\n");
	  keyb_ctrl_clearbuf();
 	  keyb_ctrl_enable=1;
	  ack();
	  break;
	case 0xf5:    /* default, w/disable */
	  h_printf("8042: write port 0x60 set default, w/disable\n");
	  keyb_8042_reset();
	  keyb_ctrl_enable = 0;
	  ack();
	  break;
	case 0xf6:    /* set default */
	  h_printf("8042: write port 0x60 set default\n");
	  keyb_8042_reset();
	  ack();
	  break;
	case 0xf7:    /* set all keys to typematic */
	case 0xf8:    /* set all keys to make/break */
	case 0xf9:    /* set all keys to make */
	case 0xfa:    /* set all keys to typematic make/break */
	  h_printf("8042: write port 0x60 set mode (0x%02x)\n", value);
	  keyb_ctrl_clearbuf();
	  ack();
	  /* set mode ??? */
	  break;
	case 0xfb:    /* set single key to typematic & wait */
	case 0xfc:    /* set to make/break & wait */
	case 0xfd:    /* set to make & wait */
	  h_printf("8042: write port 0x60 set mod (0x%02x)\n", value);
	  keyb_ctrl_clearbuf();
	  ack();
	  wstate=value;
	  break;
	case 0xfe:    /* resend */
	  h_printf("8042: write port 0x60 resend\n");
	  output_byte_8042(port60_buffer);
	  break;
	case 0xff:    /* reset */
	  h_printf("8042: write port 0x60 reset\n");
	  ack();
	  rstate=0xff; /* wait for port 60h read */
	  break;
	default:
	  h_printf("8042: write port 0x60 unsupported command 0x%02x =>Error\n", value);
	  output_byte_8042(0xfe);
	  break;
      }
      break;
    case 0x60:
      h_printf("8042: write 8042 command byte 0x%02x\n", value);
      keyb_ctrl_command=value;
      /* no ack() */
      wstate=0;
      break;
    case 0xd1:
      h_printf("8042: drive output port lines, value=0x%02x\n", value);
      switch (value) {
        case 0xdf:	/* enable A20 */
	  h_printf("8042: enable A20 line\n");
	  set_a20(1);
	  break;
        case 0xdd:	/* disable A20) */
	  h_printf("8042: disable A20 line\n");
	  set_a20(0);
	  break;
      }
      port60_ready=0;
      wstate=0;
      break;
    case 0xed:        /* set LED mode */
    {
      t_modifiers modifiers = 0;
      h_printf("8042: write port 0x60 set LED mode to 0x%02x\n", value);
      /* TESTME this mapping is an educated guess */
      if (value & 0x01) modifiers |= MODIFIER_SCR;
      if (value & 0x02) modifiers |= MODIFIER_NUM;
      if (value & 0x04) modifiers |= MODIFIER_CAPS;
      keyb_client_set_leds(modifiers);
      ack();
      wstate=0;
      break;
    }
    case 0xf0:        /* get/set keyboard scan map */
      h_printf("8042: write port 0x60 get/set keyboard scan map 0x%02x\n", value);
      ack();
      if (value == 0)
 	 output_byte_8042(keyb_ctrl_scanmap);
      else
         keyb_ctrl_scanmap=value;
      wstate=0;
      break;

    case 0xf3:        /* set typematic rate */
      h_printf("8042: write port 0x60 set typematic rate 0x%02x\n", value);
      keyb_ctrl_typematic = value;
      ack();
      wstate=0;
      break;
    default:
      h_printf("8042: write port 0x60 illegal state (0x%02x), resending\n", 
	       wstate);
      wstate=0;
      write_port60(value);
  }
}

/* write to port 64h (8042 command register) */
static void write_port64(Bit8u value) {
   k_printf("8042: write port64h, =%02x\n",value);

       switch(value) {
	  case 0x20:       /* read 8042 command byte */
             output_byte_8042(keyb_ctrl_command);
	     break;
	  
	  case 0x60:       /* write 8042 command byte */
	     wstate=0x60;
	     break;

#if 0 /* not sure if these are ok and/or needed. */
	  
	  case 0xa4:       /* passwort installed test */
	     output_byte_8042(0xfa);   /* no password */
	     break;

	  case 0xa5:       /* load password */
	     /* XXX ... we should read bytes from port60 until 0 is found */
	     break;

	  case 0xa9:       /* aux interface test */
	     output_byte_8042(0x00);  /* ok */
	     break;
	  
	  case 0xaa:       /* 8042 self test */
	     output_byte_8042(0x55);  /* ok */
	     break;
	  
	  case 0xab:       /* keyboard interface test */
	     output_byte_8042(0x00);  /* ok */
	     break;
	  
	  case 0xc0:       /* read 8042 input port */
	     output_byte_8042(0xff);   /* just send _something_... */
	     break;
#endif	  
	  case 0xd1:       /* next write to port 0x60 drives hardware port */
	     wstate=0xd1;
	     break;

	  case 0xf0 ... 0xff: /* produce 6ms pulse on hardware port */
	     h_printf("8042: produce 6ms pulse on hardware port, ignored\n");
	     wstate=0;
	     port60_ready=0;
	     break;

	  default:
	     h_printf("8042: write port 0x64 unsupported command 0x%02x, ignored\n",
		      value);
	     /* various other commands... ignore */
	     break;
       }
}
#endif

static Bit8u read_port60(void)
{
  Bit8u r = port60_buffer;
  port60_ready = 0;
   
  h_printf("8042: read port 0x60 = 0x%02x\n", r);

#if KEYB_CMD
  switch (rstate) {
    case 0xf2:        /* get keyboard type, MSB */
      h_printf("8042: read port 0x60, getting keyboard type (MSB)\n");
      output_byte_8042(0x83);
      rstate = 0x72;
      break;
    case 0x72:        /* get keyboard type, LSB */
      h_printf("8042: read port 0x60, getting keyboard type (LSB)\n");
      output_byte_8042(0xab);
      rstate = 0;
      break;
    case 0xff:        /* reset keyboard */
      h_printf("8042: read port 0x60, resetting\n");
      output_byte_8042(0xaa);  /* BAT completion code */
      rstate = 0;
      break;
    default:          /* invalid state ?! */
      rstate = 0;
      break;
  }
#endif   
  return r;
}


Bit8u keyb_io_read(ioport_t port)
{
  Bit8u r = 0;

  switch (port) {
  case 0x60:     
      r = read_port60();
     
      /* We ought to untrigger IRQ1, in case DOS was reading port60h with interrupts off, 
       * but currently the PIC code doesn't support this. */
#if 0     
      if (!port60_ready)
         pic_untrigger(PIC_IRQ1);
#endif
     
      k_printf("8042: read port 0x60 read=0x%02x\n",r);
    break;

  case 0x61:
    /* Handle only PC-Speaker right now */
    r = spkr_io_read(port);
    break;

  case 0x64:
    r= 0x1c | (port60_ready ? 0x01 : 0x00);
    k_printf("8042: read port 0x64 status check=0x%02x, port60_ready=%d\n",
	     r, port60_ready);
  }
  return r;
}

void keyb_io_write(ioport_t port, Bit8u value)
{
  switch (port) {
  case 0x60:
    k_printf("8042: write port 0x60 outb = 0x%x\n", value);
#if KEYB_CMD
     write_port60(value);
#endif
    break;

  case 0x61:
    if (value & 0x80) {
      k_printf("8042: IRQ ACK, %i\n", port60_ready);
      port60_ready = 0;
      int_check_queue();   /* reschedule irq1 if appropriate */
    }
    spkr_io_write(port, value);
    break;

  case 0x64:
    k_printf("8042: write port 0x64 outb = 0x%x\n", value);
    write_port64(value);
    break;
  }
}

void keyb_8042_init(void)
{
  emu_iodev_t  io_device;

  /* 8042 keyboard controller */
  io_device.read_portb   = keyb_io_read;
  io_device.write_portb  = keyb_io_write;
  io_device.read_portw   = NULL;
  io_device.write_portw  = NULL;
  io_device.read_portd   = NULL;
  io_device.write_portd  = NULL;
  io_device.handler_name = "8042 Keyboard data";
  io_device.start_addr   = 0x0060;
  io_device.end_addr     = 0x0060;
  io_device.irq          = 1;
  io_device.fd 	   = -1;
  port_register_handler(io_device, 0);

  io_device.handler_name = "8042 Keyboard command";
  io_device.start_addr   = 0x0064;
  io_device.end_addr     = 0x0064;
  io_device.irq 	 = EMU_NO_IRQ;
  port_register_handler(io_device, 0);

  io_device.handler_name = "Keyboard controller port B";
  io_device.start_addr   = 0x0061;
  io_device.end_addr     = 0x0061;
  port_register_handler(io_device, 0);
}

void keyb_8042_reset(void)
{
#if KEYB_CMD   
  rstate = 0;
  wstate = 0;
  keyb_ctrl_scanmap    = 1;
  keyb_ctrl_typematic  = 0x23;
  keyb_ctrl_enable     = 1;
  keyb_ctrl_isdata     = 0;
  keyb_ctrl_clearbuf();
#endif
  port60_buffer = 0;
  port60_ready = 0;
}
