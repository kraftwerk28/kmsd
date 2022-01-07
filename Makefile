CFLAGS = -std=c11 -I. -Wall -pedantic -Wno-comment
DEBUG_FLAGS = -g -fsanitize=address,undefined
RELEASE_FLAGS = -O3
LFLAGS =
OUTFILE = a.out

all:
	gcc $(CFLAGS) $(DEBUG_FLAGS) -o $(OUTFILE) main.c $(LFLAGS)

release:
	gcc $(CFLAGS) $(RELEASE_FLAGS) -o $(OUTFILE) main.c $(LFLAGS)
