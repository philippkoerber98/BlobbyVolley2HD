# HD asset layout

Runtime artwork lives under `data/gfx` and `data/Levels/<level>/background.webp`.
WebP carries the image and alpha channel. PNG remains a supported input format
when there is no corresponding WebP. The build includes only the preferred file.

The five `blobbym` sprites represent the blob animation. `volleyball.webp`
provides the continuously rotating ball panels; lighting is applied separately.
`sch11` through `sch15` and `schball` are ground shadows. The background's
center pole is composited above moving ground shadows.

Historical `.bmp` strings in code and preferences are logical names. They map
to the HD image and dimensions from `gfx/metrics.txt`; they do not refer to
packaged legacy artwork. Backgrounds retain native resolution, and the renderer
crops the sides for the classic 4:3 view instead of squeezing them.

The UI uses `Nunito.ttf` at runtime. Its OFL notice is `menu-font-license.txt`.
No bitmap glyph files are required. `win/icon-hd.ico` is the checked-in Windows
resource icon used by `win/res.rc`; it is not regenerated during a normal build.

See `RUNTIME-FONT.md`, `DISPLAY.md`, and `ARCHITECTURE.md` for implementation details.

`champion.webp` is a separate native-resolution victory pose, with a soft blob
arm gripping the trophy. Its magenta body/hand material is recolored at runtime
using the winner's RGB color, including color cycling; neutral highlights and
the gold/silver trophy remain unchanged. Keep this source lossless and preserve
its alpha channel and material key when replacing it. The shared RGBA texture
path caches the colored sprite until the selected color changes and draws it
with linear filtering in both renderers. It does not affect gameplay sprites.
