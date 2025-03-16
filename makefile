CC = gcc
CFLAGS = -Wall -Wextra -g
TARGETS = mts
all: $(TARGETS)

mts: mts.c
	$(CC) $(CFLAGS) -o mts mts.c -pthread

clean:
	rm -f $(TARGETS)