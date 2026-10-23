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
    file(MAKE_DIRECTORY "${WORK}/dump/Assembly-CSharp" "${WORK}/dump/UnityEngine.CoreModule")
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
execute_process(COMMAND "${GAMECHECK}" --dump "${DUMP}" --image "${GAMECHECK}" --mods "${mods}"
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
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
