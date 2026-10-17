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

## Controller sliders and secondary pages

Slider input steps immediately, waits 300 ms, then repeats every 45 ms while
holding left/right on the D-pad or left stick. Release, opposing directions and
stick reversal reset the repeat state. Repeat input is buffered separately from
activation/navigation edges, so it cannot repeatedly activate buttons. Bot
strength steps by one stored reaction-delay unit (4%), avoiding rounding away
1% changes. RGB, speed, volume and deadzone retain their own step sizes.

Credits, replay selection, LAN browsing and the lobby use the options palette,
runtime Nunito text, compact 34-pixel buttons and rounded panels. In lists,
up/down changes the selection; left/right moves between the list and actions.
A/Enter activates, B/Escape returns or dismisses a dialog. Replay lists also
support activation by A or double-click. Modal dialogs disable their underlying
controls. The selected level provides the blurred, darkened page background.

Hosting a LAN server first asks for a server name and persists it as
`lan_server_name`. Names fit the existing 32-byte discovery field without
changing the network protocol. A dedicated server's name remains configurable
through `name` in `data/server.xml`. Dedicated servers locate `data` relative to
the executable even when started from another working directory.

LAN regressions (`assets-smoke-test <installed-data> <output> --lan-only`) use
real local UDP connections on ports 23456 and 23457. They verify both dedicated
and player-hosted servers: advertised names, two clients, opening/joining a
lobby, rules handshake, match start and cleanup after disconnect. This does
not test public Internet matchmaking, another PC's firewall or router behavior.

Secondary-page actions use fixed-width, centered labels. Utilities sit above
the list, primary actions below it, and Back is aligned to the bottom-right
720-pixel content edge. Replay deletion is separated from play/info. Lobby
selectors share one width; leave/start and modal confirm/cancel form aligned
rows with consistent spacing.

Legacy `lang_*.xml` lookup keys remain unchanged; their display values use
consistent capitalization and corrected terminology in all six languages.
Connection, close, back and main-menu actions have distinct labels rather than
sharing OK/Cancel. The controller Back action recognizes these return/dismiss
labels independently of the active language. Catalog checks validate all legacy
and modern entries, and runtime tests compare shared action labels across both
catalogs for every supported language.
