# LAN tournaments

## Hosting and limits

Choose **Host tournament** in the LAN browser. The host-name dialog also sets
2–32 simultaneous server connections (default 16). This includes the playing
host, entrants, waiting players and spectators. The dedicated process itself
uses no connection slot. Existing ordinary LAN matches are still available.
Open **LAN tournament** inside a server lobby to join its tournament view.

The dedicated server uses the same competition and networking code. Set
`maximum_clients` in `data/server.xml` to 2–32 (default 16). Remote organizers
enter `tournament_password` through **Organizer access**. If that setting is
empty, the server prints a newly generated organizer code in its console.
The first tournament connection from loopback receives organizer access; a
remote client never receives it merely by connecting first. Authority persists
independently of elimination and is not broadcast as a secret to other clients.

The organizer chooses entrant capacity (not greater than server slots), format,
points, simulation speed and the maximum number of concurrent matches (1–16).
The organizer can remove an entrant during registration without disconnecting
them from the server. Rules use the first server rule set. LAN retains the existing classic court and
unchanged 75-step normal simulation; this feature adds no 2-vs-2 mode.
The tournament name is editable. Registration can be closed only when all
entrants are connected and ready, with at least two entrants (six for groups).
Existing ordinary matches must finish before a tournament starts.

## Formats and progression

- Round robin: everyone meets everyone once. An optional final pits the top two
  against each other. A 32-person round robin contains 496 group matches.
- Knockout: seeded by a random draw; non-power-of-two entry counts get explicit
  byes. An optional third-place match uses the semifinal losers.
- Groups + knockout: two groups for 6–16 entrants, four for 17–32; group sizes
  differ by at most one. The best two per group qualify, with cross-group
  pairings (A1–B2, B1–A2, similarly C/D). An optional bronze match is available.

The organizer can draw a preview before starting and redraw while registration
is open. The final start keeps that preview's seed and pairing. Changing entrants
or settings invalidates the preview. The server prevents a participant from
appearing in two concurrent matches. Readiness is reset after a match, and both
players must return to the overview before their next match launches.

Group/ranking order is match wins, then a mini-table between players tied on
wins (wins, point difference, points scored), overall point difference, points
scored, and finally a persisted seeded draw. This final tie rule is displayed
in the standings; there is no unannounced name/connection-order tie breaker.
Forfeits are explicit wins without fabricated ball-point scores. Knockout
placement is represented by the bracket and final/bronze results rather than
inventing unique ranks for everyone eliminated in the same round.

## UI and spectating

Overview, bracket/schedule, standings and participants share the existing menu
palette and Nunito type. Mouse, ordinary menu navigation, shoulder buttons and
Page Up/Down work across tabs. The personal overview identifies entrants and
eliminated spectators; match cards expose waiting/live/interrupted/completed
status. Large brackets and tables have pagination. All active clients receive
the authoritative tournament snapshot; no client supplies a result.

Waiting and eliminated people can watch one live match at a time. The server
streams canonical snapshots at up to 30 Hz, with fixed-step renderer
interpolation on the spectator. Watchers never receive a player input source,
never enter the match's player list and cannot pause or send game inputs.
The server renders no graphics. Watching does not consume an extra connection.

## Interruptions, identity and persistence

An unfinished match is interrupted rather than silently awarding a loss.
Participants return to the overview. With both participants reconnected and
ready, an in-memory match checkpoint can resume; the organizer can instead
restart or award a clearly marked forfeit. Disconnected active entrants reserve
a slot for 60 seconds, so an unrelated late spectator cannot steal that place.
After the reservation expires the entrant may still reconnect if space exists.
Late arrivals after registration closes are spectators only.

Reconnect identity uses an opaque random token, saved in the client's config
per server address/port. Names are display values, not authentication. The server
sends a token only to its owner and excludes all tokens from public snapshots.
Do not distribute your personal config or server tournament saves as public
assets: they contain these private reconnect identities.

Server saves are stored in its existing writable application directory under
`Tournaments/server-<port>-a.xml` and `-b.xml`. Alternating versioned snapshots
retain a previous valid file if a write is interrupted. Loading selects the
newest valid revision. Completed results survive a server restart; matches that
were in progress are marked interrupted and must be replayed after restart
(the live checkpoint itself is deliberately not persisted). An organizer must
resume a loaded running competition. Returning to menus does not stop an
in-game hosted server; closing the application does. Host migration is not
implemented. A finished tournament can be replaced with a new registration
once all match players have returned to the overview.

## Verification

`tournament-test` exercises sizes 2–32 for every format, byes, preview stability,
optional final/bronze, concurrent scheduling, group pairings, result uniqueness,
public/private serialization, recovery and bounded packet decoding.

`assets-smoke-test <installed-data> <output> --tournament-only` uses real local
UDP clients against both dedicated and in-game configurations. It covers admin
checks, settings, launch/handshake, passive live streaming, capacity, forfeit
results and stable reconnect identities, followed by a 32-client/16-match
capacity test and an actual client UI/match-return lifecycle test. A server
restart test also truncates the newest save and verifies recovery from the
previous one. The 32-client check delivered at least 149 updates per client in
two seconds on the development PC. This is a local functional load test, not a guarantee for every
PC, wireless network or long-running event.

`--tournament-ui` captures all tabs and the group-to-knockout bracket in six
languages with SDL and OpenGL. Existing ordinary LAN, replay, controller and
physics regressions remain applicable.
