all:
	gcc -g -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wformat=2 -Wno-gnu -o test example_tests.c

clean:
	rm test

