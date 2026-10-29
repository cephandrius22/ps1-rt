#include "damage.h"
#include "ai.h"
#include <math.h>

#define KNOCK_PER_DAMAGE 0.07f /* m/s of shove per point: pistol ~2.4, full shotgun ~7 */
#define KNOCK_MAX        8.0f  /* m/s: rockets don't fling monsters across the map */

/* Shove a monster flat away from `from`, adding to any shove in progress */
static void knock_back(Entity *e, float amount, Vec3 from) {
    Vec3 away = vec3(e->position.x - from.x, 0, e->position.z - from.z);
    float len = vec3_length(away);
    if (len < 1e-4f) {
        return;
    }
    Vec3 knock = vec3_add(e->enemy.knock, vec3_mul(away, amount * KNOCK_PER_DAMAGE / len));
    float speed = vec3_length(knock);
    e->enemy.knock = speed > KNOCK_MAX ? vec3_mul(knock, KNOCK_MAX / speed) : knock;
}

DamageResult damage_entity(Scene *s, int index, float amount, Vec3 from, EventQueue *ev) {
    DamageResult res = entity_damage(&s->entities, index, amount);
    if (res == DAMAGE_IGNORED) {
        return res;
    }

    Entity *e = &s->entities.items[index];
    if (enemy_def(e->type)) {
        knock_back(e, amount, from);
    }
    GameEventType type = res == DAMAGE_KILLED ? EV_ENEMY_DIED : EV_ENEMY_PAIN;
    events_push(ev, (GameEvent){.type = type, .pos = e->position,
                                .entity = index, .amount = amount});
    return res;
}
