# Rules for agents working on PhasmoMods Loader

These are the user's standing rules and decisions, gathered from earlier sessions. Every agent follows them: implementers,
reviewers and planners. When a rule here and a plan disagree, ask the user. Dates are when the user decided. Decisions marked
"don't re-ask" are settled.

Where things live:
- The work in progress: `docs/superpowers/plans/` and `docs/superpowers/specs/`. Stage 2b's ledger is
  `.superpowers/sdd/stage2b/progress.md` (gitignored, force-added on the cloud branch). Read the ledger's last entries before
  starting, and append to it as you go, so the next agent can pick up after a usage limit.
- How implementers and reviewers work: `.superpowers/sdd/implementer-rules.md` and `reviewer-rules.md`.
- Game facts: `sdk/map/*.h` entry notes and `sdk/systems/*.md`. Raw numbers live only in gitignored `re/dump/`.
- Problems not fixed yet: `FINDINGS.md`. Ideas not scheduled: `docs/mod-ideas.md`.

## What AI may and may not make (AI_POLICY.md)

- AI is used for reverse engineering, C++ programming and documentation.
- **AI never generates assets, UI, player-facing text or ideas** (user, 2026-11-03). No art or sound; no new screens,
  windows or visual design; nothing a player reads in the game (journal entries, ghost descriptions, popups, notices, menu
  labels); no ghosts, tells, mods or designs. Carry out the user's ideas and put the user's own words into the code. Where
  code needs player-facing text the user has not written, add a table entry marked `PM_TEXT_TODO` and list it in the
  ledger for the user, and don't invent the wording.
- **Player-facing text lives in string tables, never in code** (user, 2026-11-03, SMAPI-style): the loader's
  `i18n/default.json` and each mod's `mods/<folder>/i18n/default.json`, read with `pm::i18n::text("key", {{"token", value}})`
  (`core/text_table.h`). Text AI wrote before the policy stays until the user replaces it, each entry marked `PM_TEXT_AI`;
  don't rewrite it yourself. `docs/player-text.md` explains the tables; `text_table_test` checks them against the code.

## Safety and the anti-cheat

- **The game has a startup anti-cheat that reports the player's account to moderation.** A loader DLL in the game folder
  (a `version.dll` proxy, and a standard BepInEx install) has triggered it. So: never put loader files in the game folder,
  never launch Phasmophobia, never inject. Only the user launches the game, for each live test, and is told the ban risk first.
- **Never build anti-cheat evasion:** no hiding modules, no suppressing its report, no patching its checks. Nothing in the SDK
  maps, calls, alters or describes the anti-cheat or its report. Never copy how a cheat menu evades it.
- **Kinetic Games' position (as the user reported it, corrected 2026-11-03):** mods are fine in private lobbies only, as long as
  they don't add excessive amounts of in-game currency or collectables, which would break progression. So mods run only in
  private lobbies. Playable Ghost's payout is fair and based on vanilla numbers (user, 2026-11-03): don't raise it again.
- **Never weaken the game's own checks on who sent an RPC**, even incidentally. A refusal can harm an account
  (`sdk/systems/photon.md`, "The game's checks on an RPC"). Never send a game RPC by name or invoke an RPC's body.
- BepInEx was removed from the project (2026-10-21). Don't re-add a BepInEx plugin or mention it in docs.
- No credentials, machine paths, personal details or derived game data in any committed file.

## How to work

- **Never invent game behaviour** (2026-10-24). Every claim about what the game does, and every mechanic a mod mirrors, is
  proven from reverse-engineered game code, cited by map key. If a behaviour is unknown, say so and reverse engineer it. Never
  fill the gap with a guess. When the user asks for something "like vanilla", reverse engineer the vanilla path and cite it.
  Reviewers check every behaviour claim for its evidence.
- **Fix causes, not symptoms** (2026-11-01). When the loader or a mod misbehaves, find what our code does differently from
  vanilla and fix that. Never add a hook, patch or detour whose only job is to undo a side effect of our own code. "Vanilla does
  this too" is a claim that needs proof from decompiled code or a vanilla run. (The example the user rejected: an exit-hang
  workaround that hooked the runtime's shutdown and cancelled Photon's socket. The unmodded game exits fine, and every earlier
  exit hang was our own bug.)
- **Research or ask before choosing a convention** (2026-10-30): names, prefixes, file formats, API shape, documentation
  standard, versioning, manifest and dependency schemes. Check established SDKs and mod loaders (SMAPI, BepInEx, MelonLoader,
  Thunderstore, Steamworks, the Unity and Windows SDK references), say which one a choice follows, or ask. When a choice is a
  matter of taste or hard to reverse, give the user the options.
- **Dynamic over hard-coded** (2026-10-24). Any count, range, list, order, layout size or name the game holds is read from the
  game through the map at runtime, never written into the code. Fail closed only when it cannot be read. Example: the first
  custom ghost type is the largest `GhostType` value plus 1, and vanilla ghost ids come from the enum's own value names.
- **Vanilla UI only** (2026-10-21). Show mod content on surfaces the game already has: journal pages, lobby boards, the truck
  monitor, cloned game widgets, the game's own popup. Never invent a window, panel or overlay. A design that needs one goes to
  the user as a question first. A copied widget keeps the game's own button component and click events, so hover, press,
  selection and navigation come from the game; never switch the component off and poll the mouse. Existing code gets no pass:
  flag any mod-drawn window you touch. Those still waiting for the user's design carry `PM_UI_TODO` (FINDINGS.md 31).
- **Small behaviour differences in a migration are allowed** (2026-10-24), but each one is recorded in the task report and the
  ledger, and offline test coverage never drops.
- Stop and ask (BLOCKED or NEEDS_CONTEXT) rather than guess when a task needs an architectural choice its brief does not make.
  Bad work is worse than no work.

## The SDK

- **The loader does all interfacing with the game** (2026-10-23). Mods, `core/` and `host/` use only the de-obfuscated map:
  never addresses, offsets, layouts, obfuscated names or direct IL2CPP calls. The interface is low-level and game-shaped
  (game classes, methods and fields under readable names).
- **The SDK is for other people too.** Community authors must be able to make mods with no reverse engineering (2026-10-30):
  players, the ghost, map data, rooms, sanity, equipment, weather, the journal, voice chat, everything the bundled mods need,
  and whatever fills a gap. A common question that needs reverse engineering to answer is an SDK gap (stage 2b Task 17).
- **Reverse engineering reaches the SDK** (2026-11-01). A game member you decompile and rely on becomes a documented map entry
  when a mod author could reasonably use it: something a mod would read, call, hook or change. Skip internals with no use to an
  author (obfuscated helpers, look-alike decoys, compiler-generated bodies, plumbing a mapped entry already covers). Behaviour an
  author needs goes in the entry's note or `sdk/systems/<area>.md`. Never leave a useful fact only in a gitignored report.
- **Reverse-engineering notes go into the docs** (user, 2026-11-03) wherever they may help a mod author or someone
  expanding the SDK: author-facing facts in `sdk/systems/<area>.md` and the entry notes, and the workflow, tools and pitfalls
  of mapping the game in `sdk/internals/`. They follow the address rule below, and nothing about the anti-cheat is written.
- **Obfuscated names are never named** (user, 2026-11-03). The game's obfuscated (Malayalam) names are shuffled on every
  update, so code, docs, notes and map entries describe an obfuscated member only by its shape and function. No entry keeps
  one, not even as a hint (all removed 2026-11-03). `no_obfuscated_name_in_the_docs` guards the map and the docs.
- **No addresses in the SDK's docs** (2026-10-30). `sdk/README.md`, `sdk/systems`, `sdk/guides`, `sdk/examples` and the comments
  in `sdk/map` and `sdk/api` cite map keys (for example `Difficulty.ApplyDifficulty`), never an RVA, a field offset or a raw
  address. Build numbers may stay. Raw numbers go in gitignored `re/dump/expect`, `re/dump/notes` and the build ledger.
  Plans and specs under `docs/superpowers` are maintainer history and outside this rule.
- **Every public declaration is documented as an interface** (2026-10-30), so an author never needs the implementation: what
  it does, parameters (units, range, null, game type), return (and the value when unresolved), thread, game phase, host only or
  every client, networking and side effects, and for a hook when the game calls it and what skipping `original` does
  (Doxygen, stage 2b Task 14 Step 1e).
- **The prefix is `pm`** for PhasmoMods (renamed from `pg`, a Playable Ghost leftover, in stage 2b Task 14 Step 0):
  `pm::`, `Pm*` types, `PM_` macros, `pmmod_*` exports, the `pmcore` and `pmmod` targets, and the `// pm:not-game-data:`
  marker. `phasmo::` (the game) stays. The host still loads a mod built before the rename by its old export names
  (`core/mod_exports.h`), logging once, until the next major contract version. Read `pg` names in older plans and specs as `pm`. No old name may come back (`no_old_prefix_left`).
- **Typed handles** for the game API (`pm::api::Ref<D>`), so a wrong object type fails to compile (stage 2b Task 14 Step 0b,
  moved to after the playtest by the user, 2026-11-03). The C host contract stays `void*`.
- **The host contract only grows at the end.** `PmHostApi` and every published struct only grow at the end, every content
  struct begins with `uint32_t size`, and a mod built for an older minor keeps loading.
- **A new game build runs automatically** (2026-11-01). On an unseen build the consent notice is asked again once, with a
  paragraph that the game updated. Mods whose entries resolve run; the rest stay off and are named.

## Mods, lobbies and players

- **The loader owns all logistics** (2026-10-28, widened 2026-10-29): mod matching, lobby rules, loading, dependencies, content
  registration, sync and saves, networking. No mod implements its own "does every player run me" check, and mods get at most one
  generic read-only query, never a per-feature switch. Reject per-mod mismatch handling in plans and reviews. Mods message
  peers only through the SDK's one channel; what they put in their own messages is their business (2026-11-01).
- **The host's list is the lobby's list** (2026-11-03). Every mod treats loading the same. The host has the definitive say on
  the lobby's mods. Any joining player must have those mods or is kicked, vanilla players included. No player can change which
  mods are part of the lobby. A joiner never idles anyone else's mods.
- **A contract only begins when every player's mods are confirmed** (2026-11-03). On Start the master closes the room, waits
  for the server's confirmation, re-reads who is there, then starts or reopens the room with the waiting popup. Nobody gets in
  late, so a contract is always fully modded and no decision changes during one. Every guarantee about who is in a contract
  rests on this one rule, so no mod and no permission gets its own check (2026-11-03: not for the Echo's voice recording, and
  not for future permissions beyond `recordsVoice`).
- **Lobby rules** (2026-10-28 to 2026-11-02):
  - Every mod is required by every player. `PM_MOD_VANILLA_PEERS` is reserved and ignored.
  - A vanilla host: mods are off for every player in that lobby, with a vanilla-UI notice, and back on after leaving.
  - A joiner lacking a host mod, or another build of one, is shown the lobby's full required list, then removed.
  - A joiner with extra mods only stays; its extras are off for that lobby, with a notice.
  - Private lobbies only: in a public lobby every mod is off for that room, the host's too. Single player counts as private.
  - Steam achievements are left alone. Don't raise blocking them again unless the user does.
  - Nobody changes the list from inside a room: the Mods panel's changes wait until out of the room.
  - No downloading mods from the host: mods are native DLLs, so it would be remote code execution.
- **No mod code runs before consent** (2026-11-02). The host reads manifests only until "I agree", then loads DLLs.
  `recordsVoice` is declared in the manifest.
- **Dependencies are the loader's** (2026-10-29): required and optional, declared in the manifest. Custom Ghosts requires
  Spectral Flame; Playable Ghost optionally uses Custom Ghosts. Dependencies start first, a missing required one leaves its
  dependents off with the reason in the Mods panel, and cycles are refused.
- **Mod ids are `<author>.<mod>`** (2026-10-31, as SMAPI). Bundled mods use the author `phasmomods`, and the `phasmomods` id
  author and `PhasmoMods` manifest author are reserved for mods in the install folder. Mods declare themselves in
  `manifest.json`.
- **Licence:** Apache-2.0 (as MelonLoader), copyright Codyjames345. A community mod's release ships LICENSE and NOTICE.
  No paid code signing; SECURITY.md relies on GitHub's private vulnerability reporting.
- **Saves and other players:** a custom ghost type or custom evidence value never reaches a save file or a player without the
  content.
- **The journal in the lobby** (2026-10-28): every journal change a mod makes (custom pages, list entries, evidence rows and
  buttons, lines on vanilla pages) also shows in the lobby's journal while content is active there. How the lobby journal is
  built comes from reverse engineering.

## Ghosts

- **AI and player follow the same rules** (2026-10-31). A ghost's player-controlled and AI versions always share one ruleset:
  targets, kill limits, tie-breaks, speeds, held-back rules, tells and abilities. Never introduce a difference unless the user
  allows that specific one; fix it or ask. The player makes the decisions the AI makes (where, whom, when) but keeps the AI's
  senses, limits and tells. Allowed exceptions, all by the user:
  1. the Yokai player hears voices at any volume within the game's range;
  2. Echo Speak, Kikimora Tidy and Gremlin Short-out are used on demand rather than on timers;
  3. interactions are aimed and paid for rather than rolled, with the type's odds setting the cost;
  4. free steering in hunts (no automatic walking), though the Bogeyman's search still runs on reaching a hiding place and the
     Black Shuck's scent corridor stays;
  5. the Mimic player picks what it copies at each native switch;
  6. provisional, judged in the first in-game test: investigators after the ghost player move down one slot.
- **Every ghost is identifiable** (2026-10-30) in single player, on zero evidence, on every map and in every weather, all at
  once. Behaviour that shows only in multiplayer, with evidence, or in some weather is fine as an extra.
- **Custom ghost tells** (2026-10-21, don't re-ask): every custom ghost has a unique zero-evidence tell; nothing strays from
  vanilla mechanics; footsteps use the game's own step system; hunt speed rules set the hunt's own base speed so the game
  computes the real speed, and there are no speed caps anywhere. Per-ghost decisions are in
  `docs/superpowers/specs/2026-10-21-custom-ghost-tells-design.md` and the ledgers.
- An extra sign a ghost always shows is a tell (like the Mimic's orbs), not evidence. Evidence priority follows the game's
  keep-last rule.
- **Ghost Tells** (2026-10-21): the 18 approved tells for Spirit, Mare, Shade, Yurei, Goryo and Onryo run on every contract,
  never only on zero-evidence ones, and no document may mention such a limit. Keep vanilla mechanics; no tell overlaps another
  ghost's. The Goryo has no leash.
- **Spectral Flame and Tempest** decisions are in their design specs and are settled; don't re-ask them.
- **Playable Ghost** decisions are in `docs/design.md`; build on them, don't re-ask. Evidence and unique behaviours stay exactly
  vanilla. The win is vanilla. The ghost player has no body in the contract and is out of the game's player list.
- **The ghost that leaves the building** (2026-11-01): one ghost type will leave the house as its tell. It is stage 2b Task 18,
  held until the user says to start; the SDK side is Task 17.

## Testing and releases

- Each task: tests first (red, then green), then `.\validate.ps1`. A task that moves values also runs
  `.\tools\gamecheck.ps1 -IfConfigured -Expect` and shows the result in its report. Never weaken a check to make it pass.
- Live checks collect in the smoke list; no task waits on one. The user runs every in-game test.
- User-facing text has no em dash and uses active voice.
- Commit messages: an imperative subject under about 72 characters, then prose saying what was wrong and why this change is
  right (no bullet lists, no conventional-commit prefixes).
- The release after stage 2b Task 14 is public; the updater moves every install to it. The merge and the release each need the
  user's go at that moment.
- **Branches** (user, 2026-11-03): work and test locally, push to `development` to test with friends (each push publishes a
  pre-release that only those who ticked the loader's box take), merge to `main` with a merge commit, never a squash, then tag on
  `main` with `tools/release.ps1` and merge `main` back. `docs/releasing.md` has the steps. A push to `development` is public, so it
  needs the user's go as a merge does.
