AR=ar
CC=gcc
RANLIB=ranlib

ARFLAGS=rc
CFLAGS=-Wall -O3 -fno-strict-aliasing

OBJS = \
	bspfile.o \
	cmdlib.o \
	lbmlib.o \
	mathlib.o \
	polylib.o \
	scriplib.o \
	threads.o \
	trilib.o \
	wadlib.o

all: libcommon.a

libcommon.a: $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)
	$(RANLIB) $@

clean:
	rm -f *.o libcommon.a

