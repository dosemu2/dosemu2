/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* This module should support selection for dosemu under X

 * Adapted by Uwe Bonnes bon@elektron.ikp.physik.th-darmstadt.de

 * from the screen.c module for rxvt by
 * This module is all new by Robert Nation
 * (nation@rocket.sanders.lockheed.com).
 */

#include <string.h>
#include "screen.h"
#include "emu.h"
#include "keyb_clients.h"    /* for paste_text() */

#define PROP_SIZE 1024  /* chunk size for retrieving the selection property */
#define MARGIN 0 /*  perhaps needed 23.8.95 */

int selstartx = 0,selstarty = 0;
int selected = SELECTION_CLEAR;

static unsigned char *selection_text = NULL;

#if 0 /* used at all?? */
static int selection_length;		/* length of selection text */

void scr_start_selection(int x,int y)
{ 
  char *text = "scr_start_selection";
  selected = SELECTION_INIT;
  selstartx = x;
  selstarty = y;
  X_printf("X: mouse selection started at %d %d\n", x,y);
  selection_text=text;
  X_printf("X: selection_text = %s \n", selection_text);
}

/***************************************************************************
 *  Extend the selection.
 ****************************************************************************/
void scr_extend_selection(int x,int y)
{
  selected = SELECTION_INIT;
  selstartx = x;
  selstarty = y;
  X_printf("X: mouse selection extended at %d %d\n", x,y);
  
}

/***************************************************************************
 *  Make the selection currently delimited by the selection end markers.
 ***************************************************************************/
void scr_make_selection(Display *dpy,int time)
{
#if 0 /* Not used below */
  int b,tx;
  int start_row, start_col, end_row, end_col;
#endif
  
  X_printf("X: mouse make the selection\n");
/*  if((selected == SELECTION_BEGIN)||
     (selected == SELECTION_INIT))
    {
      scr_clear_selection();
      selanchor_col = (selstartx - MARGIN)/font_width;
      selanchor_row = (selstarty - MARGIN)/font_height;
      selend_col = (selstartx - MARGIN)/font_width;
      selend_row = (selstarty - MARGIN)/font_height;
      selected = SELECTION_COMPLETE;
      return;
    }
  
  if(selected != SELECTION_ONGOING)
    return;

  selected = SELECTION_COMPLETE;

  if (selection_text != NULL)
    {
      free(selection_text);
      selection_text = NULL;
    }
  
  selection_screen = current_screen;
*/  /* Set start_row, col and end_row, col to point to the 
   * selection endpoints. */
/*    if(((selend_col <= selanchor_col)&&(selend_row == selanchor_row))||
     (selend_row < selanchor_row))
    {
      start_row = selend_row;
      start_col = selend_col;
      end_row = selanchor_row;
      end_col = selanchor_col;
    }
  else
    {
      end_row = selend_row;
      end_col = selend_col;
      start_row = selanchor_row;
      start_col = selanchor_col;
    }

  total = (end_row - start_row +1)*(font_width+1);
  str = (unsigned char *)malloc(total + 1);
  selection_text = str;


  ptr = str;
*/  /* save all points between start and end with selection flag */
/*  for(j=start_row; j <= end_row ;j++)
    {
      a=0;
      b = MyWinInfo.cwidth -1 ;
      if(j == start_row)
	  a = start_col;
      if(j == end_row)
	  b = end_col;

      tx=(j+MyWinInfo.saved_lines-MyWinInfo.offset)*
	(MyWinInfo.cwidth+1);
      if(((tx + a) < 0)||
	 ((tx+b)>=(MyWinInfo.saved_lines+MyWinInfo.cheight)*
	  (MyWinInfo.cwidth+1)))
	{
	  scr_clear_selection();
	  return;
	}
      for(i = a ; i <= b ; i++)
	{
	  *ptr++ =  cScreen->text[tx+i];
	}
      if(b == (MyWinInfo.cwidth-1))
	{
	  ptr--;
	  while(*ptr == ' ' && ptr >= str)
	    ptr--;
	  ptr++;
	  *ptr++ = '\n';
	}
    }  
  *ptr = 0;
*/
  selection_length = strlen(/*str*/selection_text);
/*
  if(selection_length <=0)
    return;
  XSetSelectionOwner(dpy,XA_PRIMARY,W,(Time)time);
  if (XGetSelectionOwner(dpy,XA_PRIMARY) != W)
    error("Could not get primary selection");
  
*/  /*  Place in CUT_BUFFER0 for backup.
   */
  XChangeProperty(dpy,DefaultRootWindow(dpy),XA_CUT_BUFFER0,
		  XA_STRING,8,PropModeReplace,selection_text,selection_length);
  
  return;
}


/***************************************************************************
 *  respond to a request for our current selection.
 ****************************************************************************/
void scr_send_selection(Display *dpy,int time,int requestor,int target,int property)
{
  XEvent event;
  
  event.xselection.type = SelectionNotify;
  event.xselection.selection = XA_PRIMARY;
  event.xselection.target = XA_STRING;
  event.xselection.requestor = requestor;
  event.xselection.time = time;

  X_printf("X: mouse selection requested by external\n");

  if (target == XA_STRING)
    {
      XChangeProperty(dpy,requestor,property,XA_STRING,8,PropModeReplace,
		      selection_text,selection_length);
      event.xselection.property = property;
    }
  else
    {
      event.xselection.property = None;
    }
  XSendEvent(dpy,requestor,False,0,&event);
}
#endif

/***************************************************************************
 *  Request the current primary selection
 ***************************************************************************/
void scr_request_selection(Display *dpy,Window W,int time,int x,int y)
{
  Atom sel_property;

  X_printf("X: mouse selection requested at %d %d\n", x,y);
  

  /*  First check that the release is within the window.
   */
/*  if (x < 0 || x >= MyWinInfo.pwidth || y < 0 || y >= MyWinInfo.pheight)
    {
      X_printf("X: MyWinInfo.pwidth %d MyWinInfo.pheight %d\n",MyWinInfo.pwidth,MyWinInfo.pheight);
      X_printf("X: mouse selection outside window\n");
            return; 
    }
*/  if (selection_text != NULL) 
    {
      /* The selection is internal
       */
      X_printf("X: mouse selection internal\n");
      paste_text(selection_text,strlen(selection_text));
      return;
    }
  X_printf("X: mouse display %d\n", (Bit32u)dpy);
  
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
void scr_paste_primary(Display *dpy,int window,int property,int Delete)
{
  Atom actual_type;
  int actual_format;
  long nitems, bytes_after, nread;
  unsigned char *data, *data2;

  X_printf("X: mouse paste received at\n" );
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

#if 0
/***************************************************************************
 *  Clear the current selection.
 ***************************************************************************/
void scr_clear_selection(void)
{
#if 0 /* Unused below */
  int j,x,i;
#endif

  selected = SELECTION_CLEAR;
  
  X_printf("X: mouse selection clear requested\n");

/*  for(j = 0 ; j < MyWinInfo.saved_lines + MyWinInfo.cheight ;j++)
    {
      x=j*(MyWinInfo.cwidth+1);
      for(i = 0 ; i < (MyWinInfo.cwidth +1) ; i++)
	cScreen->rendition[x+i] &= ~RS_SELECTED;
    }  

  for(j = 0 ; j < MyWinInfo.cheight ;j++)
    {
      x=j*(MyWinInfo.cwidth+1);
      for(i = 0 ; i < (MyWinInfo.cwidth +1) ; i++)
	cScreen->rendition[x+i] &= ~RS_SELECTED;
    }  

  selanchor_row = selanchor_col = selend_row = selend_col = 0;
*/
}

void scr_delete_selection(void)
{
#if 0 /* Unused below */
  int j,x,i;
#endif

  selected = SELECTION_CLEAR;

  X_printf("X: mouse selection clear requested\n");
  selection_text= NULL; /* added by bo 23.7.95 */

/*  if (selection_text != NULL) 
    {
      safefree(selection_text,"sel_text","clear_sel");
      selection_text = NULL;
      selection_length = 0;
    }

  for(j = 0 ; j < MyWinInfo.saved_lines + MyWinInfo.cheight ;j++)
    {
      x=j*(MyWinInfo.cwidth+1);
      for(i = 0 ; i < (MyWinInfo.cwidth +1) ; i++)
	cScreen->rendition[x+i] &= ~RS_SELECTED;
    }  

  for(j = 0 ; j < MyWinInfo.cheight ;j++)
    {
      x=j*(MyWinInfo.cwidth+1);
      for(i = 0 ; i < (MyWinInfo.cwidth +1) ; i++)
	cScreen->rendition[x+i] &= ~RS_SELECTED;
    }  

  selanchor_row = selanchor_col = selend_row = selend_col = 0;
*/
}

//#if 0

This mess is no longer used ? -- Bart

/*  Send count characters directly to the command.
 */
/*
void send_string(Display *dpy,unsigned char *buf,int count)
{
  int i, write_bytes, wrote;
  XKeyEvent *synthetic_event;
  KeySym mykeysym,*mykeysym_lower,*mykeysym_upper ;
  char single;

  if (count <= 0)
    return;
*//*initialize the synthetic event
*/
/*  synthetic_event->display=dpy;  
  synthetic_event->root=DefaultRootWindow(dpy);  
 
  X_printf("X: mouse selection text is : %s\n", buf);
  for (i=0; i< count; i++)
    {
      single=buf[i];
      X_printf("X: mouse selection processing char %c 0x%x (%d from %d)\n",
	       buf[i],buf[i],i+1, count);
      synthetic_event->type=KeyPress;
*//* what's wrong with 
if (isupper (buf[i]))  ????
using that upsets the whole thing
 */
/*      if (isupper (single))  
	{
	  X_printf("X: mouse selection found uppercase \n");
	  synthetic_event->keycode=50;
	  X_process_key(synthetic_event);
	}
      X_printf("X: mouse selection single is %c \n", single);
      synthetic_event->keycode=XKeysymToKeycode(dpy,buf[i]);
      X_printf("X: mouse selection keycode: %d \n",synthetic_event->keycode);
      synthetic_event->type=KeyPress;
      X_process_key(synthetic_event);
      synthetic_event->type=KeyRelease ;
      X_process_key(synthetic_event);
      if (isupper (buf[i])) 
	{
	  X_printf("X: mouse selection text is : %s\n", buf);
	  X_printf("X: mouse selection processed uppercase \n");
	  synthetic_event->keycode=50;
	  X_process_key(synthetic_event);
	}
    }
}
*/
#endif
