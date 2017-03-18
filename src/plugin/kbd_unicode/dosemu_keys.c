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
	if (config.console_keyb == KEYB_RAW || config.console_video) {
		t_shiftstate shiftstate = get_modifiers_r(input_keyboard_state.shiftstate);
		k_printf("KBD: switching to console #%d\n",vc_num);
		shiftstate &= ~(MODIFIER_ALT|MODIFIER_CTRL);
		set_shiftstate(shiftstate);
		vt_activate(vc_num);
		return TRUE;
	}
	return FALSE;
}

Boolean handle_dosemu_keys(Boolean make, t_keysym key)
{
	Boolean result = TRUE;
	switch(key) {
	/* C-A-D is disabled */
	case DKY_DOSEMU_REBOOT:
		if (make) {
			k_printf("KBD: Ctrl-Alt-Del: rebooting dosemu\n");
			dos_ctrl_alt_del();
		}
		break;
	case DKY_DOSEMU_EXIT:
		if (make) {
			k_printf("KBD: Ctrl-Alt-PgDn: bye bye!\n");
			leavedos_once(0);
		}
		break;

	case DKY_DOSEMU_FREEZE:
		if (make) {
			if (!dosemu_frozen) {
				freeze_dosemu_manual();
			} else {
				unfreeze_dosemu();
			}
		}
		break;

	case DKY_DOSEMU_VT_1:
	case DKY_DOSEMU_VT_2:
	case DKY_DOSEMU_VT_3:
	case DKY_DOSEMU_VT_4:
	case DKY_DOSEMU_VT_5:
	case DKY_DOSEMU_VT_6:
	case DKY_DOSEMU_VT_7:
	case DKY_DOSEMU_VT_8:
	case DKY_DOSEMU_VT_9:
	case DKY_DOSEMU_VT_10:
	case DKY_DOSEMU_VT_11:
	case DKY_DOSEMU_VT_12:
		if (make) {
			int vc_num;
			vc_num = (key - DKY_DOSEMU_VT_1) +1;
			result = switch_to_console(vc_num);
		}
		break;

	case DKY_MOUSE_UP:
	case DKY_MOUSE_DOWN:
	case DKY_MOUSE_LEFT:
	case DKY_MOUSE_RIGHT:
	case DKY_MOUSE_UP_AND_LEFT:
	case DKY_MOUSE_UP_AND_RIGHT:
	case DKY_MOUSE_DOWN_AND_LEFT:
	case DKY_MOUSE_DOWN_AND_RIGHT:
	case DKY_MOUSE_BUTTON_LEFT:
	case DKY_MOUSE_BUTTON_MIDDLE:
	case DKY_MOUSE_BUTTON_RIGHT:
		mouse_keyboard(make, key);        /* mouse emulation keys */
		break;

	case DKY_DOSEMU_HELP:
	case DKY_DOSEMU_REDRAW:
	case DKY_DOSEMU_SUSPEND:
	case DKY_DOSEMU_RESET:
	case DKY_DOSEMU_MONO:
	case DKY_DOSEMU_PAN_UP:
	case DKY_DOSEMU_PAN_DOWN:
	case DKY_DOSEMU_PAN_LEFT:
	case DKY_DOSEMU_PAN_RIGHT:
		if (Keyboard->handle_keys) {
			Keyboard->handle_keys(make, key);
		} else
		{
			result = FALSE;
		}
		break;

#if 0
	case DKY_MOUSE_GRAB:
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
