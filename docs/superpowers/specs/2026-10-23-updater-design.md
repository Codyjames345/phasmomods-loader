# Updater: design

Status: approved by the user on 2026-10-23 ("yes that's right"). Where a decision is Claude's call rather than the
user's, it says so.

## Intent

When the user publishes a new release of the suite (the launcher, the host and the bundled mods),
everyone who has the launcher gets it automatically and plays on it, with no download page and no
zip to extract. This matters more than for most apps for two reasons:

- A modded lobby admits only players with the same mods in the same versions, so a friend left on
  an old release cannot play with the others.
- Under the game SDK design (`2026-10-23-game-sdk-design.md`), a game update that breaks nothing
  needs only a new package carrying the new checked-build record. The updater is how that record
  reaches every player.

## Decided by the user (2026-10-23)

- Releases are published on GitHub, at `Codyjames345/phasmomods-loader`, each containing the
  launcher and every bundled mod.
- The repository becomes public, so launchers read its releases without a login. The user
  confirmed this a second time knowing that, once the game SDK exists, the public source includes
  its de-obfuscated map of the game's main systems.
- When the latest release is older than the installed one (a bad release was deleted), launchers
  go back to it, so everyone returns to one version. This uses Velopack's version-downgrade option.
- Before the first release, every commit's author email is rewritten to the user's GitHub no-reply
  address. `main` is force-pushed once, at a quiet moment the user chooses, because other sessions
  commit in this repository. Commits made afterwards use the no-reply address.
- Updating uses Velopack, the maintained Windows self-update framework (the successor to
  Squirrel), approved as a new dependency: its C++ library in the launcher, and its `vpk`
  packaging tool in the release process.
- Updates install automatically, without asking.

## Goals

- Every start of the launcher checks the repository's latest release. When it differs from the
  installed version, newer or older, the launcher downloads it, verifies it, installs it and
  carries on with it. A launch through Steam's launch option must still start the game normally.
- One release is one version of the whole suite. Players never mix a launcher from one release
  with mods from another.
- A failed check (no network, GitHub down or rate-limited, a broken download) never stops anyone
  playing. The launcher carries on with the installed version and says why it could not update.
- Publishing a release is one documented step for the user.

## Non-goals

- Update channels, betas or opt-in pre-releases.
- Per-mod downloads or choosing versions. The release is the unit.
- Code signing (Claude's call, see "Trust").
- Updating the game, or anything inside the game folder.

## Anti-goals

- A token, password or any other credential inside the launcher or the package.
- An update that leaves a player with a half-replaced install. Velopack swaps the whole version or
  nothing.
- An update that touches the player's settings, consent, logs or checked-build records in the data
  directory.
- Anything that changes how the suite reaches the game. Loading stays injection into a running
  game, from outside the game folder.

## How it works

### Install and layout (Claude's calls)

- Players install once with Velopack's `Setup.exe`, which installs per user, without
  administrator rights, into `%LOCALAPPDATA%\PhasmoModsLoader`. There is no stable
  `PhasmoMods Loader.exe` at that root: Velopack keeps the running version in `current\`, so the
  stable path is `current\PhasmoMods Loader.exe`, which an update replaces in place. The Steam launch
  option and shortcuts therefore never change after an update.
- The install folder is deliberately not `%LOCALAPPDATA%\PhasmoMods`, the existing data directory
  (`launcher.ini`, consent, logs, the host's copies in `bin`). Velopack replaces and uninstalls its
  own folder, and must never be able to touch player data.
- The release packages the same files the portable zip carries today (`tools/payload.psd1`), plus
  the checked-build records.
- Velopack's app id is `PhasmoModsLoader`. Its C++ library is vendored in `third_party/` at a
  pinned version, as MinHook is.

### When the launcher updates

- `VelopackApp::Build().Run()` is the first statement of the launcher's `wWinMain`, as Velopack
  requires.
- On every start the launcher checks the release feed through Velopack's GitHub source, anonymously.
  GitHub allows 60 anonymous requests an hour per IP address, far more than launches need.
- A release that differs from the installed one is downloaded and verified against the checksums
  in its feed. That includes an older one, through Velopack's downgrade option (decided by the
  user). The launcher then
  hands over to Velopack's updater, which applies it and restarts the launcher with the same
  command line. The launcher's existing window shows what is happening ("Updating to 1.4.0").
- The restart must not break a launch made through Steam's launch option. Steam's environment and
  process tracking have to reach the restarted launcher, so the game starts as a Steam game and
  Steam still shows it as running. This is checked live. If it cannot be made reliable, the
  fallback is to apply the update when the launcher exits, so the next launch uses it. In that case
  the launcher says so, and the user is told before the fallback is accepted.
- Attach to a running game works the same way: update first, then attach. A game already running
  with mods loaded is unaffected by an update, because the game loads copies of the DLLs from the
  data directory, so the install can be replaced underneath it.

### Versions

- A release has one version number for the whole suite, `MAJOR.MINOR.PATCH`, tagged `vMAJOR.MINOR.PATCH`.
  The launcher, the host and the package all report it.
- The join guard keeps comparing each mod's build ID, the first 8 bytes of its DLL's SHA-256, as it
  does today. Its wire format does not change.
- When builds differ, its message adds one line telling the player to restart the game through the
  launcher to update (Claude's call). It does not name versions: that would change the presence
  wire format, and a client speaking an older format would then go unheard instead of being caught
  as a mismatch.
- A mod that arrives new in an update starts off until the player ticks it, in the launcher or the
  in-game Mods panel (Claude's call). This keeps a lobby's mod lists matching after an update, and
  the consent notice already asks again whenever the installed mods change.

### Trust (Claude's call)

- Downloads come over HTTPS from the repository's releases, and Velopack checks each package
  against the size and SHA checksum in the release feed.
- Releases are built and published by GitHub Actions from a tagged commit, not uploaded by hand, so
  every release traces to a commit in the public history.
- The release is not code-signed. Windows SmartScreen warns once when a player first runs
  `Setup.exe`, and the installation instructions say so. Velopack can sign later (Azure Trusted
  Signing or a certificate) without changing this design.
- The remaining risk: anyone who takes over the GitHub account can publish a release that every
  launcher installs. The account must have two-factor authentication. That is the user's to
  confirm, not something the build can check.

### Order of work (Claude's call)

The updater is built before the game SDK. It is smaller, independent, and lets every later SDK
stage reach players by itself.

- Until the SDK's checked-build records exist, the package keeps today's compiled-in game hashes,
  and a game update needs a new build.
- The SDK plan that introduces the records (its migration stage) also adds them to the package and
  adds the release script's stale-record refusal.

### Publishing a release

1. Locally, the user runs the game SDK's offline check whenever the game or the map has changed.
   That updates the checked-build record in `sdk/builds/`. They commit it, bump the suite version
   and push a `v` tag.
   - A script does the bump and the tag.
   - It refuses when the working tree is dirty.
   - Once the records exist, it also refuses when the record is stale: when the map's digest differs
     from the one the record was checked with. CI repeats that check.
2. A GitHub Actions workflow on the tag runs on a Windows runner: build, the full offline test
   suite, `vpk pack`, then `vpk upload github --publish` with the workflow's own `GITHUB_TOKEN`
   (the release is in the same repository, so no other token exists).
3. The workflow refuses to publish when any step fails. A failed release publishes nothing.

The offline game check cannot run in CI, because it needs the game's files and those are never
uploaded anywhere. That is why it is a local step before tagging.

### Making the repository public

Before the user switches the repository to public (the switch itself is theirs to make):

- `gitleaks` over the whole history: run 2026-10-23, 20 commits scanned, no leaks.
- A scan of the history and the tree for home-folder paths, machine names, Steam IDs and personal
  details: 2026-10-23, none found in any file. The one exception is commit metadata (next point).
- Every commit's author email is the user's personal address. It is rewritten before the first
  release (decided above).
  - The rewrite must come before the first `v` tag. Otherwise published releases would point at
    commits that are no longer on `main`.
  - The force push is the user's go, given at the moment it happens.
- Derived game data stays out, as `.gitignore` already requires: no dumps, no `names.tsv`, no
  decompiled output, and no game binaries in the repository, its releases or the CI cache.
- The README and the installation instructions are rewritten for a public reader: what the suite
  is, the ban risk, private lobbies only, and how to install with `Setup.exe`.

### Moving existing players over

The user and the friends who have the portable zip install once with `Setup.exe`, update the Steam
launch option to the new `PhasmoMods Loader.exe` path, and delete the old folder. Their settings and
consent carry over, because they live in the data directory. The installation instructions cover
this once.

## Verification

- Gate 2: offline tests for the launcher's update decisions (newer, same or older version; a
  failed check carries on with the installed version; the message shown), and for the release
  script's refusals (dirty tree, a missing tag, and, once the SDK's records exist, a stale record).
- Gate 3, without GitHub: Velopack reads a local folder as a source. Install version N from a
  folder, put N+1 in that folder, start the launcher, and confirm it updates, restarts and attaches,
  while the data directory is untouched. Repeat with a corrupted N+1 package: the install stays on
  N and the launcher says why.
- Gate 3, through Steam: the same update with the launcher started by Steam's launch option, and
  the game starting normally afterwards as a Steam game.
- The first real release: publish it from a tag, install it from GitHub on a clean user profile,
  then publish a second release and watch the first one update itself.
- The workflow is checked with `actionlint` and `zizmor`.

## Constraints

- Windows x64 only. C++ with MSVC, CMake and Ninja for the launcher. The release workflow uses
  GitHub's Windows runners and the .NET SDK they carry for `vpk`.
- No credentials in the repository, the launcher or the package.
- No file in the game folder.

## Left to the builder

- How the Velopack C++ library is vendored and linked into the launcher.
- The release script's language (PowerShell, like the rest of the tooling) and its exact checks.
- The wording in the launcher window and the join guard message, following the project's copy
  rules.

