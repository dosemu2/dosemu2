/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* This module should support selection for dosemu under X

 * Adapted by Uwe Bonnes bon@elektron.ikp.physik.th-darmstadt.de

 * from the screen.c module for rxvt by
 * This module is all new by Robert Nation
 * (nation@rocket.sanders.lockheed.com).
 */

#include <string.h>
#include <limits.h>
#include "screen.h"
#include "emu.h"
#include "keyb_clients.h"    /* for paste_text() */
#include "vgaemu.h"
#include "vgatext.h"

#if CONFIG_SELECTION

#define PROP_SIZE 1024  /* chunk size for retrieving the selection property */
#define MARGIN 0 /*  perhaps needed 23.8.95 */

static t_unicode *sel_text = NULL;
static Time sel_time;
enum {
  TARGETS_ATOM,
  TIMESTAMP_ATOM,
  COMPOUND_TARGET,
  UTF8_TARGET,
  TEXT_TARGET,
  STRING_TARGET,
  NUM_TARGETS
};

static void get_selection_targets(Display *display, Atom *targets)
{
  /* Get atom for COMPOUND_TEXT/UTF8/TEXT type. */
  targets[TARGETS_ATOM] = XInternAtom(display, "TARGETS", True);
  targets[TIMESTAMP_ATOM] = XInternAtom(display, "TIMESTAMP", True);
  targets[COMPOUND_TARGET] = XInternAtom(display, "COMPOUND_TEXT", True);
  targets[UTF8_TARGET] = XInternAtom(display, "UTF8_STRING", True);
  targets[TEXT_TARGET] = XInternAtom(display, "TEXT", True);
  targets[STRING_TARGET] = XA_STRING;
}

static char *get_selection_string(char *charset)
{
	struct char_set_state paste_state;
	struct char_set *paste_charset;
	t_unicode *u = sel_text;
	char *s, *p;
	size_t sel_space = 0;

	while (sel_text[sel_space])
		sel_space++;
	paste_charset = lookup_charset(charset);
	sel_space *= MB_LEN_MAX;
	p = s = malloc(sel_space);
	init_charset_state(&paste_state, paste_charset);

	while (*u) {
		size_t result = unicode_to_charset(&paste_state, *u++,
						   p, sel_space);
		if (result == -1) {
			warn("save_selection unfinished2\n");
			break;
		}
		p += result;
		sel_space -= result;
	}
	*p = '\0';
	cleanup_charset_state(&paste_state);
	return s;
}

/*
 * Send selection data to other window.
 */
static void send_selection(Display *display, Time time, Window requestor, Atom target, Atom property)
{
	XEvent e;
	static int first = 1;
	static Atom targets[NUM_TARGETS];

	if (first) {
		get_selection_targets(display, targets);
	}
	e.xselection.type = SelectionNotify;
	e.xselection.selection = XA_PRIMARY;
	e.xselection.requestor = requestor;
	e.xselection.time = time;
	e.xselection.serial = 0;
	e.xselection.send_event = True;
	e.xselection.target = target;
	e.xselection.property = property;

	if (sel_text == NULL || target == None) {
		X_printf("X: Window 0x%lx requested selection, but it's empty!\n",   
			(unsigned long) requestor);
		e.xselection.property = None;
	}
	else if (target == targets[TARGETS_ATOM]) {
		X_printf("X: selection: TARGETS\n");
		XChangeProperty(display, requestor, property, XA_ATOM, 32,
			PropModeReplace, (char *)targets, NUM_TARGETS);
	}
	else if (target == targets[TIMESTAMP_ATOM]) {
		X_printf("X: timestamp atom %lu\n", sel_time);
		XChangeProperty(display, requestor, property, XA_INTEGER, 32,
			PropModeReplace, (char *)&sel_time, 1);
	}
	else if (target == targets[STRING_TARGET] ||
		 target == targets[COMPOUND_TARGET] ||
		 target == targets[UTF8_TARGET] ||
		 target == targets[TEXT_TARGET]) {
		/* from
		http://www.pps.jussieu.fr/~jch/software/UTF8_STRING/UTF8_STRING.text */
		char *send_text, *charset;
		if (target == targets[UTF8_TARGET])
			charset = "utf8";
		else if (target == targets[STRING_TARGET])
			charset = "iso8859-1";
		else if (target == targets[COMPOUND_TARGET])
			charset = "iso2022"; /* hope for the best */
		else /* TEXT */ {
			t_unicode *u = sel_text;
			charset = "iso8859-1";
			target = targets[STRING_TARGET];
			while (*u && *u < 0x100) u++;
			/* if any non-iso8859-1 characters are found,
			   use COMPOUND_TARGET */
			if (*u) {
				charset = "iso2022";
				target = targets[COMPOUND_TARGET];
			}
		}
		send_text = get_selection_string(charset);
		X_printf("X: selection: %s\n",send_text);
		XChangeProperty(display, requestor, property, target, 8, PropModeReplace, 
			send_text, strlen(send_text));
		X_printf("X: Selection sent to window 0x%lx as %s\n", 
			(unsigned long) requestor, XGetAtomName(display, target));
		free(send_text);
	}
	else
	{
		e.xselection.property = None;
		X_printf("X: Window 0x%lx requested unknown selection format %ld %s\n",
			(unsigned long) requestor, (unsigned long) target,
			 XGetAtomName(display, target));
	}
	XSendEvent(display, requestor, False, 0, &e);
}

static void scr_paste_primary(Display *dpy,Window window,int property,int Delete);

/***************************************************************************
 *  Request the current primary selection
 ***************************************************************************/
static void scr_request_selection(Display *dpy,Window W,int time)
{
  Atom sel_property;

  X_printf("X: mouse selection requested\n");
  X_printf("X: mouse display %p\n", dpy);
  
  if (XGetSelectionOwner(dpy,XA_PRIMARY) == None) 
    {
      /*  No primary selection so use the cut buffer.
       */
      X_printf("X: mouse XGetSelectionOwner\n");
      scr_paste_primary(dpy,DefaultRootWindow(dpy),XA_CUT_BUFFER0,False);
      return;
    }
  X_printf("X: mouse XGetSelectionOwner done\n");
  X_printf("X: mouse Window %d\n", (Bit32u)W);
  sel_property = XInternAtom(dpy,"VT_SELECTION",False);
  XConvertSelection(dpy,XA_PRIMARY,XA_STRING,sel_property,W,time);
  X_printf("X: mouse request done\n");

}

/***************************************************************************
 *  Respond to a notification that a primary selection has been sent
 ****************************************************************************/
static void scr_paste_primary(Display *dpy,Window window,int property,int Delete)
{
  Atom actual_type;
  int actual_format;
  long nitems, bytes_after, nread;
  unsigned char *data, *data2;

  X_printf("X: mouse paste received\n" );
  if (property == None)
    return;

  nread = 0;
  do 
    {
      if (XGetWindowProperty(dpy,window,property,nread/4,PROP_SIZE,Delete,
			     AnyPropertyType,&actual_type,&actual_format,
			     &nitems,&bytes_after,(unsigned char **)&data)
	  != Success)
	return;
      if (actual_type != XA_STRING)
	return;
      
      data2 = data;

      paste_text(data2,nitems);


      nread += nitems;
      XFree(data2);
    } while (bytes_after > 0);
}

void X_handle_selection(Display *display, Window mainwindow, XEvent *e)
{
  switch(e->type) {
    /* Selection-related events */
    /* The user made a selection in another window, so our selection
     * data is no longer needed. 
     */
  case SelectionClear:
    clear_selection_data();
    break;
  case SelectionNotify: 
    scr_paste_primary(display,e->xselection.requestor,
		      e->xselection.property,True);
    X_printf("X: SelectionNotify event\n");
    break;
    /* Some other window wants to paste our data, so send it. */
  case SelectionRequest:
    send_selection(display,
		   e->xselectionrequest.time,
		   e->xselectionrequest.requestor,
		   e->xselectionrequest.target,
		   e->xselectionrequest.property);
    break;
  case ButtonRelease:
    switch (e->xbutton.button)
      {
      case Button1 :
      case Button3 : {
	char *send_text;
	sel_text = end_selection();
	sel_time = e->xbutton.time;
	if (sel_text == NULL)
	  break;
	XSetSelectionOwner(display, XA_PRIMARY, mainwindow, sel_time);
	if (XGetSelectionOwner(display, XA_PRIMARY) != mainwindow)
	  {
	    X_printf("X: Couldn't get primary selection!\n");
	    return;
	  }

	/* this stores the selection in the cut buffer, an obsolete
	   way of copy/paste that only supports Latin-1 */
	send_text = get_selection_string("iso8859-1");
	XChangeProperty(display, DefaultRootWindow(display), XA_CUT_BUFFER0,
		   XA_STRING, 8, PropModeReplace, send_text, strlen(send_text));
	free(send_text);
	break;
      }
      case Button2 :
	X_printf("X: mouse Button2Release\n");
	scr_request_selection(display,mainwindow,e->xbutton.time);
	break;
      }
  }
}
#endif
