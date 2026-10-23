# Game SDK Stage 2a: Platform Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give the game API everything the migration of the suite needs, so that stage 2b can move every mod, `core/` and `host/` onto it without anything reaching IL2CPP directly.

**Architecture:** This plan adds five things:
- **Runtime services in the host contract:** strings, arrays, lists, type objects, object creation, alive checks, virtual dispatch and GC holds. The IL2CPP object layouts then live only in the host.
- **New entry kinds:** static fields, icalls, IL2CPP exports, scene facts and native UnityPlayer sites. Each kind is resolved at startup and, where the files allow, by the offline check.
- **Per-body hook dispatch:** a detour gets the right `original` for whichever twin it was entered through, so hooks with different twin policies coexist.
- **The scene-fact check:** an offline reader built on Python and UnityPy (approved 2026-10-23).
- **API hardening:** the review items listed before the SDK reaches outside authors.

**Tech Stack:** C++20 / MSVC / CMake / Ninja, MinHook 1.3.4 with HDE64, Cpp2IL (external), Python 3 with UnityPy (pinned, offline check only), PowerShell.

**Spec:** `docs/superpowers/specs/2026-10-23-game-sdk-design.md` (approved; "Third-party mods" added 2026-10-23). The foundation plan `docs/superpowers/plans/2026-10-23-game-sdk-foundation.md` is done and merged; read its Global Constraints too.

## Global Constraints

- Everything in the foundation plan's Global Constraints still holds. Above all:
  - the resolver never picks among candidates;
  - facts fail closed;
  - nothing crashes or hangs the game at startup;
  - no machine paths, personal details or derived game data are committed;
  - nothing touches the anti-cheat.
- **Host contract versioning:** this plan raises the host contract to **2.1**. `PgHostApi` only grows at the end. A mod built for 2.0 keeps loading.
- **Documentation:** every new public declaration in `core/mod_api.h`, `sdk/api/*.h` and `sdk/fake/*.h` is documented: what it does, its thread (main thread unless stated), and what happens when the entry is unresolved.
- **Approved dependency:** UnityPy is approved for the offline check only, pinned to one version in `tools/assetcheck-requirements.txt`. Python runs isolated (`-I`), on copies of the game's files outside the game folder.
- **Migration is out of scope:** nothing in this plan changes a mod's behaviour. The existing raw IL2CPP paths keep working until stage 2b replaces them.

## Review Focus

1. **A twin entered through one body runs its own chain:** a detour that calls `original` from body B must reach B's chain, never A's. Pinned in Task 4's `original_follows_the_entered_body`.
2. **Reading a static field on an unresolved or not-yet-initialised class** must not crash. It returns a default and logs once. Pinned in Task 2's `static_field_unresolved_returns_default`.
3. **A scene path that is missing in the running scene** never dereferences null. The lookup returns null, logs the fact once per scene, and the feature stays off. Pinned in Task 5's `missing_scene_path_is_null_and_logged_once`.
4. **A native site whose bytes changed:** the patch is refused, nothing is written, and the log says why. Pinned in Task 6's `changed_bytes_refuse_the_patch`.
5. **A string with non-ASCII text** (Malayalam names, UTF-16 surrogate pairs) round-trips through the string services. Pinned in Task 1's `utf8_round_trip_with_surrogates`.

---

### Task 1: Runtime services in the host contract

**Files:**
- Modify: `core/mod_api.h`. Version 2.1, and new `PgHostApi` members appended at the end.
- Modify: `host/suite.cpp` (fills the new members), `host/game_map.*` (type objects for Type entries).
- Create: `sdk/api/runtime.h`, `sdk/api/runtime.cpp` (added to `pgmod`).
- Modify: `sdk/fake/fake_game.*`.
- Test: `tests/game_api_test.cpp`.

**Interfaces:**
- New `PgHostApi` members, plain C:
  - `void* (*string_new)(const char* utf8);`
  - `size_t (*string_utf8)(const void* str, char* out, size_t size);`
  - `size_t (*array_length)(const void* arr);`
  - `void* (*array_data)(void* arr);` (a pointer to element 0)
  - `void* (*array_new)(const char* elementTypeKey, size_t length);`
  - `int32_t (*list_count)(const void* list);`
  - `void* (*list_items)(const void* list);` (the backing array)
  - `void* (*type_object)(const char* typeKey);` (System.Type for a Type entry)
  - `void* (*object_new)(const char* typeKey);`
  - `bool (*is_instance)(const void* obj, const char* typeKey);` (follows the class's parents)
  - `bool (*alive)(const void* unityObject);` (UnityEngine.Object's implicit bool)
  - `const void* (*virtual_method)(const void* obj, const char* methodKey);` (the override the object runs)
  - `uint32_t (*gc_hold)(void* obj);`
  - `void (*gc_release)(uint32_t handle);`
  - `void (*log_exception)(const char* where, const void* exception);`
- In `sdk/api/runtime.h`, namespace `pg::api`:
  - `std::string to_utf8(const void* str)` and `void* make_string(std::string_view)`.
  - `template <class T> struct Array { void* raw; size_t size() const; T& operator[](size_t) const; }` for value elements, and `RefArray` for reference elements.
  - `List` with the same accessors.
  - `Type<D>` gains `type_object()`, `make()` and `is(obj)`.
  - `bool alive(const void*)`.
  - `Method<...>::virtual_call(obj, args...)` calls the override, falling back to the declared body.
  - `il2cpp::Held` moves to `sdk/api/held.h`, built on the contract's gc_hold and gc_release. The core copy stays for the host's own code until stage 2b.

- [ ] **Step 1: Write the failing tests** against the fake backend:
  - `utf8_round_trip_with_surrogates`: `"ടഴദബ"` and `"😀"` survive `make_string`, then `to_utf8`.
  - `array_and_list_access`: a fake `int[3]` and a fake `List` of three references read back by index.
  - `type_object_and_make`: return the fake's objects for a Type entry, and null when it is unresolved.
  - `virtual_call_reaches_override`: a fake override registered for a derived class is called.
  - `alive_false_for_destroyed`.
  - `mod_built_for_2_0_still_loads`, in mods_test: a 2.0 mod against a 2.1 host.
- [ ] **Step 2: Run** `ctest --test-dir build -R "game_api|mods" --output-on-failure`. Expected: it fails to compile.
- [ ] **Step 3: Implement.** The host side uses `core/il2cpp.*`, which is the only place that knows the layouts.
- [ ] **Step 4: Run** `.\validate.ps1`. Expected: pass.
- [ ] **Step 5: Commit.**

### Task 2: Static fields, icalls and IL2CPP exports as entries

**Files:**
- Modify: `sdk/api/entry.h` (kinds `Icall` and `Export`; static fields keep `Kind::Field` with `kStatic`).
- Modify: `sdk/resolver/*`, `sdk/runtime/runtime_model.*`, `host/game_map.*`.
- Modify: `sdk/offline/gamecheck.cpp` (exports from the PE export table; icalls listed as "checked at startup only").
- Modify: `sdk/api/game_api.h` (`StaticField<D, T>`, `Icall<D, Sig>`, and an `Export` handle for host-only use).
- Test: `tests/sdk_resolver_test.cpp`, `tests/game_api_test.cpp`.

**Interfaces:**
- `PgGameEntry` for a static field carries the FieldInfo* (already present). `StaticField::get/set` go through two new contract functions, `static_get(key, out, size)` and `static_set(key, in, size)` (with the write barrier for references).
- An icall entry's `name` is the icall string, for example `UnityEngine.Animator::SetBoolID`. The runtime resolves it through `il2cpp_resolve_icall` at startup.
- An export entry names a GameAssembly export, for example `il2cpp_shutdown`, and resolves from the module's export table both at runtime and offline. Only the host may hook one.

- [ ] **Step 1: Write the failing tests:**
  - `static_field_get_set` (fake).
  - `static_field_unresolved_returns_default`: it returns 0, logs once, and does not crash.
  - `icall_resolves_by_name` (fake runtime).
  - `export_resolves_offline_from_pe` (ImageBuilder with an export table).
  - `missing_export_is_missing`.
- [ ] **Step 2: Run** the focused tests. Expected: they fail to compile.
- [ ] **Step 3: Implement.** Reading a static field may run the class's static constructor in the game, so the docs say static reads belong in game-time code, never at startup.
- [ ] **Step 4: Run** `.\validate.ps1`. Expected: pass.
- [ ] **Step 5: Commit.**

### Task 3: API hardening for outside authors

These are the Task 9 and Task 10 review items from the foundation ledger.

**Files:** `sdk/api/game_api.h`, `sdk/api/game_api.cpp`, `core/mod_api.h`, `host/suite.cpp`, `host/game_map.cpp`, `host/hooks.cpp`, `sdk/offline/gamecheck.cpp`, `sdk/README.md`, and the tests named below.

- [ ] **Step 1: Write the failing tests:**
  - `registration_needs_a_call`: move `slot` and `registered` into variable templates, so dropping the odr-use fails on MSVC. Assert the exact required set in `only_called_entries_are_required`.
  - `wrapper_arity_matches_entry`: a `static_assert` gives a compile error for a wrong-arity wrapper. Show it with a `PG_EXPECT_COMPILE_FAIL` check in a separate CMake try-compile test.
  - `log_once_is_atomic`: two threads call an unresolved optional entry, and exactly one log line appears.
  - `gamecheck_mods_checks_api_version`: a version-1 fake mod is reported as "would not load (contract 1.0)".
  - `gamecheck_unreadable_mods_dir_exits_2`.
  - `publish_twice_is_refused`.
- [ ] **Step 2: Run** the focused tests. Expected: they fail.
- [ ] **Step 3: Implement** them, plus these doc fixes:
  - the `pgmod_requires` array lifetime, stated the same way in both headers;
  - struct parameters "by address, the callee may modify";
  - a "do not keep across frames" warning on every reference getter;
  - calls are non-virtual unless `virtual_call` is used;
  - document `desc`/`use`.
- [ ] **Step 4: Run** `.\validate.ps1`. Expected: pass.
- [ ] **Step 5: Commit.**

### Task 4: Per-body hook dispatch

This replaces the refusal of mixed twin policies.

**Files:** `sdk/api/game_api.h` (the hook API), `host/entry_hooks.*`, `host/hooks.cpp`, `core/hooks.h`, `host/context.cpp`, `core/mod_api.h` (2.1), and `tests/hooks_test.cpp`.

**Interfaces:**
- `pg::api::hook<M>(Detour detour, const char* name, Twins twins)`, where a detour has the signature `R detour(pg::api::Original<M> original, A... args)`. `original(args...)` calls the next detour in the chain of the body the call came in through.
- The mod-side SDK instantiates one trampoline function per body slot, `template <int I> R body_entry(A... a)`, for up to `kMaxBodies = 8` bodies. A method with more twins than that is refused with a log line.
- Each body has its own MinHook chain. An all-twins hook adds one hook per body, and a CanonicalOnly hook adds one to the canonical body only.
- `create_shared` and the refusal of mixed policies go. Raw `hooks::create` stays for the host's own code and for unmigrated mods until stage 2b, and it is per body.
- The contract's `hook_entry` becomes `hook_body(key, bodyIndex, detour, original, name)` plus `body_count(key)`. The old member stays and keeps working until the major version changes.
- `pg::context::track<M>()` tracks every body by the same per-body mechanism.

- [ ] **Step 1: Write the failing tests:**
  - `original_follows_the_entered_body`: twins A and B have different chains (an all-twins hook D1, then a CanonicalOnly hook D2 on A). A call through B runs D1 and then B's real code. A call through A runs D2, then D1, then A's real code.
  - `mixed_policies_now_coexist`: both orders.
  - `more_than_eight_bodies_refused`.
  - `rollback_per_body`: a mods restart removes every per-body hook newest first.
  - `tracking_counts_every_body`.
- [ ] **Step 2: Run** `ctest -R hooks`. Expected: it fails.
- [ ] **Step 3: Implement** it, then delete the refusal code and its tests, which the new behaviour replaces. The ledger entry for the foundation's Task 10 explains why.
- [ ] **Step 4: Run** `.\validate.ps1`. Expected: pass.
- [ ] **Step 5: Commit.**

### Task 5: Scene and asset facts

**Files:**
- Modify: `sdk/api/entry.h` (`Kind::Scene`, with the fields `scene`, `path` and `component`; `Kind::Layer`; `Kind::AnimatorParam`).
- Create: `sdk/map/Scene.h` with the first entries: the journal's `GhostHuntingBookContent` path and the PlayerView layer. Read `core/ui.cpp`, `mods/playable_ghost/journal.cpp` and `mods/custom_ghosts/pages.cpp` for the paths used today.
- Modify: `sdk/api/game_api.h` (`SceneObject<D>::find(root)`, `Layer<D>::index()`, `AnimatorParam<D>::hash()`).
- Modify: `host/game_map.cpp`. Layers resolve at startup through the mapped `LayerMask.NameToLayer`. Scene paths resolve lazily on first `find`, are cached per scene load, and are logged once when missing. Animator parameters are checked against `Animator.parameters` on first use.
- Create: `tools/assetcheck.py` and `tools/assetcheck-requirements.txt` (UnityPy, pinned).
- Modify: `tools/gamecheck.ps1`. It copies the needed `Phasmophobia_Data` level and asset files, makes a venv in `re/dump/venv` on first run, runs `python -I tools/assetcheck.py`, and merges its lines into the report.
- Test: `tests/game_api_test.cpp` and a `tests/assetcheck_test.ps1` with a tiny synthetic fixture. If UnityPy cannot write one, the fixture is a recorded JSON of what the reader emits, and the parser is tested on it.

**Interfaces:**
- `assetcheck.py --data <copied Phasmophobia_Data> --entries <tsv from gamecheck>` prints `OK|MISSING <key> <detail>` per scene, layer or animator entry. It exits 1 on any missing required entry.
- `gamecheck --scene-entries <tsv>` writes the scene, layer and animator entries for it.

- [ ] **Step 1: Write the failing tests:**
  - `missing_scene_path_is_null_and_logged_once` (fake).
  - `layer_by_name` (fake NameToLayer).
  - `animator_param_checked` (fake).
  - The assetcheck parser fixture: one present path, one missing, and the layer table.
- [ ] **Step 2: Run** them. Expected: they fail.
- [ ] **Step 3: Implement** it. Find which level files hold the menu and the journal by reading them with UnityPy from a scratch copy. Earlier RE found the menu UI in `level2` and the MonoScript names in `globalgamemanagers.assets`. Record the findings in `sdk/systems/scene.md`.
- [ ] **Step 4: Run** `.\validate.ps1`. Then run the configured `tools/gamecheck.ps1`, with the game and Cpp2IL paths only in your process environment. Expected: the scene entries report OK on build 25811160.
- [ ] **Step 5: Commit.**

### Task 6: Native UnityPlayer sites

**Files:**
- Modify: `sdk/api/entry.h`. `Kind::NativeSite` takes `assembly = "UnityPlayer"`, `pattern` (wildcards allowed), `patternOffset`, and `within`, where `within` is the name of an exported UnityPlayer function to search inside, or `".text"` with a bounded unique search.
- Create: `sdk/map/UnityPlayer.h`, with the steering site (today `UnityPlayer+0xA4FB7A`, expected bytes `F3 0F 10 83 90 02 00 00`) and `NavMeshAgent::GetInternalAgent` (today `+0xA432F0`, prologue `48 8B 51 60`). Read `mods/playable_ghost/steer.cpp` and `steer_stub.asm`.
- Modify: `sdk/offline/gamecheck.cpp`, which also maps a copy of UnityPlayer.dll and resolves native sites in it, and `tools/gamecheck.ps1`, which copies UnityPlayer.dll.
- Modify: `host/game_map.*` (native sites resolved over the loaded UnityPlayer module), and `core/mod_api.h` 2.1: `bool (*patch_site)(const char* key, const void* expected, size_t expectedSize, const void* stub, size_t stubSize, void** page)`.
  - The host checks the bytes, allocates a page within rel32 range, writes the jump, and returns the page.
  - Removal goes through the rollback group.
  - Nothing is written if the bytes differ.
- Test: `tests/sdk_code_test.cpp` (resolving a native site in an ImageBuilder module), and `tests/hooks_test.cpp` (`changed_bytes_refuse_the_patch` and `patch_and_rollback`, using real code pages in the test process).

The steering migration itself is stage 2b. This task only provides the kind, the two entries, and the host's patch service.

- [ ] **Step 1: Write the failing tests** named above.
- [ ] **Step 2: Run** them. Expected: they fail.
- [ ] **Step 3: Implement** it.
- [ ] **Step 4: Run** `.\validate.ps1`. Then run the configured gamecheck. Expected: the two native sites resolve to `+0xA4FB7A` and `+0xA432F0` on the installed UnityPlayer.dll.
- [ ] **Step 5: Commit.**

---

## The plans that follow

- **2b, migration:** one task per area: `core/game.*` and `core/players.h`; `core/ui.*`, `core/notice.cpp` and `core/bindings.cpp`; `host/`; Spectral Flame; Ghost Tells; Custom Ghosts, split by file group; Playable Ghost, split by file group, including steering on the native site service. Each task works the same way:
  1. Add the entries the files need, with RE notes.
  2. Prove each with the offline check, and compare every value against the constant it replaces before deleting the constant.
  3. Switch the files to the game API.
  4. Run the existing tests on the fake backend.
  5. Close with a live check by the user.

  About 1,140 raw sites are counted today.
- **2c, safety switch-over:**
  - checked-build records (`sdk/builds/`, map digest) replace `host/build_check.cpp`;
  - the records go into the package, and the release script refuses a stale one;
  - the source scan test rejects raw addresses, offsets, obfuscated names and direct IL2CPP outside `sdk/`;
  - the Mods panel shows unavailable mods with the reason;
  - the memory notes' RE facts move into entries and overviews;
  - the twins of hooked methods are investigated and reported;
  - the first tracked entry (the weighted ghost-type roll) replaces the return-address check.
- **2d, outside authors:**
  - the AppData mods folder;
  - `author=`;
  - hashed channels from 64 to 255;
  - the consent line for other authors;
  - the generated game API reference and the documentation test;
  - the template mod;
  - the `PhasmoMods-SDK-<version>.zip` built by package.ps1 and attached by the workflow.
