#include "test_framework.h"
#include "event.h"

TEST(test_event_push_and_clear) {
    EventQueue q = {0};
    events_push(&q, (GameEvent){.type = EV_FOOTSTEP});
    events_push(&q, (GameEvent){.type = EV_PICKUP, .amount = 25.0f});

    ASSERT(q.count == 2);
    ASSERT(q.items[0].type == EV_FOOTSTEP);
    ASSERT(q.items[1].type == EV_PICKUP);
    ASSERT_NEAR(q.items[1].amount, 25.0f, 1e-6f);

    events_clear(&q);
    ASSERT(q.count == 0);
}

TEST(test_event_overflow_drops_newest) {
    EventQueue q = {0};
    for (int i = 0; i < EVENT_QUEUE_MAX; i++) {
        events_push(&q, (GameEvent){.type = EV_FOOTSTEP});
    }
    events_push(&q, (GameEvent){.type = EV_PICKUP});

    ASSERT(q.count == EVENT_QUEUE_MAX);
    ASSERT(q.items[EVENT_QUEUE_MAX - 1].type == EV_FOOTSTEP);
}

int main(void) {
    TEST_SUITE("event");
    RUN_TEST(test_event_push_and_clear);
    RUN_TEST(test_event_overflow_drops_newest);
    TEST_REPORT();
}
