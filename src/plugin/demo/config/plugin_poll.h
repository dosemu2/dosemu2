/* 
 * (C) Copyright 1992, ..., 2002 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * This is file plugin_poll.h for use within the src/plugin/<name>/
 *
 * It should contain a valid call to a poll function of the plug-in such as
 *
 *    {
 *       extern int my_plugin_need_poll;
 *       extern void my_plugin_poll(int);
 *       if (my_plugin_need_poll) my_plugin_poll(VM86_RETURN_VALUE);
 *    }
 * 
 * Don't forget the curly brackets around your statement.
 */

{
	extern int my_plugin_need_poll;
	extern void my_plugin_poll(int);
	if (my_plugin_need_poll) my_plugin_poll(VM86_RETURN_VALUE);
}
