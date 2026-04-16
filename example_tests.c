#include <assert.h>

#define DTEST_IMPL

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

int main(void) {
  REGISTER_TEST(test_addition_true);
  REGISTER_TEST(test_addition_false);
  REGISTER_TEST(test_multiplication_true);
  REGISTER_TEST(test_multiplication_false);

  // RUN_TESTS;
  RUN_TESTS_ERROR
  // RUN_TESTS_VERBOSE;

  return 0;
}
