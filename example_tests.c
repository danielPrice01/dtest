#define DTEST_IMPL
#define MAX_TIME_PER_TEST 2  // redefine to change timeout length
#define PRINT_PASSED 1
#define PRINT_OUTPUT 1
#define PRINT_GROUPS_SILENCED 0

#include "dtest.h"

void test_addition_true(void) {
  ASSERT_TRUE(1 + 1 == 2);
}
void test_addition_false(void) {
  ASSERT_EQ(1 + 1, 3);
}
void test_multiplication_true(void) {
  ASSERT_EQ(2 * 2, 4);
}
void test_multiplication_false(void) {
  ASSERT_TRUE(2 * 2 == 5);
}
void test_strings_equal_true(void) {
  ASSERT_STR_EQ("hello", "hello");
}
void test_strings_equal_false(void) {
  ASSERT_STR_EQ("hello", "world");
}
void test_runs_within_timeout(void) {
  sleep(1);
}
void test_runs_beyond_timeout(void) {
  sleep(100);
}

int main(int argc, char** argv) {
  START_GROUP(addition);
  REGISTER_TEST(test_addition_true);
  REGISTER_TEST(test_addition_false);

  START_GROUP(multiplication);
  REGISTER_TEST(test_multiplication_true);
  REGISTER_TEST(test_multiplication_false);

  START_GROUP(string);
  REGISTER_TEST(test_strings_equal_true);
  REGISTER_TEST(test_strings_equal_false);

  START_GROUP(sleep);
  REGISTER_TEST(test_runs_within_timeout);
  REGISTER_TEST(test_runs_beyond_timeout);

  SILENCE_GROUP(addition);

  // RUN_TESTS(); // THIS WORKS AS WELL
  RUN_TESTS(argc, argv);
}
