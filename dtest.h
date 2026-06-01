#ifndef DTEST_H
#define DTEST_H

// TODO custom assert commands, that are more descriptive. Get a sense of what
// the output for things are, print out what line and file they failed on

/*
 * Visible when calling #include "dtest.h" but without #define DTEST_IMPL_
 * before
 */

void add_test(void (*fn)(void), const char* name);
void start_group(const char* group);
void end_group(void);

// after every test, call REGISTER_TEST to add it to tests that will be executed
#define REGISTER_TEST(test) add_test(test, #test)
#define START_GROUP(group) start_group(#group)
#define END_GROUP() end_group()

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
#include <time.h>
#include <unistd.h>

typedef enum {
  PASSED,
  FAILED,
  TIMEOUT,
} Result;

typedef struct {
  void (*fn)(void);
  const char* name;
  uint8_t name_len;
  int16_t gid;

  pid_t pid;
  int pipefd[2];
  time_t start_time;
} TestCase;

/*
 * Redefinable macros
 */

#ifndef MAX_TESTS
#define MAX_TESTS 128
#endif  // MAX_TESTS

#ifndef MAX_GROUPS
#define MAX_GROUPS 16
#endif  // MAX_GROUPS

#ifndef MAX_TEST_NAME_LEN
#define MAX_TEST_NAME_LEN 48
#endif  // MAX_TEST_NAME_LEN

#ifndef MAX_GROUP_NAME_LEN
#define MAX_GROUP_NAME_LEN 48
#endif  // MAX_GROUP_NAME_LEN

#ifndef MAX_MSG_LEN
#define MAX_MSG_LEN 1024
#endif  // MAX_MSG_LEN

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
 * accept input from cli to run group of tests, or individual tests
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

char groups[MAX_GROUPS][MAX_GROUP_NAME_LEN];
uint16_t num_groups = 0;
int16_t curr_group = -1;

size_t max_len = 0;

static inline uint16_t parse_argv(const int argc, const char** argv);
static inline int string_compare(const void* item, const void* to_compare_to);
static inline int32_t find_index(
    const char* elt,
    int (*comparator_fn)(const void* item, const void* to_compare_to));
static inline void execute_test(TestCase* test);
static inline void get_res_fmt(const Result res,
                               char** result_str,
                               char** ansi_col);
static inline void format_result(char* formatted_result,
                                 const size_t final_space,
                                 const TestCase* curr_test);

int run_tests(const int argc, const char** argv) {
  uint16_t tests_found = parse_argv(argc, argv);
  if (tests_found == 0 && argc > 1)
    return 1;

  if (tests_found > 0)
    num_tests = tests_found;

  size_t final_col =
      max_len + PERIOD_PADDING + SPACE_PADDING + MAX_RESULT_LEN + 1;

  uint16_t tests_passed = 0;
  printf("\n");

  // output, and call the function
  for (uint16_t i = 0; i < num_tests; ++i) {
    TestCase* curr_test = &tests[i];

    if (PRINT_OUTPUT) {
      int pipefd[2];
      if (pipe(pipefd) != 0) {
        perror("pipe");
        return -1;
      }

      curr_test->pipefd[0] = pipefd[0];
      curr_test->pipefd[1] = pipefd[1];
    }

    curr_test->start_time = time(NULL);

    pid_t pid = fork();
    if (pid == -1) {
      perror("fork");
      return -1;
    } else if (pid == 0) {
      execute_test(curr_test);
    } else {
      curr_test->pid = pid;
      if (PRINT_OUTPUT)
        close(curr_test->pipefd[1]);
    }
  }

  // iterate through each test, read from read end of pipe, print requested
  // output
  for (uint16_t i = 0; i < num_tests; ++i) {
    TestCase* curr_test = &tests[i];

    Result result = FAILED;

    // wait for child, killing it if it exceeds MAX_TIME_PER_TEST, and set the
    // result
    int status = 0;
    pid_t r;
    for (;;) {
      r = waitpid(curr_test->pid, &status, WNOHANG);

      if (r == -1) {
        perror("waitpid");
        return -1;
      }

      if (r == curr_test->pid) {
        if (WIFEXITED(status)) {
          if (WEXITSTATUS(status) != 0) {
            result = FAILED;
          } else {
            result = PASSED;
            tests_passed++;
          }
        } else if (WIFSIGNALED(status)) {
          result = FAILED;
        }
        break;
      }

      if (time(NULL) - curr_test->start_time >= MAX_TIME_PER_TEST) {
        kill(curr_test->pid, SIGKILL);
        waitpid(curr_test->pid, &status, 0);
        result = TIMEOUT;
        break;
      }

      usleep(10000);
    }

    char buf[MAX_MSG_LEN];
    ssize_t buf_read_n;
    if (PRINT_OUTPUT) {
      if ((buf_read_n = read(curr_test->pipefd[0], buf, sizeof(buf) - 1)) ==
          -1) {
        perror("read");
        return -1;
      }
      close(curr_test->pipefd[0]);

      if (buf_read_n == -1)
        continue;

      buf[buf_read_n] = '\0';
    }

    if (!PRINT_PASSED && result == PASSED)
      continue;

    char* ansi_col;  // 31 = red, 32 = green, 33 = yellow
    char* result_str;

    get_res_fmt(result, &result_str, &ansi_col);

    // consists of test name, and necessary number of '.' as padding for all
    // results to end on same column
    size_t final_space = final_col - MAX_RESULT_LEN;
    char formatted_result[MAX_FINAL_COL + 1];
    format_result(formatted_result, final_space, curr_test);
    formatted_result[final_space] = '\0';

    // name ... result
    printf("%.*s\x1b[%s%s\x1b[0m\n", (int)final_space, formatted_result,
           ansi_col, result_str);

    if (PRINT_OUTPUT) {
      if (buf_read_n) {
        printf("\x1b[33m> \x1b[0m");
        printf("\x1b[33m%s\x1b[0m", buf);
      }
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

  uint8_t name_len = strnlen(name, MAX_TEST_NAME_LEN);
  if (name_len == MAX_TEST_NAME_LEN) {
    printf("Skipped test [name exceeds maximum length]\n");
    return;
  }

  TestCase* new_test = &tests[num_tests];
  new_test->fn = fn;
  new_test->name = name;
  new_test->name_len = name_len;
  new_test->gid = curr_group;

  num_tests++;
  max_len = (name_len > max_len) ? name_len : max_len;
}

void start_group(const char* group) {
  if (num_groups == MAX_GROUPS) {
    printf("Skipped group [max number of groups reached]\n");
    return;
  }

  if (strnlen(group, MAX_GROUP_NAME_LEN) == MAX_GROUP_NAME_LEN) {
    printf("Skipped group [name exceeds maximum length]\n");
    return;
  }

  curr_group = num_groups + 1;
  memcpy(groups[curr_group - 1], group, MAX_GROUP_NAME_LEN);

  num_groups++;
}

void end_group(void) {
  curr_group = -1;
}

static inline uint16_t parse_argv(const int argc, const char** argv) {
  if (argc <= 1)
    return 0;

  uint16_t tests_found = 0;
  uint16_t left_idx = 0;
  int32_t right_idx;

  for (int i = 1; i < argc; ++i) {
    // [...] denotes a group
    if (argv[i][0] == '[') {
      // TODO
      // find the gid for corresponding argvi[i]
      // modify find_index to accept a starting index, rather than 0? and set it
      // to left_idx while(find_index(gid, gid_compare != -1);
      for (;;) {
        break;
      }
    } else {
      right_idx = find_index(argv[i], string_compare);
    }

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

  return tests_found;
}

static inline int string_compare(const void* item, const void* to_compare_to) {
  return strncmp((const char*)item, (const char*)to_compare_to,
                 MAX_TEST_NAME_LEN);
}

static inline int32_t find_index(
    const char* elt,
    int (*comparator_fn)(const void* item, const void* to_compare_to)) {
  for (uint16_t i = 0; i < num_tests; ++i) {
    if (comparator_fn(elt, tests[i].name) == 0)
      return i;
  }

  return -1;
}

static inline void execute_test(TestCase* test) {
  if (PRINT_OUTPUT) {
    // close read end of pipe and redirect stdout to write end
    close(test->pipefd[0]);
    dup2(test->pipefd[1], STDOUT_FILENO);
    dup2(test->pipefd[1], STDERR_FILENO);
  } else {
    // redirect stdout && stderr to /dev/null so nothing is printed to
    // terminal
    int devnull_fd = open("/dev/null", O_WRONLY);
    dup2(devnull_fd, STDOUT_FILENO);
    dup2(devnull_fd, STDERR_FILENO);
    close(devnull_fd);
  }

  test->fn();

  if (PRINT_OUTPUT)
    close(test->pipefd[1]);

  _exit(0);
}

static inline void get_res_fmt(const Result res,
                               char** result_str,
                               char** ansi_col) {
  switch (res) {
    case PASSED:
      *result_str = "passed";
      *ansi_col = "32m";
      return;

    case FAILED:
      *result_str = "failed";
      *ansi_col = "31m";
      return;

    case TIMEOUT:
      *result_str = "timed out";
      *ansi_col = "31m";
      return;

    default:
      *result_str = "ERROR";
      *ansi_col = "31m";
      return;
  }
}

static inline void format_result(char* formatted_result,
                                 const size_t final_space,
                                 const TestCase* curr_test) {
  for (size_t i = 0; i < final_space; ++i) {
    if (i < curr_test->name_len)
      formatted_result[i] = curr_test->name[i];
    else if (i == curr_test->name_len)
      formatted_result[i] = ' ';
    else if (i == final_space - 1)
      formatted_result[i] = ' ';
    else if (i > curr_test->name_len)
      formatted_result[i] = '.';
  }
}

#endif  // DTEST_IMPL

#endif  // DTEST_H
