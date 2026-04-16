# dtest

Simple, one header file, C unit test case framework.

---

## Usage:
define macro ```DTEST_IMPL``` at the top of the file, then include ```dtest.h```. Create your test cases, and in main call the macro ```REGISTER_TEST(test)``` on each of your tests. Finally, call ```RUN_TESTS``` with optional additions ```_VERBOSE | _ERROR``` for increased debug output.

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
    RUN_TESTS_ERROR
    return 0;
}
```
