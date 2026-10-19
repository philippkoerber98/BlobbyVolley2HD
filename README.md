# Blobby Volley 2 HD

A standalone HD fork of [Blobby Volley 2](https://github.com/danielknobe/blobbyvolley2),
with modern menus, HD WebP artwork, runtime Nunito text, controller support,
independent rendering FPS, and classic 4:3 or extended 16:9 local matches.
The original authors and GPL-2.0-or-later license remain in `AUTHORS` and `COPYING`.

## System requirements

- **Windows release target: Windows 10 or Windows 11, 64-bit (x64).**
  Windows 2000/XP/Vista/7/8 are not supported targets of this HD build.
  SDL2 already excludes Windows 2000; current Microsoft C++ runtimes target
  Windows 10/11. Older operating systems have not been validated with these dependencies.
- A graphics driver supporting SDL2 rendering or desktop OpenGL 2.0.
- Linux and macOS source builds are retained; they must be tested on their target platforms.

The Windows target is a project support policy, not a claim that every older
Windows version is technically unable to run any specially compiled variant.
See [Microsoft runtime requirements](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist)
and [SDL2 Windows support](https://wiki.libsdl.org/SDL2/FAQWindows).

## Get the source

```sh
git clone https://github.com/philippkoerber98/BlobbyVolley2HD.git
cd BlobbyVolley2HD
```

## Build on Windows

Use Visual Studio 2022 with the Desktop development with C++ workload,
a Windows SDK, CMake 3.16 or newer, Git, and vcpkg. Build for x64.

```powershell
git clone https://github.com/microsoft/vcpkg.git out/tools/vcpkg
./out/tools/vcpkg/bootstrap-vcpkg.bat -disableMetrics
cmake -S . -B out/build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE="$PWD/out/tools/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build out/build --config Release --parallel 4
cmake --install out/build --config Release --prefix out/install
```

The manifest installs SDL2, SDL2_image with WebP decoding, SDL2_mixer with Vorbis decoding, PhysFS, and Boost.
Start the installed game from the installation directory:

```powershell
cd out/install
./blobby.exe
```

For a portable package with the required runtime DLLs and license notices:

```powershell
./tools/package_friends.ps1 | Set-Content -Encoding utf8 out/friends-package-path.txt
python tools/finalize_friends_package.py
```

Extract the complete resulting ZIP to share it. Keep the DLLs and `data` folder
beside `blobby.exe`; no installer is required.

## Build on Linux or macOS

A C++11 compiler, CMake 3.16+, SDL2 2.0.18+, SDL2_image with WebP support,
SDL2_mixer 2.6+ with Vorbis decoding, PhysFS, Boost, and desktop OpenGL development files are required.
Lua and TinyXML2 are bundled under `deps/`.

On Debian/Ubuntu:

```sh
sudo apt-get install build-essential cmake libsdl2-dev libsdl2-image-dev libsdl2-mixer-dev libphysfs-dev libboost-dev libgl-dev
```

On macOS with Homebrew:

```sh
brew install cmake sdl2 sdl2_image sdl2_mixer physfs boost
```

Then configure and build outside the source tree:

```sh
cmake -S . -B out/build -DCMAKE_BUILD_TYPE=Release
cmake --build out/build --parallel
```

Run `out/build/src/blobby` from the project directory so that `data/` is found.
For a macOS application bundle see `macos/CMakeLists.txt`.

## Verify a change

```powershell
cmake --build out/build --config Release --target assets-smoke-test display-viewport-test controller-test
New-Item -ItemType Directory -Force out/check | Out-Null
./out/build/src/Release/assets-smoke-test.exe "$PWD/out/install/data" "$PWD/out/check" --hardware
./out/build/src/Release/display-viewport-test.exe
./out/build/src/Release/controller-test.exe
```

The hardware suite exercises SDL and OpenGL, menus, translations, HD assets,
window controls, frame timing, replays, and both bot/court modes. It needs an
interactive desktop and creates screenshots in the specified output directory.

## Working on the code

See [the architecture guide](doc/ARCHITECTURE.md) for the main components.
Project-owned C++ follows `.clang-format`; do not reformat vendored libraries.
New assets belong in `data/gfx` or a folder in `data/Levels`. WebP is preferred;
a PNG can be used when no matching WebP exists. Graphics archives contain HD assets only.

Normal gameplay uses 75 simulation steps per second at 100% speed. Render FPS
is separate. Network games retain the classic court. The dedicated server
uses port 1234 by default.

## Level folders and audio

Built-in courts live in `data/Levels/<name>/` in source and are packaged into
`data/levels.zip`. Add your own local-only folders to `Custom Levels/`.
Only `background.webp` (or `background.png`) is required. Optional `level.ogg`,
`victory.ogg`, `bums.ogg`, `chat.ogg` and `pfiff.ogg` override that level's sounds;
WAV works too. Effects and victory sounds fall back to the global sounds; level
music stays silent without a usable local track. Optional `ball.webp` or
`ball.png` replaces the ball artwork; missing or invalid images use the standard
ball. Use a square transparent image with a centered ball, ideally 512 x 512 or
larger. Ball size and physics remain unchanged.
No per-level configuration files are needed. Reopen options to discover new levels.
See [custom levels and audio](doc/CUSTOM-LEVELS.md) for details.

## LAN tournaments

Host a tournament directly from the LAN browser or use the dedicated server.
Configure up to 32 shared player/spectator slots, round robin, knockout or groups
with knockout, and parallel matches. Results, standings and live spectating are
managed by the server. See [LAN tournaments](doc/LAN-TOURNAMENTS.md) for setup,
organizer access, reconnect/save behavior and test limitations.

### Local play and built-in levels

Choose **Local game** for a single match or a tournament with 2–32 participants,
including mixed humans and bots. Participants take turns sharing the two input
seats. Profiles preserve names, colours and colour cycling. General PC settings
are separate from match rules, participants and level selection.

Built-in levels ship in `data/levels.zip`. Do not unpack or replace its contents;
restore this file from the same release if damaged. `Custom Levels` remains editable
and is available for local play only. LAN/online use the host's built-in level and
require matching level assets. All peers must run the same version. See
[Local play and profiles](doc/LOCAL-PLAY.md) for details.
