CFLAGS_EXT	= -g
LDFLAGS_EXT	=
LIBS_EXT	=

#{ OS specific part

## Linux
CFLAGS_OS = -Wall
LDFLAGS_OS =
LIBS_OS =
LEX	= flex

## SunOS 4.1
#CC	= gcc
#CFLAGS_OS = -DUSE_ON_EXIT
#LDFLAGS_OS =
#LIBS_OS =

## A certain SVR4.2 based OS
#CC	= /usr/abiccs/bin/cc
#CFLAGS_OS = -DUSE_FILIO_H
#LDFLAGS_OS =
#LIBS_OS	= -lnsl -lsocket

#}

#{ Proxy sockets

## SOCKS
#CFLAGS_PS = -DSOCKS -Dconnect=Rconnect -Dselect=Rselect
#LIBS_PS = -lsocks
#LDFLAGS_PS = -L$$HOME/lib

## Term
#CFLAGS_PS = -DTERMNET -I$$HOME/include -DNO_DIAL_CANCELING
#LDFLAGS_PS = -L$$HOME/lib
#LIBS_PS = -ltermnet
##CFLAGS_PS = -Dgethostbyname=term_gethostbyname -Dconnect=term_connect \
##	-Dshutdown=term_shutdown -Dclose=term_close \
##	-Drecv=term_recv -Dsend=term_send -Dsocket=term_socket

#}

CFLAGS	= $(CFLAGS_OS) $(CFLAGS_PS) $(CFLAGS_EXT)
LFLAGS	=
LDFLAGS	= $(LDFLAGS_OS) $(LDFLAGS_PS) $(LDFLAGS_EXT)
LIBS	= $(LIBS_EXT) $(LIBS_PS) $(LIBS_OS)

all: modemu

OBJS	= modemu.o sockbuf.o ttybuf.o stty.o telopt.o sock.o atcmd.o \
	timeval.o commx.o cmdarg.o verbose.o \
	lex.yy.o
modemu: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

lex.yy.c:
	$(LEX) $(LFLAGS) cmdlex.l

TAR	= tar
TARTAR	= modemu.tar
TARTARSRC = modemu-0.0.1.lsm README COPYING TODO modemu.1 \
	Makefile depend defs.h modemu.c \
	cmdlex.l cmdlex.h  sockbuf.c sockbuf.h  ttybuf.c ttybuf.h \
	stty.c stty.h  telopt.c telopt.h  sock.c sock.h  atcmd.c atcmd.h \
	timeval.c timeval.h  commx.c commx.h  cmdarg.c cmdarg.h \
	verbose.c verbose.h \
	xc-sends-mail
tar: $(TARTAR)
$(TARTAR): $(TARTARSRC)
	$(TAR) cvf $(TARTAR) $(TARTARSRC)
clean:
	$(RM) modemu core *.o lex.yy.c
tags:
	$(RM) lex.yy.c
	etags -t *.[ch]
diff:
	$(RM) diffs
	-for i in *.org; do \
		diff -u $$i `basename $$i .org` >> diffs; \
	done
DEPS_	= $(OBJS:.o=.c)
DEPS	= $(DEPS_:lex.yy.c=cmdlex.l)
#depend:	$(DEPS)
dep:
	gcc -MM -x c $(DEPS) | sed s/cmdlex.l.o/lex.yy.c/ > depend

include depend
