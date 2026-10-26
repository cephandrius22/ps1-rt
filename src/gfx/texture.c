#include "texture.h"
#include <stdio.h>
#include <stdlib.h>

#pragma pack(push, 1)
typedef struct {
    uint8_t  id_length;
    uint8_t  color_map_type;
    uint8_t  image_type;
    uint16_t cm_first_entry;
    uint16_t cm_length;
    uint8_t  cm_entry_size;
    uint16_t x_origin;
    uint16_t y_origin;
    uint16_t width;
    uint16_t height;
    uint8_t  bpp;
    uint8_t  descriptor;
} TGAHeader;
#pragma pack(pop)

int texture_load_tga(Texture *tex, const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open TGA: %s\n", filename);
        return -1;
    }

    TGAHeader hdr;
    fread(&hdr, sizeof(TGAHeader), 1, f);

    if (hdr.image_type != 2) {
        fprintf(stderr, "Only uncompressed true-color TGA supported (type %d)\n", hdr.image_type);
        fclose(f);
        return -1;
    }

    if (hdr.bpp != 24 && hdr.bpp != 32) {
        fprintf(stderr, "Only 24/32-bit TGA supported (got %d)\n", hdr.bpp);
        fclose(f);
        return -1;
    }

    // Skip ID field
    if (hdr.id_length > 0) fseek(f, hdr.id_length, SEEK_CUR);

    tex->width = hdr.width;
    tex->height = hdr.height;
    int channels = hdr.bpp / 8;

    tex->pixels = malloc(tex->width * tex->height * 3);

    int top_to_bottom = (hdr.descriptor >> 5) & 1;

    for (int y = 0; y < tex->height; y++) {
        int dst_y = top_to_bottom ? y : (tex->height - 1 - y);
        uint8_t *row = tex->pixels + dst_y * tex->width * 3;

        for (int x = 0; x < tex->width; x++) {
            uint8_t bgr[4];
            fread(bgr, channels, 1, f);
            row[x * 3 + 0] = bgr[2]; // R
            row[x * 3 + 1] = bgr[1]; // G
            row[x * 3 + 2] = bgr[0]; // B
        }
    }

    fclose(f);
    printf("Loaded TGA '%s': %dx%d\n", filename, tex->width, tex->height);
    return 0;
}

void texture_free(Texture *tex) {
    free(tex->pixels);
    tex->pixels = NULL;
}

Vec3 texture_sample(const Texture *tex, float u, float v) {
    if (!tex->pixels) return vec3(1, 0, 1); // magenta = missing

    // Wrap UVs
    u = u - (float)(int)u;
    v = v - (float)(int)v;
    if (u < 0) u += 1.0f;
    if (v < 0) v += 1.0f;

    // Nearest-neighbor (PS1 style)
    int px = (int)(u * tex->width) % tex->width;
    int py = (int)(v * tex->height) % tex->height;
    if (px < 0) px += tex->width;
    if (py < 0) py += tex->height;

    const uint8_t *p = tex->pixels + (py * tex->width + px) * 3;
    return vec3(p[0] / 255.0f, p[1] / 255.0f, p[2] / 255.0f);
}
