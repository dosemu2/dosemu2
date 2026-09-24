# Host-side build of the test harness. The loader itself is built with dj64.
CFLAGS ?= -O2 -g -Wall -Wextra -Wno-unused-parameter
OBJS = neexe.o neload.o nedump.o

all: nedump restub286

nedump: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

restub286: restub286.o
	$(CC) $(CFLAGS) -o $@ $<

neexe.o: neexe.c neexe.h
neload.o: neload.c neload.h neexe.h
nedump.o: nedump.c neload.h neexe.h

clean:
	rm -f nedump restub286 restub286.o $(OBJS)

.PHONY: all clean
