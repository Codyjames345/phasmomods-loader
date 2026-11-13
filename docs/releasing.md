# Branches, development builds and releases

For maintainers. `main` is what players have, `development` is what's being tried out, and a release only ever comes from a tag.

| Where | Who gets it | How |
| --- | --- | --- |
| Your machine | You | `.\build.ps1`, then `dist\PhasmoMods\PhasmoMods Loader.exe`. The updater leaves this copy alone. |
| `development` | Anyone who ticks the loader's development box | Push. Each push that changes more than documentation publishes a development build. |
| `main` | Nobody yet | Merge. Pushing to `main` only validates. |
| A `v*` tag on `main` | Every player, automatically | `tools\release.ps1`. |

A push of nothing but Markdown or `docs/` runs no workflow, so it can't trigger a build. A tag always does.

## Day to day

1. Work on `development`, testing locally with `.\build.ps1`. Run `.\validate.ps1` before pushing, and
   `.\tools\gamecheck.ps1 -IfConfigured -Expect` when the map changed, committing the record it rewrites: a build whose map has no
   record fails to publish.
2. Push to `development`. The workflow builds and validates it once, then publishes a pre-release if that passes. Testers get it the
   next time their loader starts.
3. Everyone in a lobby needs the same build, so testers restart their loaders after each push.

## Releasing

From a clean checkout on `main`:

```powershell
git switch main
git pull
git merge --no-ff development       # a merge commit, never a squash
.\tools\release.ps1 -Version 0.3.0 -Push
git switch development
git merge main                      # brings the new VERSION back
git push
```

- **Write the entries first.** `CHANGELOG.md` must have at least one entry under `## [Unreleased]` before
  `tools\release.ps1` accepts a version; until then it refuses and says so. When it runs, it turns that section into
  `## [<version>] - <today>` and opens a fresh empty `## [Unreleased]` above it, in the same commit as the `VERSION`
  bump. The workflow puts a release's section on its GitHub release page, and a development build's notes are the
  Unreleased section as it stands when that build is packed. Entries are what players read, so a person writes them
  (`AI_POLICY.md`).
- **Merge, don't squash.** `development` lives on, and a squash hides what it has already merged, so the next merge conflicts with
  its own past.
- **Tag only on `main`.** `tools/release.ps1` bumps `VERSION`, tags and pushes atomically, and the workflow refuses a tag that isn't on
  `main`. Never move a tag a release has been published from.
- **Merge back straight away,** so `development`'s `VERSION` is the new release and its builds number on from it.
- **An urgent fix** branches from `main`, merges to `main`, releases, and is then merged back into `development` the same way.

## The private assets

The Squatter's AssetBundle (`mods/custom_ghosts/assets/squatter.bundle`) holds Mixamo's clips, which may not go in this public
repository, so it is gitignored. Every build still ships it (`tools/payload.psd1`), and staging fails without it.
Its entry there pins the build the code needs by its SHA-256: a stage, a package and CI each refuse any other build, so
no branch ever ships a bundle its code was not made with.

- **Locally,** build the bundle in Unity (`sdk/guides/custom-animation.md`) and copy it to that path, or place the pinned
  one from a clone of the private repository with `.\tools\place_assets.ps1 -Assets <clone>`.
- **On GitHub,** each job fetches the private repository `phasmomods-private` and places the pinned build with
  `tools/place_assets.ps1`. The repository keeps every published build at `pinned/<SHA-256>/squatter.bundle`, never
  replaced, so each branch finds its own. Set up once, under this repository's Settings, Secrets and variables, Actions:
  - the variable `ASSETS_REPOSITORY`: the private repository, as `owner/name`;
  - the secret `ASSETS_DEPLOY_KEY`: the private half of an SSH key whose public half is a read-only deploy key of the
    private repository, so the build can read that one repository and nothing else.
- **A new bundle:** after copying it into place, run `.\tools\publish_assets.ps1 -Private <clone>` (or set
  `PM_PRIVATE_ASSETS` to the clone). It pushes the build to the private repository under its hash, then moves the pin in
  `tools/payload.psd1`; commit that with the code that needs the new bundle. Until the pin moves, the local stage refuses
  the new bundle, so it cannot be forgotten.
- **Pull requests from forks** get no secrets, so their checks fail at the fetch. A maintainer reviews them and runs the build.

## How development builds work

- **Version:** the next minor's pre-release, numbered by the workflow's run: after 0.2.0, `0.3.0-dev.12`. The job writes it into its
  own copy of `VERSION`, so only release versions are ever committed.
- **Channel:** Velopack's `dev`, published under a `dev-*` tag as a GitHub pre-release, so GitHub's latest-release link and every player
  who hasn't ticked the box stay on the release. Releases stay on Velopack's default channel, `win`.
- **The checkbox:** it saves the choice in `%LOCALAPPDATA%\PhasmoMods\update_channel.txt`, checks the other feed, and asks the player
  to confirm the version it found before installing it, unless mods are loaded. Declining, or a check that fails, puts the choice
  back. Unticking reads the release's feed and moves down to its version, which is why the updater allows a lower version.
- **Only three stay on GitHub.** Velopack's GitHub source reads only the newest ten releases and filters them afterwards, so ten
  development builds would hide the latest release from every player. The job deletes older ones (`tools/dev_releases.ps1`).
- **Testing the switch without GitHub:** `PM_UPDATE_SOURCE` names a folder to update from instead, for either channel.

## The compile cache

Almost all of a CI run is compiling: from nothing, 521 compiles of 200 source files took about 15 minutes, since each test
target compiles the SDK's files again. The validate and development jobs compile through [ccache](https://ccache.dev), so a
compile whose inputs it has seen before (the source, every header it includes, the flags) comes from its store instead.
`actions/cache` keeps the store between runs, keyed by the commit and restored from the latest one. On this machine a build from
nothing hit 58% from the repeats alone, and the same build again took 11 seconds instead of 149.

- **How:** the job downloads the pinned ccache release, checks its hash, and names it in `PM_CCACHE`; `build.ps1` hands it to
  CMake as `CMAKE_C_COMPILER_LAUNCHER` and `CMAKE_CXX_COMPILER_LAUNCHER`. Each run's log ends the build with its hit counts.
- **Local builds don't use it.** A cache hit replays the `/showIncludes` header list of the compile that stored it, so with
  several worktrees sharing one store Ninja could learn another checkout's headers and miss a rebuild. A CI runner starts with
  no Ninja state, so this cannot bite there.
- **Releases don't use it.** The release job compiles from source alone, so what every player installs owes nothing to an
  earlier run.
- **What it saved:** the first run with the cache, which filled it, compiled for 11 minutes; the next, a one-file change,
  missed 7 of 657 compiles and compiled for 2 minutes 20 seconds. A development build's number reaches the compiler
  through `PM_SUITE_VERSION`, so those targets' files can miss on a development push; in that run it cost little.
- **Moving to a newer ccache:** change the version and hash in both jobs together; `tests/dev_release_test.ps1` checks they
  match.
