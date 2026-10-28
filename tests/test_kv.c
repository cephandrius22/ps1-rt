#include "test_framework.h"
#include "kv.h"
#include <string.h>

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

TEST(test_kv_string) {
    char buf[16];
    ASSERT(kv_string("entity ghoul 0 0 0 mat=rust_wall health=40", "mat", buf, sizeof(buf)));
    ASSERT(strcmp(buf, "rust_wall") == 0);
    ASSERT(!kv_string("entity ghoul 0 0 0", "mat", buf, sizeof(buf)));

    /* Truncated to fit */
    char tiny[4];
    ASSERT(kv_string("x mat=abcdefgh", "mat", tiny, sizeof(tiny)));
    ASSERT(strcmp(tiny, "abc") == 0);

    /* Quoted: spaces kept, quotes dropped */
    char msg[32];
    ASSERT(kv_string("brush trigger_once message=\"It's open\" wait=1", "message", msg, sizeof(msg)));
    ASSERT(strcmp(msg, "It's open") == 0);
}

int main(void) {
    TEST_SUITE("kv");
    RUN_TEST(test_kv_float_found);
    RUN_TEST(test_kv_float_default);
    RUN_TEST(test_kv_int);
    RUN_TEST(test_kv_string);
    TEST_REPORT();
}
