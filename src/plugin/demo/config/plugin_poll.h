/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * This is file plugin_poll.h for use within the src/plugin/<name>/
 *
 * It should contain a valid call to a poll function of the plug-in such as
 *
 *       if (my_plugin_need_poll) my_plugin_poll(VM86_RETURN_VALUE);
 * 
 */

if (my_plugin_need_poll) my_plugin_poll(VM86_RETURN_VALUE);
