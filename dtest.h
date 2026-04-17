#ifndef DTEST_H
#define DTEST_H

// TODO make all the passed/failed/timeout end on the same column.
// accept single test to run just that one

#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

typedef enum {
  PASSED,
  FAILED,
  TIMEOUT,
} Result;

typedef struct {
  void (*fn)(void);
  char* name;
  int pipefd0;
  int pipefd1;
  pid_t pid;
} TestCase;

void add_test(void (*fn)(void), char* name);

// after every test, call REGISTER_TEST to add it to tests that will be executed
#define REGISTER_TEST(test) add_test(test, #test)

#ifdef DTEST_IMPL

#ifndef MAX_TESTS
#define MAX_TESTS 128
#endif  // MAX_TESTS

#define MAX_MSG_LEN 1024

#ifndef MAX_TIME_PER_TEST
#define MAX_TIME_PER_TEST 1  // in s
#endif                       // MAX_TIME_PER_TEST

#ifndef PRINT_OUTPUT
#define PRINT_OUTPUT 1
#endif  // PRINT_OUTPUT

#ifndef PRINT_PASSED
#define PRINT_PASSED 0
#endif  // PRINT_PASSED

int run_tests(void);  // returns 0 when all tests passed, 1 ow

// in main call RUN_TESTS corresponding to how much output you want printed
#define RUN_TESTS return run_tests();

TestCase tests[MAX_TESTS];
uint16_t num_tests = 0;

static inline char* enum_to_str(Result res) {
  switch (res) {
    case PASSED:
      return "passed";
    case FAILED:
      return "failed";
    case TIMEOUT:
      return "timed out";
    default:
      return "ERROR";
  }
}

int run_tests(void) {
  uint16_t tests_passed = 0;
  printf("\n");

  // iterate through each test, fork (isolation in case of segfault), redirect
  // output, and call the function
  for (size_t i = 0; i < num_tests; ++i) {
    TestCase* curr_test = &tests[i];

    if (PRINT_OUTPUT) {
      int pipefd[2];
      if (pipe(pipefd) != 0) {
        perror("pipe");
      }

      curr_test->pipefd0 = pipefd[0];
      curr_test->pipefd1 = pipefd[1];
    }

    pid_t pid = fork();
    if (pid == -1) {
      perror("fork");
      return -1;
    } else if (pid == 0) {
      // child

      if (PRINT_OUTPUT) {
        // close read end of pipe and redirect stdout to write end
        close(curr_test->pipefd0);
        dup2(curr_test->pipefd1, STDOUT_FILENO);
        dup2(curr_test->pipefd1, STDERR_FILENO);
      } else {
        int devnull_fd = open("/dev/null", O_WRONLY);
        dup2(devnull_fd, STDOUT_FILENO);
        dup2(devnull_fd, STDERR_FILENO);
        close(devnull_fd);
      }

      alarm(MAX_TIME_PER_TEST);

      curr_test->fn();

      if (PRINT_OUTPUT) {
        close(curr_test->pipefd1);
      }

      _exit(0);
    } else {
      // parent
      curr_test->pid = pid;
    }
  }

  // iterate through each test, read from read end of pipe, print requested
  // output
  for (size_t i = 0; i < num_tests; ++i) {
    TestCase* curr_test = &tests[i];

    if (PRINT_OUTPUT) {
      close(curr_test->pipefd1);
    }

    int status = 0;
    if (waitpid(curr_test->pid, &status, 0) == -1) {
      perror("waitpid");
    }

    char buf[MAX_MSG_LEN];
    if (PRINT_OUTPUT) {
      ssize_t n;
      if ((n = read(curr_test->pipefd0, buf, sizeof(buf) - 1)) == -1) {
        perror("read");
      }
      close(curr_test->pipefd0);
      if (n == -1) {
        continue;
      }
      buf[n] = '\0';
    }

    Result result = FAILED;
    if (WIFEXITED(status)) {
      if (WEXITSTATUS(status) != 0) {
        result = FAILED;
      } else {
        result = PASSED;
        tests_passed++;
      }
    } else if (WIFSIGNALED(status)) {
      if (WTERMSIG(status) == SIGALRM) {
        result = TIMEOUT;
      } else {
        result = FAILED;
      }
    }

    if (!PRINT_PASSED && result == PASSED) {
      continue;
    }

    // 31 = red, 32 = green, 33 = yellow
    const char* ansi_col = (result == PASSED) ? "32m" : "31m";

    // name ... result
    printf("%s ... \x1b[%s%s\x1b[0m\n", curr_test->name, ansi_col,
           enum_to_str(result));

    if (PRINT_OUTPUT) {
      printf("\x1b[33m%s\x1b[0m", buf);
    }
  }

  printf("\nTests passed: [%u/%u]\n\n", tests_passed, num_tests);

  return (tests_passed == num_tests) ? 0 : 1;
}

void add_test(void (*fn)(void), char* name) {
  if (num_tests == MAX_TESTS) {
    return;
  }

  TestCase* new_test = &tests[num_tests];
  new_test->name = name;
  new_test->fn = fn;

  num_tests++;
}

#endif  // DTEST_IMPL

#endif  // DTEST_H
