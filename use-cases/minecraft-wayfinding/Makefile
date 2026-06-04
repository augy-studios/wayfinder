CC      = gcc
CFLAGS  = -std=c11 -O2 -Wall -Wextra -I../..
LDFLAGS = -lm

demo: demo.c theta_star.c ../../wayfind.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f demo

.PHONY: clean
