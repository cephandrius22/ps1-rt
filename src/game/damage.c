#include "damage.h"

DamageResult damage_entity(Scene *s, int index, float amount, EventQueue *ev) {
    DamageResult res = entity_damage(&s->entities, index, amount);
    if (res == DAMAGE_IGNORED) {
        return res;
    }

    const Entity *e = &s->entities.items[index];
    GameEventType type = res == DAMAGE_KILLED ? EV_ENEMY_DIED : EV_ENEMY_PAIN;
    events_push(ev, (GameEvent){.type = type, .pos = e->position,
                                .entity = index, .amount = amount});
    return res;
}
