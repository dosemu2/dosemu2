/* 
 * (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * This is file plugin_ioselect.h for use within the src/plugin/<name>/
 *
 * It hooks within io_select(). This hook  should look such as
 *
 *     if (my_plugin_fd != -1)
 *          if (FD_ISSET(my_plugin_fd, &fds)) my_plugin_ioselect();
 * 
 * You _must_ have add the fd within my_plugin_init() to the ioselect
 * mechanisme such as
 *
 *   add_to_io_select(my_plugin_fd, 0);
 *
 * Within your my_plugin_ioselect() you have to do the proper
 * file operation and return as fast as possible or, when needed,
 * issue a callback to DOS (using do_call_back(farptr)), as this
 * also would give control back to the vm86.
 *
 */

if (my_plugin_fd != -1)
        if (FD_ISSET(my_plugin_fd, &fds)) my_plugin_ioselect();
