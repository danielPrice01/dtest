all:
	gcc -g -Wall -Wextra -Wpedantic -o test example_tests.c

clean:
	rm test

