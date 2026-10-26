#include "event.h"

void events_clear(EventQueue *q) {
    q->count = 0;
}

void events_push(EventQueue *q, GameEvent ev) {
    if (q->count >= EVENT_QUEUE_MAX) {
        return;
    }
    q->items[q->count++] = ev;
}
