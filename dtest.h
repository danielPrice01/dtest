#ifndef DTEST_H
#define DTEST_H

#include <unistd.h>

// TODO custom assert commands, that are more descriptive. Get a sense of what
// the output for things are, print out what line and file they failed on

/*
 * Visible when calling #include "dtest.h" but without #define DTEST_IMPL_
 * before
 */

typedef enum {
  PASSED,
  FAILED,
  TIMEOUT,
} Result;

typedef struct {
  void (*fn)(void);
  char* name;
  size_t name_len;
  int pipefd[2];
  pid_t pid;
} TestCase;

void add_test(void (*fn)(void), const char* name);

// after every test, call REGISTER_TEST to add it to tests that will be executed
#define REGISTER_TEST(test) add_test(test, #test)

#ifdef DTEST_IMPL

/*
 * Visible when #define DTEST_IMPL included
 */

#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

/*
 * Redefinable macros
 */

#ifndef MAX_TESTS
#define MAX_TESTS 128
#endif  // MAX_TESTS

#ifndef MAX_MSG_LEN
#define MAX_MSG_LEN 1024
#endif  // MAX_MSG_LEN

#ifndef MAX_TEST_NAME_LEN
#define MAX_TEST_NAME_LEN 48
#endif  // MAX_TEST_NAME_LEN

#ifndef MAX_TIME_PER_TEST
#define MAX_TIME_PER_TEST 1  // in s
#endif                       // MAX_TIME_PER_TEST

#ifndef PRINT_OUTPUT
#define PRINT_OUTPUT 1
#endif  // PRINT_OUTPUT

#ifndef PRINT_PASSED
#define PRINT_PASSED 0
#endif  // PRINT_PASSED

/*
 * in main call RUN_TESTS with either no arguments or with (argc, argv) to
 * accept input from stdin to run group of tests, or individual tests
 */

int run_tests(const int argc,
              const char** argv);  // returns 0 when all tests passed, 1 ow

/*
 * Fixed macros
 */

// GET_RUN_TEST expands to be equal to the 6th value (index = 5)
#define GET_RUN_TEST(_0, _1, _2, _3, _4, CHOSEN_TEST_TYPE, ...) CHOSEN_TEST_TYPE

// if __VA_ARGS__ empty, concat operand removes it from macro definition
// RUN_TESTS expands into CHOSEN_TEST_TYPE(__VA_ARGS__) =>
// RUN_TESTS_NO_ARGS() or RUN_TESTS_YES_ARGS(argc, argv)

// In case of 1 or 3-4 arguments supplied: does not compile with
// "RUN_TESTS_required_0_or_2_arguments"
// In case of >4 arguments supplied: does not compile with "called object is not
// a function or function pointer"
#define RUN_TESTS(...)                                                   \
  GET_RUN_TEST(_, ##__VA_ARGS__, BAD_TEST, BAD_TEST, RUN_TESTS_YES_ARGS, \
               BAD_TEST, RUN_TESTS_NO_ARGS)                              \
  (__VA_ARGS__)

#define RUN_TESTS_NO_ARGS() return run_tests(0, NULL);
#define RUN_TESTS_YES_ARGS(argc, argv) \
  return run_tests((const int)argc, (const char**)argv);
#define BAD_TEST(...) RUN_TESTS_requires_0_or_2_arguments

#define MAX_RESULT_LEN (9)
#define PERIOD_PADDING (5)
#define SPACE_PADDING (1)
#define MAX_FINAL_COL \
  (MAX_TEST_NAME_LEN + PERIOD_PADDING + (2 * SPACE_PADDING) + MAX_RESULT_LEN)

TestCase tests[MAX_TESTS];
uint16_t num_tests = 0;

size_t max_len = 0;

static inline int string_compare(const void* item, const void* to_compare_to) {
  return strncmp((const char*)item, (const char*)to_compare_to,
                 MAX_TEST_NAME_LEN);
}

static inline int32_t find_index(
    const char* elt,
    int (*comparator_fn)(const void* item, const void* to_compare_to)) {
  for (int32_t i = 0; i < num_tests; ++i) {
    if (comparator_fn(elt, tests[i].name) == 0) {
      return i;
    }
  }

  return -1;
}

static inline void res_to_str_col_len(const Result res,
                                      char** result_str,
                                      char** ansi_col,
                                      size_t* result_len) {
  switch (res) {
    case PASSED:
      *result_str = "passed";
      *ansi_col = "32m";
      *result_len = 6;
      return;

    case FAILED:
      *result_str = "failed";
      *ansi_col = "31m";
      *result_len = 6;
      return;

    case TIMEOUT:
      *result_str = "timed out";
      *ansi_col = "31m";
      *result_len = 9;
      return;

    default:
      *result_str = "ERROR";
      *ansi_col = "31m";
      *result_len = 5;
      return;
  }
}

static inline void format_result(char* formatted_result,
                                 const size_t final_space,
                                 const TestCase* curr_test) {
  for (size_t i = 0; i < final_space; ++i) {
    if (i < curr_test->name_len) {
      formatted_result[i] = curr_test->name[i];
    } else if (i == curr_test->name_len) {
      formatted_result[i] = ' ';
    } else if (i == final_space - 1) {
      formatted_result[i] = ' ';
    } else if (i > curr_test->name_len) {
      formatted_result[i] = '.';
    }
  }
}

int run_tests(const int argc, const char** argv) {
  uint16_t tests_found = 0;
  if (argc > 1) {
    // TODO implement calling individual tests
    // TODO implement calling groups of tests (make new macros, START_GROUP,
    // END_GROUP)

    // TODO implement a sorting algo if there are enough things that are being
    // checked

    uint16_t left_idx = 0;
    int32_t right_idx = 0;

    for (int i = 1; i < argc; ++i) {
      // call a "finding" function to return the index
      right_idx = find_index(argv[i], string_compare);

      if (right_idx == -1) {
        printf("Test '%s' not found\n", argv[i]);
        continue;
      } else if (right_idx > left_idx) {
        TestCase tmp = tests[right_idx];
        tests[right_idx] = tests[left_idx];
        tests[left_idx] = tmp;
      }

      left_idx++;
      tests_found++;
    }
  }

  if (tests_found == 0 && argc > 1) {
    return 1;
  }

  if (tests_found > 0) {
    num_tests = tests_found;
  }

  size_t final_col =
      max_len + PERIOD_PADDING + SPACE_PADDING + MAX_RESULT_LEN + 1;

  uint16_t tests_passed = 0;
  printf("\n");

  // iterate through each test, fork (isolation in case of segfault), redirect
  // output, and call the function
  for (uint16_t i = 0; i < num_tests; ++i) {
    TestCase* curr_test = &tests[i];

    if (PRINT_OUTPUT) {
      int pipefd[2];
      if (pipe(pipefd) != 0) {
        perror("pipe");
      }

      curr_test->pipefd[0] = pipefd[0];
      curr_test->pipefd[1] = pipefd[1];
    }

    pid_t pid = fork();
    if (pid == -1) {
      perror("fork");
      return -1;
    } else if (pid == 0) {
      // child

      if (PRINT_OUTPUT) {
        // close read end of pipe and redirect stdout to write end
        close(curr_test->pipefd[0]);
        dup2(curr_test->pipefd[1], STDOUT_FILENO);
        dup2(curr_test->pipefd[1], STDERR_FILENO);
      } else {
        int devnull_fd = open("/dev/null", O_WRONLY);
        dup2(devnull_fd, STDOUT_FILENO);
        dup2(devnull_fd, STDERR_FILENO);
        close(devnull_fd);
      }

      alarm(MAX_TIME_PER_TEST);

      curr_test->fn();

      if (PRINT_OUTPUT) {
        close(curr_test->pipefd[1]);
      }

      _exit(0);
    } else {
      // parent
      curr_test->pid = pid;
    }
  }

  // iterate through each test, read from read end of pipe, print requested
  // output
  for (uint16_t i = 0; i < num_tests; ++i) {
    TestCase* curr_test = &tests[i];

    if (PRINT_OUTPUT) {
      close(curr_test->pipefd[1]);
    }

    int status = 0;
    if (waitpid(curr_test->pid, &status, 0) == -1) {
      perror("waitpid");
    }

    char buf[MAX_MSG_LEN];
    ssize_t buf_read_n;
    if (PRINT_OUTPUT) {
      if ((buf_read_n = read(curr_test->pipefd[0], buf, sizeof(buf) - 1)) ==
          -1) {
        perror("read");
      }
      close(curr_test->pipefd[0]);
      if (buf_read_n == -1) {
        continue;
      }
      buf[buf_read_n] = '\0';
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

    char* ansi_col;  // 31 = red, 32 = green, 33 = yellow
    char* result_str;
    size_t result_len;

    res_to_str_col_len(result, &result_str, &ansi_col, &result_len);

    // consists of test name, and necessary number of '.' as padding for all
    // results to end on same column
    size_t final_space = final_col - MAX_RESULT_LEN;
    char formatted_result[MAX_FINAL_COL];
    format_result(formatted_result, final_space, curr_test);

    // name ... result
    printf("%.*s\x1b[%s%s\x1b[0m\n", (int)final_space, formatted_result,
           ansi_col, result_str);

    if (PRINT_OUTPUT) {
      if (buf_read_n) {
        printf("\x1b[33m> \x1b[0m");
      }
      printf("\x1b[33m%s\x1b[0m", buf);
    }
  }

  printf("\nTests passed: [%u/%u]\n\n", tests_passed, num_tests);

  return (tests_passed == num_tests) ? 0 : 1;
}

void add_test(void (*fn)(void), const char* name) {
  if (num_tests == MAX_TESTS) {
    printf("Skipped test [max number of tests reached]\n");
    return;
  }

  size_t name_len = strnlen(name, MAX_TEST_NAME_LEN);
  if (name_len == MAX_TEST_NAME_LEN) {
    printf("Skipped test [name exceeds maximum length]\n");
  }

  TestCase* new_test = &tests[num_tests];
  new_test->name = (char*)name;
  new_test->name_len = name_len;
  new_test->fn = fn;

  num_tests++;
  max_len = (name_len > max_len) ? name_len : max_len;
}

#endif  // DTEST_IMPL

#endif  // DTEST_H
