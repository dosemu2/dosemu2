#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>

char insmod_syslog=0;

void insmod_setsyslog (const char *name)
{
	openlog (name,LOG_CONS,LOG_DAEMON);
	insmod_syslog = 1;
}

/*
	Generate an error message either on stderr or the syslog facility
*/
void insmod_error (const char *ctl, ...)
{
	char buf[1000];
	va_list list;
	va_start (list,ctl);
	vsprintf (buf,ctl,list);
#ifdef HACKER_TOOL
	{
	  extern int silent_poll_mode;
	  if (silent_poll_mode) return;
	}
#endif
	if (insmod_syslog){
		syslog (LOG_ERR,"%s",buf);
	}else{
		fprintf (stderr,"%s\n",buf);
	}
}
/*
	Generate debug message either on stderr or the syslog facility
*/
void insmod_debug (const char *ctl, ...)
{
	char buf[1000];
	va_list list;
	va_start (list,ctl);
	vsprintf (buf,ctl,list);
	if (insmod_syslog){
		syslog (LOG_DEBUG,"%s",buf);
	}else{
		fprintf (stderr,"%s\n",buf);
	}
}
