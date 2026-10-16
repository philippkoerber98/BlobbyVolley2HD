# Options menu

The options screen uses one settings session with the tabs Game, Players,
Controls, Graphics and Audio. Completed edits immediately update the active
renderer, sound, language and frame limit and save `config.xml` and
`inputconfig.xml`. Back leaves directly; there is no Apply button.
Names and bindings are saved when their capture dialog is completed.

Mouse: click tabs and settings; drag slider handles/tracks. Click selectors to
cycle forward, Shift-click to cycle backward.

Controller: LB/RB switch categories, D-pad or left stick navigates rows and
changes values, A activates, B goes back. Keyboard equivalents are
Page Up/Page Down, arrows, Enter and Escape. Capturing a binding suspends
normal menu navigation. Names use the keyboard and preserve UTF-8 characters.

RGB controls and a live blob preview are available for both players. Bot
selection/strength, network side and legacy joystick/mouse/keyboard bindings
remain available. Speed presets retain their original values: 40%, 80%, 100%,
120% and 160%; speed is independent of rendering FPS.

All interface text, including the Options heading, now uses runtime Nunito TTF
atlases. See `doc/RUNTIME-FONT.md`; no pre-rendered glyph PNGs are needed.
The menu backdrop is generated from the selected court at runtime, with a cached
menu-only blur and a translucent dark overlay. Gameplay keeps the sharp court.
The font license is included in `gfx.zip` as `gfx/menu-font-license.txt`.

Validation: `assets-smoke-test <installed-data> <output-directory> --hardware`
captures all five pages in SDL and OpenGL and checks input navigation, immediate
settings persistence, text input, RGB sliders, live background switching and
the reset confirmation. Restore defaults loads the separately shipped
`default-config.xml` and `default-inputconfig.xml`, including controller defaults.


The main menu now uses the approved left-hand list with the original HD logo,
contextual hints, and the currently selected court blurred and darkened behind
it. Mouse, arrows/D-pad, A/Enter and B/Escape work throughout; quitting first
asks for confirmation and defaults to staying in the menu.

The compact centered TV-style HUD shows names, colours, service and scores in
a shallow bar at the top. Time and target sit directly below; the clickable
pause hint is at the top right. Pause/leave and victory
dialogs share the warm brown/gold style. The champion screen shows the winner
blob freely, a trophy and animated confetti without a podium box or gold line.
Confetti runs on render time, uses no simulation RNG, and avoids the central
text and buttons. Victory supports rematches (local),
returning to the menu and saving replays; replay playback retains restart and
return-to-replays actions. Network pause keeps the existing chat below its
compact dialog.

Modal blur captures the already rendered court and HUD once per dialog and
caches a small blurred texture in both SDL and OpenGL. No blurred image files
are written, and no framebuffer readback occurs during normal gameplay.
`testGameMenus` checks safe defaults, controller actions, pause/resume, leaving
a match and the victory screen, and exports renderer screenshots.

Rounded panels use a cached white alpha mask with supersampled coverage at the
physical output scale, drawn in nine non-overlapping slices. SDL uses floating
point destination rectangles; OpenGL uses filtered texture regions. This keeps
corners antialiased at native, HD and 4K sizes without blurring panel contents.
The rounded-panel regression checks coverage and seamless transparency at
800x600 and 3200x2400 in both renderers.
