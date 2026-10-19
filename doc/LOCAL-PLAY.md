# Local play, profiles and level packaging

The main menu opens a local setup instead of launching an immediate match.
Setup has Game, Participants and Overview tabs. Rules, score target, simulation
speed, court geometry and level belong to this session. Global options contain
General, Controls, Graphics and Audio only. Graphics keeps FPS, shadows, blood,
renderer and window/display preferences; resetting options preserves profiles,
match defaults and tournament saves.

A local tournament uses the same pure Tournament model as LAN but schedules
one real DuelMatch at a time. It supports 2–32 entrants, humans and scripted bots,
round robin, knockout and groups followed by knockout (at least six entrants).
Registration locks when the bracket is accepted. Results, bracket/standings,
entrant identity, bot configuration and next pairing are saved to alternating
local-cup-a.xml/local-cup-b.xml in the user preference directory. Interrupted
matches are replayed from their start, not assigned a random result.

Before each match, two physical input seats are assigned independently of the
participant profiles. Keyboard presets, controller slots, mouse and joystick
presets can be reused by different people. Conflicting keyboard keys, the same
controller slot and two simultaneous mice are rejected. Both human players must
release, press jump and release again before the match starts; bots are ready automatically. The Controls shortcut (mouse or LB/RB)
returns to the same match preparation after editing global bindings.

Profiles live in profiles.xml, independently of config.xml. Existing left/right
names, colours and morph settings migrate once. A participant edit is session-only
until explicitly saved. Save profile updates an existing profile; New profile
creates another. Network entry publishes the selected profile to both legacy
identity slots while leaving device bindings unchanged. Preferred network side
and control preset remain personal and independent: server-authoritative simulation can be mirrored independently
for each player. Spectators use case-insensitive alphabetical full-name order,
folding common Latin diacritics, with participant ID breaking equal-name ties.

Source assets remain under data/Levels. Build output packages only their runtime
WebP/PNG and OGG/WAV files into data/levels.zip; PNG duplicates with a WebP peer are
excluded. Built-in folders are not installed loose. Custom Levels remains editable
beside the Windows executable, and is listed only for local setup. No custom-level
file is uploaded or downloaded by a network session.

The host chooses a built-in level, rules and speed before starting its server. Dedicated servers
use the `level` entry in server.xml, default strand1.bmp. All clients receive that
selection, without changing their saved local level. A catalog fingerprint over
sorted asset paths and effective bytes is required in the identity handshake.
A mismatch or older handshake never enters matchmaking or tournament registration.
Identical contents in loose source folders and ZIP archives produce the same CRC32.
This catches accidental modifications/version differences; it is not cryptographic
anti-cheat or protection against deliberately patched clients. An archive can still
be deliberately modified/deleted. Restore levels.zip from the same release if needed.
Network protocol minor version is 108; every participant needs the same release.
Network physics remains classic 4:3; local matches support either court geometry.

## Verification

The installed archive-only package was checked with `assets-smoke-test`:
`--local-session` exercises all local formats for 2–32 entrants, mixed rosters,
profile and tournament persistence, catalog mismatch rejection, six-language UI,
bot simulation and a two-virtual-controller ready/release/conflict regression.
`--tournament-only` exercises authoritative LAN/dedicated coordination with
multiple clients, reconnects, live viewing and capacity limits. `--hardware`
checks SDL/OpenGL menus, rendering, window controls, replay determinism and
classic/widescreen bots. Controller mapping and viewport standalone tests pass.
LAN testing used loopback clients on one PC, not separate physical PCs.
