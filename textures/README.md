# textures/

Hand-made textures, as uncompressed 24- or 32-bit TGA files.

A material or `.map` face naming a texture that isn't procedural looks
here: `crate` → `textures/crate.tga`. A `.level` material can also give a
path directly: `material sign art/sign.tga 1 1 1`.

`make editor` copies these next to the procedural textures, so TrenchBroom
lists them under `rt/`. Texture size sets UV scale in `.map` files, as in
the editor (64 px = 2 m). Magenta (255, 0, 255) is transparent on sprites.
