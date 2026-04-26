all:
	gcc -g -Wall -Wextra -Wpedantic -Wno-gnu -o test example_tests.c

clean:
	rm test

