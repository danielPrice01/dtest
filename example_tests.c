#include <assert.h>

#define DTEST_IMPL
#define MAX_TIME_PER_TEST 2  // redefine to change timeout length

#include "dtest.h"

void test_addition_true(void) {
  assert(1 + 1 == 2);
}
void test_addition_false(void) {
  assert(1 + 1 == 3);
}
void test_multiplication_true(void) {
  assert(2 * 2 == 4);
}
void test_multiplication_false(void) {
  assert(2 * 2 == 5);
}
void test_runs_within_timeout(void) {
  sleep(1);
}
void test_runs_beyond_timeout(void) {
  sleep(100);
}

int main(void) {
  REGISTER_TEST(test_addition_true);
  REGISTER_TEST(test_addition_false);
  REGISTER_TEST(test_multiplication_true);
  REGISTER_TEST(test_multiplication_false);
  REGISTER_TEST(test_runs_within_timeout);
  REGISTER_TEST(test_runs_beyond_timeout);

  // RUN_TESTS;
  RUN_TESTS_ERROR;
  // RUN_TESTS_VERBOSE;
}
