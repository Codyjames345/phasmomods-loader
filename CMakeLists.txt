cmake_minimum_required(VERSION 3.24)
project(PhasmoMods LANGUAGES C CXX ASM_MASM RC)

# The suite version: one line in VERSION, compiled into the launcher and the host.
file(STRINGS VERSION PM_SUITE_VERSION LIMIT_COUNT 1)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS VERSION)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Every native target uses the same warnings and Windows settings.
function(pm_options target)
  target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4 /permissive- /utf-8 /EHsc>)
  target_compile_definitions(${target} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endfunction()

add_library(minhook STATIC
  third_party/minhook/src/buffer.c
  third_party/minhook/src/hook.c
  third_party/minhook/src/trampoline.c
  third_party/minhook/src/hde/hde64.c)
target_include_directories(minhook PUBLIC third_party/minhook/include)

# Shared by the host and every mod: game and Unity bindings, UI from game classes, all through the game API.
# The IL2CPP layer (host/il2cpp.*) is the host's alone: no mod can include or link it.
add_library(pmcore STATIC
  core/log.cpp
  core/game.cpp
  core/ui.cpp
  core/notice.cpp
  core/bindings.cpp)
target_include_directories(pmcore PUBLIC core sdk/api sdk/map)
pm_options(pmcore)

# The game SDK's resolver: finds the game's types, members and code from the map, by shape.
file(GLOB PMSDK_RESOLVER_SOURCES CONFIGURE_DEPENDS sdk/resolver/*.cpp)
add_library(pmsdk_resolver STATIC ${PMSDK_RESOLVER_SOURCES})
target_include_directories(pmsdk_resolver PUBLIC sdk/api sdk/resolver)
target_link_libraries(pmsdk_resolver PUBLIC minhook)
target_include_directories(pmsdk_resolver PRIVATE third_party/minhook/src/hde)
pm_options(pmsdk_resolver)

# The offline check (tools/gamecheck.ps1): the map resolved against a Cpp2IL dump and GameAssembly.dll.
# With --mods it also loads mod DLLs and names those the host would switch off.
add_executable(gamecheck sdk/offline/gamecheck.cpp sdk/offline/dump_model.cpp)
target_include_directories(gamecheck PRIVATE core sdk/map sdk/offline sdk/runtime)
target_link_libraries(gamecheck PRIVATE pmsdk_resolver bcrypt)
pm_options(gamecheck)
# The map's digest, for what must refuse a stale checked-build record (tools/check_build_record.ps1).
add_custom_command(TARGET gamecheck POST_BUILD COMMAND gamecheck --map-digest "${CMAKE_BINARY_DIR}/map-digest.txt")

# A mod's side of the host contract (core/mod_api.h) and the game API it calls (sdk/api, sdk/map).
add_library(pmmod STATIC core/mod_glue.cpp sdk/api/game_api.cpp sdk/api/runtime.cpp sdk/api/content.cpp sdk/api/assets.cpp
  sdk/api/bundles.cpp sdk/api/models.cpp sdk/api/keybinds.cpp sdk/api/player_anim.cpp
  sdk/api/third_person.cpp sdk/api/talk_loudness.cpp sdk/api/screen_effects.cpp)
target_include_directories(pmmod PUBLIC sdk/api sdk/map)
target_link_libraries(pmmod PUBLIC pmcore)
pm_options(pmmod)

# The host: startup checks, the only MinHook instance, consent, running the chosen mods (and the
# in-game Mods panel), and the join guard.
add_library(ModHost SHARED
  host/main.cpp
  host/il2cpp.cpp
  host/find_child.cpp
  host/hooks.cpp
  host/entry_hooks.cpp
  host/context.cpp
  host/net.cpp
  host/host_game.cpp
  host/runtime_services.cpp
  host/build_check.cpp
  host/suite.cpp
  host/consent.cpp
  host/mods.cpp
  host/mod_deps.cpp
  host/menu_panel.cpp
  host/mods_panel.cpp
  host/room_actions.cpp
  host/lobby_gate.cpp
  host/game_map.cpp
  host/scene_engine.cpp
  host/mod_session.cpp
  host/room_watch.cpp
  host/steering.cpp
  host/steer_stub.asm
  host/content_types.cpp
  host/content_target.cpp
  host/content_journal.cpp
  host/content_difficulty.cpp
  host/content_evidence.cpp
  host/content_localised.cpp
  host/content_text.cpp
  sdk/api/game_api.cpp
  sdk/api/runtime.cpp
  sdk/runtime/content_acting.cpp
  sdk/runtime/content_registry.cpp
  sdk/runtime/runtime_model.cpp
  sdk/runtime/scene_facts.cpp)
# host/hooks.cpp checks a native site's bytes with HDE64, vendored inside MinHook.
target_include_directories(ModHost PRIVATE host sdk/map sdk/runtime third_party/minhook/src/hde)
target_link_libraries(ModHost PRIVATE pmcore pmsdk_resolver minhook bcrypt)
pm_options(ModHost)
# PM_HOST: host-only parts of the game API (pm::api::host_only), which mods never see.
target_compile_definitions(ModHost PRIVATE PM_HOST PM_SUITE_VERSION="${PM_SUITE_VERSION}")

# Mods. Each is one DLL with a manifest.json beside it in the release (tools/payload.psd1).
add_library(PlayableGhost SHARED
  mods/playable_ghost/mod.cpp
  mods/playable_ghost/control.cpp
  mods/playable_ghost/actions.cpp
  mods/playable_ghost/hud.cpp
  mods/playable_ghost/lobby.cpp
  mods/playable_ghost/session.cpp
  mods/playable_ghost/room.cpp
  mods/playable_ghost/round.cpp
  mods/playable_ghost/picker.cpp
  mods/playable_ghost/leave.cpp
  mods/playable_ghost/journal.cpp
  mods/playable_ghost/ghostbook.cpp
  mods/playable_ghost/mapview.cpp
  mods/playable_ghost/panorama.cpp
  mods/playable_ghost/senses.cpp
  mods/playable_ghost/body.cpp
  mods/playable_ghost/outlist.cpp
  mods/playable_ghost/contract_end.cpp
  mods/playable_ghost/loadout.cpp
  mods/playable_ghost/reward.cpp
  mods/playable_ghost/content_link.cpp
  mods/playable_ghost/hide.cpp
  mods/playable_ghost/outcome.cpp
  mods/playable_ghost/guess.cpp
  mods/playable_ghost/mimic.cpp
  mods/playable_ghost/typestate_host.cpp
  mods/playable_ghost/marks.cpp
  mods/playable_ghost/alerts.cpp
  mods/playable_ghost/perceive_view.cpp
  mods/playable_ghost/form.cpp
  mods/playable_ghost/walk_state.cpp
  mods/playable_ghost/yokai_ears.cpp
  mods/playable_ghost/chances.cpp
  mods/playable_ghost/footsteps.cpp)
target_include_directories(PlayableGhost PRIVATE mods/playable_ghost)
target_link_libraries(PlayableGhost PRIVATE pmmod)
pm_options(PlayableGhost)

add_library(CustomGhosts SHARED
  mods/custom_ghosts/mod.cpp
  mods/custom_ghosts/behaviour.cpp
  mods/custom_ghosts/hunt.cpp
  mods/custom_ghosts/kikimora.cpp
  mods/custom_ghosts/carry.cpp
  mods/custom_ghosts/echo.cpp
  mods/custom_ghosts/restraint.cpp
  mods/custom_ghosts/sound.cpp
  mods/custom_ghosts/squatter.cpp
  mods/custom_ghosts/squatter_voice.cpp
  mods/custom_ghosts/squatter_figure.cpp
  mods/custom_ghosts/squatter_ragdoll.cpp
  mods/custom_ghosts/squatter_door.cpp
  mods/custom_ghosts/apparition.cpp
  mods/custom_ghosts/hydra.cpp
  mods/custom_ghosts/sensor_copy.cpp
  mods/custom_ghosts/asterion.cpp
  mods/custom_ghosts/ghost_sight.cpp
  mods/custom_ghosts/lithobolia.cpp
  mods/custom_ghosts/gremlin.cpp
  mods/custom_ghosts/bogeyman.cpp
  mods/custom_ghosts/brownie.cpp
  mods/custom_ghosts/lethe.cpp
  mods/custom_ghosts/tempest.cpp
  mods/custom_ghosts/mesmer.cpp)
target_include_directories(CustomGhosts PRIVATE mods/custom_ghosts)
target_link_libraries(CustomGhosts PRIVATE pmmod)
pm_options(CustomGhosts)

# The SDK's worked example (sdk/guides/adding-a-ghost.md): built and tested, never shipped.
add_library(ExampleGhost SHARED sdk/examples/example_ghost/mod.cpp)
target_include_directories(ExampleGhost PRIVATE sdk/examples/example_ghost)
target_link_libraries(ExampleGhost PRIVATE pmmod)
pm_options(ExampleGhost)

add_library(SpectralFlame SHARED
  mods/spectral_flame/mod.cpp
  mods/spectral_flame/contract.cpp
  mods/spectral_flame/flames.cpp)
target_include_directories(SpectralFlame PRIVATE mods/spectral_flame)
target_link_libraries(SpectralFlame PRIVATE pmmod)
pm_options(SpectralFlame)

add_library(GhostTells SHARED
  mods/ghost_tells/mod.cpp
  mods/ghost_tells/world.cpp
  mods/ghost_tells/gate.cpp
  mods/ghost_tells/hold.cpp
  mods/ghost_tells/spirit.cpp
  mods/ghost_tells/rolls.cpp
  mods/ghost_tells/mare.cpp
  mods/ghost_tells/shade.cpp
  mods/ghost_tells/yurei.cpp
  mods/ghost_tells/goryo.cpp
  mods/ghost_tells/onryo.cpp
  mods/custom_ghosts/apparition.cpp)
# The Brownie's facing test and the Asterion's door angles (pure rules), and the suite's model copies.
target_include_directories(GhostTells PRIVATE mods/ghost_tells mods/custom_ghosts)
# The map's headers before the mods' folders, so no mod's header stands for one of them.
target_include_directories(GhostTells BEFORE PRIVATE sdk/api sdk/map)
target_link_libraries(GhostTells PRIVATE pmmod)
pm_options(GhostTells)

# Emotes: an eight-dance wheel on a rebindable key (docs/superpowers/specs/2026-11-10-emotes-design.md).
add_library(Emotes SHARED mods/emotes/mod.cpp mods/emotes/wheel.cpp mods/emotes/emote.cpp)
target_include_directories(Emotes PRIVATE mods/emotes mods/custom_ghosts)
target_include_directories(Emotes BEFORE PRIVATE sdk/api sdk/map)
target_link_libraries(Emotes PRIVATE pmmod)
pm_options(Emotes)

add_executable(pminject tools/inject.cpp)
target_include_directories(pminject PRIVATE tools core)
pm_options(pminject)
target_compile_definitions(pminject PRIVATE UNICODE _UNICODE)

add_library(pmloader STATIC tools/inject.cpp)
target_include_directories(pmloader PUBLIC tools core)
pm_options(pmloader)
target_compile_definitions(pmloader PRIVATE PM_LOADER_NO_MAIN UNICODE _UNICODE)

# Velopack's prebuilt C++ library (version in third_party/velopack/VERSION.txt), used by the launcher's updater.
add_library(velopack SHARED IMPORTED)
set_target_properties(velopack PROPERTIES
  IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/third_party/velopack/velopack_libc.dll
  IMPORTED_IMPLIB ${CMAKE_SOURCE_DIR}/third_party/velopack/velopack_libc.lib
  INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/third_party/velopack)

add_executable(ModLauncher WIN32 tools/launcher.cpp host/mod_deps.cpp tools/update.cpp tools/update_velopack.cpp tools/launcher.rc)
target_link_libraries(ModLauncher PRIVATE pmloader velopack user32 shell32 gdi32 comctl32)
# Common Controls 6, which the update popup's task dialog needs; it also gives the window the current Windows style.
target_link_options(ModLauncher PRIVATE "/MANIFESTDEPENDENCY:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'")
set_target_properties(ModLauncher PROPERTIES OUTPUT_NAME "PhasmoMods Loader")
pm_options(ModLauncher)
target_compile_definitions(ModLauncher PRIVATE UNICODE _UNICODE PM_SUITE_VERSION="${PM_SUITE_VERSION}")
add_custom_command(TARGET ModLauncher POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:velopack> $<TARGET_FILE_DIR:ModLauncher>)

# Offline tests (no game required): ctest --test-dir build
enable_testing()
function(pm_test name)
  add_executable(${name}_test tests/${name}_test.cpp ${ARGN})
  target_include_directories(${name}_test PRIVATE core host mods/playable_ghost mods/custom_ghosts mods/spectral_flame mods/ghost_tells tools sdk/api sdk/map)
  # The shipped string tables, which tests read through tests/text_fixture.h.
  target_compile_definitions(${name}_test PRIVATE PM_TEXT_DIR="${CMAKE_SOURCE_DIR}")
  pm_options(${name}_test)
  add_test(NAME ${name} COMMAND ${name}_test)
endfunction()

pm_test(session mods/playable_ghost/session.cpp)
target_compile_definitions(session_test PRIVATE PM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
pm_test(payout)
pm_test(ghostbook mods/playable_ghost/ghostbook.cpp)
pm_test(room_pick)
pm_test(exit_route)
pm_test(tether)
pm_test(startup)
target_include_directories(startup_test PRIVATE third_party/minhook/include sdk/api sdk/resolver sdk/runtime)
target_compile_definitions(startup_test PRIVATE PM_HOST PM_SUITE_VERSION="${PM_SUITE_VERSION}")
target_include_directories(startup_test PRIVATE sdk/map)
pm_test(build_check)
target_link_libraries(build_check_test PRIVATE bcrypt)
pm_test(inject)
pm_test(assets sdk/api/assets.cpp)
target_compile_definitions(inject_test PRIVATE UNICODE _UNICODE)
pm_test(update tools/update.cpp tools/launcher.cpp host/mod_deps.cpp)
target_link_libraries(update_test PRIVATE pmloader user32 shell32)
target_compile_definitions(update_test PRIVATE UNICODE _UNICODE PM_LAUNCHER_TEST PM_SUITE_VERSION="${PM_SUITE_VERSION}")
pm_test(launcher host/mod_deps.cpp)
target_link_libraries(launcher_test PRIVATE user32 shell32)
target_compile_definitions(launcher_test PRIVATE UNICODE _UNICODE PM_SOURCE_DIR="${CMAKE_SOURCE_DIR}" PM_SUITE_VERSION="${PM_SUITE_VERSION}")
pm_test(lobby mods/playable_ghost/session.cpp sdk/api/game_api.cpp sdk/api/runtime.cpp sdk/fake/fake_game.cpp
  sdk/runtime/scene_facts.cpp sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp)
# Before the mods' folders, as for content_difficulty_test below.
target_include_directories(lobby_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
pm_test(lobby_gate host/lobby_gate.cpp host/entry_hooks.cpp sdk/api/game_api.cpp sdk/api/runtime.cpp sdk/fake/fake_game.cpp sdk/runtime/scene_facts.cpp
  sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp core/log.cpp)
target_include_directories(lobby_gate_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
target_compile_definitions(lobby_gate_test PRIVATE PM_HOST)
pm_test(room_actions host/host_game.cpp sdk/api/game_api.cpp sdk/api/runtime.cpp sdk/fake/fake_game.cpp sdk/runtime/scene_facts.cpp
  sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp core/log.cpp)
target_include_directories(room_actions_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
pm_test(form sdk/api/game_api.cpp sdk/api/runtime.cpp sdk/fake/fake_game.cpp sdk/runtime/scene_facts.cpp
  sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp core/log.cpp)
# Before the mods' folders, as for content_difficulty_test below.
target_include_directories(form_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
pm_test(mod_list)
pm_test(mod_manifest)
target_compile_definitions(mod_manifest_test PRIVATE PM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
pm_test(mod_deps host/mod_deps.cpp)
target_compile_definitions(mod_deps_test PRIVATE PM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
pm_test(presence)
pm_test(join_guard host/mod_deps.cpp)
pm_test(start_close)
pm_test(text_table)
target_compile_definitions(text_table_test PRIVATE PM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_include_directories(join_guard_test PRIVATE sdk/runtime)
pm_test(service_registry)
pm_test(consent)
pm_test(consent_ui host/mod_session.cpp)
pm_test(menu_panel sdk/api/game_api.cpp sdk/api/runtime.cpp sdk/fake/fake_game.cpp sdk/runtime/scene_facts.cpp
  sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp)
target_include_directories(menu_panel_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
pm_test(notice)
pm_test(held)
pm_test(held_scan)
target_compile_definitions(held_scan_test PRIVATE PM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
pm_test(raw_scan)
target_compile_definitions(raw_scan_test PRIVATE PM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
pm_test(logistics_scan)
target_compile_definitions(logistics_scan_test PRIVATE PM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
pm_test(mods_panel host/mod_deps.cpp sdk/api/game_api.cpp sdk/api/runtime.cpp sdk/fake/fake_game.cpp sdk/runtime/scene_facts.cpp
  sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp)
target_include_directories(mods_panel_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
pm_test(custom_ghosts sdk/fake/fake_game.cpp sdk/runtime/scene_facts.cpp sdk/runtime/content_registry.cpp
  sdk/runtime/content_acting.cpp mods/custom_ghosts/gremlin.cpp mods/custom_ghosts/hunt.cpp mods/custom_ghosts/squatter.cpp
  mods/custom_ghosts/squatter_figure.cpp mods/custom_ghosts/squatter_ragdoll.cpp mods/custom_ghosts/squatter_door.cpp
  mods/custom_ghosts/sensor_copy.cpp
  mods/custom_ghosts/ghost_sight.cpp mods/custom_ghosts/squatter_voice.cpp mods/custom_ghosts/sound.cpp)
target_compile_definitions(custom_ghosts_test PRIVATE PM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(custom_ghosts_test PRIVATE pmmod)
# Before the mods' folders, as for content_difficulty_test below.
target_include_directories(custom_ghosts_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
pm_test(squatter mods/custom_ghosts/squatter_voice.cpp mods/custom_ghosts/sound.cpp mods/custom_ghosts/squatter_figure.cpp
  mods/custom_ghosts/squatter_ragdoll.cpp
  sdk/api/bundles.cpp sdk/api/models.cpp sdk/api/game_api.cpp
  sdk/api/runtime.cpp sdk/fake/fake_game.cpp sdk/runtime/scene_facts.cpp sdk/runtime/content_registry.cpp
  sdk/runtime/content_acting.cpp core/log.cpp)
target_compile_definitions(squatter_test PRIVATE PM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
# Before the mods' folders, as for content_difficulty_test below.
target_include_directories(squatter_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
pm_test(squatter_door mods/custom_ghosts/squatter_door.cpp sdk/api/game_api.cpp sdk/api/runtime.cpp
  sdk/fake/fake_game.cpp sdk/runtime/scene_facts.cpp sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp
  core/log.cpp)
# Before the mods' folders, as for content_difficulty_test below.
target_include_directories(squatter_door_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
pm_test(squatter_ragdoll mods/custom_ghosts/squatter_ragdoll.cpp sdk/api/game_api.cpp sdk/api/runtime.cpp
  sdk/fake/fake_game.cpp sdk/runtime/scene_facts.cpp sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp
  core/log.cpp)
# Before the mods' folders, as for content_difficulty_test below.
target_include_directories(squatter_ragdoll_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
# A mod's rebindable keys (sdk/api/keybinds.h).
pm_test(keybinds sdk/api/keybinds.cpp sdk/api/game_api.cpp sdk/api/runtime.cpp sdk/fake/fake_game.cpp
  sdk/runtime/scene_facts.cpp sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp core/log.cpp)
target_include_directories(keybinds_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
# The Emotes mod's rules, message and emotes' table (mods/emotes/emote_rule.h, emote_net.h, emote_assets.h).
pm_test(emotes)
target_include_directories(emotes_test PRIVATE mods/emotes)
pm_test(emotes_play)
target_include_directories(emotes_play_test PRIVATE mods/emotes)
pm_test(emotes_wheel)
target_include_directories(emotes_wheel_test PRIVATE mods/emotes)
# Playing a clip on a player's own body (sdk/api/player_anim.h).
pm_test(player_anim sdk/api/player_anim.cpp sdk/api/game_api.cpp sdk/api/runtime.cpp sdk/fake/fake_game.cpp
  sdk/runtime/scene_facts.cpp sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp core/log.cpp)
target_include_directories(player_anim_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
# The third-person view and the shown body (sdk/api/third_person.h).
pm_test(third_person sdk/api/third_person.cpp sdk/api/game_api.cpp sdk/api/runtime.cpp sdk/fake/fake_game.cpp
  sdk/runtime/scene_facts.cpp sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp core/log.cpp)
target_include_directories(third_person_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
# A sound counted as a player talking (sdk/api/talk_loudness.h).
pm_test(talk_loudness sdk/api/talk_loudness.cpp sdk/api/game_api.cpp sdk/api/runtime.cpp sdk/fake/fake_game.cpp
  sdk/runtime/scene_facts.cpp sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp core/log.cpp)
target_include_directories(talk_loudness_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
# A screen blur and vignette of a mod's own (sdk/api/screen_effects.h).
pm_test(screen_effects sdk/api/screen_effects.cpp sdk/api/game_api.cpp sdk/api/runtime.cpp sdk/fake/fake_game.cpp
  sdk/runtime/scene_facts.cpp sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp core/log.cpp)
target_include_directories(screen_effects_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
pm_test(models sdk/api/bundles.cpp sdk/api/models.cpp sdk/api/game_api.cpp sdk/api/runtime.cpp sdk/fake/fake_game.cpp
  sdk/runtime/scene_facts.cpp sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp core/log.cpp)
target_include_directories(models_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
pm_test(brownie mods/custom_ghosts/brownie.cpp sdk/api/game_api.cpp sdk/api/runtime.cpp sdk/fake/fake_game.cpp
  sdk/runtime/scene_facts.cpp sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp core/log.cpp)
target_compile_definitions(brownie_test PRIVATE PM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
# Before the mods' folders, as for content_difficulty_test below.
target_include_directories(brownie_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
pm_test(restraint mods/custom_ghosts/restraint.cpp sdk/api/content.cpp sdk/api/game_api.cpp sdk/api/runtime.cpp
  sdk/fake/fake_game.cpp sdk/runtime/scene_facts.cpp sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp
  core/log.cpp)
# Before the mods' folders, as for content_difficulty_test below.
target_include_directories(restraint_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
pm_test(actions mods/playable_ghost/content_link.cpp sdk/api/content.cpp sdk/api/game_api.cpp sdk/api/runtime.cpp
  sdk/fake/fake_game.cpp sdk/runtime/scene_facts.cpp sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp
  core/log.cpp)
# Before the mods' folders, as for content_difficulty_test below.
target_include_directories(actions_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
pm_test(hunt_rules mods/custom_ghosts/hunt.cpp sdk/api/game_api.cpp sdk/api/runtime.cpp sdk/fake/fake_game.cpp
  sdk/runtime/scene_facts.cpp sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp core/log.cpp)
# Before the mods' folders, as for content_difficulty_test below.
target_include_directories(hunt_rules_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
pm_test(movement host/steering.cpp host/steer_stub.asm host/hooks.cpp tests/steer_harness.asm
  mods/playable_ghost/walk_state.cpp
  sdk/api/game_api.cpp sdk/api/runtime.cpp sdk/fake/fake_game.cpp sdk/runtime/scene_facts.cpp
  sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp core/log.cpp)
target_include_directories(movement_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
target_include_directories(movement_test PRIVATE third_party/minhook/src/hde)
target_link_libraries(movement_test PRIVATE minhook)
# PM_HOST: the host's steering reads its entries through pm::api::host_only, which the test defines over the fake.
target_compile_definitions(movement_test PRIVATE PM_HOST)
pm_test(senses mods/playable_ghost/session.cpp sdk/api/game_api.cpp sdk/api/runtime.cpp sdk/fake/fake_game.cpp
  sdk/runtime/scene_facts.cpp sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp core/log.cpp)
# Before the mods' folders, as for content_difficulty_test below.
target_include_directories(senses_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
pm_test(loadout mods/playable_ghost/session.cpp sdk/api/game_api.cpp sdk/api/runtime.cpp sdk/fake/fake_game.cpp
  sdk/runtime/scene_facts.cpp sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp core/log.cpp)
target_include_directories(loadout_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
pm_test(round mods/playable_ghost/round.cpp mods/playable_ghost/session.cpp sdk/api/content.cpp sdk/api/game_api.cpp sdk/api/runtime.cpp
  sdk/fake/fake_game.cpp sdk/runtime/scene_facts.cpp sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp
  core/log.cpp)
target_include_directories(round_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
pm_test(contract_end mods/playable_ghost/contract_end.cpp sdk/api/game_api.cpp sdk/api/runtime.cpp
  sdk/fake/fake_game.cpp sdk/runtime/scene_facts.cpp sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp
  core/log.cpp)
target_include_directories(contract_end_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
pm_test(leave mods/playable_ghost/leave.cpp sdk/api/game_api.cpp sdk/api/runtime.cpp
  sdk/fake/fake_game.cpp sdk/runtime/scene_facts.cpp sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp
  core/log.cpp)
target_include_directories(leave_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
pm_test(outlist mods/playable_ghost/outlist.cpp sdk/api/game_api.cpp sdk/api/runtime.cpp
  sdk/fake/fake_game.cpp sdk/runtime/scene_facts.cpp sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp
  core/log.cpp)
target_include_directories(outlist_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
pm_test(mod_session host/mod_session.cpp core/log.cpp)
pm_test(net sdk/api/game_api.cpp sdk/api/runtime.cpp sdk/fake/fake_game.cpp sdk/runtime/scene_facts.cpp
  sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp core/log.cpp)
target_include_directories(net_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
target_compile_definitions(net_test PRIVATE PM_HOST)
# The host contract's alive and the runtime services' array check: it includes host/il2cpp.cpp and the services' source.
pm_test(runtime_services core/log.cpp)
target_include_directories(runtime_services_test BEFORE PRIVATE sdk/api sdk/map sdk/runtime sdk/resolver)
target_include_directories(runtime_services_test PRIVATE third_party/minhook/include)
pm_test(room_watch host/room_watch.cpp sdk/api/game_api.cpp sdk/api/runtime.cpp sdk/fake/fake_game.cpp
  sdk/runtime/scene_facts.cpp sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp core/log.cpp)
target_include_directories(room_watch_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
target_compile_definitions(room_watch_test PRIVATE PM_HOST)
pm_test(reward sdk/api/game_api.cpp sdk/api/runtime.cpp sdk/fake/fake_game.cpp
  sdk/runtime/scene_facts.cpp sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp core/log.cpp)
target_include_directories(reward_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
pm_test(spectral_flame mods/spectral_flame/contract.cpp mods/spectral_flame/flames.cpp sdk/fake/fake_game.cpp
  sdk/runtime/scene_facts.cpp sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp)
target_link_libraries(spectral_flame_test PRIVATE pmmod)
# Before the mods' folders, as for content_difficulty_test below.
target_include_directories(spectral_flame_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
pm_test(ghost_tells mods/ghost_tells/world.cpp mods/ghost_tells/gate.cpp mods/ghost_tells/hold.cpp
  mods/ghost_tells/rolls.cpp mods/ghost_tells/mare.cpp mods/ghost_tells/spirit.cpp sdk/fake/fake_game.cpp sdk/runtime/scene_facts.cpp
  sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp)
target_link_libraries(ghost_tells_test PRIVATE pmmod)
# Before the mods' folders, as for content_difficulty_test.
target_include_directories(ghost_tells_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
pm_test(content_target host/content_target.cpp mods/playable_ghost/ghostbook.cpp
  sdk/fake/fake_game.cpp sdk/runtime/scene_facts.cpp sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp)
target_link_libraries(content_target_test PRIVATE pmmod)
# Before the mods' folders, as for content_difficulty_test.
target_include_directories(content_target_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
pm_test(glue mods/playable_ghost/yokai_ears.cpp mods/playable_ghost/chances.cpp mods/playable_ghost/footsteps.cpp
  mods/playable_ghost/mimic.cpp mods/playable_ghost/guess.cpp mods/playable_ghost/session.cpp mods/playable_ghost/hide.cpp
  sdk/fake/fake_game.cpp sdk/runtime/scene_facts.cpp
  sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp)
target_link_libraries(glue_test PRIVATE pmmod)
# core/bindings.cpp over the fake game backend.
pm_test(bindings core/bindings.cpp sdk/fake/fake_game.cpp sdk/runtime/scene_facts.cpp sdk/runtime/content_registry.cpp
  sdk/runtime/content_acting.cpp)
target_link_libraries(bindings_test PRIVATE pmmod)
target_include_directories(bindings_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
# Playable Ghost's body and room view over the fake game backend, the sources included as actions_test's is.
pm_test(body mods/playable_ghost/session.cpp sdk/api/content.cpp sdk/api/game_api.cpp sdk/api/runtime.cpp
  sdk/fake/fake_game.cpp sdk/runtime/scene_facts.cpp sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp
  core/log.cpp)
target_include_directories(body_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
pm_test(panorama sdk/api/content.cpp sdk/api/game_api.cpp sdk/api/runtime.cpp
  sdk/fake/fake_game.cpp sdk/runtime/scene_facts.cpp sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp
  core/log.cpp)
target_include_directories(panorama_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
# core/game.cpp's room gates over the fake game backend.
pm_test(game_gates host/host_game.cpp sdk/fake/fake_game.cpp sdk/runtime/scene_facts.cpp sdk/runtime/content_registry.cpp
  sdk/runtime/content_acting.cpp)
target_link_libraries(game_gates_test PRIVATE pmmod)
target_include_directories(game_gates_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
# Before the mods' folders, as for content_difficulty_test.
target_include_directories(glue_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
pm_test(score)
pm_test(typestate)
pm_test(perceive)
pm_test(hooks host/hooks.cpp host/entry_hooks.cpp host/context.cpp sdk/api/game_api.cpp core/log.cpp)
target_include_directories(hooks_test PRIVATE sdk/api sdk/map third_party/minhook/src/hde)
target_link_libraries(hooks_test PRIVATE minhook)
target_compile_definitions(hooks_test PRIVATE PM_HOOKS_TEST)
# A mod for the tests that load one (mods_test, gamecheck): it requires GhostAI.UnAppear and Camera.
add_library(pm_fake_mod SHARED tests/fixtures/fake_mod/mod.cpp)
target_link_libraries(pm_fake_mod PRIVATE pmmod)
pm_options(pm_fake_mod)
# The same mod as built against host contract 2.0, for mods_test's older-minor check.
add_library(pm_fake_mod_2_0 SHARED tests/fixtures/fake_mod/mod.cpp)
target_link_libraries(pm_fake_mod_2_0 PRIVATE pmmod)
target_compile_definitions(pm_fake_mod_2_0 PRIVATE "PM_FAKE_MOD_API_VERSION=((2u << 16) | 0u)")
pm_options(pm_fake_mod_2_0)
# And against host contract 2.1, the minor version before content.
add_library(pm_fake_mod_2_1 SHARED tests/fixtures/fake_mod/mod.cpp)
target_link_libraries(pm_fake_mod_2_1 PRIVATE pmmod)
target_compile_definitions(pm_fake_mod_2_1 PRIVATE "PM_FAKE_MOD_API_VERSION=((2u << 16) | 1u)")
pm_options(pm_fake_mod_2_1)
# And against host contract 2.2, the released minor before PM_GHOST_NO_MIMIC_COPY.
add_library(pm_fake_mod_2_2 SHARED tests/fixtures/fake_mod/mod.cpp)
target_link_libraries(pm_fake_mod_2_2 PRIVATE pmmod)
target_compile_definitions(pm_fake_mod_2_2 PRIVATE "PM_FAKE_MOD_API_VERSION=((2u << 16) | 2u)")
pm_options(pm_fake_mod_2_2)
# And as built against the first contract (version 1, no minor), for gamecheck's version check.
add_library(pm_fake_mod_1 SHARED tests/fixtures/fake_mod/mod.cpp)
target_link_libraries(pm_fake_mod_1 PRIVATE pmmod)
target_compile_definitions(pm_fake_mod_1 PRIVATE PM_FAKE_MOD_API_VERSION=1u)
pm_options(pm_fake_mod_1)
# Another mod, which uses the first when it is there, for mods_test's dependency checks.
add_library(pm_fake_mod_optional SHARED tests/fixtures/fake_mod/mod.cpp)
target_link_libraries(pm_fake_mod_optional PRIVATE pmmod)
target_compile_definitions(pm_fake_mod_optional PRIVATE PM_FAKE_MOD_ID="fake-optional" PM_FAKE_MOD_CHANNEL=201
  PM_FAKE_MOD_ASKS="fake-mod")
pm_options(pm_fake_mod_optional)
# One that says in its DLL that it records voice, for the check against its manifest.
add_library(pm_fake_mod_voice SHARED tests/fixtures/fake_mod/mod.cpp)
target_link_libraries(pm_fake_mod_voice PRIVATE pmmod)
target_compile_definitions(pm_fake_mod_voice PRIVATE PM_FAKE_MOD_ID="fake-voice" PM_FAKE_MOD_CHANNEL=202 PM_FAKE_MOD_FLAGS=PM_MOD_RECORDS_VOICE)
pm_options(pm_fake_mod_voice)
# One that requires no game entry, which runs whatever a build lacks.
add_library(pm_fake_mod_plain SHARED tests/fixtures/fake_mod/mod.cpp)
target_link_libraries(pm_fake_mod_plain PRIVATE pmmod)
target_compile_definitions(pm_fake_mod_plain PRIVATE PM_FAKE_MOD_ID="fake-plain" PM_FAKE_MOD_CHANNEL=203 PM_FAKE_MOD_NO_ENTRIES)
pm_options(pm_fake_mod_plain)
# Another on that one's channel.
add_library(pm_fake_mod_same_channel SHARED tests/fixtures/fake_mod/mod.cpp)
target_link_libraries(pm_fake_mod_same_channel PRIVATE pmmod)
target_compile_definitions(pm_fake_mod_same_channel PRIVATE PM_FAKE_MOD_ID="fake-same" PM_FAKE_MOD_CHANNEL=203 PM_FAKE_MOD_NO_ENTRIES)
pm_options(pm_fake_mod_same_channel)
# One built before the rename, exporting the old names, which the host still loads (core/mod_exports.h).
add_library(pm_fake_mod_old_exports SHARED tests/fixtures/fake_mod/mod.cpp)
target_link_libraries(pm_fake_mod_old_exports PRIVATE pmmod)
target_compile_definitions(pm_fake_mod_old_exports PRIVATE PM_FAKE_MOD_OLD_EXPORTS)
pm_options(pm_fake_mod_old_exports)
# A mod that throws out of what the host asks before it starts, as a mod written without the SDK's glue could.
add_library(pm_fake_mod_throws SHARED tests/fixtures/fake_mod/throwing.cpp)
target_include_directories(pm_fake_mod_throws PRIVATE core)
pm_options(pm_fake_mod_throws)
target_compile_options(pm_fake_mod_throws PRIVATE /EHs /EHc-)  # its extern "C" functions do throw
add_library(pm_fake_mod_throws_requires SHARED tests/fixtures/fake_mod/throwing.cpp)
target_include_directories(pm_fake_mod_throws_requires PRIVATE core)
target_compile_definitions(pm_fake_mod_throws_requires PRIVATE PM_FAKE_THROW_IN_REQUIRES)
pm_options(pm_fake_mod_throws_requires)
target_compile_options(pm_fake_mod_throws_requires PRIVATE /EHs /EHc-)
pm_test(mods host/mods.cpp host/mod_deps.cpp host/mod_session.cpp host/hooks.cpp core/log.cpp)
target_include_directories(mods_test PRIVATE third_party/minhook/src/hde)
target_link_libraries(mods_test PRIVATE minhook bcrypt)
target_compile_definitions(mods_test PRIVATE PM_FAKE_MOD="$<TARGET_FILE:pm_fake_mod>"
  PM_FAKE_MOD_2_0="$<TARGET_FILE:pm_fake_mod_2_0>" PM_FAKE_MOD_2_1="$<TARGET_FILE:pm_fake_mod_2_1>"
  PM_FAKE_MOD_2_2="$<TARGET_FILE:pm_fake_mod_2_2>"
  PM_FAKE_MOD_OPTIONAL="$<TARGET_FILE:pm_fake_mod_optional>" PM_FAKE_MOD_VOICE="$<TARGET_FILE:pm_fake_mod_voice>"
  PM_FAKE_MOD_PLAIN="$<TARGET_FILE:pm_fake_mod_plain>" PM_FAKE_MOD_SAME_CHANNEL="$<TARGET_FILE:pm_fake_mod_same_channel>"
  PM_FAKE_MOD_THROWS="$<TARGET_FILE:pm_fake_mod_throws>" PM_FAKE_MOD_THROWS_REQUIRES="$<TARGET_FILE:pm_fake_mod_throws_requires>"
  PM_FAKE_MOD_1="$<TARGET_FILE:pm_fake_mod_1>" PM_FAKE_MOD_OLD_EXPORTS="$<TARGET_FILE:pm_fake_mod_old_exports>"
  PM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_dependencies(mods_test pm_fake_mod pm_fake_mod_1 pm_fake_mod_2_0 pm_fake_mod_2_1 pm_fake_mod_2_2 pm_fake_mod_optional pm_fake_mod_voice
  pm_fake_mod_plain pm_fake_mod_same_channel pm_fake_mod_throws pm_fake_mod_throws_requires pm_fake_mod_old_exports)
pm_test(mod_glue core/mod_glue.cpp sdk/api/game_api.cpp sdk/api/runtime.cpp sdk/api/content.cpp sdk/fake/fake_game.cpp
  sdk/runtime/scene_facts.cpp sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp core/log.cpp)
target_include_directories(mod_glue_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
pm_test(game_api sdk/api/game_api.cpp sdk/api/runtime.cpp sdk/fake/fake_game.cpp sdk/runtime/scene_facts.cpp
  sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp core/log.cpp)
target_include_directories(game_api_test PRIVATE sdk/api sdk/map sdk/fake sdk/runtime)
pm_test(content sdk/api/content.cpp sdk/api/game_api.cpp sdk/api/runtime.cpp sdk/fake/fake_game.cpp
  sdk/runtime/scene_facts.cpp sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp host/content_types.cpp
  host/content_text.cpp host/content_evidence.cpp
  core/log.cpp)
# Before the mods' folders, as for content_difficulty_test below.
target_include_directories(content_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime sdk/resolver)
target_include_directories(content_test PRIVATE sdk/examples/example_ghost)
pm_test(content_journal sdk/api/content.cpp sdk/api/game_api.cpp sdk/api/runtime.cpp sdk/fake/fake_game.cpp
  sdk/runtime/scene_facts.cpp sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp host/content_types.cpp
  host/content_text.cpp host/content_journal.cpp host/content_evidence.cpp host/content_localised.cpp
  host/content_difficulty.cpp core/log.cpp mods/spectral_flame/contract.cpp)
# Before the mods' folders, as for content_difficulty_test below.
target_include_directories(content_journal_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime sdk/resolver)
pm_test(content_difficulty sdk/api/content.cpp sdk/api/game_api.cpp sdk/api/runtime.cpp sdk/fake/fake_game.cpp
  sdk/runtime/scene_facts.cpp sdk/runtime/content_registry.cpp sdk/runtime/content_acting.cpp host/content_types.cpp
  host/content_text.cpp host/content_difficulty.cpp core/log.cpp)
# Before the mods' folders, so no mod's header stands for one of the map's on a file system that ignores
# case.
target_include_directories(content_difficulty_test BEFORE PRIVATE sdk/api sdk/map sdk/fake sdk/runtime sdk/resolver)
# Wrappers that must not compile (PM_EXPECT_COMPILE_FAIL 1 to 7) and the same written right (0), built
# only by the test, which expects each wrong one to fail with its static_assert. The test builds them
# outside the developer environment validate.ps1 builds in, so it gets this configure's compiler
# environment.
foreach(case RANGE 0 7)
  add_library(compile_fail_${case} OBJECT EXCLUDE_FROM_ALL tests/compile_fail/wrappers.cpp)
  target_include_directories(compile_fail_${case} PRIVATE core sdk/api sdk/map)
  target_compile_definitions(compile_fail_${case} PRIVATE PM_EXPECT_COMPILE_FAIL=${case})
  pm_options(compile_fail_${case})
endforeach()
set(PM_COMPILER_ENVIRONMENT)
foreach(var INCLUDE LIB LIBPATH PATH)
  if(DEFINED ENV{${var}})
    string(REPLACE ";" "\\;" value "$ENV{${var}}")
    list(APPEND PM_COMPILER_ENVIRONMENT "${var}=${value}")
  endif()
endforeach()
add_test(NAME wrapper_arity_matches_entry COMMAND ${CMAKE_COMMAND} -DBUILD=${CMAKE_BINARY_DIR}
  -P ${CMAKE_SOURCE_DIR}/tests/compile_fail_test.cmake)
set_tests_properties(wrapper_arity_matches_entry PROPERTIES ENVIRONMENT "${PM_COMPILER_ENVIRONMENT}")
pm_test(sdk_types)
target_link_libraries(sdk_types_test PRIVATE pmsdk_resolver)
pm_test(sdk_resolver)
target_link_libraries(sdk_resolver_test PRIVATE pmsdk_resolver)
pm_test(sdk_code)
target_link_libraries(sdk_code_test PRIVATE pmsdk_resolver)
pm_test(sdk_dump sdk/offline/dump_model.cpp)
target_include_directories(sdk_dump_test PRIVATE sdk/offline)
target_link_libraries(sdk_dump_test PRIVATE pmsdk_resolver)
target_compile_definitions(sdk_dump_test PRIVATE PM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
pm_test(runtime_model sdk/runtime/runtime_model.cpp sdk/offline/dump_model.cpp)
target_include_directories(runtime_model_test PRIVATE sdk/runtime sdk/offline)
target_link_libraries(runtime_model_test PRIVATE pmsdk_resolver)
target_compile_definitions(runtime_model_test PRIVATE PM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
pm_test(game_map sdk/runtime/runtime_model.cpp sdk/runtime/scene_facts.cpp sdk/api/game_api.cpp)
target_include_directories(game_map_test PRIVATE sdk/map sdk/runtime)
target_link_libraries(game_map_test PRIVATE pmsdk_resolver)
target_compile_definitions(game_map_test PRIVATE PM_HOST PM_GAME_MAP_TEST)
# The host's static field access, with host/il2cpp.cpp compiled in and its exports faked.
pm_test(il2cpp_static core/log.cpp)
target_include_directories(il2cpp_static_test PRIVATE sdk/runtime sdk/resolver)
pm_test(map)
target_include_directories(map_test PRIVATE sdk/map)
target_link_libraries(map_test PRIVATE pmsdk_resolver)
target_compile_definitions(map_test PRIVATE PM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
# gamecheck's failure paths that the built tool cannot be driven into; it supplies its own dump reader.
pm_test(gamecheck_main)
target_include_directories(gamecheck_main_test PRIVATE sdk/map sdk/offline sdk/runtime)
target_link_libraries(gamecheck_main_test PRIVATE pmsdk_resolver bcrypt)
add_test(NAME gamecheck COMMAND ${CMAKE_COMMAND} -DGAMECHECK=$<TARGET_FILE:gamecheck>
  -DWORK=${CMAKE_BINARY_DIR}/gamecheck_test -DFAKE_MOD=$<TARGET_FILE:pm_fake_mod>
  -DFAKE_MOD_1=$<TARGET_FILE:pm_fake_mod_1> -DFAKE_MOD_VOICE=$<TARGET_FILE:pm_fake_mod_voice>
  -DEXPECT_DUMP=${CMAKE_SOURCE_DIR}/tests/fixtures/expect_dump
  -DDUMP=${CMAKE_SOURCE_DIR}/tests/fixtures/diffable -P ${CMAKE_SOURCE_DIR}/tests/gamecheck_test.cmake)


# The API reference, from sdk/Doxyfile. Not part of the default build: tools/docs.ps1 needs Doxygen, and validate.ps1 runs it.
add_custom_target(docs
  COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_SOURCE_DIR}/tools/docs.ps1"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMENT "Building the API reference"
  VERBATIM)