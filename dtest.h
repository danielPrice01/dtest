#ifndef DTEST_H
#define DTEST_H

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

int run_tests(int8_t flag);  // returns 0 when all tests passed, 1 ow
void add_test(void (*fn)(void), char* name);

// after every test, call REGISTER_TEST to add it to tests that will be executed
#define REGISTER_TEST(test) add_test(test, #test)

// in main call RUN_TESTS corresponding to how much output you want printed
#define RUN_TESTS return run_tests(0);
#define RUN_TESTS_VERBOSE return run_tests(1);
#define RUN_TESTS_ERROR return run_tests(2);

#ifdef DTEST_IMPL

#define MAX_TESTS 128
#define MAX_MSG_LEN 1024

#ifndef MAX_TIME_PER_TEST
#define MAX_TIME_PER_TEST 1  // in s
#endif

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

int run_tests(int8_t flag) {
  uint16_t tests_passed = 0;
  printf("\n");

  // iterate through each test, fork (isolation in case of segfault), redirect
  // output, and call the function
  for (size_t i = 0; i < num_tests; ++i) {
    TestCase* curr_test = &tests[i];

    if (flag != 0) {
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

      if (flag == 0) {
        int devnull_fd = open("/dev/null", O_WRONLY);
        dup2(devnull_fd, STDOUT_FILENO);
        dup2(devnull_fd, STDERR_FILENO);
        close(devnull_fd);
      } else if (flag != 0) {
        // close read end of pipe and redirect stdout to write end
        close(curr_test->pipefd0);
        dup2(curr_test->pipefd1, STDOUT_FILENO);
        dup2(curr_test->pipefd1, STDERR_FILENO);
      }

      alarm(MAX_TIME_PER_TEST);

      curr_test->fn();

      if (flag != 0) {
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

    if (flag != 0) {
      close(curr_test->pipefd1);
    }

    int status = 0;
    if (waitpid(curr_test->pid, &status, 0) == -1) {
      perror("waitpid");
    }

    char buf[MAX_MSG_LEN];
    if (flag != 0) {
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

    Result result;
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

    // 31 = red, 32 = green, 33 = yellow
    const char* ansi_col = (result == PASSED) ? "32m" : "31m";

    // name ... result
    printf("%s ... \x1b[%s\%s\x1b[0m\n", curr_test->name, ansi_col,
           enum_to_str(result));

    // (debug statements) flag: 0 = none, 1 = verbose, 2 = only on error
    if (flag == 1) {
      printf("\x1b[33m%s\x1b[0m", buf);
    } else if (flag == 2 && result == FAILED) {
      printf("\x1b[33m%s\x1b[0m", buf);
    }
  }

  printf("\nTests passed: [%u/%u]\n\n", tests_passed, num_tests);

  return (tests_passed == num_tests) ? 0 : 1;
}

void add_test(void (*fn)(void), char* name) {
  TestCase* new_test = &tests[num_tests];
  new_test->name = name;
  new_test->fn = fn;

  num_tests++;
}

#endif  // DTEST_IMPL

#endif  // DTEST_H
