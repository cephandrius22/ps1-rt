/*
 * kv.h — Read optional key=value pairs from a text line.
 *
 * Example: kv_float("entity door 0 0 -5 hinge_x=2", "hinge_x", 0) → 2.0
 * Missing keys return the default.
 */
#ifndef KV_H
#define KV_H

float kv_float(const char *line, const char *key, float def);
int   kv_int(const char *line, const char *key, int def);

#endif
