DEBUG_FLAGS = -Wall -g -fsanitize=address,undefined -pedantic
RELEASE_FLAGS = -Wall -O3
all:
	@gcc $(DEBUG_FLAGS) main.c

dist:
	@gcc $(RELEASE_FLAGS) main.c
