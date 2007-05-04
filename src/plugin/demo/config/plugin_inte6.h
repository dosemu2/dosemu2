/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * This is file plugin_inte6.h for use within the src/plugin/<name>/
 *
 * It should contain a valid call to a DOSEMU_HELPER service function of
 * the plug-in such as
 *
 *    case (DOS_HELPER_PLUGIN+MYFUNCTION_OFFSET): {
 *       extern int my_plugin_inte6(void);
 *       if ( ! my_plugin_inte6()) return 0;
 *       break;
 *    }
 * 
 * Don't forget the curly brackets around your statement.
 */

case (DOS_HELPER_PLUGIN+1):
	if ( ! demo_plugin_inte6() ) return 0;
	break;

