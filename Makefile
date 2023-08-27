SHELL := /bin/bash
TARGET := pe_trader pe_exchange

CC := gcc

CFLAGS     := -Wall -Wvla -Werror -std=c11

LIBDIR := lib
SRCDIR := src
BUILDDIR := build
TESTDIR := test
vpath %.h $(LIBDIR)
vpath %.c $(SRCDIR)
vpath %.o $(BUILDDIR)

SRCFILES        := $(wildcard $(SRCDIR)/*.c)
OBJFILES        := $(subst $(SRCDIR),$(BUILDDIR),$(patsubst %.c, %.o, $(wildcard $(SRCDIR)/*.c)))

_dummy := $(shell mkdir -p $(BUILDDIR))

.PHONY: tests

all: main pe_trader

main: $(SRCFILES) main.c
	#$(CC) -Wall -Wvla -Werror -std=c11 $(SRCFILES) btree/btree.c -o pe_exchange -I$(LIBDIR) -O2
	$(CC) -Wall -Wvla -Werror -std=gnu11 $(SRCFILES) main.c btree/btree.c -o main -I$(LIBDIR) `pkg-config --cflags --libs gtk+-3.0` `pkg-config --cflags --libs cairo` -rdynamic -lpthread -lm
	#$(CC) main.c -o main `pkg-config --cflags gtk+-3.0` `pkg-config --libs gtk+-3.0`

debug:$(TARGET)

.PHONY: all
.PHONY: clean


say:
	echo $(OBJFILES)
	echo $(notdir $(OBJFILES))

pe_trader:pe_trader.c pe_common.o $(BUILDDIR)/btree.o
	$(CC) $(CFLAGS) $< -o $@ -I$(LIBDIR) $(BUILDDIR)/pe_common.o $(BUILDDIR)/btree.o

pe_exchange:$(notdir $(OBJFILES)) $(BUILDDIR)/btree.o
	$(CC) $(CFLAGS) $(OBJFILES) $(BUILDDIR)/btree.o -o $@

.SUFFIXES: .c .o

$(BUILDDIR)/btree.o:btree/btree.c
	$(CC) $(CFLAGS) -g $< -c -o $@

%.o: %.c %.h
	$(CC) -I$(LIBDIR) $(CFLAGS) -O0 -g -c $< -o $(BUILDDIR)/$@

run: all
	#./pe_exchange products.txt ./trader_a ./trader_b ./trader_c ./trader_d
	./main products.txt ./trader_a ./trader_b ./trader_c ./trader_d

clean:
	killall -q pe_trader || :
	rm -f $(BUILDDIR)/*.o *.obj $(TARGET) test_reader test_writer unit_test main

test_writer: test_writer.c
	$(CC) -Wall -Wvla -Werror -std=c11 $< -o $@
test_reader: test_reader.c
	$(CC) -Wall -Wvla -Werror -std=c11 $< -o $@

tests: clean pe_trader test_writer test_reader
	$(CC) -Wall -Wvla -Werror -std=c11 $(SRCFILES) btree/btree.c -o pe_exchange -I$(LIBDIR) -O2 -D PEX_TEST
	gcc unit_test.c libcmocka-static.a build/*.o -o unit_test -Isrc -Ilib


run_tests: tests
	tests/E2E/run_e2e.sh ./pe_exchange ./test_writer ./test_reader tests/E2E/
	./unit_test
	



















