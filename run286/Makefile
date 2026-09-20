# Host-side build of the test harness. The loader itself is built with dj64.
CFLAGS ?= -O2 -g -Wall -Wextra -Wno-unused-parameter
OBJS = neexe.o neload.o nedump.o

all: nedump

nedump: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

neexe.o: neexe.c neexe.h
neload.o: neload.c neload.h neexe.h
nedump.o: nedump.c neload.h neexe.h

clean:
	rm -f nedump $(OBJS)

.PHONY: all clean
