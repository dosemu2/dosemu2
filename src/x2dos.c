/* X */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <X11/Intrinsic.h> /* Include standard Toolkit Header file.
       We do no need "StringDefs.h" */
#include <X11/StringDefs.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Label.h> /* Include the Label widget's header file. */
#include <X11/Xaw/Cardinals.h> /* Definition of ZERO. */
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Paned.h>
#include <X11/Xaw/AsciiText.h>

/* X-callbacks */
static void Quit();             /* the callback of the main quit button */
static void my_key_event_proc(Widget w, XtPointer clData, XEvent *, Boolean * ctd );
static void my_mouse_event_proc(Widget w, XtPointer clData, XEvent *, Boolean * ctd );

/* X-Vars: */
String fallback_resources[] = {
  "*PName.Label:    X-DOS",
  "*PName*height:            348",
  "*PName*width:             720",
  NULL };


/* X-pipes */
int keypipe;
int mousepipe;

/* X-exec */
pid_t child;

char * keyname;
char * mousename;

static unsigned char KeyTab[128] = {
    0,   0,   0,   0,   0,   0,   0,   0,   0,   1, /*   0 -   9 */
    2,   3,   4,   5,   6,   7,   8,   9,  10,  11, /*  10 -  19 */
   12,  13,  14,  15,  16,  17,  18,  19,  20,  21, /*  20 -  29 */
   22,  23,  24,  25,  26,  27,  28,  29,  30,  31, /*  30 -  39 */
   32,  33,  34,  35,  36,  37,  38,  39,  40,  41, /*  40 -  49 */
   42,  43,  44,  45,  46,  47,  48,  49,  50,  51, /*  50 -  59 */
   52,  53,  54,  55,  56,  57,  58,  59,  60,  61, /*  60 -  69 */
   62,  63,  64,  65,  66,  67,  68,  69,  70,  71, /*  70 -  79 */
   72,  73,  74,  75,  76,  77,  78,  79,  80,  81, /*  80 -  89 */
   82,  83,   0,   0,  86,  87,  88,  71,  72,  73, /*  90 -  99 */
   75,  76,  77,  79,  80,  81,  82,  83,   0,   0, /* 100 - 109 */
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0, /* 110 - 119 */
    0,   0,   0,   0,   0,   0,   0,   0            /* 120 - 128 */
};

char *progname;

void main( int argc, char * argv[] )
{

  char * args[6];

  XtAppContext app_con;
  Widget toplevel, outer, ScreenArea, PName, Qbutton;


  /* make keyboard-pipe */
  keyname = tempnam("/tmp", "x2dosK");
  mousename = tempnam("/tmp", "x2dosK");
  args[0] = "dos";
  args[1] = "-Y";
  args[2] = keyname;
  args[3] = "-Z";
  args[4] = mousename;
  args[5] = NULL;

  if ( ( mkfifo(keyname,0666) ) != 0 ){
    fprintf(stdout, "ERROR: Could not make the keyboard-pipe!\n");
    exit(-1);
  }
  if ( ( keypipe = open(keyname, O_RDWR) ) < 0 ){
    fprintf(stdout, "ERROR: Could not open the keyboard-pipe!\n");
    exit(-1);
  }

  if ( ( mkfifo(mousename,0666) ) != 0 ){
    fprintf(stdout, "ERROR: Could not make the mouse-pipe!\n");
    exit(-1);
  }
  if ( ( mousepipe = open(mousename, O_RDWR) ) < 0 ){
    fprintf(stdout, "ERROR: Could not open the mouse-pipe!\n");
    exit(-1);
  }

  /* start dos */
  if ( ( child = fork() ) < 0 ){
    fprintf(stdout, "ERROR: Could fork child!\n");
    exit(-1);
  }
  if ( child != 0 ){
    /*
     * Initialize the Toolkit, set the fallback resources, and get
     * the application context associated with this application.
     */

    toplevel = XtAppInitialize(&app_con, "Xdos", NULL, ZERO, &argc, argv,
          fallback_resources, NULL, ZERO);

    outer = XtCreateManagedWidget( "paned", panedWidgetClass, toplevel,
                                  NULL, ZERO);

    Qbutton = XtCreateManagedWidget("quit", commandWidgetClass, outer, NULL,ZERO);
    XtAddCallback(Qbutton, XtNcallback, Quit, NULL);

    ScreenArea = XtCreateManagedWidget("ScreenArea", boxWidgetClass, outer,
           NULL, ZERO);

    PName = XtCreateManagedWidget("PName", labelWidgetClass, ScreenArea,
      NULL, ZERO);

    XtAddEventHandler(PName, KeyPressMask | KeyReleaseMask, False, my_key_event_proc, NULL );
    XtAddEventHandler(PName,
        ButtonPressMask |
        ButtonReleaseMask |
        EnterWindowMask |
        LeaveWindowMask |
        PointerMotionMask,
        False, my_mouse_event_proc, NULL );

    /*
     * Create the windows, and set their attributes according
     * to the Widget data.
     */

    XtRealizeWidget(toplevel);

    /*
     * Now process the events.
     */

    XtAppMainLoop(app_con);

 }
  else {
    execvp( "dos", args );
  }
  exit(0);
}

static void
Quit(widget, client_data, call_data)
Widget  widget;
XtPointer client_data, call_data;
{
    XtDestroyApplicationContext(XtWidgetToApplicationContext(widget));
    close ( keypipe );
    close ( mousepipe );
    kill(child, SIGHUP);
    remove( keyname );
    free( keyname );

    remove( mousename );
    free( mousename );
    exit( 0 );
}

/*
 * Called when an event is received in an arbitrary paint window.
 */
void
my_key_event_proc(Widget w, XtPointer clData, XEvent * event, Boolean * ctd )
{

  char buffer[4];
  char KMask;
  int Anz = 0;
  int i;

  if ( event->xkey.type == KeyPress )
    KMask = 0;
  else
    KMask = 0x80;

  switch (event->xkey.keycode){
  case 97: /* HOME */
    buffer[0] = 224;
    buffer[1] = 71;
    Anz = 2;
    break;
  case 98: /* Up */
    buffer[0] = 224;
    buffer[1] = 72;
    Anz = 2;
    break;
  case 99: /* Pg Up */
    buffer[0] = 224;
    buffer[1] = 73;
    Anz = 2;
    break;
  case 100: /* Left */
    buffer[0] = 224;
    buffer[1] = 75;
    Anz = 2;
    break;
  case 102: /* Right */
    buffer[0] = 224;
    buffer[1] = 77;
    Anz = 2;
    break;
  case 103: /* End */
    buffer[0] = 224;
    buffer[1] = 79;
    Anz = 2;
    break;
  case 104: /* Down */
    buffer[0] = 224;
    buffer[1] = 80;
    Anz = 2;
    break;
  case 105: /* Pg Down */
    buffer[0] = 224;
    buffer[1] = 81;
    Anz = 2;
    break;
  case 106: /* Ins */
    buffer[0] = 224;
    buffer[1] = 82;
    Anz = 2;
    break;
  case 107: /* Del */
    buffer[0] = 224;
    buffer[1] = 83;
    Anz = 2;
    break;
  case 108: /* KP_Enter */
    buffer[0] = 224;
    buffer[1] = 28;
    Anz = 2;
    break;
  case 109: /* Right_Ctrl */
    buffer[0] = 224;
    buffer[1] = 29;
    Anz = 2;
    break;
  case 110: /* Pause */
    buffer[0] = 225;
    buffer[1] = 29;
    buffer[2] = 69;
    Anz = 3;
    break;
  case 111: /* PrintScreen */
    buffer[0] = 224;
    buffer[1] = 42;
    buffer[2] = 224;
    buffer[3] = 55;
    Anz = 4;
    break;
  case 112: /* KP_/ */
    buffer[0] = 224;
    buffer[1] = 53;
    Anz = 2;
    break;
  case 113: /* Right_Alt */
    buffer[0] = 224;
    buffer[1] = 56;
    Anz = 2;
    break;
  case 114: /* Ctrl-Pause */
    buffer[0] = 224;
    buffer[1] = 70;
    Anz = 2;
    break;
  default:
    buffer[0] = KeyTab[event->xkey.keycode];
    Anz = 1;
  }

  for ( i = 0; i < Anz; i++ )
    buffer[i] = buffer[i] | KMask;

  if ( Anz > 0 ) {
    write(keypipe,buffer,Anz);
    kill(child, SIGIO);
  }
  return;
}


void
my_mouse_event_proc(Widget w, XtPointer clData, XEvent * event, Boolean * ctd )
{
  char buffer[3*sizeof(int)];
  int button;
  int Anz = 0;

  if ( event->type == MotionNotify ){
    ((int *) buffer)[0] = event->xmotion.x;
    ((int *) buffer)[1] = event->xmotion.y;
    button = 0;
    if ( event->xmotion.state & Button1Mask )
      button = 0x04;
    if ( event->xmotion.state & Button2Mask )
      button = 0x02;
    if ( event->xmotion.state & Button3Mask )
      button = 0x01;
    ((int *) buffer)[2] = button;
    Anz = 3 * sizeof(int);
  }
  else {
    if ( ( event->type == ButtonPress ) ){
      ((int *) buffer)[0] = event->xbutton.x;
      ((int *) buffer)[1] = event->xbutton.y;
      button = 0;
      switch ( event->xbutton.button ){
      case Button1:
 button = 0x04;
 break;
      case Button2:
 button = 0x02;
 break;
      case Button3:
 button = 0x01;
 break;
      default:
      }
      ((int *) buffer)[2] = button;
      Anz = 3 * sizeof(int);
    }
    else {
      if ( ( event->type == ButtonRelease ) ){
 ((int *) buffer)[0] = event->xbutton.x;
 ((int *) buffer)[1] = event->xbutton.y;
 /* Button state before event: */
 button = 0;
 if ( event->xbutton.state & Button1Mask )
   button = 0x04;
 if ( event->xbutton.state & Button2Mask )
   button = 0x02;
 if ( event->xbutton.state & Button3Mask )
   button = 0x01;
 /* set button to 0 */
 switch ( event->xbutton.button ){
 case Button1:
   button = button & ~0x04;
   break;
 case Button2:
   button = button & ~0x02;
   break;
 case Button3:
   button = button & ~0x01;
   break;
 default:
 }
 ((int *) buffer)[2] = button;
 Anz = 3 * sizeof(int);
      }
      else {
 if ( ( event->type == EnterNotify ) || ( event->type == LeaveNotify ) ){
   ((int *) buffer)[0] = event->xcrossing.x;
   ((int *) buffer)[1] = event->xcrossing.y;
   button = 0;
   if ( event->xcrossing.state & Button1Mask )
     button = 0x04;
   if ( event->xcrossing.state & Button2Mask )
     button = 0x02;
   if ( event->xcrossing.state & Button3Mask )
     button = 0x01;
   ((int *) buffer)[2] = button;
   Anz = 3 * sizeof(int);
 }
      }
    }
  }
  if ( Anz > 0 ) {
    write(mousepipe,buffer,Anz);
    kill(child, SIGIO);
  }
  return;
}
