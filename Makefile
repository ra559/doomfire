# Build the DoomFire programs.
#
#   make                 build doomfire and doomfire-base
#   make doomfire        build the full version (fire + text)
#   make doomfire-base   build the fire-only base version
#   make clean           remove the built binaries

CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra
LDFLAGS ?=

.PHONY: all clean

all: doomfire doomfire-base

doomfire: doomfire.c Makefile
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

doomfire-base: complete_doomfire_base.c Makefile
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

clean:
	rm -f doomfire doomfire-base
