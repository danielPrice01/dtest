#ifndef DTEST_H
#define DTEST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Visible when calling #include "dtest.h" but without #define DTEST_IMPL
 * before
 */

void add_test(void (*fn)(void), const char* name);

void start_group(const char* name);
void end_group(void);
void silence_group(const char* name);

// after every test, call REGISTER_TEST to add it to tests that will be executed
#define REGISTER_TEST(test) add_test(test, #test)
#define START_GROUP(group) start_group(#group)
#define END_GROUP() end_group()
#define SILENCE_GROUP(group) silence_group(#group)

/*
 * Custom assert commands to provide more descriptive error messages on failure
 */

#define DTEST_FAIL(fmt, ...)                                            \
  do {                                                                  \
    printf("%s:%d:%s: failed assertion: " fmt "\n", __FILE__, __LINE__, \
           __func__, ##__VA_ARGS__);                                    \
    exit(1);                                                            \
  } while (0)

/* Base asserts */

#define ASSERT_TRUE(condition)      \
  do {                              \
    if (!(condition))               \
      DTEST_FAIL("%s", #condition); \
  } while (0)

#define ASSERT_BINARY(a, op, b)          \
  do {                                   \
    if (!((a)op(b)))                     \
      DTEST_FAIL("%d %s %d", a, #op, b); \
  } while (0)

#define ASSERT_STR_CMP(a, op, b)         \
  do {                                   \
    if (!(strcmp((a), (b)) op 0))        \
      DTEST_FAIL("%s %s %s", a, #op, b); \
  } while (0)

/* Derived asserts */

#define ASSERT_FALSE(condition) ASSERT_TRUE(!(condition))

#define ASSERT_EQ(a, b) ASSERT_BINARY(a, ==, b)
#define ASSERT_NEQ(a, b) ASSERT_BINARY(a, !=, b)
#define ASSERT_LT(a, b) ASSERT_BINARY(a, <, b)
#define ASSERT_LTE(a, b) ASSERT_BINARY(a, <=, b)
#define ASSERT_GT(a, b) ASSERT_BINARY(a, >, b)
#define ASSERT_GTE(a, b) ASSERT_BINARY(a, >=, b)

#define ASSERT_STR_EQ(a, b) ASSERT_STR_CMP(a, ==, b)
#define ASSERT_STR_NEQ(a, b) ASSERT_STR_CMP(a, !=, b)

#define ASSERT_NULL(ptr) ASSERT_BINARY(ptr, ==, NULL)
#define ASSERT_NOT_NULL(ptr) ASSERT_BINARY(ptr, !=, NULL)

#ifdef DTEST_IMPL

/*
 * Visible when #define DTEST_IMPL included
 */

#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
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
  int32_t gid;

  pid_t pid;
  int pipefd[2];
  time_t start_time;
} TestCase;

typedef struct {
  const char* name;
  uint8_t silenced;
} Group;

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

#if MAX_TEST_NAME_LEN > 255
#error "MAX_TEST_NAME_LEN should not exceed 255 (uint8_t)"
#endif

#ifndef MAX_GROUP_NAME_LEN
#define MAX_GROUP_NAME_LEN 48
#endif  // MAX_GROUP_NAME_LEN

#if MAX_GROUP_NAME_LEN > 255
#error "MAX_GROUP_NAME_LEN should not exceed 255 (uint8_t)"
#endif

#ifndef MAX_MSG_LEN
#define MAX_MSG_LEN 1024
#endif  // MAX_MSG_LEN

/* in seconds */
#ifndef MAX_TIME_PER_TEST
#define MAX_TIME_PER_TEST 1
#endif  // MAX_TIME_PER_TEST

#ifndef PRINT_OUTPUT
#define PRINT_OUTPUT 1
#endif  // PRINT_OUTPUT

#ifndef PRINT_PASSED
#define PRINT_PASSED 0
#endif  // PRINT_PASSED

#ifndef PRINT_GROUPS_SILENCED
#define PRINT_GROUPS_SILENCED 1
#endif  // PRINT_GROUPS_SILENCED

// returns 0 when all tests passed, or 1 otherwise
int run_tests(const int argc, const char** argv);

/*
 * Fixed macros
 */

/*
 * in main call RUN_TESTS with either no arguments or with (argc, argv) to
 * accept input from cli to run group of tests, or individual tests
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

Group groups[MAX_GROUPS];
uint16_t num_groups = 0;
int16_t curr_group = -1;

uint16_t silenced_tests = 0;

size_t max_len = 0;

static inline uint16_t parse_argv(const int argc, const char** argv);
static inline int32_t string_compare(const void* item,
                                     const void* to_compare_to,
                                     size_t max_length);
static inline int32_t gid_compare(const void* gid,
                                  const void* test,
                                  size_t null);
static inline int32_t find_gid(const char* group_name, size_t group_name_len);
static inline int32_t find_index(uint16_t starting_index,
                                 uint8_t is_group,
                                 const void* elt,
                                 int (*comparator_fn)(const void* item,
                                                      const void* to_compare_to,
                                                      size_t max_length));
static inline int8_t swap_tests(uint16_t* left_idx,
                                int32_t* right_idx,
                                uint16_t* tests_found);

static inline void drain_pipe(int fd, char* buf, size_t* buf_len);
static inline void execute_test(TestCase* test);
static inline void get_res_fmt(const Result res,
                               char** result_str,
                               char** ansi_col);
static inline void format_result(char* formatted_result,
                                 const size_t final_space,
                                 const TestCase* curr_test);

int run_tests(const int argc, const char** argv) {
  silenced_tests = 0;

  uint16_t tests_found = parse_argv(argc, argv);
  if (tests_found == 0 && argc > 1)
    return 1;

  if (tests_found > 0)
    num_tests = tests_found;

  if (num_tests == 0)
    return 0;

  size_t final_col =
      max_len + PERIOD_PADDING + SPACE_PADDING + MAX_RESULT_LEN + 1;

  uint16_t tests_passed = 0;
  printf("\n");

  // output, and call the function
  for (uint16_t i = 0; i < num_tests; ++i) {
    TestCase* curr_test = &tests[i];

    if (curr_test->gid != -1 && groups[curr_test->gid].silenced) {
      silenced_tests++;
      continue;
    }

    if (PRINT_OUTPUT) {
      int pipefd[2];
      if (pipe(pipefd) != 0) {
        perror("pipe");
        return -1;
      }

      curr_test->pipefd[0] = pipefd[0];
      curr_test->pipefd[1] = pipefd[1];
      fcntl(curr_test->pipefd[0], F_SETFL, O_NONBLOCK);
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
    if (curr_test->gid != -1 && groups[curr_test->gid].silenced)
      continue;

    Result result = FAILED;

    char buf[MAX_MSG_LEN];
    size_t buf_len = 0;

    // wait for child, killing it if it exceeds MAX_TIME_PER_TEST, and set the
    // result
    int status = 0;
    pid_t r;
    for (;;) {
      if (PRINT_OUTPUT)
        drain_pipe(curr_test->pipefd[0], buf, &buf_len);

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

        if (PRINT_OUTPUT)
          drain_pipe(curr_test->pipefd[0], buf, &buf_len);

        result = TIMEOUT;
        break;
      }

      usleep(10000);
    }

    if (!PRINT_PASSED && result == PASSED) {
      if (PRINT_OUTPUT)
        close(curr_test->pipefd[0]);
      continue;
    }

    if (PRINT_OUTPUT) {
      close(curr_test->pipefd[0]);
      buf[buf_len] = '\0';
    }

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

    if (PRINT_OUTPUT && buf_len) {
      printf("\x1b[33m> \x1b[0m");
      printf("\x1b[33m%s\x1b[0m", buf);
    }
  }

  printf("\nTests passed: [%u/%u]\n\n", tests_passed,
         num_tests - silenced_tests);

  return (tests_passed == (num_tests - silenced_tests)) ? 0 : 1;
}

void add_test(void (*fn)(void), const char* name) {
  if (num_tests == MAX_TESTS) {
    printf("Skipped test %s [maximum number of tests exceeded]\n", name);
    return;
  }

  uint8_t name_len = strnlen(name, MAX_TEST_NAME_LEN);
  if (name_len == MAX_TEST_NAME_LEN) {
    printf("Skipped test %s [name exceeds maximum length]\n", name);
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

void start_group(const char* name) {
  curr_group = -1;

  if (num_groups == MAX_GROUPS) {
    printf("Skipped group %s [maximum number of groups exceeded]\n", name);
    return;
  }

  if (strnlen(name, MAX_GROUP_NAME_LEN) == MAX_GROUP_NAME_LEN) {
    printf("Skipped group %s [name exceeds maximum length]\n", name);
    return;
  }

  curr_group = num_groups;

  Group* new_group = &groups[curr_group];
  new_group->name = name;
  new_group->silenced = 0;

  num_groups++;
}

void end_group(void) {
  curr_group = -1;
}

void silence_group(const char* name) {
  for (uint16_t i = 0; i < num_groups; ++i) {
    if (string_compare(name, groups[i].name, MAX_GROUP_NAME_LEN) == 0) {
      groups[i].silenced = 1;

      if (PRINT_GROUPS_SILENCED)
        printf("Group [%s] silenced\n", name);
    }
  }
}

static inline uint16_t parse_argv(const int argc, const char** argv) {
  if (argc <= 1)
    return 0;

  uint16_t tests_found = 0;
  uint16_t left_idx = 0;
  int32_t right_idx;

  for (int i = 1; i < argc; ++i) {
    // [...] denotes a group
    size_t arg_len = strnlen(argv[i], MAX_GROUP_NAME_LEN);
    if (arg_len >= 2 && argv[i][0] == '[' && argv[i][arg_len - 1] == ']') {
      int32_t gid = find_gid(argv[i], arg_len);
      if (gid == -1) {
        printf("Group %s not found\n", argv[i]);
        continue;
      }

      groups[gid].silenced = 0;

      while ((right_idx = find_index(left_idx, 1, &gid, gid_compare)) != -1) {
        swap_tests(&left_idx, &right_idx, &tests_found);
      }
    } else {
      right_idx = find_index(0, 0, argv[i], string_compare);
      if (swap_tests(&left_idx, &right_idx, &tests_found) == -1) {
        printf("Test '%s' not found\n", argv[i]);
        continue;
      }

      // unsilence group the test is part of
      int32_t gid = tests[left_idx - 1].gid;
      if (gid != -1 && groups[gid].silenced)
        groups[gid].silenced = 0;
    }
  }

  return tests_found;
}

// returns 0 when true, to match string_compare
static inline int32_t gid_compare(const void* gid,
                                  const void* test,
                                  size_t null) {
  (void)null;
  return *(int32_t*)gid != ((TestCase*)test)->gid;
}

static inline int32_t string_compare(const void* item,
                                     const void* to_compare_to,
                                     size_t max_length) {
  return strncmp((const char*)item, (const char*)to_compare_to, max_length);
}

static inline int32_t find_gid(const char* group_name, size_t group_name_len) {
  if (group_name_len <= 2)
    return -1;

  for (uint16_t i = 0; i < num_groups; ++i) {
    // reject groups that are a prefix of requested group
    if (string_compare(group_name + 1, groups[i].name, group_name_len - 2) ==
            0 &&
        groups[i].name[group_name_len - 2] == '\0')
      return i;
  }

  return -1;
}

static inline int32_t find_index(uint16_t starting_index,
                                 uint8_t is_group,
                                 const void* elt,
                                 int (*comparator_fn)(const void* item,
                                                      const void* to_compare_to,
                                                      size_t max_length)) {
  for (uint16_t i = starting_index; i < num_tests; ++i) {
    const void* arg =
        is_group ? (const void*)&tests[i] : (const void*)tests[i].name;

    if (comparator_fn(elt, arg, MAX_TEST_NAME_LEN) == 0)
      return i;
  }

  return -1;
}

static inline int8_t swap_tests(uint16_t* left_idx,
                                int32_t* right_idx,
                                uint16_t* tests_found) {
  if (*right_idx == -1) {
    return -1;
  } else if (*right_idx > *left_idx) {
    TestCase tmp = tests[*right_idx];
    tests[*right_idx] = tests[*left_idx];
    tests[*left_idx] = tmp;
  }

  (*left_idx)++;
  (*tests_found)++;
  return 0;
}

static inline void drain_pipe(int fd, char* buf, size_t* buf_len) {
  char tmp[4096];
  ssize_t n;

  while ((n = read(fd, tmp, sizeof(tmp))) > 0) {
    size_t space = (MAX_MSG_LEN - 1) - *buf_len;
    if (space > 0) {
      size_t to_copy = ((size_t)n < space) ? (size_t)n : space;
      memcpy(buf + *buf_len, tmp, to_copy);
      *buf_len += to_copy;
    }
  }
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

#endif  // DTEST_IMPL

#endif  // DTEST_H
