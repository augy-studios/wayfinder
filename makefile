# Makefile - build wayfind on Debian 13 (needs: build-essential)
#
#   make            # build bench + example
#   make bench      # the benchmark binary
#   make example    # the usage example
#   make portable   # build without -march=native (for older CPUs / migration)
#   make run        # build + run the benchmark
#   make clean

CC      ?= gcc
# -O3 -march=native unlocks the per-cell nanosecond figures. Drop -march=native
# if you will move the binary to a different CPU than the one you build on.
CFLAGS  ?= -O3 -march=native -std=c11 -Wall -Wextra -Wpedantic -funroll-loops
LDLIBS  := -lm

CORE    := wayfind.c

.PHONY: all run portable clean

all: bench example

bench: bench.c $(CORE) wayfind.h
	$(CC) $(CFLAGS) -o $@ bench.c $(CORE) $(LDLIBS)

example: example.c $(CORE) wayfind.h
	$(CC) $(CFLAGS) -o $@ example.c $(CORE) $(LDLIBS)

# Same code, portable baseline (no CPU-specific instructions).
portable: CFLAGS := -O3 -std=c11 -Wall -Wextra -Wpedantic -funroll-loops
portable: clean all

run: bench
	./bench

clean:
	rm -f bench example *.o