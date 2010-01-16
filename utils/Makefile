CC=gcc
STRIP=strip

CFLAGS=-O2 -g -Wall
STRIPFLAGS=--strip-unneeded

pak: pak.o
	$(CC) $(CFLAGS) -o $@.db $<
	$(STRIP) $(STRIPFLAGS) -o $@ $@.db

clean:
	rm -f pak pak.db pak.o

.PHONY: clean

