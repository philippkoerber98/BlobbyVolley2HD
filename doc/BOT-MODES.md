# Per-bot classic and widescreen modes

Bot selection and strength remain unchanged. ScriptedInputSource selects the
original scripts/<name>.lua for 4:3 and scripts/wide/<name>.lua for 16:9.
This applies to all six selectable bots: axji-0-2, com_11, gintonicV9, hyp014,
reduced and Union. There is no shared replacement bot and no separate selector.
All original source files and the shared classic api.lua/bot_api.lua are intact.

Wide prediction worlds use the real court width (1066.6667), speed (5.4), net,
wall collisions and physical radii. There is no coordinate compression. Mirroring
uses each match's width. Difficulty errors are cleared while a wide serve is
being aligned; the original classic error/delay logic is preserved.

The individual adapted sources retain their respective tactics:
- axji-0-2: field-relative waiting and defensive positions.
- com_11: field-relative home positions and native prediction.
- gintonicV9: spawn-relative serve choices and width-aware net/defensive logic.
- hyp014: real movement grid, native net limits/attack targets, no impossible
  serve grid alignment or sub-step jump tolerance.
- reduced: real movement timing, width-relative waiting positions.
- Union: native serve/spawn comparisons, reach tolerance and net-hit bounds.

The wide-only scripts/wide/api.lua handles dead-ball recovery without jumping
and rescues a stalled serve after 240 simulation ticks. Normal rally tactics
remain the chosen bot's own OnGame. This helper is never loaded in classic mode.
The original copyright/license comments are retained in every adapted source.

Tests cover every bot serving from both sides on both geometries, including
non-grid starting positions. Six 4500-tick wide bot-versus-bot sessions check
continued serve progress, contacts and actual net crossings, not just finite
simulation values. Shared SDL/OpenGL, replay and frame-timing regressions run too.
