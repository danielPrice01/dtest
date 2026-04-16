#ifndef DTEST_H
#define DTEST_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

typedef enum {
  PASSED,
  FAILED,
} Result;

typedef struct {
  void (*fn)(void);
  char* name;
  int pipefd0;
  int pipefd1;
  pid_t pid;
  Result result;
} TestCase;

void run_tests(int8_t flag);
void add_test(void (*fn)(void), char* name);

// after every test, call REGISTER_TEST to add it to tests that will be executed
#define REGISTER_TEST(test) add_test(test, #test)

// in main call RUN_TESTS corresponding to how much output you want printed
#define RUN_TESTS run_tests(0);
#define RUN_TESTS_VERBOSE run_tests(1);
#define RUN_TESTS_ERROR run_tests(2);

#ifdef DTEST_IMPL

#define MAX_TESTS 128
#define MAX_MSG_LEN 128

TestCase tests[MAX_TESTS];
uint16_t num_tests = 0;

static inline char* enum_to_str(Result res) {
  switch (res) {
    case PASSED:
      return "passed";
    case FAILED:
      return "failed";
    default:
      return "ERROR";
  }
}

void run_tests(int8_t flag) {
  uint16_t tests_passed = 0;
  printf("\n");

  for (size_t i = 0; i < num_tests; ++i) {
    TestCase* curr_test = &tests[i];

    int pipefd[2];
    if (pipe(pipefd) != 0) {
      perror("pipe");
    }

    curr_test->pipefd0 = pipefd[0];
    curr_test->pipefd1 = pipefd[1];

    pid_t pid = fork();
    if (pid == -1) {
      perror("fork");
      return;
    } else if (pid == 0) {
      // child
      // close read end of pipe and redirect stdout to write end
      close(curr_test->pipefd0);
      dup2(curr_test->pipefd1, STDOUT_FILENO);
      dup2(curr_test->pipefd1, STDERR_FILENO);

      curr_test->fn();

      close(curr_test->pipefd1);
      _exit(0);
    } else {
      // parent
      curr_test->pid = pid;
    }
  }

  for (size_t i = 0; i < num_tests; ++i) {
    TestCase* curr_test = &tests[i];
    close(curr_test->pipefd1);

    int status = 0;
    if (waitpid(curr_test->pid, &status, 0) == -1) {
      perror("waitpid");
    }

    char buf[MAX_MSG_LEN];
    size_t n;
    if ((n = read(curr_test->pipefd0, buf, sizeof(buf) - 1)) == -1) {
      perror("read");
    }
    buf[n] = '\0';

    if (WIFEXITED(status)) {
      if (WEXITSTATUS(status) != 0) {
        curr_test->result = FAILED;
      } else {
        curr_test->result = PASSED;
        tests_passed++;
      }
    } else if (WIFSIGNALED(status)) {
      curr_test->result = FAILED;
    }

    // 31 = red, 32 = green, 33 = yellow
    const char* ansi_col = (curr_test->result == PASSED) ? "32m" : "31m";

    // name ... result
    printf("%s ... \x1b[%s\%s\x1b[0m\n", curr_test->name, ansi_col,
           enum_to_str(curr_test->result));

    // (debug statements) flag: 0 = none, 1 = verbose, 2 = only on error
    if (flag == 1) {
      printf("\x1b[33m%s\x1b[0m", buf);
    } else if (flag == 2 && curr_test->result == FAILED) {
      printf("\x1b[33m%s\x1b[0m", buf);
    }
  }

  printf("\nTests passed: [%u/%u]\n\n", tests_passed, num_tests);
}

void add_test(void (*fn)(void), char* name) {
  TestCase* new_test = &tests[num_tests];
  new_test->name = name;
  new_test->fn = fn;

  num_tests++;
}

#endif  // DTEST_IMPL

#endif  // DTEST_H
