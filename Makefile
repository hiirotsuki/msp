CC ?= gcc
CFLAGS ?= -ansi -pedantic -Wall -Wextra

all: mspconv

mspconv: msp.c
	$(CC) $(CFLAGS) -I. $? -o $@ $(LDFLAGS)

clean:
	rm -f mspconv mspconv.exe *.o


.PHONY: clean
