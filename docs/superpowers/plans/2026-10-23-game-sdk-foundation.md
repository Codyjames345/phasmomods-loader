# Game SDK Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The loader can find game types, methods, fields, enum values and inlined code by name, shape and machine-code fingerprint, at startup and offline. Mods reach them through a typed game API, and the first real entries and mod calls run on it.

**Architecture:** A model-agnostic resolver (`sdk/resolver`) matches declarative entries (`sdk/api/entry.h`, declared per game type under `sdk/map`) against a `GameModel`. The host builds that model from the IL2CPP runtime. The offline check (`gamecheck`) builds it from a Cpp2IL dump, and both read machine code through one PE image reader using HDE64. The host publishes the results through the host contract (version 2). Mods call `phasmo::` wrappers whose entries register themselves when used, so the host knows each mod's requirements before starting it.

**Tech Stack:** C++20 / MSVC / CMake / Ninja, MinHook 1.3.4 and its HDE64 (vendored), Cpp2IL 2022.1.0-pre-release.21 (external tool), PowerShell, Node for the existing `re/` scripts.

**Spec:** `docs/superpowers/specs/2026-10-23-game-sdk-design.md` (approved 2026-10-23). This is stage 1 of its three stages. The roadmap at the end says what stages 2 and 3 cover. Read the spec before starting.

## Global Constraints

- Terms: **game SDK** is the whole kit and the `sdk/` directory; **game API** is what mods call (namespace `phasmo`); **host contract** is `core/mod_api.h`.
- Audited build: Steam build 25811160, GameAssembly.dll sha256 `e300eda993f2479922628f1b44f59e60c156c240f5a07c7289c4bd0fe4b790c8`. The host's compiled-in hash gate in `host/build_check.cpp` stays unchanged in this plan. The checked-build records replace it in stage 2.
- No new dependency. Instruction decoding uses `third_party/minhook/src/hde/hde64.h`.
- Cpp2IL runs as `--use-processor attributeinjector --output-as diffable-cs`, which emits `[Address(RVA = ..., Length = ...)]`, `[FieldOffset(Offset = ...)]` and enum literals.
- Derived game data is never committed: `re/dump/`, `re/names.tsv`, `re/map-addresses.tsv` and `re/out/` are gitignored. Map entries (names, shapes, facts, hints, notes) are committed.
- Paths to the game and to Cpp2IL come from `-GameDir`/`PG_GAME_DIR` and `-Cpp2Il`/`PG_CPP2IL`, never from the repository.
- The anti-cheat class, `CloudEvents.SelfReport` and the moderation bindings are never mapped, hooked or called.
- The resolver never picks among candidates. Zero means `Missing`, and more than one distinct behaviour means `Ambiguous`.
- Confidence is one of `Live` (seen in the game; the note gives the date), `Decompiled`, `Inferred`. A note never claims more than its confidence.
- The host contract carries only plain pointers, integers and C strings. Nothing thrown crosses it.
- Managed objects kept across frames are `il2cpp::Held` (`tests/held_scan_test.cpp` enforces this).
- Migrated code keeps its exact behaviour. Hooks created for existing code use `Twins::CanonicalOnly`, so every hook stays on the body it hooks today.
- No hex address or field offset is added to `mods/`, `host/` or `core/` by this plan.

## Review Focus

1. **A stale hint.** After an update, an entry's last obfuscated name may now belong to a different, wrong-shaped member. It must be ignored, not accepted. Pinned in Task 3's `stale_hint_never_decides`.
2. **The two forms of type names.** The runtime writes `System.Collections.Generic.List<UnityEngine.Vector3>`, `GhostAI/ദശസറദബഹജഭ` and `Player&`, while the dump writes `List<Vector3>`, `ദശസറദബഹജഭ` and `out Player`. Both must canonicalise alike. Pinned in Task 1's paired cases.
3. **A function split across several unwind entries** (chained unwind info). Its code facts must cover the whole function, and looking one up must not crash. Pinned in Task 4's `chained_unwind_entry_maps_to_primary`.
4. **A folded twin**: two names, one address. It must be hooked once, not twice. Pinned in Task 10's `folded_twin_hooked_once`.
5. **Calling an optional entry that did not resolve** must never jump to null. It logs once and returns the type's default. Pinned in Task 9's `unresolved_optional_call_returns_default`.

---

### Task 1: Entry vocabulary and canonical type names

**Files:**
- Create: `sdk/api/entry.h`, `sdk/resolver/types.h`, `sdk/resolver/types.cpp`
- Modify: `CMakeLists.txt`: a static library `pgsdk_resolver` (`sdk/resolver/*.cpp`, includes `sdk/api` and `sdk/resolver`, links `minhook` for HDE64), and `pg_test(sdk_types)`
- Test: `tests/sdk_types_test.cpp`

**Interfaces:**
- Produces, in `namespace pg::sdk`:
  - `enum class Kind : uint8_t { Type, Method, Field, EnumValue, Code };`
  - `enum class Confidence : uint8_t { Inferred, Decompiled, Live };`
  - `enum EntryFlags : uint32_t { kStatic = 1, kOptional = 2, kTrack = 4 };`
  - `struct Fact { enum class Is : uint8_t { Stores, StoresConst, Loads, Calls, CalledBy }; Is is; const char* method; int64_t value = 0; bool negate = false; };`
  - `struct EntryDesc` with these fields, all with default member initialisers so map headers use designated initialisers:
    - `key`, `kind`, `confidence`, `flags`
    - `owner`: key of the declaring Type entry
    - `name`: the real name, or null when obfuscated
    - `hint`: the obfuscated name in the last mapped build
    - `assembly`: image name without `.dll`
    - `ns`
    - `type`: a field's type, a method's return type
    - `std::span<const char* const> params`
    - `std::span<const char* const> fieldTypes`: for a Type found by shape, field types it must have
    - `std::span<const char* const> values`: for an enum Type found by shape, value names it must have
    - `std::span<const Fact> facts`
    - `pattern`: hex bytes with `??`
    - `within`: key of the Method searched
    - `int32_t patternOffset`
    - A type string `"@Key"` means the type that entry `Key` resolved to.
  - `std::string canonical_type(std::string_view raw);`

- [ ] **Step 1: Write the failing tests.** Each row is one `CHECK(canonical_type(in) == out)`:

| raw (runtime form) | raw (dump form) | canonical |
| --- | --- | --- |
| `System.Boolean` | `bool` | `bool` |
| `System.Single` | `Single` / `float` | `float` |
| `System.Int32[]` | `Int32[]` / `int[]` | `int[]` |
| `System.Collections.Generic.List<UnityEngine.Vector3>` | `List<Vector3>` | `List<Vector3>` |
| ``System.Collections.Generic.Dictionary`2<System.String,Player>`` | `Dictionary<string, Player>` | `Dictionary<string,Player>` |
| `GhostAI/ദശസറദബഹജഭ` | `ദശസറദബഹജഭ` | `ദശസറദബഹജഭ` |
| `Player&` | `out Player` / `ref Player` | `Player&` |
| `UnityEngine.Vector3` | `Vector3` | `Vector3` |
| `System.Void` | `void` | `void` |

  Also cover `System.Object`, `System.String`, `System.Byte`, `System.Int64`, `System.Double`, `System.UInt32` and `System.Char`, which map to their C# keywords.
- [ ] **Step 2: Run** `.\build.ps1; ctest --test-dir build -R sdk_types --output-on-failure`. Expected: it fails to compile.
- [ ] **Step 3: Implement** `canonical_type`:
  - strip namespaces and declaring types (everything up to the last `.` or `/`) from every component
  - drop the generic arity suffix
  - map the System primitives to their keywords
  - turn a leading `out ` or `ref ` into a trailing `&`
  - remove spaces after commas
- [ ] **Step 4: Run** the same command. Expected: pass.
- [ ] **Step 5: Commit.**

### Task 2: The game model and resolution by real name

**Files:**
- Create: `sdk/resolver/model.h`, `sdk/resolver/resolver.h`, `sdk/resolver/resolver.cpp`, `tests/sdk_fixture.h` (a `ModelBuilder` for tests)
- Test: `tests/sdk_resolver_test.cpp`, `pg_test(sdk_resolver)` linking `pgsdk_resolver`

**Interfaces:**
- Produces, in `namespace pg::sdk`. Every type string in a model is already canonical:
  - `struct ModelField { std::string name, type; int32_t offset = 0; bool isStatic = false; bool hasLiteral = false; int64_t literal = 0; };`
  - `struct ModelMethod { std::string name, ret; std::vector<std::string> params; bool isStatic = false; uint64_t rva = 0; const void* runtime = nullptr; };` Here `runtime` is the `MethodInfo*` in the host and null offline.
  - `struct ModelType { std::string assembly, ns, name; int declaring = -1; bool isEnum = false; std::vector<ModelField> fields; std::vector<ModelMethod> methods; void* runtime = nullptr; };`
  - `struct GameModel { std::vector<ModelType> types; };`
  - `enum class Status { Resolved, Missing, Ambiguous, Blocked };`
  - `struct Result` with these fields:
    - `const EntryDesc* desc`, `Status status`
    - `std::string detail`
    - `std::vector<std::string> candidates`
    - `int type = -1; int member = -1;`
    - `uint64_t rva = 0;`
    - `std::vector<uint64_t> twins;`
    - `std::vector<std::string> aliases;`
    - `int32_t offset = 0; int64_t value = 0;`
    - `std::string currentName;`
  - `struct Resolution { std::vector<Result> results; const Result* find(std::string_view key) const; bool required_resolved() const; };`
  - `class CodeImage;` (forward; Task 4)
  - `Resolution resolve(std::span<const EntryDesc* const> entries, const GameModel& model, const CodeImage* image);` The `image` may be null, and then entries needing code are `Missing` with the detail `no code image`.
  - In `tests/sdk_fixture.h`: `struct ModelBuilder { int type(std::string assembly, std::string ns, std::string name, int declaring = -1, bool isEnum = false); void field(int type, std::string name, std::string type_, int32_t offset, bool isStatic = false); void literal(int enumType, std::string name, int64_t value); void method(int type, std::string name, std::string ret, std::vector<std::string> params, uint64_t rva, bool isStatic = false); GameModel build() const; };`

- [ ] **Step 1: Write the failing tests:**
  - `type_by_name`: the `GhostAI` entry (assembly `Assembly-CSharp`, ns `""`) gives `Resolved` with that type's index.
  - `method_by_name_and_params`: two `Raycast` overloads in `Physics` with params `{"Ray","RaycastHit&","float","int","QueryTriggerInteraction"}` and `{"Vector3","Vector3","RaycastHit&","float","int"}`. The entry with the first list gets the first overload's rva.
  - `method_by_name_without_params_is_ambiguous_with_overloads`: `Ambiguous`, with both signatures in `candidates`.
  - `field_by_name`: offset `0x40` comes back.
  - `enum_value_by_name`: owner is an enum Type entry, and `hunting` gives `value == 2`.
  - `at_type_resolves_through_entry`: a param `"@GhostState"` matches the type the `GhostState` entry resolved to.
  - `missing_owner_blocks_members`: the members of an unresolved type are `Blocked`, and their detail names the owner.
  - `dependency_order_is_independent_of_declaration_order`: listing members before their owner gives the same results.
  - `required_resolved_ignores_optional`: a `kOptional` entry that is `Missing` leaves `required_resolved()` true.
- [ ] **Step 2: Run** `ctest --test-dir build -R sdk_resolver --output-on-failure`. Expected: it fails to compile.
- [ ] **Step 3: Implement** `resolve`:
  - Topologically order the entries by their references: `owner`, `@Key` in `type`/`params`/`fieldTypes`, `facts[].method` and `within`. A cycle makes every entry in it `Missing`, with the detail `cycle`.
  - Then apply the real-name strategy per kind.
- [ ] **Step 4: Run** the same command. Expected: pass.
- [ ] **Step 5: Commit.**

### Task 3: Resolution by shape

**Files:**
- Modify: `sdk/resolver/resolver.cpp`
- Test: `tests/sdk_resolver_test.cpp`

**Interfaces:**
- Consumes: Task 2's model and results.
- Produces: shape rules for every entry whose `name` is null:
  - **Type:** in the same assembly and namespace, nested in the owner's resolved type (or top-level without an owner), enum-ness matching `values` being non-empty, every name in `values` present as a literal, and every type in `fieldTypes` present among its instance fields.
  - **Field:** the owner's fields with the canonical `type` and the `kStatic` flag.
  - **Method:** the owner's methods with the canonical `ret`, `params` and `kStatic` flag.
  - **The hint** never decides. All candidates are evaluated. `currentName` reports the member's name in this build, and `detail` says `renamed from <hint>` when it differs.

- [ ] **Step 1: Write the failing tests:**
  - `enum_type_by_values`: an obfuscated nested enum in `GhostAI` with literals `idle 0, wander 1, hunting 2, favouriteRoom 3` is found by `values = {"idle","wander","hunting"}`, alongside a second nested enum with other values.
  - `field_by_unique_type`: the only `NavMeshAgent` field of `GhostAI` is found at `0x40`.
  - `field_shape_with_many_candidates_is_ambiguous`: thirteen `bool` fields and no facts give `Ambiguous`, listing all thirteen offsets.
  - `stale_hint_never_decides`: the hint names a `float` field, but the entry wants `bool`. The result is the unique `bool` field, `currentName` is that field's name, and the detail says `renamed from <hint>`.
  - `hint_cannot_break_a_tie`: two `bool` fields where the hint names one gives `Ambiguous`.
- [ ] **Step 2: Run** `ctest --test-dir build -R sdk_resolver --output-on-failure`. Expected: the new tests fail.
- [ ] **Step 3: Implement** the shape rules.
- [ ] **Step 4: Run** the same command. Expected: pass.
- [ ] **Step 5: Commit.**

### Task 4: Reading machine code

**Files:**
- Create: `sdk/resolver/image.h`, `sdk/resolver/image.cpp`, `sdk/resolver/code.h`, `sdk/resolver/code.cpp`, and `tests/sdk_image_fixture.h` (an `ImageBuilder` that writes a minimal mapped PE: DOS and NT headers, an exception directory, and a `.text` region)
- Test: `tests/sdk_code_test.cpp`, `pg_test(sdk_code)` linking `pgsdk_resolver`

**Interfaces:**
- Produces, in `namespace pg::sdk`:
  - `class CodeImage`:
    - `explicit CodeImage(std::span<const uint8_t> mapped);` The bytes are laid out by RVA, as a loaded module is.
    - `bool valid() const;`
    - `std::span<const uint8_t> function(uint64_t rva) const;` The whole function containing `rva`. Its bounds come from the exception directory's `RUNTIME_FUNCTION` table. A chained entry (`UNW_FLAG_CHAININFO`) maps to its primary function's start, and the span runs to the end of the last chunk that is contiguous with it. Empty when `rva` is in no function.
    - `uint64_t function_start(uint64_t rva) const;`
  - `std::vector<uint8_t> map_pe_file(const std::filesystem::path& file);` Lays the file out by RVA for the offline check. Empty on any malformed header.
  - `struct Access { uint64_t at; int32_t disp; bool store; bool hasImm; int64_t imm; };` A memory operand `[reg + disp]` whose base is neither `rip` nor `rsp`.
  - `struct CodeFacts { std::vector<uint64_t> callTargets; std::vector<Access> accesses; std::vector<std::string> normalized; };`
  - `CodeFacts read_code(std::span<const uint8_t> fn, uint64_t rva);` This decodes with `hde64_disasm`:
    - `callTargets` holds the absolute targets of `call rel32` and of `jmp rel32` leaving the function.
    - `normalized` holds one token per instruction:
      - rip-relative operands replaced by their absolute address
      - in-function branch targets made relative to the start
      - the method's own runtime-metadata flag (the first `cmp byte ptr [rip+X], 0` in the first six instructions) masked as `FLAG` wherever `X` appears

- [ ] **Step 1: Write the failing tests.** The byte sequences are hand-assembled in the test:
  - `function_bounds_from_pdata`: two functions at `0x1000` (`0x20` bytes) and `0x1040`. `function(0x1010).size() == 0x20`, and `function(0x1030)` is empty.
  - `chained_unwind_entry_maps_to_primary`: a primary chunk at `0x2000` and a chained chunk at `0x2030`. `function_start(0x2038) == 0x2000`, and nothing crashes on a chain that points outside the table.
  - `call_targets`: `E8 rel32` at `0x1000` targeting `0x5000` gives `callTargets == {0x5000}`. An internal `E8` to `0x1010` is not listed.
  - `byte_store_with_imm`: `C6 87 D8 00 00 00 01` (`mov byte ptr [rdi+0xD8], 1`) gives one `Access{disp=0xD8, store=true, hasImm=true, imm=1}`.
  - `register_store_has_no_imm`: `40 88 AF F3 00 00 00` (`mov byte ptr [rdi+0xF3], bpl`) gives `store=true, hasImm=false`.
  - `load_is_not_store`: `80 B8 D8 00 00 00 00` (`cmp byte ptr [rax+0xD8], 0`) gives `store=false`.
  - `rip_and_rsp_are_not_field_accesses`: `C6 05 ...` and `C6 44 24 20 01` give no accesses.
  - `normalized_masks_own_flag`: two functions that differ only in the `X` of their leading `cmp byte ptr [rip+X], 0` / `mov byte ptr [rip+X], 1` normalise equal. Two that differ in another rip-relative target do not.
- [ ] **Step 2: Run** `ctest --test-dir build -R sdk_code --output-on-failure`. Expected: it fails to compile.
- [ ] **Step 3: Implement** `CodeImage`, `map_pe_file` and `read_code`.
- [ ] **Step 4: Run** the same command. Expected: pass.
- [ ] **Step 5: Commit.**

### Task 5: Fingerprints, twins and code patterns

**Files:**
- Modify: `sdk/resolver/resolver.cpp`
- Test: `tests/sdk_resolver_test.cpp`, which builds models with `ModelBuilder` and code with `ImageBuilder`

**Interfaces:**
- Consumes: Task 4's `CodeImage` and `read_code`.
- Produces: these rules in `resolve`.
  - **Facts filter candidates**, and each fact's `method` must be a `Resolved` Method entry:
    - `Stores`, `StoresConst` and `Loads` test a candidate field's offset against `read_code(that method).accesses`.
    - `Calls` tests whether the candidate method's code calls the fact's method (or a twin of it).
    - `CalledBy` tests whether the fact method's code calls the candidate.
    - `negate` inverts a fact.
  - **Twins:** the owner's other methods with the same `ret`, `params` and static flag.
    - An equal rva is an alias (folded).
    - Equal length and equal `normalized` make a twin.
    - Candidates that are all twins or aliases of each other count as one behaviour. The lowest rva is canonical; the others go in `twins` and `aliases`.
  - **Code entry:** parse `pattern` and search only `function(within.rva)`. Exactly one match gives `rva = match + patternOffset`. No match is `Missing`, and several are `Ambiguous`.

- [ ] **Step 1: Write the failing tests:**
  - `stores_const_picks_cooldown`: a class with bools at `0xD8` and `0xF3`. `Hunting` holds `mov [rdi+0xF3], bpl` and `mov byte [rdi+0xD8], 1`. The entry `{type "bool", facts {StoresConst Hunting 1}}` gives `offset 0xD8`.
  - `negated_fact_picks_flag`: `{Stores Hunting, StoresConst Hunting 1 negate}` gives `0xF3`.
  - `fact_on_unresolved_method_blocks`: `Blocked`.
  - `folded_alias_is_one_behaviour`: `Init` and an obfuscated twin at the same rva give `Resolved`, with the twin's name in `aliases` and empty `twins`.
  - `identical_twins_are_one_behaviour`: `LookAtPlayer` and two byte-identical copies at other rvas give `twins.size() == 2`.
  - `decoy_with_other_metadata_is_not_a_twin`: a copy that differs only in one rip-relative target (as `UnAppear` and its decoy do) leaves `twins` empty.
  - `pattern_inside_method`: the pattern `"C6 87 ?? ?? ?? ?? 01"` within `Hunting` gives the instruction's rva. A second match gives `Ambiguous`.
- [ ] **Step 2: Run** `ctest --test-dir build -R "sdk_resolver|sdk_code" --output-on-failure`. Expected: the new tests fail.
- [ ] **Step 3: Implement** the rules.
- [ ] **Step 4: Run** the same command. Expected: pass.
- [ ] **Step 5: Commit.**

### Task 6: The model from a Cpp2IL dump

**Files:**
- Create: `sdk/offline/dump_model.h`, `sdk/offline/dump_model.cpp`
- Create: `tests/fixtures/diffable/Assembly-CSharp/Ghost.cs` and `tests/fixtures/diffable/UnityEngine.CoreModule/UnityEngine/Camera.cs`. These are hand-written in Cpp2IL's DiffableCs format, using made-up names and addresses.
- Test: `tests/sdk_dump_test.cpp`, `pg_test(sdk_dump sdk/offline/dump_model.cpp)` with `PG_SOURCE_DIR`

**Interfaces:**
- Produces: `bool pg::sdk::load_dump(const std::filesystem::path& diffableCsRoot, std::span<const std::string> assemblies, GameModel& out, std::string& error);` It reads only the named assembly folders. Each type's assembly is its top folder, and its namespace is the path below that.
  - Methods get their rva from `[Address(RVA = "0x...")]`, and methods without one (abstract or extern) get rva 0.
  - Fields get their offset from `//Field offset: 0x..`, and a static field is one declared `static`.
  - Enum members `name = value,` become literal static fields.
  - Nested types get `declaring`.
  - Parameter names and default values are dropped. Every type string goes through `canonical_type`.

- [ ] **Step 1: Write the fixtures and the failing tests:**
  - `nested_enum_with_literals`: `Ghost` holds a nested `internal enum` with `idle = 0, wander = 1, hunting = 2`, so `isEnum` is set, it has three literals, and `declaring` is `Ghost`.
  - `method_rva_and_params`: `public void ChangeState(<enum> a, PhotonObjectInteract b = null, bool c = false)` with RVA `0x1290` gives params `{"<enum>","PhotonObjectInteract","bool"}` and `rva == 0x1290`.
  - `field_offsets_and_static`: an instance `bool` at `0xD8` and a `static Ghost` field.
  - `namespace_from_path`: `Camera` is in namespace `UnityEngine`, assembly `UnityEngine.CoreModule`.
  - `compiler_generated_nested_class`: a nested `<>c__DisplayClass` with fields named `<>4__this` parses without error.
  - `unreadable_root_reports_error`: a missing root returns false and names the path.
- [ ] **Step 2: Run** `ctest --test-dir build -R sdk_dump --output-on-failure`. Expected: it fails to compile.
- [ ] **Step 3: Implement** `load_dump`, as a line-oriented parser that tracks class nesting by tab depth.
- [ ] **Step 4: Run** the same command. Expected: pass.
- [ ] **Step 5: Commit.**

### Task 7: The first map entries and the offline check

**Files:**
- Create: `sdk/map/all.h` (includes every map header and defines `inline constexpr const pg::sdk::EntryDesc* kAllEntries[]`), `sdk/map/GhostAI.h`, `sdk/map/Camera.h`
- Create: `sdk/offline/gamecheck.cpp` (the executable `gamecheck`, linking `pgsdk_resolver`)
- Create: `tools/gamecheck.ps1`
- Modify: `.gitignore` (add `/re/dump/` and `/re/map-addresses.tsv`), `validate.ps1` (adds `& tools/gamecheck.ps1 -IfConfigured`), `re/decomp.mjs` and `re/slimpe.mjs` (accept map keys)
- Test: `tests/map_test.cpp` (`pg_test(map)`, `PG_SOURCE_DIR`)

**Interfaces:**
- Produces:
  - Map entries as `inline constexpr pg::sdk::EntryDesc` objects in `namespace pg::map`, named `<Type>` and `<Type>_<member>`, with keys `"<Type>"` and `"<Type>.<member>"`.
  - Each entry has a `///` comment above it holding its behaviour note, and its confidence is the descriptor field.
  - Every entry is listed in `kAllEntries`.
  - The entries for this task, with the values the offline check must report for build 25811160:

| key | how found | expected |
| --- | --- | --- |
| `GhostAI` | name, `Assembly-CSharp`, ns `""` | type |
| `GhostState` | shape: owner `GhostAI`, `values {"idle","wander","hunting"}`, hint `ദശസറദബഹജഭ` | type |
| `GhostState.idle` / `.wander` / `.hunting` | name | 0 / 1 / 2 |
| `GhostAI.state` | shape: type `@GhostState` | +0x30 |
| `GhostAI.agent` | shape: type `NavMeshAgent` | +0x40 |
| `GhostAI.Awake` | name, params `{}` | 0x20211C0 |
| `GhostAI.Hunting` | name, params `{"bool","int","PhotonMessageInfo"}` | 0x2022400 |
| `GhostAI.ChangeState` | name, params `{"@GhostState","PhotonObjectInteract","bool"}` | 0x2021290 |
| `GhostAI.UnAppear` | name, params `{}` | 0x202E140, no twins |
| `GhostAI.LookAtPlayer` | name, params `{"Player"}` | 0x2023410, twins 0x2025B70 and 0x2027FF0 |
| `GhostAI.TemporarilyStopWander` | name, params `{"float"}` | 0x2024900, one folded alias |
| `GhostAI.isHunting` | shape `bool`, facts `{Stores GhostAI.Hunting}`, `{StoresConst GhostAI.Hunting 1, negate}` | +0xF3 |
| `GhostAI.postHuntCooldown` | shape `bool`, facts `{StoresConst GhostAI.Hunting 1}` | +0xD8 |
| `Camera` | name, `UnityEngine.CoreModule`, ns `UnityEngine` | type |
| `Camera.set_fieldOfView` | name, params `{"float"}` | 0x51B6CC0 |
| `Camera.set_cullingMask` | name, params `{"int"}` | 0x51B6B60 |
| `Camera.get_cullingMask`, `get_/set_nearClipPlane`, `get_/set_farClipPlane` | name | resolved |

  - Notes for the GhostAI fields come from today's RE:
    - `isHunting`: `GhostAI.Hunting` stores its first argument there; `Decompiled`.
    - `postHuntCooldown`: `GhostAI.Hunting` sets it to 1 and `Awake` clears it; earlier RE names it the post-hunt cooldown; `Decompiled`.
  - `gamecheck --dump <DiffableCs root> --image <GameAssembly.dll> [--addresses <tsv>]`:
    - It prints one line per entry: `OK`, `MISSING`, `AMBIGUOUS` or `BLOCKED`, then the key and the value, then twins, aliases and `renamed from` where present.
    - Then a summary line `<n> entries: <r> resolved, <m> missing, <a> ambiguous, <b> blocked`.
    - With `--addresses` it writes `key<TAB>kind<TAB>value<TAB>length` for resolved entries. `length` is the function's size from the unwind table for methods and code entries, and empty otherwise.
    - Exit 0 when every required entry resolved, 1 when not, 2 on bad arguments or unreadable input.
  - `tools/gamecheck.ps1 [-GameDir <dir>] [-Cpp2Il <exe>] [-IfConfigured]`:
    - It falls back to `PG_GAME_DIR` and `PG_CPP2IL`. Unconfigured with `-IfConfigured`, it prints `Offline game check: not configured (set PG_GAME_DIR and PG_CPP2IL).` and exits 0; unconfigured without it, it exits 2 with the same text.
    - It copies `GameAssembly.dll`, `Phasmophobia.exe`, `Phasmophobia_Data/globalgamemanagers` and `Phasmophobia_Data/il2cpp_data/Metadata/global-metadata.dat` to `re/dump/game/`.
    - It runs Cpp2IL into `re/dump/cs/` unless `re/dump/build.txt` already holds the same two hashes.
    - It runs `build/gamecheck.exe` with `--addresses re/map-addresses.tsv`, regenerates `re/names.tsv` with `node re/names.mjs re/dump/cs/DiffableCs`, and exits with gamecheck's code.
  - `re/decomp.mjs` accepts a map key such as `GhostAI.Hunting` wherever it accepts an RVA.
  - `re/slimpe.mjs` accepts a map key in place of a `<from>-<to>` range, as that function's start and length.
  - Both say `Run tools/gamecheck.ps1 first.` when `re/map-addresses.tsv` or `re/names.tsv` is missing.

- [ ] **Step 1: Write the failing test** `tests/map_test.cpp`:
  - `every_entry_listed`: scan `sdk/map/*.h` for `inline constexpr pg::sdk::EntryDesc` and `CHECK` that each name appears in `kAllEntries`.
  - `keys_unique`.
  - `every_entry_has_a_note`: the line above each descriptor starts with `///`.
  - `no_anticheat_entries`: no key or name contains `SelfReport` or `Moderation`.
- [ ] **Step 2: Run** `ctest --test-dir build -R map --output-on-failure`. Expected: it fails to compile.
- [ ] **Step 3: Implement** the headers, `gamecheck.cpp`, `tools/gamecheck.ps1` and the `decomp.mjs` change.
- [ ] **Step 4: Run** `.\validate.ps1` without `PG_GAME_DIR`. Expected: pass, with the not-configured line.
- [ ] **Step 5: Check against the real build.** With `PG_GAME_DIR` and `PG_CPP2IL` set to this PC's game and Cpp2IL, run `.\tools\gamecheck.ps1`. Expected: exit 0, and every row of the table above with exactly those values.
- [ ] **Step 6: Prove it fails.**
  - Point `GhostAI.postHuntCooldown`'s fact at `StoresConst GhostAI.Hunting 2`. Expected: `MISSING`, exit 1.
  - Drop its facts entirely. Expected: `AMBIGUOUS`, listing 13 offsets.
  - Restore it.
- [ ] **Step 7: Run the update drill.** Copy `re/dump/cs` to a scratch folder. In the copy of `GhostAI.cs`:
  - rename the obfuscated field at `0xD8` to a new made-up name
  - move the `NavMeshAgent` field from `0x40` to `0x48`, swapping offsets with the field there
  
  Run `build\gamecheck.exe --dump <copy>\DiffableCs --image re\dump\game\GameAssembly.dll`. Expected:
  - exit 0
  - `GhostAI.postHuntCooldown` still `+0xD8`, with `renamed from ടഴദബടഠഹടന`
  - `GhostAI.agent` now `+0x48`
  - no other line changed
- [ ] **Step 8: Check the tools.** Run `node re/decomp.mjs re/out/hunting.c GhostAI.Hunting`. Expected: it decompiles `0x2022400`. Then run `node re/slimpe.mjs <copy of GameAssembly.dll> <scratch>\slim.dll GhostAI.Hunting`. Expected: it writes a slim PE covering that function.
- [ ] **Step 9: Commit** (none of `re/dump`, `re/names.tsv` or `re/map-addresses.tsv` is staged).

### Task 8: The host resolves the map at startup

**Files:**
- Create: `sdk/runtime/runtime_model.h`, `sdk/runtime/runtime_model.cpp`, `host/game_map.h`, `host/game_map.cpp`
- Modify: `core/il2cpp.cpp` and `core/il2cpp.h` (expose a `RuntimeApi` of the extra exports below), `host/main.cpp`, `CMakeLists.txt` (ModHost links `pgsdk_resolver`; adds the new sources)
- Test: `tests/runtime_model_test.cpp` (`pg_test(runtime_model sdk/runtime/runtime_model.cpp)`, with a fake `RuntimeApi`)

**Interfaces:**
- Produces:
  - `struct pg::sdk::RuntimeApi`: function pointers for the exports the model needs, beyond those `core/il2cpp.cpp` already resolves: `il2cpp_image_get_class_count`, `il2cpp_image_get_class`, `il2cpp_class_get_namespace`, `il2cpp_class_get_declaring_type`, `il2cpp_class_is_enum`, `il2cpp_class_get_methods`, `il2cpp_method_get_name`, `il2cpp_method_get_param_count`, `il2cpp_method_get_param`, `il2cpp_method_get_return_type`, `il2cpp_method_get_flags`, `il2cpp_field_get_name`, `il2cpp_field_get_offset`, `il2cpp_field_get_flags`, and the ones it already has for images, field types and static values.
  - `bool pg::sdk::load_runtime(const RuntimeApi& api, uintptr_t gameAssemblyBase, std::span<const std::string> assemblies, GameModel& out);` Here `rva = methodPointer - base` for pointers inside GameAssembly, and 0 otherwise. Literal fields (`FIELD_ATTRIBUTE_LITERAL`) take their value through `il2cpp_field_static_get_value`.
  - `namespace pg::gamemap`:
    - `bool resolve(void* gameAssembly);` Builds the model for the assemblies named by `kAllEntries`, resolves them with a `CodeImage` over the loaded module, and logs one line per failed entry plus the summary `game map: <r>/<n> resolved in <ms> ms`.
    - `const sdk::Result* result(std::string_view key);`
    - `const PgGameEntry* entry(const char* key);` (Task 9 fills this in).
  - `host/main.cpp` calls `pg::gamemap::resolve(g_gameAssembly)` right after `il2cpp::init` succeeds and before `pg::game::resolve()`. Unresolved entries never make the host inactive: only the mods that need them stay off (Task 9).

- [ ] **Step 1: Write the failing tests** in `runtime_model_test` against a fake API serving two classes:
  - `enumerates_types_methods_fields`: rvas are relative to the given base, offsets are kept, and nested types get `declaring`.
  - `enum_literals_read_through_static_value`.
  - `pointer_outside_module_gets_zero_rva`.
  - `types_are_canonical`: a field whose runtime type name is `System.Collections.Generic.List<UnityEngine.Vector3>` has type `List<Vector3>`.
- [ ] **Step 2: Run** `ctest --test-dir build -R runtime_model --output-on-failure`. Expected: it fails to compile.
- [ ] **Step 3: Implement** `load_runtime`, the `RuntimeApi` resolution in `core/il2cpp.cpp`, and `host/game_map.cpp`.
- [ ] **Step 4: Run** `.\validate.ps1`. Expected: pass.
- [ ] **Step 5: Commit.** The live check is Task 11's.

### Task 9: Host contract version 2 and the game API

**Files:**
- Modify: `core/mod_api.h` (version 2, `PgGameEntry`, new `PgHostApi` members, `pgmod_requires`), `core/mod_glue.cpp`/`.h` (`mod::bind` binds the game API; exports `pgmod_requires`), `host/suite.cpp` (fills the new members), `host/game_map.cpp` (`entry`), `host/mods.cpp` (refuses a mod whose required keys did not resolve, naming them), `sdk/offline/gamecheck.cpp` (`--mods <dir>`)
- Create: `sdk/api/game_api.h`, `sdk/api/game_api.cpp` (added to `pgmod`), `sdk/fake/fake_game.h`, `sdk/fake/fake_game.cpp`
- Create: the `phasmo::` wrappers at the end of `sdk/map/GhostAI.h` and `sdk/map/Camera.h`
- Test: `tests/game_api_test.cpp` (links `pgmod` pieces and `sdk/fake/fake_game.cpp`), `tests/mods_test.cpp`

**Interfaces:**
- Produces in `core/mod_api.h`:
  - `#define PGMOD_API_VERSION 2u`
  - `enum PgEntryKind : uint32_t { PG_ENTRY_TYPE = 1, PG_ENTRY_METHOD, PG_ENTRY_FIELD, PG_ENTRY_ENUM_VALUE, PG_ENTRY_CODE };`
  - `struct PgGameEntry { uint32_t kind; void* klass; const void* method; void* code; void* const* twins; uint32_t twinCount; int32_t offset; void* field; int64_t value; };`
  - New `PgHostApi` members, appended:
    - `const PgGameEntry* (*game_entry)(const char* key);` Null when the key is unknown or unresolved.
    - `bool (*hook_entry)(const char* key, void* detour, void** original, const char* name, bool allTwins);`
    - `void (*set_reference)(void* obj, int32_t offset, void* value);` The GC write barrier.
    - `int32_t (*inside_entry)(const char* key);`
  - `typedef size_t (*PgModRequiresFn)(const char* const** keys);` The optional export `"pgmod_requires"` returns the keys the mod uses without `kOptional`.
- Produces in `sdk/api/game_api.h`, namespace `pg::api`:
  - `template <const sdk::EntryDesc& D> struct Use { static inline const bool registered = detail::register_use(&D); };`
  - Every accessor below is a template whose members are themselves `template <int = 0>`. A wrapper's entry is therefore registered only by the code that calls it.
  - `template <const sdk::EntryDesc& D, class Sig> struct Method;` and the specialisation `<D, R(A...)>`:
    - `R operator()(A... a) const` calls `code(a..., methodInfo)`.
    - `bool available() const`.
  - `template <const sdk::EntryDesc& D, class T> struct Field { T get(const void* obj) const; void set(void* obj, T value) const; };` When `T` is `void*` (a managed reference), `set` goes through `set_reference`.
  - `template <const sdk::EntryDesc& D> struct EnumValue { int32_t operator()() const; };`
  - `template <const sdk::EntryDesc& D> struct Type { void* klass() const; bool is(const void* obj) const; };`
  - `enum class Twins { All, CanonicalOnly };`
  - `template <auto& M> bool hook(void* detour, void** original, const char* name, Twins twins = Twins::All);`
  - `template <auto& M> bool inside();`
  - `bool bind(const PgHostApi* host, std::string& missing);` Looks up every used entry. False when a required one is missing, and `missing` names them.
  - `size_t used_required(const char* const** keys);`
  - When an entry is unresolved, `operator()`, `get` and `set` log `<key> is not available in this game build` once and return `R{}`, or do nothing.
- Produces in `sdk/map/GhostAI.h`:
  - `namespace phasmo { struct GhostAI { static constexpr pg::api::Type<pg::map::GhostAI> type{}; static constexpr pg::api::Field<pg::map::GhostAI_state, int32_t> state{}; ... static constexpr pg::api::Method<pg::map::GhostAI_UnAppear, void(void*)> UnAppear{}; ... }; namespace GhostState { inline constexpr pg::api::EnumValue<pg::map::GhostState_hunting> hunting{}; ... } }`
  - One wrapper for every entry of Task 7. `Camera` gets the same treatment in `sdk/map/Camera.h`.
- Produces in `sdk/fake/fake_game.h`:
  - `namespace pg::fake`
  - `void reset();`
  - `const PgHostApi* host();` A complete host contract in which only the game functions are live.
  - `void method(const sdk::EntryDesc& d, void* fn);`
  - `void field(const sdk::EntryDesc& d, int32_t offset);`
  - `void value(const sdk::EntryDesc& d, int64_t v);`
  - `void type(const sdk::EntryDesc& d, void* klass);`
  - `std::vector<std::string> hooked();`

- [ ] **Step 1: Write the failing tests** in `game_api_test`:
  - `call_reaches_fake`: `fake::method(map::GhostAI_UnAppear, &fakeUnAppear)`, then `api::bind(fake::host(), missing)`, then `phasmo::GhostAI::UnAppear(ghost)`. Expected: `fakeUnAppear` saw `ghost` and a null method info.
  - `field_get_set`: `phasmo::GhostAI::state.set(obj, 2)`, then `get(obj) == 2`, stored at the fake offset.
  - `reference_set_uses_barrier`: `Field<..., void*>::set` calls the fake `set_reference`.
  - `enum_value`: `phasmo::GhostState::hunting() == 2`.
  - `only_called_entries_are_required`: `used_required` lists `GhostAI.UnAppear` and not `GhostAI.LookAtPlayer`, which is declared in the same header and never called in this test binary.
  - `bind_names_missing_required`: with no fake for `GhostAI.UnAppear`, `bind` returns false and `missing == "GhostAI.UnAppear"`.
  - `unresolved_optional_call_returns_default`: an optional wrapper that did not resolve returns `0`, logs once, and never calls through null.
  
  And in `mods_test`:
  - `mod_with_unresolved_requirement_stays_off`: a fake mod whose `pgmod_requires` lists a key the host lacks is not started, and the log line names the key.
- [ ] **Step 2: Run** `ctest --test-dir build -R "game_api|mods" --output-on-failure`. Expected: it fails to compile.
- [ ] **Step 3: Implement** all of it. `mod::bind` checks `PGMOD_API_VERSION == 2` and calls `api::bind`. Every mod's `PgModInfo` already uses `PGMOD_API_VERSION`, so the five mods are rebuilt with no source change. `gamecheck --mods <dir>` loads each mod DLL in the folder, calls `pgmod_requires`, and after the summary prints `would switch off <mod id>: <keys>` for each failure.
- [ ] **Step 4: Run** `.\validate.ps1`. Expected: pass.
- [ ] **Step 5: Commit.**

### Task 10: Hooks by entry, twins and caller context

**Files:**
- Create: `host/entry_hooks.h`, `host/entry_hooks.cpp`, `host/context.cpp`
- Modify: `host/suite.cpp` (wires `hook_entry` and `inside_entry`), `CMakeLists.txt`
- Test: `tests/hooks_test.cpp` (real MinHook against test functions, as it does now)

**Interfaces:**
- Produces:
  - `bool pg::entry_hooks::hook(void* canonical, std::span<void* const> twins, void* detour, void** original, const char* name, bool allTwins);`
    - It hooks the canonical body through `pg::hooks::create`, and each distinct twin address through its own trampoline.
    - Twins call the same detour, and the detour's `original` reaches the canonical chain, which is correct because twins are byte-identical behaviour.
    - An address equal to one already hooked by this call is skipped, so a folded alias is hooked once.
  - `template <auto& M> void pg::context::track();` Used in `host/context.cpp` as `PG_TRACK(phasmo::Type::Method)`. It instantiates a typed detour from `M`'s signature that holds an RAII depth counter for the entry, so an exception unwinding through it still decrements.
  - `int32_t pg::context::depth(const char* key);`
  - No real entry is tracked in this plan. Stage 2 adds the first: the weighted ghost-type roll.

- [ ] **Step 1: Write the failing tests** in `hooks_test`:
  - `twin_calls_reach_detour`: two identical test functions `a` and `b`. Hooking with `allTwins = true` makes calls to both reach the detour.
  - `canonical_only_leaves_twin`: with `allTwins = false`, a call to `b` does not reach it.
  - `folded_twin_hooked_once`: a twin list containing the canonical address creates one MinHook hook, and the log has no `already hooked` line.
  - `depth_inside_tracked_call`: a tracked test function calls a probe that records `depth("Test.F")`, which is 1 inside and 0 after. When the tracked function throws, depth is 0 after the catch.
- [ ] **Step 2: Run** `ctest --test-dir build -R hooks --output-on-failure`. Expected: it fails to compile.
- [ ] **Step 3: Implement** it.
- [ ] **Step 4: Run** `.\validate.ps1`. Expected: pass.
- [ ] **Step 5: Commit.**

### Task 11: First call through the game API, docs, and the live check

**Files:**
- Modify: `mods/ghost_tells/shade.cpp`. `kUnAppearRva` and `game::game_assembly_address` go, and `g_unAppear(ghost, nullptr)` becomes `phasmo::GhostAI::UnAppear(ghost)`.
- Create: `sdk/README.md`, covering:
  - the terms
  - how a mod calls the game API
  - how to add an entry: strategy order, `@Key`, facts, twins, `Code` entries, confidence, the `///` note
  - the commands `tools/gamecheck.ps1` and `node re/decomp.mjs <out> <key>`
  - that every RE fact belongs in an entry's note or a system overview, never only in a memory note
- Create: `sdk/systems/ghost-ai.md`, the ghost AI overview: what is mapped so far, the twins finding, the fingerprint method, and today's RE facts for these entries.
- Modify: `README.md` (one paragraph pointing agents and contributors at `sdk/README.md`)
- Test: `tests/ghost_tells_test.cpp` keeps passing. Task 9's `call_reaches_fake` already pins the call path. The Shade's own tick needs most of `core/game`, which stage 2 migrates, so this task checks the Shade live.

- [ ] **Step 1: Migrate** `shade.cpp`. Then grep `mods/ghost_tells/shade.cpp` for `0x2`, `il2cpp::bind` and `game_assembly_address`. Expected: no matches in the migrated code.
- [ ] **Step 2: Check requirements.** Run `.\validate.ps1`, then `.\tools\gamecheck.ps1 -IfConfigured`. Expected:
  - both pass
  - gamecheck's `--mods` section switches nothing off
  - the Ghost Tells DLL's `pgmod_requires` lists exactly `GhostAI.UnAppear`, and no other mod lists any key yet
- [ ] **Step 3: Write the docs** (`sdk/README.md`, `sdk/systems/ghost-ai.md`, the README paragraph).
- [ ] **Step 4: The live check** (user's go: the user launches the game; the ban risk is restated first). Inject the build with `build\pginject.exe inject build\ModHost.dll ...` as usual. Expected in `%LOCALAPPDATA%\PhasmoMods\mods.log`:
  - `game map: <n>/<n> resolved in <ms> ms`, with `ms` well inside the 60-second startup deadline
  - every mod started
  - with a Shade (the user sets the ghost type, or the Custom Ghosts picker does), the Shade's stare tell still makes it vanish

  Record the measured `ms` in `sdk/systems/ghost-ai.md`.
- [ ] **Step 5: Commit.** Then update Claude's memory note `playable-ghost-project` to point RE work at `sdk/README.md` and `tools/gamecheck.ps1`.

---

## Roadmap: the stages after this plan

Each stage gets its own plan when the previous one has landed, because it builds on the interfaces above.

- **Stage 2, map and migrate everything the suite uses.** One task per mod plus `core/`, `host/` and the UI code. The work:
  - Every address, offset and name lookup becomes an entry, with the offline check proving each value equals today's constant.
  - Scene and asset facts get their entries and the UnityPy reader in `tools/gamecheck.ps1`, after pinning UnityPy.
  - Playable Ghost's UnityPlayer steering stub becomes a native-patch entry kind owned by the host.
  - The first tracked entry: the weighted ghost-type roll replaces the return-address check.
  - IL2CPP layouts (strings, arrays, lists) move behind the host contract.
  - Checked-build records (`sdk/builds/`, with the map digest) replace `host/build_check.cpp`'s hashes. They are added to the updater's package and release refusals.
  - The Mods panel shows unavailable mods with the reason.
  - The source scan test rejecting raw addresses, offsets, obfuscated names and direct IL2CPP use outside `sdk/`.
  - The memory notes' RE facts move into entries and overviews.
  - Twins of hooked methods are investigated and reported to the user.
- **Stage 3, map the remaining systems.** One task per system from the spec's list, each starting with its written inventory in `sdk/systems/<system>.md`. Then batch decompilation, naming, notes at `Decompiled` confidence or better, and the coverage report test, which must reach zero.
