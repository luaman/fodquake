CC=gcc
STRIP=strip

CFLAGS=-g -Wall -O3 -fno-strict-aliasing -I../common
STRIPFLAGS=--strip-unneeded --remove-section=.comment

OBJS = \
	entities.o \
	light.o \
	ltface.o \
	threads.o \
	trace.o

all: light

light: $(OBJS)
	$(CC) $(CFLAGS) -L../common $(OBJS) -o $@.db -lcommon -lm
	$(STRIP) $(STRIPFLAGS) $@.db -o $@

clean:
	rm -f light light.db *.o

