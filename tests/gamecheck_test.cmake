# A failed gamecheck run leaves no address table behind: the RE scripts would read the previous
# build's addresses from it. Run by ctest with -DGAMECHECK=<exe> -DWORK=<scratch dir>.
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(table "${WORK}/map-addresses.tsv")

foreach(case "no-such-dump" "unreadable-image")
  file(WRITE "${table}" "GhostAI.Hunting\tmethod\t0x1\t0x1\n")
  if(case STREQUAL "no-such-dump")
    set(args --dump "${WORK}/no-such-dump" --image "${GAMECHECK}")
  else()
    # A dump folder with the assemblies the map names, and an image that is not a PE32+ file.
    file(MAKE_DIRECTORY "${WORK}/dump/Assembly-CSharp" "${WORK}/dump/UnityEngine.AnimationModule"
      "${WORK}/dump/UnityEngine.CoreModule")
    file(WRITE "${WORK}/not-an-image.dll" "not a PE file")
    set(args --dump "${WORK}/dump" --image "${WORK}/not-an-image.dll")
  endif()
  execute_process(COMMAND "${GAMECHECK}" ${args} --addresses "${table}" RESULT_VARIABLE code OUTPUT_QUIET ERROR_QUIET)
  if(NOT code EQUAL 2)
    message(FATAL_ERROR "${case}: gamecheck exited ${code}, expected 2")
  endif()
  if(EXISTS "${table}")
    message(FATAL_ERROR "${case}: the previous address table survived a failed run")
  endif()
endforeach()
message(STATUS "A failed run leaves no address table.")

# --mods: after the summary, each mod whose required entries did not resolve is named with them. The
# fake mod requires GhostAI.UnAppear (unresolved in the fixture dump) and Camera (resolved). A file
# that is not a mod is passed over.
set(mods "${WORK}/mods")
file(MAKE_DIRECTORY "${mods}/fake")
file(COPY "${FAKE_MOD}" DESTINATION "${mods}/fake")
file(WRITE "${mods}/not-a-mod.dll" "not a PE file")
set(sceneEntries "${WORK}/scene-entries.tsv")
execute_process(COMMAND "${GAMECHECK}" --dump "${DUMP}" --image "${GAMECHECK}" --mods "${mods}"
  --scene-entries "${sceneEntries}" RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT code EQUAL 1)
  message(FATAL_ERROR "--mods: gamecheck exited ${code}, expected 1\n${out}${err}")
endif()
string(FIND "${out}" "entries:" summary)
string(FIND "${out}" "would switch off fake-mod: GhostAI.UnAppear\n" line)
# The line ends after the one key: the resolved Camera is not named.
if(summary EQUAL -1 OR line LESS summary)
  message(FATAL_ERROR "--mods: no switch-off line naming only GhostAI.UnAppear after the summary\n${out}${err}")
endif()
message(STATUS "--mods names the mod that would be switched off and what it lacks.")

# An icall cannot be checked without the running game: it is listed as checked at startup only and
# counted apart from the entries that failed.
string(FIND "${out}" "STARTUP Animator.SetBoolID UnityEngine.Animator::SetBoolID (checked at startup only)\n" icall)
string(REGEX MATCH "missing, [0-9]+ ambiguous, [0-9]+ blocked, 1 checked at startup only, 2 checked in the scene files\n"
  counted "${out}")
if(icall EQUAL -1 OR NOT counted)
  message(FATAL_ERROR "the icall is not listed and counted as checked at startup only\n${out}${err}")
endif()
message(STATUS "An icall is listed as checked at startup only.")

# A scene fact is not in the code: it is listed as checked in the scene files, counted apart from the
# failures, and written with --scene-entries for tools/assetcheck.py.
string(FIND "${out}" "SCENE Layer.PlayerView PlayerView (checked in the scene files and in the running game)\n" layer)
if(layer EQUAL -1)
  message(FATAL_ERROR "the layer is not listed as checked in the scene files\n${out}${err}")
endif()
file(READ "${sceneEntries}" written)
set(expected "Scene.JournalBookContent\tscene\t1\tResources/PCPlayer\tContent/GhostHuntingBookContent\tJournalController\t\n")
string(APPEND expected "Layer.PlayerView\tlayer\t1\t\t\t\tPlayerView\n")
if(NOT written STREQUAL expected)
  message(FATAL_ERROR "--scene-entries wrote\n${written}\nexpected\n${expected}")
endif()
message(STATUS "--scene-entries writes the scene facts for assetcheck.")

# gamecheck_mods_checks_api_version: a mod built against a contract the host does not run never
# loads, whatever it requires, so it is named with its contract. The fake mod built against version 1
# (the first contract, which had no minor) stands for one.
set(old "${WORK}/old-mods")
file(MAKE_DIRECTORY "${old}/fake")
file(COPY "${FAKE_MOD_1}" DESTINATION "${old}/fake")
execute_process(COMMAND "${GAMECHECK}" --dump "${DUMP}" --image "${GAMECHECK}" --mods "${old}"
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT code EQUAL 1)
  message(FATAL_ERROR "old contract: gamecheck exited ${code}, expected 1\n${out}${err}")
endif()
string(FIND "${out}" "would not load fake-mod (contract 1.0)\n" line)
string(FIND "${out}" "would switch off fake-mod" switched)
if(line EQUAL -1 OR NOT switched EQUAL -1)
  message(FATAL_ERROR "old contract: no line saying fake-mod would not load (contract 1.0)\n${out}${err}")
endif()
message(STATUS "--mods names a mod built against another contract.")

# gamecheck_unreadable_mods_dir_exits_2: a mods folder that does not exist, or a file given as one,
# cannot be read, and that is unreadable input.
file(WRITE "${WORK}/not-a-folder" "a file")
foreach(dir "${WORK}/no-such-folder" "${WORK}/not-a-folder")
  execute_process(COMMAND "${GAMECHECK}" --dump "${DUMP}" --image "${GAMECHECK}" --mods "${dir}"
    RESULT_VARIABLE code OUTPUT_QUIET ERROR_VARIABLE err)
  string(FIND "${err}" "gamecheck: cannot read the mods folder" said)
  if(NOT code EQUAL 2 OR said EQUAL -1)
    message(FATAL_ERROR "${dir}: gamecheck exited ${code}, expected 2 and why\n${err}")
  endif()
endforeach()
message(STATUS "An unreadable mods folder exits 2.")
