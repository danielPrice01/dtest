# dtest

Simple, fast, one header file, C unit test case framework. Zero heap allocations.

---

## Usage:
Only works on Unix systems.

1. Define macro ```DTEST_IMPL``` at the top of the file
2. Add ```#include dtest.h``` right after.
3. Create your test cases using any of the assert macros listed below, and inside main use the macro ```REGISTER_TEST(test)``` on each of your tests.

    a. You can create the tests in a separate file by including ```dtest.h``` without defining ```DTEST_IMPL```.

    b. You can put the tests into test groups using the macros ```START_GROUP(group)``` and ```END_GROUP()```.
   
5. Finally, call ```RUN_TESTS()``` or ```RUN_TESTS(argc, argv)```. If argc and argv are provided, user can run only a select number of tests -- or group of tests -- by providing ```test_name``` or ```[group_name]``` as an argument to the program.

[See example file](example_tests.c)

---
## Provided Asserts:
### True and False
- ```ASSERT_TRUE(condition)```
- ```ASSERT_FALSE(condition)```
### Comparisons
- ```ASSERT_EQ(a, b)```
- ```ASSERT_NEQ(a, b)```
- ```ASSERT_LT(a, b)```
- ```ASSERT_LTE(a, b)```
- ```ASSERT_GT(a, b)```
- ```ASSERT_GTE(a, b)```
### String
- ```ASSERT_STR_EQ(a, b)```
- ```ASSERT_STR_NEQ(a, b)```

---

## Optional Setting Modifications:
Before ```#include "dtest.h"``` and after ```#define DTEST_IMPL```, you can redefine:
- ```MAX_TESTS```: default 128
- ```MAX_GROUPS```: default 16
- ```MAX_TEST_NAME_LEN```: default 48 bytes
- ```MAX_GROUP_NAME_LEN```: default 48 bytes
- ```MAX_MSG_LEN```: default 1024 bytes
- ```MAX_TIME_PER_TEST```: default 1s
- ```PRINT_OUTPUT```: default 1 (true)
- ```PRINT_PASSED```: default 0 (false)

---

## Examples:

### Tests all contained in one file:
```C
#define DTEST_IMPL
#include "dtest.h"

void foo_test(void) { ASSERT(...) }
void bar_test(void) { ASSERT(...) }

int main(void) {
    REGISTER_TEST(foo_test);
    REGISTER_TEST(bar_test);
    RUN_TESTS();
}
```

### Tests contained in multiple files:
```C
// example_tests.h
#include "dtest.h"

void test_addition_true(void) {
    ASSERT_EQ(1 + 1, 2);
}

void test_runs_beyond_timeout(void) {
    sleep(100);
}
```

```C
// main.c
#include "example_tests.h"

#define DTEST_IMPL
#include "dtest.h"

int main(int argc, char** argv) {
  REGISTER_TEST(test_addition_true);
  REGISTER_TEST(test_runs_beyond_timeout);

  RUN_TESTS(argc, argv);
}
```
