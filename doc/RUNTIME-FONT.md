# Runtime Nunito font

All interface text, including the legacy network/lobby/replay/credits screens,
is drawn from `gfx/hd/Nunito.ttf` (Nunito SemiBold, OFL).
There are no runtime `fontNN.bmp`, `fontNN.png`, or menu glyph PNG dependencies.
The logo/title remains artwork.

`RuntimeFont.cpp` uses the vendored stb_truetype rasterizer (license at the end
of `src/third_party/stb_truetype.h`). It only reads the bundled font.
Each renderer uploads a white alpha atlas and tints glyphs at draw time.
Atlas sizes follow the physical viewport height, including window resizing and
monitor changes. Four-pixel buckets reduce regeneration; up to 16 atlases are
retained per renderer. Texture filtering is linear with transparent white
gutters. Latin and Latin Extended-A characters (through U+017F), UTF-8 decoding,
and password masking are supported. Unsupported characters use a question mark.

New menu layouts use proportional advances from the TTF. Legacy widgets retain
their fixed character cells to preserve their alignment and interaction areas.
The checked-in `data/gfx/hd/Nunito.ttf` is the ready-to-use SemiBold font.
Its OFL notice is `data/gfx/hd/menu-font-license.txt`. No font generation
script or fontTools installation is needed for builds or gameplay.

Tests rasterize accented glyphs at 24/64/144 physical pixels and check bounds
and antialiasing. The hardware smoke suite captures both SDL and OpenGL menus,
including the 4K panel/text captures and legacy text samples.
