/*
 * kv.h — Read optional key=value pairs from a text line.
 *
 * Example: kv_float("entity ghoul 0 0 -5 health=40", "health", 60) → 40.0
 * Missing keys return the default.
 */
#ifndef KV_H
#define KV_H

#include <stdbool.h>
#include <stddef.h>

/* Number after "key=", or def if the key is absent */
float kv_float(const char *line, const char *key, float def);
int   kv_int(const char *line, const char *key, int def);

/* Copy the value (up to the next space) into out; false if key is absent.
 * A value in double quotes may hold spaces (quotes are dropped).
 * Example: kv_string("... mat=rust_wall", "mat", buf, 32) → "rust_wall"
 *          kv_string("message=\"It's open\"", "message", ...) → "It's open" */
bool  kv_string(const char *line, const char *key, char *out, size_t size);

#endif
