#include "config.h"
#include "emu.h"
#include "keyboard.h"
#include "keystate.h"
#include "keynum.h"
#include "bitops.h"
#include "vc.h"
#include "mouse.h"
#include "timers.h"
#include "keyb_clients.h"

/* handle special dosemu keys like Ctrl-Alt-PgDn
 * This should only be called with 'make' events!
 * returns 1 if key was handled.
 */

static int switch_to_console(int vc_num)
{
	if (config.console_keyb || config.console_video) {
		t_shiftstate shiftstate = get_modifiers_r(input_keyboard_state.shiftstate);
		k_printf("KBD: switching to console #%d\n",vc_num);
		shiftstate &= ~(MODIFIER_ALT|MODIFIER_CTRL);
		set_shiftstate(shiftstate);
		user_vc_switch = 1;
		vt_activate(vc_num);
		return TRUE;
	}
	return FALSE;
}

Boolean handle_dosemu_keys(Boolean make, t_keysym key) 
{
	Boolean result = TRUE;
	switch(key) {

#ifdef X86_EMULATOR
	case KEY_DOSEMU_X86EMU_DEBUG:
		k_printf("KBD: Ctrl-Alt-PgUp\n");
		if (config.cpuemu) {
			if (debug_level('e') < 2) set_debug_level('e', 4);
			fflush(dbg_fd);
		}
		return 1;
#endif
	/* C-A-D is disabled */
	case KEY_DOSEMU_REBOOT:
		if (make) {
			k_printf("KBD: Ctrl-Alt-Del: rebooting dosemu\n");
			dos_ctrl_alt_del();
		}
		break;
	case KEY_DOSEMU_EXIT:
		if (make) {
			k_printf("KBD: Ctrl-Alt-PgDn: bye bye!\n");
			leavedos(0);
		}
		break;
		
	case KEY_DOSEMU_FREEZE:
		if (make) {
			if (!dosemu_frozen) {
				freeze_dosemu_manual();
			} else {
				unfreeze_dosemu();
			}
		}
		break;

	case KEY_DOSEMU_VT_1: 
	case KEY_DOSEMU_VT_2:
	case KEY_DOSEMU_VT_3:
	case KEY_DOSEMU_VT_4:
	case KEY_DOSEMU_VT_5:
	case KEY_DOSEMU_VT_6:
	case KEY_DOSEMU_VT_7:
	case KEY_DOSEMU_VT_8:
	case KEY_DOSEMU_VT_9:
	case KEY_DOSEMU_VT_10:
	case KEY_DOSEMU_VT_11:
	case KEY_DOSEMU_VT_12:
		if (make) {
			int vc_num;
			vc_num = (key - KEY_DOSEMU_VT_1) +1;
			result = switch_to_console(vc_num);
		}
		break;
			
	case KEY_MOUSE_UP:
	case KEY_MOUSE_DOWN:
	case KEY_MOUSE_LEFT:
	case KEY_MOUSE_RIGHT:
	case KEY_MOUSE_UP_AND_LEFT:
	case KEY_MOUSE_UP_AND_RIGHT:
	case KEY_MOUSE_DOWN_AND_LEFT:
	case KEY_MOUSE_DOWN_AND_RIGHT:
	case KEY_MOUSE_BUTTON_LEFT:
	case KEY_MOUSE_BUTTON_MIDDLE:
	case KEY_MOUSE_BUTTON_RIGHT:
		mouse_keyboard(make, key);        /* mouse emulation keys */
		break;

	case KEY_DOSEMU_HELP:
	case KEY_DOSEMU_REDRAW:
	case KEY_DOSEMU_SUSPEND:
	case KEY_DOSEMU_RESET:
	case KEY_DOSEMU_MONO:
	case KEY_DOSEMU_PAN_UP:
	case KEY_DOSEMU_PAN_DOWN:
	case KEY_DOSEMU_PAN_LEFT:
	case KEY_DOSEMU_PAN_RIGHT:
		if (Keyboard->handle_keys) {
			Keyboard->handle_keys(make, key);
		} else
		{
			result = FALSE;
		}
		break;

#if 0
	case KEY_MOUSE_GRAB:
		if (Keyboard == &Keyboard_X) {
			handle_X_keys(make, key);
		} else {
			result = FALSE;
		}
#endif

	default:
		result = FALSE;
		break;
	}
	return result;
}
