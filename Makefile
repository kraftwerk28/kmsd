DEBUG_FLAGS = -Wall -Wpedantic -Wno-comment -std=c11 -g -fsanitize=address,undefined
RELEASE_FLAGS = -Wall -O3
all:
	gcc $(DEBUG_FLAGS) main.c

release:
	gcc $(RELEASE_FLAGS) main.c
