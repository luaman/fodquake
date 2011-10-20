CC=gcc
STRIP=strip

CFLAGS=-g -Wall -O3 -fno-strict-aliasing -I../common
STRIPFLAGS=--strip-unneeded --remove-section=.comment

OBJS = \
	flow.o \
	soundpvs.o \
	vis.o

all: vis

vis: $(OBJS)
	$(CC) $(CFLAGS) -L../common $(OBJS) -o $@.db -lcommon -lm
	$(STRIP) $(STRIPFLAGS) $@.db -o $@

clean:
	rm -f vis vis.db *.o

