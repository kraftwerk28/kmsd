DEBUG_FLAGS = \
	-std=c11 -Wall -pedantic -Wno-comment -g \
	-fsanitize=address,undefined
RELEASE_FLAGS = -std=c11 -Wall -pedantic -O3

all:
	gcc $(DEBUG_FLAGS) main.c

release:
	gcc $(RELEASE_FLAGS) main.c
