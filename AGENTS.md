# BlobbyVolley2HD development instructions

## Project work

Follow the architecture described in `doc/ARCHITECTURE.md`. Inspect affected
modules and their dependencies before editing. Prefer readable, maintainable
code and focused changes. Preserve compatibility and game behavior when
refactoring unless the task explicitly requests behavior changes. In particular,
preserve the normal 75-step simulation, classic court behavior, widescreen bot
adaptation, and replay/network determinism. Do not reformat bundled dependencies.

After changes, review the diff, run applicable tests, check asset integrity when
relevant, and report important results and limitations. For rendering, timing or
input changes use the relevant `assets-smoke-test`, `display-viewport-test` and
`controller-test` targets; run hardware smoke tests against installed data.
For Git tooling run `python -m unittest discover -s test -p test_git_conventions.py`.

## Commits

Use `type(scope): imperative description`; scope is optional. Use English,
imperative, specific descriptions. Subjects must be at most 72 characters.
Types are lowercase and limited to:

| Type | Purpose |
|---|---|
| `feature` | New functionality |
| `bugfix` | Incorrect behavior |
| `chore` | Maintenance, build, dependencies, configuration or documentation |
| `refactor` | Internal restructuring without behavior changes |
| `style` | UI presentation or nonfunctional formatting |
| `assets` | Primarily asset additions, replacements, moves or removals |

Scopes use lowercase kebab-case. Do not use `feat`, `fix`, `docs`, `perf` or any
other undeclared type. Do not use vague descriptions such as "update files" or
"fix stuff". One commit represents one cohesive change; do not mix unrelated
work. For code and assets together, select the type by the primary purpose.
English, imperative wording and semantic cohesion still require human review.

Examples:

- `feature(rendering): implement HD resolution scaling`
- `feature(gameplay): add improved player controls`
- `bugfix(physics): correct ball collision behavior`
- `bugfix(audio): prevent overlapping sound effects`
- `chore(build): update compiler configuration`
- `refactor(game): simplify game state management`
- `style(menu): improve button alignment`
- `assets(sprites): replace player textures`
- `assets(audio): update sound effects`
- `assets(fonts): replace legacy font files`

## Branches

New working branches use `type/short-kebab-case-description`. Allowed prefixes
are exactly `feature/`, `bugfix/`, `Chore/`, `refactor/`, `style/`, `assets/`.
The uppercase C in `Chore/` is intentional. After the slash use only lowercase
kebab-case, without spaces, underscores or further slashes. Keep names short,
descriptive and aligned with the primary purpose; reject `feature/changes`.
Choose the category before creating a branch. Preserve existing capitalization
and do not rename existing branches without permission. Do not automatically
create branches unless the task requires one.

Examples:

- `feature/hd-rendering`
- `feature/multiplayer-controls`
- `bugfix/ball-collision`
- `bugfix/audio-crash`
- `Chore/update-build-system`
- `refactor/game-state`
- `style/main-menu`
- `assets/hd-sprites`
- `assets/new-sound-effects`

Existing permanent branches `main`, `master`, `develop` and temporary history
safety refs are exempt. The validator recognizes safety branches only as
`backup/history-reconstruction-YYYYMMDDTHHMMSSZ-<7-to-40-hex-sha>`.
Safety refs are not a bypass for PR naming or commit validation.

## Assets

Preserve original quality where possible. Avoid unnecessary conversions and
never silently remove assets. Preserve references when renaming. Check that
replacements remain compatible with the rendering and audio systems. Use
`assets` for primarily asset changes, and keep large replacements separate from
unrelated code changes.

## Git safety and checks

Never rewrite published history without explicit user permission. Never
force-push automatically. Preserve requested backup branches and tags.
Use the shared validator in `tools/git_conventions.py`; installation and limits
are documented in `doc/GIT-CONVENTIONS.md`. Hooks must be installed per clone;
they are not automatically active after cloning. Do not bypass validation to
hide failures. Local hooks and CI alone cannot prevent server-side creation of
invalid branches or direct pushes; required checks and native GitHub rules are
separate settings and require explicit authorization before modification.
