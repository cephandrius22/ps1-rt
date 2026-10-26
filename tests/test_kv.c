#include "test_framework.h"
#include "kv.h"

TEST(test_kv_float_found) {
    ASSERT_NEAR(kv_float("entity door 0 0 -5 hinge_x=2.5", "hinge_x", 0), 2.5f, 1e-6f);
}

TEST(test_kv_float_default) {
    ASSERT_NEAR(kv_float("entity door 0 0 -5", "hinge_x", 7.0f), 7.0f, 1e-6f);
}

TEST(test_kv_int) {
    ASSERT_EQ_INT(kv_int("entity health 0 0 0 amount=50 mat=7", "mat", 0), 7);
    ASSERT_EQ_INT(kv_int("entity health 0 0 0", "mat", 3), 3);
}

int main(void) {
    TEST_SUITE("kv");
    RUN_TEST(test_kv_float_found);
    RUN_TEST(test_kv_float_default);
    RUN_TEST(test_kv_int);
    TEST_REPORT();
}
