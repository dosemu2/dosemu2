LEX = flex
YACC = bison
YFLAGS = -d

default: par

all: par

scan.c: scan.l

parse.tab.h: parse.c

parse.c: parse.y
	$(YACC) $(YFLAGS) parse.y
	mv parse.tab.c parse.c

parse.o: parse.c
scan.o: scan.c parse.tab.h

par: scan.o parse.o
	$(CC) -o par scan.o parse.o -lfl

checkin:
	-ci scan.l parse.y

checkout:
	-co -l scan.l parse.y

clean:
	rm -f *.o scan.c parse.c parse.tab.h par parse.tab.c

install: par

dist: parse.y scan.l 
	install -d $(DISTPATH)/parse
	install -m 0644 Makefile parse.y scan.l scan.c $(DISTPATH)/parse
