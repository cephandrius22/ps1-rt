/*
 * kv.h — Read optional key=value pairs from a text line.
 *
 * Example: kv_float("entity door 0 0 -5 hinge_x=2", "hinge_x", 0) → 2.0
 * Missing keys return the default.
 */
#ifndef KV_H
#define KV_H

#include <stdbool.h>
#include <stddef.h>

float kv_float(const char *line, const char *key, float def);
int   kv_int(const char *line, const char *key, int def);

/* Copy the value (up to the next space) into out; false if key is absent.
 * Example: kv_string("... mat=rust_wall", "mat", buf, 32) → "rust_wall" */
bool  kv_string(const char *line, const char *key, char *out, size_t size);

#endif
