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

- **Merge, don't squash.** `development` lives on, and a squash hides what it has already merged, so the next merge conflicts with
  its own past.
- **Tag only on `main`.** `tools/release.ps1` bumps `VERSION`, tags and pushes atomically, and the workflow refuses a tag that isn't on
  `main`. Never move a tag a release has been published from.
- **Merge back straight away,** so `development`'s `VERSION` is the new release and its builds number on from it.
- **An urgent fix** branches from `main`, merges to `main`, releases, and is then merged back into `development` the same way.

## How development builds work

- **Version:** the next minor's pre-release, numbered by the workflow's run: after 0.2.0, `0.3.0-dev.12`. The job writes it into its
  own copy of `VERSION`, so only release versions are ever committed.
- **Channel:** Velopack's `dev`, published under a `dev-*` tag as a GitHub pre-release, so GitHub's latest-release link and every player
  who hasn't ticked the box stay on the release. Releases stay on Velopack's default channel, `win`.
- **The checkbox:** it saves the choice in `%LOCALAPPDATA%\PhasmoMods\update_channel.txt` and updates straight away, unless mods are
  loaded. Unticking reads the release's feed and moves down to its version, which is why the updater allows a lower version.
- **Only three stay on GitHub.** Velopack's GitHub source reads only the newest ten releases and filters them afterwards, so ten
  development builds would hide the latest release from every player. The job deletes older ones (`tools/dev_releases.ps1`).
- **Testing the switch without GitHub:** `PM_UPDATE_SOURCE` names a folder to update from instead, for either channel.
