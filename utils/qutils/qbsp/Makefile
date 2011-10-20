CC=gcc
STRIP=strip

CFLAGS=-g -Wall -O3 -fno-strict-aliasing -I../common
STRIPFLAGS=--strip-unneeded --remove-section=.comment

OBJS = \
	brush.o \
	csg4.o \
	map.o \
	merge.o \
	nodraw.o \
	outside.o \
	portals.o \
	qbsp.o \
	region.o \
	solidbsp.o \
	surfaces.o \
	tjunc.o \
	writebsp.o

all: qbsp

qbsp: $(OBJS)
	$(CC) $(CFLAGS) -L../common $(OBJS) -o $@.db -lcommon -lm
	$(STRIP) $(STRIPFLAGS) $@.db -o $@

clean:
	rm -f qbsp qbsp.db *.o

