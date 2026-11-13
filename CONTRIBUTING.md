# Contributing

PhasmoMods Loader is a mod loader for Phasmophobia: one program, `PhasmoMods Loader`, loads the
host (`ModHost.dll`) into the game, and mods are native DLLs that talk to the loader through its
SDK. This page is for people changing this repository. To make a mod instead, start at
[sdk/README.md](sdk/README.md).

## Building and testing

Windows only. The prerequisites are the ones the README lists (Visual Studio's C++ x64 tools, CMake,
Ninja, PowerShell and the .NET SDK); then:

```powershell
.\build.ps1        # configure and build everything into build\
.\validate.ps1     # build, then run every test: native, packaging, scripts
```

Do not run the game as part of any test, and never put loader files in the game's folder. The game
has a startup check that reports modified clients, and a loader in the game folder has triggered it
before. Every in-game test is run by the maintainer, by choice, after being told that risk.

## The rules a change must follow

The full standing rules are in [CLAUDE.md](CLAUDE.md). The ones that most often catch a first
change:

- Mods, `core/` and `host/` use only the loader's de-obfuscated map of the game (`sdk/map`,
  `sdk/api`): never raw addresses, offsets, layouts, obfuscated game names or direct IL2CPP calls.
  SDK docs cite map keys, never addresses.
- Never invent game behaviour: every claim about what the game does is proven from reverse-engineered
  game code and cited by map key (`sdk/internals/mapping.md`).
- Tests come first: write the failing test, then the change. A change that moves map values also
  refreshes its checked-build record. Never weaken a check to make it pass.
- Player-facing text lives in string tables (`i18n/default.json`), never in code, and is written by
  the project's author (`AI_POLICY.md`).
- The public shape of things only grows at the end: `PmHostApi` and published structs grow, they do
  not reorder or shrink.
- UI follows the game: mod content shows on surfaces the game already has, and uses the game's own
  widgets.
- Commit messages: an imperative subject under about 72 characters, then prose saying what was wrong
  and why the change is right. Work lands on the `development` branch;
  [docs/releasing.md](docs/releasing.md) covers releases.

## Code style

`.editorconfig` and `.clang-format` describe the code as it is: two-space indents, attached braces,
`char* p`, and lines wrapped by hand. `validate.ps1` runs `tools/style_check.ps1`, which names trailing
whitespace (Markdown exempt), a missing final newline and tab indentation in C/C++ across the tracked
text files. clang-format is not enforced across the tree: the settings match the code closely but not
exactly, mostly around hand-aligned trailing comments, so only run clang-format on what you are already
editing.

## AI policy

This project says plainly where AI took part, in [AI_POLICY.md](AI_POLICY.md): reverse engineering,
C++ and documentation yes; assets, UI, player-facing text and game design no. Read it before
sending a change that uses AI help.

## Licensing

This project is Apache-2.0 (see `LICENSE` and `NOTICE`). Contributions are provided under the same
licence: as in Apache-2.0 section 5, whatever you submit for inclusion is licensed to everyone under
Apache-2.0 as well. Community mod releases should ship `LICENSE` and `NOTICE` of their own.

## Reporting problems

Do not open a public issue for anything that could put an account or the loader's safety at risk:
use the private vulnerability reporting described in [SECURITY.md](SECURITY.md). Ordinary bugs and
mod-author questions use the issue forms under **Issues**.

## Where the documentation lives

- [sdk/README.md](sdk/README.md): what the SDK is and what exists today.
- `sdk/systems/`: one page per game system, the authority on vanilla behaviour with map keys.
- `sdk/guides/`: author guides, including adding a ghost.
- `sdk/internals/`: how the map is made and how to read the game, for people expanding the SDK.
- `docs/`: player and maintainer documentation (installation, releasing).
