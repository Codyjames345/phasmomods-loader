# A failed gamecheck run leaves no address table behind: the RE scripts would read the previous
# build's addresses from it. Run by ctest with -DGAMECHECK=<exe> -DWORK=<scratch dir>.
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(table "${WORK}/map-addresses.tsv")

file(WRITE "${WORK}/not-an-image.dll" "not a PE file")
foreach(case "no-such-dump" "unreadable-image" "unreadable-unity-player")
  file(WRITE "${table}" "GhostAI.Hunting\tmethod\t0x1\t0x1\n")
  if(case STREQUAL "no-such-dump")
    set(args --dump "${WORK}/no-such-dump" --image "${GAMECHECK}" --no-method-tables)
  elseif(case STREQUAL "unreadable-unity-player")
    # A readable dump and image, and a UnityPlayer.dll that is not a PE32+ file.
    set(args --dump "${DUMP}" --image "${GAMECHECK}" --no-method-tables --unity-player "${WORK}/not-an-image.dll")
  else()
    # A dump folder with the assemblies the map names, and an image that is not a PE32+ file.
    file(MAKE_DIRECTORY "${WORK}/dump/Assembly-CSharp" "${WORK}/dump/UnityEngine.AnimationModule" "${WORK}/dump/UnityEngine.UI"
      "${WORK}/dump/UnityEngine.CoreModule" "${WORK}/dump/mscorlib" "${WORK}/dump/PhotonUnityNetworking"
      "${WORK}/dump/PhotonRealtime" "${WORK}/dump/Photon3Unity3D" "${WORK}/dump/DoozyRuntime" "${WORK}/dump/UnityEngine.PhysicsModule"
      "${WORK}/dump/UnityEngine.AIModule" "${WORK}/dump/UnityEngine.AudioModule" "${WORK}/dump/DOTween"
      "${WORK}/dump/Micosmo.SensorToolkit" "${WORK}/dump/Unity.Services.Vivox" "${WORK}/dump/UnityEngine.UIModule" "${WORK}/dump/Unity.InputSystem"
      "${WORK}/dump/com.rlabrecque.steamworks.net" "${WORK}/dump/Assembly-CSharp-firstpass"
      "${WORK}/dump/Unity.RenderPipelines.Core.Runtime" "${WORK}/dump/Unity.RenderPipelines.Universal.Runtime")
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
execute_process(COMMAND "${GAMECHECK}" --dump "${DUMP}" --image "${GAMECHECK}" --no-method-tables --mods "${mods}"
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

# The host's own steering entries are no mod's: without a UnityPlayer.dll its native sites are missing, and the
# HOST line names them, after the summary.
string(REGEX MATCH "HOST steering unavailable: [^\n]*UnityPlayer\\.SteerSite[^\n]*\n" hostLine "${out}")
string(FIND "${out}" "HOST steering" hostAt)
if(NOT hostLine OR hostAt LESS summary)
  message(FATAL_ERROR "no HOST steering line naming UnityPlayer.SteerSite after the summary\n${out}${err}")
endif()
message(STATUS "The HOST line names the steering entries this build lacks.")

# An icall cannot be checked without the running game: it is listed as checked at startup only and
# counted apart from the entries that failed.
string(FIND "${out}" "STARTUP Animator.SetBoolID UnityEngine.Animator::SetBoolID (checked at startup only)\n" icall)
string(REGEX MATCH "missing, [0-9]+ ambiguous, [0-9]+ blocked, 54 checked at startup only, 56 checked in the scene files\n"
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
# One line per scene fact, in map order: the first, one whose path ends in a space (kept), the last.
set(first "Scene.JournalBookContent\tscene\t1\tResources/PCPlayer\tContent/GhostHuntingBookContent\tJournalController\t\n")
set(spaced "Scene.JournalGhostPageEvidence2\tscene\t1\tResources/PCPlayer\tContent/GhostHuntingBookContent/Spirit/Evidence 2 \tJournalController\t\n")
set(last "ShaderProperty.Lit.Smoothness\tshader\t0\tTanglewood_Street_House\t\tUniversal Render Pipeline/Lit\t_Smoothness\n")
string(FIND "${written}" "${first}" atFirst)
string(FIND "${written}" "${spaced}" atSpaced)
string(FIND "${written}" "${last}" atLast)
string(REGEX MATCHALL "\n" lines "${written}")
list(LENGTH lines count)
string(LENGTH "${written}" size)
string(LENGTH "${last}" lastSize)
math(EXPR lastAt "${size} - ${lastSize}")
if(NOT atFirst EQUAL 0 OR atSpaced EQUAL -1 OR NOT atLast EQUAL lastAt OR NOT count EQUAL 56)
  message(FATAL_ERROR "--scene-entries wrote\n${written}")
endif()
message(STATUS "--scene-entries writes the scene facts for assetcheck.")

# gamecheck_mods_checks_api_version: a mod built against a contract the host does not run never
# loads, whatever it requires, so it is named with its contract. The fake mod built against version 1
# (the first contract, which had no minor) stands for one.
set(old "${WORK}/old-mods")
file(MAKE_DIRECTORY "${old}/fake")
file(COPY "${FAKE_MOD_1}" DESTINATION "${old}/fake")
execute_process(COMMAND "${GAMECHECK}" --dump "${DUMP}" --image "${GAMECHECK}" --no-method-tables --mods "${old}"
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

# A DLL that records voice under a manifest that does not say so stays off in the host, so the check says so. With the
# manifest saying it, the check has only the entries to say. The fake mod that records voice requires what the fake mod does.
set(voice "${WORK}/voice-mods")
file(MAKE_DIRECTORY "${voice}/fake")
file(COPY "${FAKE_MOD_VOICE}" DESTINATION "${voice}/fake")
file(WRITE "${voice}/fake/manifest.json" "{\"id\":\"fake.voice\",\"name\":\"N\",\"author\":\"A\",\"version\":\"1.0.0\",\"description\":\"D\",\"dll\":\"v.dll\"}")
execute_process(COMMAND "${GAMECHECK}" --dump "${DUMP}" --image "${GAMECHECK}" --no-method-tables --mods "${voice}"
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
string(FIND "${out}" "would switch off fake-voice: it records voice, and its manifest does not say so\n" said)
if(NOT code EQUAL 1 OR said EQUAL -1)
  message(FATAL_ERROR "--mods: no line for a DLL recording voice against its manifest, exit ${code}\n${out}${err}")
endif()
file(WRITE "${voice}/fake/manifest.json" "{\"id\":\"fake.voice\",\"name\":\"N\",\"author\":\"A\",\"version\":\"1.0.0\",\"description\":\"D\",\"dll\":\"v.dll\",\"recordsVoice\":true}")
execute_process(COMMAND "${GAMECHECK}" --dump "${DUMP}" --image "${GAMECHECK}" --no-method-tables --mods "${voice}"
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
string(FIND "${out}" "it records voice" said)
if(NOT said EQUAL -1)
  message(FATAL_ERROR "--mods: a manifest that says it records voice was still named\n${out}${err}")
endif()
message(STATUS "--mods names a DLL that records voice against its manifest.")

# gamecheck_unreadable_mods_dir_exits_2: a mods folder that does not exist, or a file given as one,
# cannot be read, and that is unreadable input.
file(WRITE "${WORK}/not-a-folder" "a file")
foreach(dir "${WORK}/no-such-folder" "${WORK}/not-a-folder")
  execute_process(COMMAND "${GAMECHECK}" --dump "${DUMP}" --image "${GAMECHECK}" --no-method-tables --mods "${dir}"
    RESULT_VARIABLE code OUTPUT_QUIET ERROR_VARIABLE err)
  string(FIND "${err}" "gamecheck: cannot read the mods folder" said)
  if(NOT code EQUAL 2 OR said EQUAL -1)
    message(FATAL_ERROR "${dir}: gamecheck exited ${code}, expected 2 and why\n${err}")
  endif()
endforeach()
message(STATUS "An unreadable mods folder exits 2.")

# --expect: each "<key> <hex>" line is a value the key must resolve to. A dump with GhostAI.Awake at
# RVA 0x2A40 stands for the game; the files are the migration's record of replaced values.
set(expectDump "${WORK}/expect-dump")
file(MAKE_DIRECTORY "${expectDump}/Assembly-CSharp" "${expectDump}/UnityEngine.AnimationModule" "${expectDump}/UnityEngine.UI"
  "${expectDump}/UnityEngine.CoreModule" "${expectDump}/mscorlib" "${expectDump}/PhotonUnityNetworking"
  "${expectDump}/com.rlabrecque.steamworks.net" "${expectDump}/PhotonRealtime" "${expectDump}/Photon3Unity3D" "${expectDump}/DoozyRuntime" "${expectDump}/UnityEngine.PhysicsModule"
  "${expectDump}/UnityEngine.AIModule" "${expectDump}/UnityEngine.AudioModule" "${expectDump}/DOTween"
  "${expectDump}/Micosmo.SensorToolkit" "${expectDump}/Unity.Services.Vivox" "${expectDump}/UnityEngine.UIModule" "${expectDump}/Unity.InputSystem"
  "${expectDump}/Unity.TextMeshPro" "${expectDump}/Assembly-CSharp-firstpass"
  "${expectDump}/Unity.RenderPipelines.Core.Runtime" "${expectDump}/Unity.RenderPipelines.Universal.Runtime")
file(COPY "${EXPECT_DUMP}/GhostAI.cs" DESTINATION "${expectDump}/Assembly-CSharp")
function(run_expect name text)
  set(file "${WORK}/${name}.txt")
  file(WRITE "${file}" "${text}")
  execute_process(COMMAND "${GAMECHECK}" --dump "${expectDump}" --image "${GAMECHECK}" --no-method-tables --expect "${file}"
    RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
  set(expect_out "${out}${err}" PARENT_SCOPE)
  set(expect_code "${code}" PARENT_SCOPE)
endfunction()

# expect_match_passes: the map's other required entries are unresolved in this small dump, so the run
# exits 1 for them with or without the expectation; the expectation itself adds no report, no error
# and no change of exit code.
execute_process(COMMAND "${GAMECHECK}" --dump "${expectDump}" --image "${GAMECHECK}" --no-method-tables
  RESULT_VARIABLE withoutCode OUTPUT_QUIET ERROR_QUIET)
set(matchFile "${WORK}/match.txt")
file(WRITE "${matchFile}" "GhostAI.Awake 0x2A40\n")
execute_process(COMMAND "${GAMECHECK}" --dump "${expectDump}" --image "${GAMECHECK}" --no-method-tables --expect "${matchFile}"
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
string(FIND "${out}" "EXPECT" said)
if(NOT said EQUAL -1 OR NOT code EQUAL withoutCode OR NOT err STREQUAL "")
  message(FATAL_ERROR "expect_match_passes: exit ${code} (${withoutCode} without), stderr '${err}'\n${out}")
endif()
message(STATUS "A matching expectation passes.")

# expect_mismatch_names_the_key
run_expect(mismatch "GhostAI.Awake 0x2A41\n")
string(FIND "${expect_out}" "EXPECT GhostAI.Awake: map 0x2A40 != expected 0x2A41\n" said)
if(NOT expect_code EQUAL 1 OR said EQUAL -1)
  message(FATAL_ERROR "expect_mismatch_names_the_key: exit ${expect_code}\n${expect_out}")
endif()
message(STATUS "A mismatch names the key.")

# expect_unknown_key_exits_2: also a line that is not "<key> <hex>", or a value over 64 bits.
foreach(text "No.Such.Key 0x10\n" "GhostAI.Awake 2A40\n" "GhostAI.Awake\n" "GhostAI.Awake 0x10000000000000000\n")
  run_expect(unknown "${text}")
  if(NOT expect_code EQUAL 2)
    message(FATAL_ERROR "expect_unknown_key_exits_2: '${text}' exit ${expect_code}\n${expect_out}")
  endif()
endforeach()
message(STATUS "An unknown key or a malformed line exits 2.")

# gamecheck_content_enums: custom content is numbered from one past the game's own GhostType and
# EvidenceType values, read at runtime, so the check reports the ranges this build holds and asserts no
# fixed number. A dump with a ghost type added reports the wider range; neither run fails for it. The
# dump here holds only the traits struct and the two enums, so other entries' failures set the exit
# code: the CONTENT line is what is checked.
set(ghostNames Spirit Wraith Phantom Poltergeist Banshee Jinn Mare Revenant Shade Demon Yurei Oni Yokai Hantu
  Goryo Myling Onryo TheTwins Raiju Obake Mimic Moroi Deogen Thaye None Gallu Dayan Obambo Kormos Aswang Deildegast)
function(content_dump name extra)
  set(dir "${WORK}/${name}")
  file(MAKE_DIRECTORY "${dir}/Assembly-CSharp" "${dir}/UnityEngine.AnimationModule" "${dir}/UnityEngine.CoreModule" "${dir}/UnityEngine.UI" "${dir}/mscorlib" "${dir}/PhotonUnityNetworking"
    "${dir}/com.rlabrecque.steamworks.net" "${dir}/PhotonRealtime" "${dir}/Photon3Unity3D" "${dir}/DoozyRuntime" "${dir}/UnityEngine.PhysicsModule"
    "${dir}/UnityEngine.AIModule" "${dir}/UnityEngine.AudioModule" "${dir}/DOTween"
    "${dir}/Micosmo.SensorToolkit" "${dir}/Unity.Services.Vivox" "${dir}/UnityEngine.UIModule" "${dir}/Unity.TextMeshPro" "${dir}/Unity.InputSystem" "${dir}/Assembly-CSharp-firstpass"
    "${dir}/Unity.RenderPipelines.Core.Runtime" "${dir}/Unity.RenderPipelines.Universal.Runtime")
  set(values "")
  set(i 0)
  foreach(ghost IN LISTS ghostNames)
    string(APPEND values "\t\t${ghost} = ${i},\n")
    math(EXPR i "${i} + 1")
  endforeach()
  string(APPEND values "${extra}")
  set(fields "")
  set(offset 0)
  foreach(type "Kind" "Kind" "List<Evidence>" "List<Evidence>" "string" "bool" "bool" "bool")
    string(APPEND fields "\t[FieldOffset(Offset = \"0x${offset}\")]\n\tpublic ${type} field${offset}; //Field offset: 0x${offset}\n")
    math(EXPR offset "${offset} + 8")
  endforeach()
  file(WRITE "${dir}/Assembly-CSharp/Traits.cs" "//Type is in global namespace\n\npublic struct Traits\n{\n\tinternal enum Kind\n\t{\n${values}\t}\n\n${fields}}\n")
  file(WRITE "${dir}/Assembly-CSharp/Evidence.cs" "//Type is in global namespace\n\npublic enum Evidence\n{\n\tNone = 0,\n\tEMF = 1,\n\tSpiritBox = 2,\n\tultraViolet = 3,\n\tGhostOrb = 4,\n\tGhostWritingBook = 5,\n\tTemperature = 6,\n\tDotsProjector = 7,\n}\n")
  execute_process(COMMAND "${GAMECHECK}" --dump "${dir}" --image "${GAMECHECK}" --no-method-tables RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
  set(content_out "${out}${err}" PARENT_SCOPE)
endfunction()
content_dump(content-ok "")
string(FIND "${content_out}" "CONTENT GhostType 0..30, EvidenceType 0..7\n" said)
if(said EQUAL -1)
  message(FATAL_ERROR "gamecheck_content_enums: no CONTENT line with the audited ranges\n${content_out}")
endif()
content_dump(content-more "\t\tWisp = 31,\n")
string(FIND "${content_out}" "CONTENT GhostType 0..31, EvidenceType 0..7\n" said)
if(said EQUAL -1)
  message(FATAL_ERROR "gamecheck_content_enums: an added ghost type is not in the range\n${content_out}")
endif()
message(STATUS "The CONTENT line reports the ranges custom content is numbered after.")

# gamecheck_writes_the_build_record: the file is the two libraries' SHA-256 and the map's digest, named for the
# build, and nothing else; writing it again replaces it. A library that cannot be read writes nothing and exits 2.
set(records "${WORK}/records/sdk/builds")
set(engines "${CMAKE_CURRENT_LIST_DIR}/fixtures/engine")
file(WRITE "${WORK}/GameAssembly.dll" "the game's code")
file(WRITE "${WORK}/UnityPlayer.dll" "the engine")
file(SHA256 "${WORK}/GameAssembly.dll" gameHash)
file(SHA256 "${WORK}/UnityPlayer.dll" unityHash)
execute_process(COMMAND "${GAMECHECK}" --map-digest "${WORK}/digest.txt" RESULT_VARIABLE code OUTPUT_QUIET ERROR_VARIABLE err)
if(NOT code EQUAL 0)
  message(FATAL_ERROR "--map-digest: exit ${code}\n${err}")
endif()
file(READ "${WORK}/digest.txt" digest)
string(STRIP "${digest}" digest)
if(NOT digest MATCHES "^[0-9a-f]+$")
  message(FATAL_ERROR "--map-digest wrote '${digest}'")
endif()
string(SUBSTRING "${gameHash}" 0 16 gameShort)
string(SUBSTRING "${unityHash}" 0 16 unityShort)
set(name "${gameShort}-${unityShort}.txt")
foreach(round 1 2)
  execute_process(COMMAND "${GAMECHECK}" --write-record "${records}" --image "${WORK}/GameAssembly.dll"
    --unity-player "${WORK}/UnityPlayer.dll" --game-data "${engines}/known" RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
  if(NOT code EQUAL 0 OR NOT out STREQUAL "RECORD ${name}\n")
    message(FATAL_ERROR "gamecheck_writes_the_build_record: exit ${code}\n${out}${err}")
  endif()
endforeach()
file(READ "${records}/${name}" written)
set(wanted "# A game build the offline check passed. tools/gamecheck.ps1 writes this file.\ngameassembly=${gameHash}\nunityplayer=${unityHash}\nmap=${digest}\n")
if(NOT written STREQUAL wanted)
  message(FATAL_ERROR "gamecheck_writes_the_build_record: wrote\n${written}\nwanted\n${wanted}")
endif()
file(GLOB writtenRecords "${records}/*")
list(LENGTH writtenRecords recordCount)
if(NOT recordCount EQUAL 1)
  message(FATAL_ERROR "gamecheck_writes_the_build_record: ${recordCount} files in the folder")
endif()
execute_process(COMMAND "${GAMECHECK}" --write-record "${WORK}/no-record" --image "${WORK}/no-such.dll"
  --unity-player "${WORK}/UnityPlayer.dll" --game-data "${engines}/known" RESULT_VARIABLE code OUTPUT_QUIET ERROR_VARIABLE err)
if(NOT code EQUAL 2 OR EXISTS "${WORK}/no-record")
  message(FATAL_ERROR "an unreadable library: exit ${code}\n${err}")
endif()
message(STATUS "The check writes the build record.")

# A build on another Unity or metadata version, or whose versions cannot be read, gets no record: the host stays inactive
# on it. The check says so too, and that counts as a failure, so tools/gamecheck.ps1 would not ask for a record.
foreach(data "other-unity" "other-metadata" "no-such-folder")
  execute_process(COMMAND "${GAMECHECK}" --write-record "${WORK}/refused-record" --image "${WORK}/GameAssembly.dll"
    --unity-player "${WORK}/UnityPlayer.dll" --game-data "${engines}/${data}" RESULT_VARIABLE code OUTPUT_QUIET ERROR_VARIABLE err)
  string(FIND "${err}" "gamecheck: no record: this loader's layouts were written for Unity" said)
  if(NOT code EQUAL 2 OR said EQUAL -1 OR EXISTS "${WORK}/refused-record")
    message(FATAL_ERROR "${data}: a record was written, or no reason given: exit ${code}\n${err}")
  endif()
  execute_process(COMMAND "${GAMECHECK}" --dump "${DUMP}" --image "${GAMECHECK}" --no-method-tables --game-data "${engines}/${data}"
    RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
  string(FIND "${out}" "ENGINE refused: this loader's layouts were written for Unity" said)
  if(NOT code EQUAL 1 OR said EQUAL -1)
    message(FATAL_ERROR "${data}: the check did not refuse the versions: exit ${code}\n${out}${err}")
  endif()
endforeach()
execute_process(COMMAND "${GAMECHECK}" --dump "${DUMP}" --image "${GAMECHECK}" --no-method-tables --game-data "${engines}/known"
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
string(FIND "${out}" "ENGINE 2022.3.40f1, metadata 31\n" said)
if(said EQUAL -1)
  message(FATAL_ERROR "the check does not name the versions it accepts\n${out}${err}")
endif()
message(STATUS "A build on another engine or metadata version is refused, and gets no record.")

# The image is not a game build, so without --no-method-tables it is refused: the check never falls back to the
# dump's addresses on its own.
execute_process(COMMAND "${GAMECHECK}" --dump "${DUMP}" --image "${GAMECHECK}"
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT code EQUAL 2 OR NOT err MATCHES "holds no code-gen module for")
  message(FATAL_ERROR "an image without code-gen modules was accepted: ${code} ${err}")
endif()
