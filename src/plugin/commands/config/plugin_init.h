/* 
 * (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * This is file plugin_init.h for use within the src/plugin/<name>/
 *
 * It should contain a valid call to the init function of the plug-in such as
 *
 *    {
 *       extern void my_plugin_init(void);
 *       my_plugin_init();
 *    }
 * 
 * Don't forget the curly brackets around your statement.
 */

{
	extern void commands_plugin_init(void);
	commands_plugin_init();
}
