# Portable Steam launcher implementation plan

> **For agentic workers:** Use superpowers:subagent-driven-development or superpowers:executing-plans task by task, with independent review between tasks.

**Goal:** Distribute one portable launcher and native DLL that friends can install easily, preferably loading automatically when Steam starts Phasmophobia.

**Architecture:** Harden and reuse the existing native injector, observe process-specific fresh native activation logs, and provide a small Win32 launcher. Prove Steam command wrapping and startup timing before promising automatic loading. Keep all distributed files outside the game directory and leave the executable unchanged.

**Tech Stack:** Existing Windows x64/MSVC/C++20/CMake/PowerShell toolchain; no new dependency or runtime.

**Spec:** The user superseded the earlier loader choice: any easy, distributable loading method is acceptable; automatic Steam launch loading is preferred. Preserve gameplay and rewards from `docs/design.md`.

## Problem and evidence

The native mod now loads into an already running vanilla game: the parent session observed verified hashes, successful steering, active status and added ghost controls at 18:00:37. This proves a late-load path, not Steam wrapping, safe early startup or a complete multiplayer contract. The existing development command is unsuitable for friends because it requires commands, trusts unchecked remote operations, reports injection without activation, and suggests unsafe hot unloading.

`tools/inject.cpp` contains the only injector implementation and one caller each of `inject` and `eject`; reuse it instead of introducing another. `src/main.cpp` already has a bounded 60-second worker, binary gating, atomic activation states and process-lifetime pinning. `tests/startup_test.cpp` directly exercises production startup with fake Win32 boundaries. Adapt `package.ps1` and `validate.ps1` to the standalone route. CMake already builds `pginject`. The workspace has no Git repository or CI.

## Decisions and boundaries

- The user's delegated method choice authorizes the standalone route; no further architecture confirmation is needed.
- Ship `PlayableGhostLauncher.exe`, `PlayableGhost.dll`, a short README, SHA-256 manifest and MinHook license in a zip. Do not bundle a loader framework, injector development tools, game files or private logs.
- Keep both exact binary checks and steering instruction checks. No hash acceptance switch, binary patching, automatic offset recovery or protection bypass.
- Keep one native loading implementation shared by the launcher and any retained development command. Rename development eject behavior to stop, or remove that command from its advertised usage: `PG_Shutdown` stops gameplay and retains the module.
- The release launcher does not stop/unload/reload the mod. A restart is required to change versions or recover from inactive initialization.
- Reuse the native log with a process ID on every line, a captured pre-injection byte offset and an ordered fresh loading/active sequence. Do not add a new exported status protocol. Never treat a DWORD remote-thread result as a 64-bit HMODULE.
- Do not write Steam credentials, Steam configuration, the game's executable or its libraries. The user pastes one displayed launch-option command if the wrap route is proven. Do not add a resident watcher or startup task.

## Review focus

- Steam arguments with spaces, quotes, Unicode and backslashes preserve their meaning (Task 2).
- Remote allocation/write/thread/wait failure never reports successful loading or frees a live thread's argument (Task 1).
- Wrong game instance, duplicate load, existing incompatible mod, game exit and access denial are explicit outcomes (Tasks 1 and 2).
- A DLL mapping without actual native activation cannot be reported as playable (Task 1).
- Package failure or stale loader artifacts cannot produce a seemingly current portable release (Task 3).

## Task 0: Prove the preferred launch route and timing

**Files:** No production edits; evidence stays in local integration working files.

- [ ] Windows wrapping has primary implementation references in https://github.com/SpecialKO/SKIF and https://github.com/johanhelsing/steam_dev_launcher. Confirm locally that the wrapper receives the original executable and all launch arguments, Steam keeps its normal game association, and exactly one game starts.
- [ ] Use the completed game-log marker `Loaded Level: Main Menu` (observed at line 755 of the clean launch evidence), not the seasonal scene name or a delay. Bind it to this launch: use a fresh per-launch `-logFile` when Steam supplies none; preserve an existing explicit log path and track only fresh bytes with truncation detection. Ignore stale menu markers and time out safely if current-launch readiness cannot be established. Confirm ordinary and slow startup.
- [ ] If wrapping is unsupported, ship the no-argument attach interface as the usable fallback and state automatic launch remains unavailable. Do not create a watcher or configuration-editing workaround without new direction.

## Task 1: Make loading and native activation a reliable native boundary

**Modify:** `tools/inject.cpp`, `src/log.cpp`, `CMakeLists.txt`.
**Create:** `tools/inject.h`, `tests/inject_test.cpp`.

**Interfaces:** `pg::loader::load(DWORD pid, const wchar_t* absoluteDllPath)` returns a result with an explicit outcome and Win32 error. Native log lines include the process ID alongside existing time/thread fields. Read complete fresh lines for the selected PID from the captured pre-injection offset; a fresh loading line must precede active.

- [ ] First test allocation failure, short/failed writes, remote thread creation failure, wait failure/timeout, target exit, duplicate load, thread completion with no mapped DLL, inactive startup and successful activation. Follow the current production-boundary test style; assert handle/memory ownership as well as outcomes.
- [ ] Extract the existing loader operation into the shared implementation. Use minimum required process rights, checked snapshots and every Win32 result. Fail ambiguous process selection rather than choosing the last process in a snapshot.
- [ ] Resolve the remote LoadLibraryW address from the containing system module and target module base; do not assume a local function address is valid remotely. Confirm x64 target architecture. Validate module identity with its normalized full path, not a `pg-` name prefix alone.
- [ ] Preserve unique native copies under the existing local application-data bin folder for file-lock isolation. Reject any existing PlayableGhost or `pg-` module as already loaded and explain that a restart is required to change/re-enable it. Check every directory/copy/path operation and never inject into an arbitrary executable.
- [ ] On completed load, enumerate the exact copied target module to establish actual mapping, then wait at most 65 seconds for the ordered fresh loading/active messages for that PID. Terminal inactive/stopped messages fail immediately; game exit and log errors remain distinct outcomes. An old active line, another PID or incomplete trailing line cannot satisfy success.
- [ ] Free remote argument memory only when thread completion proves it is no longer in use. On timeout/unknown wait state, retain it until process exit, close local handles and report an indeterminate result; never terminate the game or remote thread. Retrying must first inspect whether the module actually loaded.
- [ ] Test activation parsing against stale offsets, mixed PIDs, truncation, partial lines, inactive/stopped outcomes and active without a preceding fresh loading line. Run the full CTest suite and obtain independent review.

## Task 2: Add the small user launcher and proven Steam route

**Create:** `tools/launcher.cpp`, `tests/launcher_test.cpp`.
**Modify:** `CMakeLists.txt`, `tools/inject.cpp` entry-point guards as needed. Task 2 owns launcher source; Task 3 can independently own packaging/documentation once artifact names are fixed.

**Interfaces:** Build `PlayableGhostLauncher` with the Windows subsystem. No arguments opens a simple Win32 instruction/status dialog offering attach to the running game and copying Steam setup instructions. `--steam <original game command>` is available only when Task 0 proves it works. The DLL is always resolved beside the launcher, independent of working directory.

- [ ] First test command parsing/quoting round trips, missing adjacent DLL, zero/multiple candidate processes, duplicate launcher instances and failed child launch. Test paths with spaces and Unicode; test argument values containing embedded quotes and trailing backslashes. Do not invoke a shell to launch the game.
- [ ] For the proven wrapper route, preserve Steam's original executable/arguments and inherited environment with `CreateProcessW`; retain the returned child process handle and PID, then use the proven readiness condition from Task 0. Do not launch `steam://rungameid` from inside its own launch wrapper or rediscover an unrelated PID.
- [ ] For attach mode, identify exactly one Phasmophobia process and display actionable failure if none or more than one exist. Require no administrator elevation by default. Serialize operations per game PID to prevent simultaneous duplicate loads.
- [ ] Call the shared loader. Report active, inactive with native-log location, unsupported build, access denied and timeout truthfully. Keep success unobtrusive; present failures in a normal Win32 dialog. No background service remains after success.
- [ ] Verify the real Steam launch behavior, double-click attach behavior and failure dialogs. Repeat normal launch twice in separate game sessions. Confirm executable/library hashes are unchanged. Obtain independent review.

## Task 3: Produce the portable release and document the tested path

**Modify:** `package.ps1`, `validate.ps1`, `tests/package_test.ps1`, `docs/installation.md`, `docs/design.md` distribution text.
**Create:** `dist` output only, generated by packaging, never hand-maintained source.

- [ ] First extend package tests so native build/test failure, missing launcher/DLL, stale files and archive creation failure reject release output. Assert the archive allowlist contains exactly launcher, DLL, README, manifest and MinHook license, and nothing else.
- [ ] Make default validation build all native targets and run every CTest test plus package-script checks.
- [ ] Package into a fresh owned staging directory and zip it only after validation succeeds. Record artifact sizes/hashes, supported binary hashes/build and actual runtime verification limits. Do not mark a complete contract verified solely from seeing controls.
- [ ] Document unzip outside the game folder, exact verified Steam launch option or attach fallback, restart after updates, normal launch restoration by removing the launch option, expected failures and log location. Update obsolete loader-blocked wording while preserving historical evidence outside user installation instructions.
- [ ] Run the full documented command through `harness-verify`, applicable static/security/PowerShell checks, archive extraction and a fresh-folder launch. Review the actual before/after diff against the existing local baseline and obtain independent review. Report missing or failing checks without weakening them.

## End-to-end acceptance and remaining scope

A friend can unzip the portable release outside the game folder, follow the README, launch through the tested route and see native activation and ghost controls with unmodified original binaries. Demonstrate normal game exit and another successful start. Exercise a single-player contract and, when a second client is available, a private contract with both clients on the same package: role selection, movement/actions, rewards and lobby return. Preserve all reward calculations. Keep FINDINGS entry 1 open until its full gameplay checklist passes; check current FINDINGS entries and only remove issues independently resolved by this work.

Existing gameplay defects, hot unloading, unknown future game versions, a managed rewrite, larger lobbies, public matchmaking changes and resident watchers are outside this plan. A missing second client or unproven Steam wrapper is a named verification limit, not a successful acceptance check. No browser/page audit applies to this native launcher; inspect the actual Win32 dialogs. No Git/CI exists, so report local validation and runtime evidence without claiming a pipeline pass.

