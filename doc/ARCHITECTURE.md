# Code map

## Entry point and application

- `src/main.cpp`: startup, filesystem mounting, configuration, and shutdown.
- `src/ApplicationLoop.*`: unchanged fixed-step input/simulation/render loop.
- `src/BlobbyApp.*`: owns input, rendering, sound, the current state, and monitor settings.
- `src/FrameTiming.h`: fixed simulation clock; rendering never advances physics.
- `src/DisplaySettings.h` / `DisplayViewport.h`: supported display settings and aspect-preserving viewports.

## Match and simulation

- `DuelMatch.*`: combines rules, inputs, and physics for one match.
- `PhysicWorld.*` / `PhysicState.*`: deterministic simulation and its snapshots.
- `GameLogic.*` / `GameLogicState.*`: scoring, serving, and rule state.
- `CourtGeometry.h`: classic and wide court dimensions; network matches stay classic.
- `ScriptedInputSource.*`, `IScriptableComponent.*`, and `data/scripts/wide`: bot behavior and Lua API.
- `replays/`, `server/`, and `blobnet/`: replay and multiplayer protocols. Preserve serialization when refactoring.

## LAN tournaments

- `server/Tournament.*`: pure competition model, formats, brackets, standings,
  preview, results and versioned public/private snapshots.
- `server/TournamentHost.*`: authoritative coordination on the server thread,
  organizer authorization, identity, readiness, scheduling, persistence and live viewers.
- `TournamentProtocol.h`: bounded additive tournament messages; existing match
  and replay state serialization is unchanged.
- `state/TournamentState.*`: shared client connection, tournament tabs and passive
  spectator presentation. Network matches retain the connection when returning.
- `NetworkGame::view()`: locked read-only match inspection for the coordinator;
  its atomics/mutex also protect shutdown and checkpoint restoration.

See [LAN tournaments](LAN-TOURNAMENTS.md) for behavior and verification.

## Presentation

- `RenderManager.*`: shared surface operations and renderer interface.
- `RenderManagerAssets.cpp`: HD image decoding, logical names, and cached sprite dimensions.
- `RenderManagerText.cpp`: runtime glyph atlas selection and text layout.
- `RenderManagerSDL.*` / `RenderManagerGL2D.*`: the two drawing backends.
- `RuntimeFont.*`: Nunito outline rasterization; the TTF and its license are under `data/gfx`.
- `AssetTransforms.h`: image processing, crop geometry, ball rotation, and shadow/pole layering.
- `ImagePaths.h`: prefers WebP, with PNG supported when no WebP exists.
- `LevelCatalog.*` / `BackgroundSettings.h`: folder discovery, saved background IDs,
  image selection, and per-role sound fallback.
- `SoundManager.*`: SDL2_mixer Vorbis/WAV effects, streamed music, audio buses and scene transitions.
- `IMGUI.*` / `WarmUI.h`: buffered widgets, styling, and presentation helpers.

## Local sessions and player profiles

- `LocalSession.*`: persistent player profiles, isolated match settings, local
  competition progression and alternating tournament saves.
- `state/LocalSetupState.*`: local setup, entrant editor, bracket/tables,
  network profile selection and shared-device match handoff.
- `SpectatorOrder.h`: deterministic alphabetical spectator presentation.

See [Local play](LOCAL-PLAY.md) for storage, UI and archive/network behavior.

## Menus

- `state/State.*`: state transitions and the main menu.
- `state/GameState.*`: match presentation, score bar, dialogs, and victory celebration.
- `state/TabbedOptionsState.cpp`: entry points for the current options state.
- `state/OptionsMenu.*`: private options session, input capture, and immediate settings updates.
- `state/OptionsWidgets.cpp`: shared rows, buttons, sliders, and navigation.
- `state/OptionsTabs.cpp`: General, Controls, Graphics, and Audio tabs.
- `UITranslations.*` / `data/ui-translations.xml`: the six-language UI catalog.

The old graphics/input/misc options states are retired. Tests must use `OptionState`.
Options apply completed edits immediately; changing render FPS must not change match speed.

## Assets and compatibility names

Graphics are packaged from `data/gfx`; shipped level images and audio are packaged
in `data/levels.zip`. Only `Custom Levels/` is installed as editable folders.
Some source calls and saved preferences use historical names such as `gfx/titel.bmp`
or `strand1.bmp`. These are logical identifiers resolved to HD files; they do not
require physical BMP copies. `metrics.txt` stores logical sprite dimensions.
Keeping logical identifiers also preserves existing user preferences and replay presentation.

## Build, test, and tools

- Root and `src/CMakeLists.txt`: targets and dependencies.
- `data/CMakeLists.txt`: runtime archives, preferring WebP over matching PNGs.
- `test/AssetsSmokeTest.cpp` / `FrameTimingTest.cpp`: end-to-end rendering/game regressions.
- `tools/package_friends.ps1`: portable Windows package and runtime dependencies.
- `tools/finalize_friends_package.py`: asset/language validation, notices, manifest, ZIP.

Format project-owned C++ with `.clang-format`. Keep third-party sources in
`deps/`, `src/raknet/`, and `src/third_party/` separate from style changes.
Delete files only after checking CMake, dynamic Lua/file discovery, package scripts,
and the hardware smoke suite. Unreferenced C++ symbols alone do not prove that a
script, plugin-facing API, or serialized field is unused.
