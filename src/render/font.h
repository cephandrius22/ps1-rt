/*
 * font.h — 5x7 pixel text for on-screen messages.
 *
 * Each glyph is 7 rows of 5 bits (MSB = left pixel), e.g. 'A':
 *
 *   .###.  0x0E
 *   #...#  0x11
 *   #...#  0x11
 *   #####  0x1F
 *   #...#  0x11  (x3)
 *
 * Covers A-Z (lowercase drawn as uppercase), 0-9 and . , ! ? ' - : / ( ) < > %.
 * Anything else draws as a space. The HUD's big numbers and the debug
 * lines keep their compact 3x5 font in overlay.c.
 */
#ifndef FONT_H
#define FONT_H

#include "render.h"

#define FONT_COLS 5
#define FONT_ROWS 7
#define FONT_GAP  1  /* font pixels between glyphs */

/* Width in screen pixels of text drawn at scale */
int font_width(const char *text, int scale);

/* Draw text with its top-left at (x, y); off-screen pixels are clipped */
void font_draw(Framebuffer *fb, const char *text, int x, int y, int scale, uint32_t color);

#endif
