# Experimental widescreen court

Choose Options > Graphics > Court (Spielfeld): 4:3 Classic or 16:9 Breitbild.
Selection is saved immediately and applies to new local matches. Windowed mode
adopts a matching width when the court is changed; fullscreen continues using
the existing desktop fullscreen mode. Non-matching displays/windows letterbox.

Classic remains 800 x 600 with movement speed 4.5 units per simulation tick.
Widescreen is 1066.6667 x 600 with speed 5.4 (+20%). Net and spawn/serve positions
follow the court width. Ball speed, physical radii, gravity, jumps and the fixed
75 Hz normal simulation rate are unchanged. Objects keep their proportions;
the existing background scenery is temporarily stretched to fit the wider court.
The GUI retains its centered layout and its own coordinate mapping.

The geometry belongs to each DuelMatch/PhysicWorld, not a global preference.
Existing network matches stay classic: no incompatible geometry is sent to
unmodified servers. Legacy rules retain normalized 0..800 horizontal coordinates.
Each selectable bot now has a separate native 16:9 variant in scripts/wide/.
The original 4:3 Lua scripts and their constants remain untouched. Wide bots and
their physics prediction helpers use the real width and movement speed; right
players are mirrored around the actual court midpoint. See BOT-MODES.md.

Wide local replays are marked 2.1 and include BLOBBY_HD_WIDE_V1 in the embedded
rule source. This HD build restores court geometry before playback and seeking,
independently of the current graphics preference. Classic recordings remain 2.0
and old recordings default to classic. Use this HD build to play wide recordings;
older builds do not implement their geometry.

The hardware smoke tests cover classic versus wide wall reflection, unchanged
jump trajectories, net limits, reachable net/outer drops from neutral positions,
legacy bots, replay geometry and seeking, the actual options selector and local
match startup, mouse mapping, and screenshots from SDL and OpenGL. These checks
are a starting point for balance, not a promise that every attack is reachable.
