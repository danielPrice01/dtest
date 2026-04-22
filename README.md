# dtest

Simple, fast, one header file, C unit test case framework.

---

## Usage:
define macro ```DTEST_IMPL``` at the top of the file, then include ```dtest.h```. Create your test cases, and in main call the macro ```REGISTER_TEST(test)``` on each of your tests. Finally, call ```RUN_TESTS()```, which can optionally accept arguments ```(int argc, char** argv)```.

optional setting modifications: before ```#include "dtest.h"``` and after ```#define DTEST_IMPL```, you can redefine:
- ```MAX_TESTS```: default 128
- ```MAX_MSG_LEN```: default 1024 bytes
- ```MAX_TEST_NAME_LEN```: default 48 bytes
- ```MAX_TIME_PER_TEST```: default 1s
- ```PRINT_OUTPUT```: default 1 (true)
- ```PRINT_PASSED```: default 0 (false)

[See example file](example_tests.c)

---

### Example:
```C
#define DTEST_IMPL
#include "dtest.h"

void foo_test(void);
void bar_test(void);

int main(void) {
    REGISTER_TEST(foo_test);
    REGISTER_TEST(bar_test);
    RUN_TESTS();
}
```
