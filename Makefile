# TODO
CC=gcc
LD=gcc
MEMORYMODEL=64

# OS detection
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	OSFLAGS=-DLINUX
	SOEXT=so
endif
ifeq ($(UNAME_S),Darwin)
	OSFLAGS=-DMACOS
	SOEXT=dylib
endif

# Directories
INCLDIR=include
SRCDIR=src
BUILDDIR=build

# Binary
BIN=librmtristripper.a

# Source files
SRC=$(wildcard $(SRCDIR)/*.c)

# Compiler
CFLAGS=-c -m$(MEMORYMODEL) -std=gnu99 -march=native \
       -fstrict-aliasing -ffast-math \
       -I$(INCLDIR) \
       -Wall -Wextra -Wvla -Wmissing-prototypes -Wstrict-aliasing=2 \
       $(OSFLAGS)

# Archiver
ARFLAGS=rcsv

# Debug
DBGDIR=$(BUILDDIR)/debug
DBGOBJ=$(SRC:$(SRCDIR)/%.c=$(DBGDIR)/%.o)
DBGCFLAGS=-g -O0 -DDEBUG_BUILD
DBGBIN=$(DBGDIR)/$(BIN)

# Release
RELDIR=$(BUILDDIR)/release
RELOBJ=$(SRC:$(SRCDIR)/%.c=$(RELDIR)/%.o)
RELCFLAGS=-O3
RELBIN=$(RELDIR)/$(BIN)

.PHONY: all clean prep debug release

all: release example

clean:
	rm -rf $(BUILDDIR)
	rm -f example
	mkdir -p $(DBGDIR) $(RELDIR)

prep:
	mkdir -p $(DBGDIR) $(RELDIR)

# Debug
$(DBGDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) $(DBGCFLAGS) -o $@ $<

$(DBGBIN): $(DBGOBJ)
	$(AR) $(ARFLAGS) $@ $^

debug: prep $(DBGBIN)

# Release
$(RELDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) $(RELCFLAGS) -o $@ $<

$(RELBIN): $(RELOBJ)
	$(AR) $(ARFLAGS) $@ $^

release: prep $(RELBIN)

# Example
example: release
	$(CC) -O3 example.c -I$(INCLDIR) -L$(RELDIR) -lrmtristripper -o example
