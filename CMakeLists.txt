cmake_minimum_required(VERSION 3.24)
project(PhasmoMods LANGUAGES C CXX ASM_MASM RC)

# The suite version: one line in VERSION, compiled into the launcher and the host.
file(STRINGS VERSION PG_SUITE_VERSION LIMIT_COUNT 1)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS VERSION)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Every native target uses the same warnings and Windows settings.
function(pg_options target)
  target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4 /permissive- /utf-8 /EHsc>)
  target_compile_definitions(${target} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endfunction()

add_library(minhook STATIC
  third_party/minhook/src/buffer.c
  third_party/minhook/src/hook.c
  third_party/minhook/src/trampoline.c
  third_party/minhook/src/hde/hde64.c)
target_include_directories(minhook PUBLIC third_party/minhook/include)

# Shared by the host and every mod: IL2CPP access, game and Unity bindings, UI from game classes.
add_library(pgcore STATIC
  core/il2cpp.cpp
  core/log.cpp
  core/game.cpp
  core/ui.cpp
  core/notice.cpp
  core/bindings.cpp)
target_include_directories(pgcore PUBLIC core)
# il2cpp.cpp fills the game model's RuntimeApi (sdk/runtime).
target_include_directories(pgcore PRIVATE sdk/runtime sdk/resolver)
pg_options(pgcore)

# The game SDK's resolver: finds the game's types, members and code from the map, by shape.
file(GLOB PGSDK_RESOLVER_SOURCES CONFIGURE_DEPENDS sdk/resolver/*.cpp)
add_library(pgsdk_resolver STATIC ${PGSDK_RESOLVER_SOURCES})
target_include_directories(pgsdk_resolver PUBLIC sdk/api sdk/resolver)
target_link_libraries(pgsdk_resolver PUBLIC minhook)
target_include_directories(pgsdk_resolver PRIVATE third_party/minhook/src/hde)
pg_options(pgsdk_resolver)

# The offline check (tools/gamecheck.ps1): the map resolved against a Cpp2IL dump and GameAssembly.dll.
# With --mods it also loads mod DLLs and names those the host would switch off.
add_executable(gamecheck sdk/offline/gamecheck.cpp sdk/offline/dump_model.cpp)
target_include_directories(gamecheck PRIVATE core sdk/map sdk/offline)
target_link_libraries(gamecheck PRIVATE pgsdk_resolver)
pg_options(gamecheck)

# A mod's side of the host contract (core/mod_api.h) and the game API it calls (sdk/api, sdk/map).
add_library(pgmod STATIC core/mod_glue.cpp sdk/api/game_api.cpp)
target_include_directories(pgmod PUBLIC sdk/api sdk/map)
target_link_libraries(pgmod PUBLIC pgcore)
pg_options(pgmod)

# The host: startup checks, the only MinHook instance, consent, running the chosen mods (and the
# in-game Mods panel), and the join guard.
add_library(ModHost SHARED
  host/main.cpp
  host/hooks.cpp
  host/entry_hooks.cpp
  host/context.cpp
  host/net.cpp
  host/build_check.cpp
  host/suite.cpp
  host/consent.cpp
  host/mods.cpp
  host/menu_panel.cpp
  host/mods_panel.cpp
  host/room_actions.cpp
  host/game_map.cpp
  sdk/runtime/runtime_model.cpp)
target_include_directories(ModHost PRIVATE host sdk/map sdk/runtime)
target_link_libraries(ModHost PRIVATE pgcore pgsdk_resolver minhook bcrypt)
pg_options(ModHost)
target_compile_definitions(ModHost PRIVATE PG_SUITE_VERSION="${PG_SUITE_VERSION}")

# Mods. Each is one DLL with a mod.ini beside it in the release (tools/payload.psd1).
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
  mods/playable_ghost/equipment.cpp
  mods/playable_ghost/reward.cpp
  mods/playable_ghost/custom_link.cpp
  mods/playable_ghost/hide.cpp
  mods/playable_ghost/outcome.cpp
  mods/playable_ghost/mimic.cpp
  mods/playable_ghost/typestate_host.cpp
  mods/playable_ghost/marks.cpp
  mods/playable_ghost/alerts.cpp
  mods/playable_ghost/perceive_view.cpp
  mods/playable_ghost/form.cpp
  mods/playable_ghost/steer.cpp
  mods/playable_ghost/steer_stub.asm)
target_include_directories(PlayableGhost PRIVATE mods/playable_ghost)
target_link_libraries(PlayableGhost PRIVATE pgmod ws2_32)
pg_options(PlayableGhost)

add_library(CustomGhosts SHARED
  mods/custom_ghosts/mod.cpp
  mods/custom_ghosts/registry.cpp
  mods/custom_ghosts/difficulty.cpp
  mods/custom_ghosts/pages.cpp
  mods/custom_ghosts/behaviour.cpp
  mods/custom_ghosts/kikimora.cpp
  mods/custom_ghosts/echo.cpp
  mods/custom_ghosts/restraint.cpp
  mods/custom_ghosts/sound.cpp
  mods/custom_ghosts/apparition.cpp
  mods/custom_ghosts/hydra.cpp
  mods/custom_ghosts/asterion.cpp
  mods/custom_ghosts/lithobolia.cpp
  mods/custom_ghosts/gremlin.cpp
  mods/custom_ghosts/bogeyman.cpp
  mods/custom_ghosts/brownie.cpp
  mods/custom_ghosts/lethe.cpp
  mods/custom_ghosts/tempest.cpp
  mods/custom_ghosts/mesmer.cpp)
target_include_directories(CustomGhosts PRIVATE mods/custom_ghosts)
target_link_libraries(CustomGhosts PRIVATE pgmod)
pg_options(CustomGhosts)

add_library(SpectralFlame SHARED
  mods/spectral_flame/mod.cpp
  mods/spectral_flame/flames.cpp
  mods/spectral_flame/journal.cpp)
target_include_directories(SpectralFlame PRIVATE mods/spectral_flame)
target_link_libraries(SpectralFlame PRIVATE pgmod)
pg_options(SpectralFlame)

add_library(GhostTells SHARED
  mods/ghost_tells/mod.cpp
  mods/ghost_tells/world.cpp
  mods/ghost_tells/journal.cpp
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
target_link_libraries(GhostTells PRIVATE pgmod)
pg_options(GhostTells)

add_executable(pginject tools/inject.cpp)
target_include_directories(pginject PRIVATE tools core)
pg_options(pginject)
target_compile_definitions(pginject PRIVATE UNICODE _UNICODE)

add_library(pgloader STATIC tools/inject.cpp)
target_include_directories(pgloader PUBLIC tools core)
pg_options(pgloader)
target_compile_definitions(pgloader PRIVATE PG_LOADER_NO_MAIN UNICODE _UNICODE)

# Velopack's prebuilt C++ library (version in third_party/velopack/VERSION), used by the launcher's updater.
add_library(velopack SHARED IMPORTED)
set_target_properties(velopack PROPERTIES
  IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/third_party/velopack/velopack_libc.dll
  IMPORTED_IMPLIB ${CMAKE_SOURCE_DIR}/third_party/velopack/velopack_libc.lib
  INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/third_party/velopack)

add_executable(ModLauncher WIN32 tools/launcher.cpp tools/update.cpp tools/update_velopack.cpp tools/launcher.rc)
target_link_libraries(ModLauncher PRIVATE pgloader velopack user32 shell32 gdi32)
set_target_properties(ModLauncher PROPERTIES OUTPUT_NAME "PhasmoMods Loader")
pg_options(ModLauncher)
target_compile_definitions(ModLauncher PRIVATE UNICODE _UNICODE PG_SUITE_VERSION="${PG_SUITE_VERSION}")
add_custom_command(TARGET ModLauncher POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:velopack> $<TARGET_FILE_DIR:ModLauncher>)

# Offline tests (no game required): ctest --test-dir build
enable_testing()
function(pg_test name)
  add_executable(${name}_test tests/${name}_test.cpp ${ARGN})
  target_include_directories(${name}_test PRIVATE core host mods/playable_ghost mods/custom_ghosts mods/spectral_flame mods/ghost_tells tools)
  pg_options(${name}_test)
  add_test(NAME ${name} COMMAND ${name}_test)
endfunction()

pg_test(session mods/playable_ghost/session.cpp)
pg_test(payout)
pg_test(ghostbook mods/playable_ghost/ghostbook.cpp)
pg_test(room_pick)
pg_test(tether)
pg_test(startup)
target_include_directories(startup_test PRIVATE third_party/minhook/include sdk/api sdk/resolver sdk/runtime)
target_compile_definitions(startup_test PRIVATE PG_SUITE_VERSION="${PG_SUITE_VERSION}")
pg_test(build_check)
target_link_libraries(build_check_test PRIVATE bcrypt)
pg_test(inject)
target_compile_definitions(inject_test PRIVATE UNICODE _UNICODE)
pg_test(update tools/update.cpp tools/launcher.cpp)
target_link_libraries(update_test PRIVATE pgloader user32 shell32)
target_compile_definitions(update_test PRIVATE UNICODE _UNICODE PG_LAUNCHER_TEST PG_SUITE_VERSION="${PG_SUITE_VERSION}")
pg_test(launcher)
target_link_libraries(launcher_test PRIVATE user32 shell32)
target_compile_definitions(launcher_test PRIVATE UNICODE _UNICODE PG_SOURCE_DIR="${CMAKE_SOURCE_DIR}" PG_SUITE_VERSION="${PG_SUITE_VERSION}")
pg_test(lobby mods/playable_ghost/session.cpp)
pg_test(room_actions)
pg_test(pages)
pg_test(form)
pg_test(mod_list)
pg_test(presence)
pg_test(join_guard)
pg_test(consent)
pg_test(consent_ui)
pg_test(menu_panel)
pg_test(notice)
pg_test(held)
pg_test(held_scan)
target_compile_definitions(held_scan_test PRIVATE PG_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
pg_test(mods_panel)
pg_test(custom_ghosts)
target_compile_definitions(custom_ghosts_test PRIVATE PG_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
pg_test(registry)
pg_test(difficulty)
pg_test(senses mods/playable_ghost/session.cpp)
pg_test(spectral_flame)
pg_test(ghost_tells)
pg_test(score)
pg_test(typestate)
pg_test(perceive)
pg_test(hooks host/hooks.cpp host/entry_hooks.cpp host/context.cpp core/log.cpp)
target_include_directories(hooks_test PRIVATE sdk/api sdk/map)
target_link_libraries(hooks_test PRIVATE minhook)
# A mod for the tests that load one (mods_test, gamecheck): it requires GhostAI.UnAppear and Camera.
add_library(pg_fake_mod SHARED tests/fixtures/fake_mod/mod.cpp)
target_link_libraries(pg_fake_mod PRIVATE pgmod)
pg_options(pg_fake_mod)
pg_test(mods host/mods.cpp host/hooks.cpp host/build_check.cpp core/log.cpp)
target_link_libraries(mods_test PRIVATE minhook bcrypt)
target_compile_definitions(mods_test PRIVATE PG_FAKE_MOD="$<TARGET_FILE:pg_fake_mod>")
add_dependencies(mods_test pg_fake_mod)
pg_test(game_api sdk/api/game_api.cpp sdk/fake/fake_game.cpp core/log.cpp)
target_include_directories(game_api_test PRIVATE sdk/api sdk/map sdk/fake)
pg_test(sdk_types)
target_link_libraries(sdk_types_test PRIVATE pgsdk_resolver)
pg_test(sdk_resolver)
target_link_libraries(sdk_resolver_test PRIVATE pgsdk_resolver)
pg_test(sdk_code)
target_link_libraries(sdk_code_test PRIVATE pgsdk_resolver)
pg_test(sdk_dump sdk/offline/dump_model.cpp)
target_include_directories(sdk_dump_test PRIVATE sdk/offline)
target_link_libraries(sdk_dump_test PRIVATE pgsdk_resolver)
target_compile_definitions(sdk_dump_test PRIVATE PG_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
pg_test(runtime_model sdk/runtime/runtime_model.cpp sdk/offline/dump_model.cpp)
target_include_directories(runtime_model_test PRIVATE sdk/runtime sdk/offline)
target_link_libraries(runtime_model_test PRIVATE pgsdk_resolver)
target_compile_definitions(runtime_model_test PRIVATE PG_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
pg_test(game_map sdk/runtime/runtime_model.cpp)
target_include_directories(game_map_test PRIVATE sdk/map sdk/runtime)
target_link_libraries(game_map_test PRIVATE pgsdk_resolver)
pg_test(map)
target_include_directories(map_test PRIVATE sdk/map)
target_link_libraries(map_test PRIVATE pgsdk_resolver)
target_compile_definitions(map_test PRIVATE PG_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME gamecheck COMMAND ${CMAKE_COMMAND} -DGAMECHECK=$<TARGET_FILE:gamecheck>
  -DWORK=${CMAKE_BINARY_DIR}/gamecheck_test -DFAKE_MOD=$<TARGET_FILE:pg_fake_mod>
  -DDUMP=${CMAKE_SOURCE_DIR}/tests/fixtures/diffable -P ${CMAKE_SOURCE_DIR}/tests/gamecheck_test.cmake)
