# Custom levels and audio

The source layout is `data/Levels/<name>/` for built-ins and
`data/Custom Levels/<name>/` for user examples. The Windows installation and
portable package place `Levels/` and `Custom Levels/` beside `blobby.exe`.
There are no `hd` subdirectories and no level manifests or scripts.

Each folder's name identifies the level in the selection. Built-ins retain
existing saved IDs such as `strand1.bmp`; custom IDs are `custom:<folder name>`.
Renaming/removing a selected folder falls back to Strand 1. Duplicate names
between the two roots remain distinct. Reopen options to discover new folders.

Required image: `background.webp` or `background.png`. WebP takes priority.
File names are recognized without case sensitivity. Artwork keeps its native
resolution: classic 4:3 crops the sides, widescreen shows the wider field.
The net pole must remain centered and aligned with existing artwork. Use a
background from `Levels/` as a reference. Gameplay geometry is never read from
images. Broken selected images recover to the default background.

Optional audio, with `.ogg` preferred over `.wav` for the same base name:

| Base name | Playback | Bus |
|---|---|---|
| `level` | Repeating level music OR ambience | Music/background |
| `victory` | Once on entering the victory celebration | Effects |
| `bums` | Ball impact | Effects |
| `chat` | Chat notification | Effects |
| `pfiff` | Whistle | Effects |

Missing or undecodable audio falls back to the same base name in global
`data/sounds/` (packaged in `sounds.zip`). A valid WAV can also recover from a
broken OGG in the same level. If no usable default exists, that role is silent.
Existing default effects stay as WAV, without unnecessary lossy conversion.
Default tracks are provided as `data/sounds/menu.ogg`, `level.ogg` and
`victory.ogg`. Custom levels can override the level and victory tracks.
Use Ogg Vorbis, not an arbitrary codec in an Ogg container.

`menu.ogg` loops continuously and is shared by menus/options. Browsing level backgrounds,
changing graphics settings or navigating between menus does not restart it.
At match entry, the selected level soundtrack replaces it with a short fade-out
and fade-in. Level music continues during pause and victory; the victory sound
plays over it once. A rematch resets the victory gate. Back to menu restores the
menu track. No audio changes affect physics, replays or network serialization.

Master, effects and music/background volumes multiply together and take effect
immediately. Muting retains the music timeline, so unmuting does not restart it.
There is no separate ambience file/bus: place ambience in `level.ogg`.

SDL2_mixer streams one music decoder and plays predecoded effects on independent
channels. Compressed music bytes remain alive for the decoder; the entire decoded
song is not cached. Cached effects are cleared safely when changing levels, and
bad optional files are reported without aborting the game. Missing audio devices
are nonfatal. SDL/OpenGL rendering share the same audio engine.

Regression tests use a tiny generated Vorbis tone with SDL's dummy audio device.
They cover discovery, old IDs, case-insensitive names, invalid optional content,
OGG overrides, WAV fallback, looping, once-only victory, music continuity, live
bus volumes and mute. This test tone is not a runtime asset.

Music loops inside the decoder, without application restarts or per-loop fades.
Author tracks with matching endpoint waveforms: removing trailing silence alone
cannot prevent a click when the last and first samples differ. The bundled
48-second level and menu tracks have a four-millisecond endpoint correction
(no trimming or silence). Tests capture the actual mixer
output across both bundled track boundaries and reject inserted silence,
restarts and fades, including the 44.1 kHz to 48 kHz resampling path.
