# The release payload: each file's path in the release and its source in the project. Read by
# tools/payload_files.ps1, for tools/stage.ps1 (the launchable copy in dist\PhasmoMods), package.ps1 (the
# release) and their tests. Each mod is one folder under mods\ holding its DLL, its manifest.json and its string table
# (i18n/default.json, core/text_table.h); the loader's own table is i18n/default.json beside the launcher.
# Records names a folder rather than files: the offline check adds a file to it for each game build it passes.
@{
    Payload = @(
        @{ Name = 'PhasmoMods Loader.exe'; Source = 'build/PhasmoMods Loader.exe' }
        @{ Name = 'ModHost.dll'; Source = 'build/ModHost.dll' }
        @{ Name = 'i18n/default.json'; Source = 'i18n/default.json' }
        @{ Name = 'velopack_libc.dll'; Source = 'build/velopack_libc.dll' }
        @{ Name = 'README.md'; Source = 'docs/installation.md' }
        @{ Name = 'LICENSE.txt'; Source = 'LICENSE' }
        @{ Name = 'NOTICE.txt'; Source = 'NOTICE' }
        @{ Name = 'MinHook-LICENSE.txt'; Source = 'third_party/minhook/LICENSE.txt' }
        @{ Name = 'Velopack-LICENSE.txt'; Source = 'third_party/velopack/LICENSE.txt' }
        @{ Name = 'nlohmann-json-LICENSE.txt'; Source = 'third_party/nlohmann/LICENSE.MIT' }
        @{ Name = 'mods/playable-ghost/PlayableGhost.dll'; Source = 'build/PlayableGhost.dll' }
        @{ Name = 'mods/playable-ghost/manifest.json'; Source = 'mods/playable_ghost/manifest.json' }
        @{ Name = 'mods/playable-ghost/i18n/default.json'; Source = 'mods/playable_ghost/i18n/default.json' }
        @{ Name = 'mods/custom-ghosts/CustomGhosts.dll'; Source = 'build/CustomGhosts.dll' }
        @{ Name = 'mods/custom-ghosts/manifest.json'; Source = 'mods/custom_ghosts/manifest.json' }
        @{ Name = 'mods/custom-ghosts/i18n/default.json'; Source = 'mods/custom_ghosts/i18n/default.json' }
        # The Squatter's AssetBundle (its figure's clips; squatter_figure.h). Gitignored, since its Mixamo clips may not go in
        # the public repo: a local build has it from the user's Unity build, and CI fetches it from the private assets
        # repository (.github/workflows/build.yml, tools/place_assets.ps1). Custom Ghosts does not start without it, so a
        # stage or release fails. Sha256 pins the build this code needs: a stage, a package and CI each refuse any other.
        # tools/publish_assets.ps1 publishes a rebuilt one and moves the pin.
        @{ Name = 'mods/custom-ghosts/assets/squatter.bundle'; Source = 'mods/custom_ghosts/assets/squatter.bundle'; Sha256 = '08D891B72EED39EA96C0AB562C3116CA77F4E41A3D3366F9D6316BD00A1BD750' }
        # The Squatter's voice folders (sdk/api/assets.h): the launcher copies each mod's assets folder beside its DLL copy,
        # and the mod does not start without these. A recording added to a folder is listed here too.
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/lurk/.gitkeep'; Source = 'mods/custom_ghosts/assets/squatter/voice/lurk/.gitkeep' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/lurk/Lurk 01.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/lurk/Lurk 01.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/lurk/Lurk 02.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/lurk/Lurk 02.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/lurk/Lurk 03.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/lurk/Lurk 03.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/lurk/Lurk 04.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/lurk/Lurk 04.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/lurk/Lurk 05.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/lurk/Lurk 05.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/lurk/Lurk 06.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/lurk/Lurk 06.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/lurk/Lurk 07.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/lurk/Lurk 07.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/lurk/Lurk 08.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/lurk/Lurk 08.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/lurk/Lurk 09.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/lurk/Lurk 09.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/lurk/Lurk 10.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/lurk/Lurk 10.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/lurk/Lurk 11.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/lurk/Lurk 11.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/lurk/Lurk 12.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/lurk/Lurk 12.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/lurk/Lurk 13.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/lurk/Lurk 13.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/lurk/Lurk 14.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/lurk/Lurk 14.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/lurk/Lurk 15.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/lurk/Lurk 15.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/chase/.gitkeep'; Source = 'mods/custom_ghosts/assets/squatter/voice/chase/.gitkeep' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/chase/Chase 01.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/chase/Chase 01.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/chase/Chase 02.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/chase/Chase 02.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/chase/Chase 03.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/chase/Chase 03.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/chase/Chase 04.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/chase/Chase 04.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/chase/Chase 05.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/chase/Chase 05.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/chase/Chase 06.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/chase/Chase 06.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/chase/Chase 07.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/chase/Chase 07.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/chase/Chase 08.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/chase/Chase 08.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/chase/Chase 09.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/chase/Chase 09.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/chase/Chase 10.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/chase/Chase 10.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/chase/Chase 11.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/chase/Chase 11.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/chase/Chase 12.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/chase/Chase 12.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/chase/Chase 13.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/chase/Chase 13.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/chase/Chase 14.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/chase/Chase 14.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/chase/Chase 15.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/chase/Chase 15.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/chase/Chase 16.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/chase/Chase 16.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/chase/Chase 17.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/chase/Chase 17.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/search/.gitkeep'; Source = 'mods/custom_ghosts/assets/squatter/voice/search/.gitkeep' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/search/Search 01.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/search/Search 01.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/search/Search 02.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/search/Search 02.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/search/Search 03.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/search/Search 03.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/search/Search 04.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/search/Search 04.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/search/Search 05.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/search/Search 05.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/search/Search 06.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/search/Search 06.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/search/Search 07.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/search/Search 07.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/search/Search 08.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/search/Search 08.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/search/Search 09.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/search/Search 09.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/search/Search 10.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/search/Search 10.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/search/Search 11.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/search/Search 11.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/search/Search 12.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/search/Search 12.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/search/Search 13.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/search/Search 13.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/search/Search 14.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/search/Search 14.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/search/Search 15.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/search/Search 15.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/hiding/.gitkeep'; Source = 'mods/custom_ghosts/assets/squatter/voice/hiding/.gitkeep' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/hiding/Hiding 01.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/hiding/Hiding 01.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/hiding/Hiding 02.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/hiding/Hiding 02.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/hiding/Hiding 03.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/hiding/Hiding 03.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/hiding/Hiding 04.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/hiding/Hiding 04.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/hiding/Hiding 05.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/hiding/Hiding 05.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/hiding/Hiding 06.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/hiding/Hiding 06.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/hiding/Hiding 07.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/hiding/Hiding 07.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/hiding/Hiding 08.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/hiding/Hiding 08.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/hiding/Hiding 09.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/hiding/Hiding 09.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/hiding/Hiding 10.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/hiding/Hiding 10.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/hiding/Hiding 11.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/hiding/Hiding 11.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/hiding/Hiding 12.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/hiding/Hiding 12.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/hiding/Hiding 13.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/hiding/Hiding 13.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/hiding/Hiding 14.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/hiding/Hiding 14.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/hiding/Hiding 15.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/hiding/Hiding 15.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/catch/.gitkeep'; Source = 'mods/custom_ghosts/assets/squatter/voice/catch/.gitkeep' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/catch/Catch 01.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/catch/Catch 01.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/catch/Catch 02.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/catch/Catch 02.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/catch/Catch 03.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/catch/Catch 03.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/catch/Catch 04.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/catch/Catch 04.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/catch/Catch 05.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/catch/Catch 05.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/catch/Catch 06.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/catch/Catch 06.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/catch/Catch 07.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/catch/Catch 07.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/catch/Catch 08.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/catch/Catch 08.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/catch/Catch 09.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/catch/Catch 09.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/catch/Catch 10.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/catch/Catch 10.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/catch/Catch 11.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/catch/Catch 11.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/catch/Catch 12.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/catch/Catch 12.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/catch/Catch 13.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/catch/Catch 13.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/catch/Catch 14.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/catch/Catch 14.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/catch/Catch 15.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/catch/Catch 15.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/door/.gitkeep'; Source = 'mods/custom_ghosts/assets/squatter/voice/door/.gitkeep' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/door/Door 01.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/door/Door 01.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/door/Door 02.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/door/Door 02.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/door/Door 03.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/door/Door 03.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/door/Door 04.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/door/Door 04.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/door/Door 05.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/door/Door 05.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/door/Door 06.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/door/Door 06.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/door/Door 07.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/door/Door 07.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/door/Door 08.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/door/Door 08.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/door/Door 09.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/door/Door 09.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/door/Door 10.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/door/Door 10.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/door/Door 11.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/door/Door 11.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/door/Door 12.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/door/Door 12.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/door/Door 13.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/door/Door 13.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/door/Door 14.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/door/Door 14.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/door/Door 15.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/door/Door 15.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/tired/.gitkeep'; Source = 'mods/custom_ghosts/assets/squatter/voice/tired/.gitkeep' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/tired/Tired 01.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/tired/Tired 01.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/tired/Tired 02.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/tired/Tired 02.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/tired/Tired 03.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/tired/Tired 03.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/tired/Tired 04.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/tired/Tired 04.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/tired/Tired 05.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/tired/Tired 05.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/tired/Tired 06.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/tired/Tired 06.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/mope/.gitkeep'; Source = 'mods/custom_ghosts/assets/squatter/voice/mope/.gitkeep' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/mope/Mope 01.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/mope/Mope 01.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/mope/Mope 02.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/mope/Mope 02.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/mope/Mope 03.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/mope/Mope 03.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/mope/Mope 04.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/mope/Mope 04.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/mope/Mope 05.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/mope/Mope 05.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/mope/Mope 06.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/mope/Mope 06.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/mope/Mope 07.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/mope/Mope 07.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/mope/Mope 08.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/mope/Mope 08.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/mope/Mope 09.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/mope/Mope 09.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/mope/Mope 10.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/mope/Mope 10.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/excited/.gitkeep'; Source = 'mods/custom_ghosts/assets/squatter/voice/excited/.gitkeep' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/excited/Excited 01.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/excited/Excited 01.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/excited/Excited 02.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/excited/Excited 02.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/excited/Excited 03.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/excited/Excited 03.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/excited/Excited 04.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/excited/Excited 04.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/excited/Excited 05.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/excited/Excited 05.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/excited/Excited 06.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/excited/Excited 06.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/friendly/.gitkeep'; Source = 'mods/custom_ghosts/assets/squatter/voice/friendly/.gitkeep' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/friendly/Friendly 01.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/friendly/Friendly 01.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/friendly/Friendly 02.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/friendly/Friendly 02.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/friendly/Friendly 03.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/friendly/Friendly 03.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/friendly/Friendly 04.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/friendly/Friendly 04.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/friendly/Friendly 05.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/friendly/Friendly 05.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/friendly/Friendly 06.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/friendly/Friendly 06.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/friendly/Friendly 07.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/friendly/Friendly 07.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/friendly/Friendly 08.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/friendly/Friendly 08.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/friendly/Friendly 09.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/friendly/Friendly 09.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/friendly/Friendly 10.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/friendly/Friendly 10.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/friendly/Friendly 11.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/friendly/Friendly 11.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/friendly/Friendly 12.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/friendly/Friendly 12.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/friendly/Friendly 13.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/friendly/Friendly 13.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/friendly/Friendly 14.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/friendly/Friendly 14.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/friendly/Friendly 15.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/friendly/Friendly 15.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/trip/.gitkeep'; Source = 'mods/custom_ghosts/assets/squatter/voice/trip/.gitkeep' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/trip/Trip 01.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/trip/Trip 01.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/trip/Trip 02.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/trip/Trip 02.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/trip/Trip 03.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/trip/Trip 03.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/trip/Trip 04.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/trip/Trip 04.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/trip/Trip 05.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/trip/Trip 05.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/trip/Trip 06.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/trip/Trip 06.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/trip/Trip 07.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/trip/Trip 07.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/trip/Trip 08.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/trip/Trip 08.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/trip/Trip 09.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/trip/Trip 09.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/trip/Trip 10.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/trip/Trip 10.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/trip/Trip 11.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/trip/Trip 11.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/trip/Trip 12.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/trip/Trip 12.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/trip/Trip 13.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/trip/Trip 13.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/trip/Trip 14.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/trip/Trip 14.wav' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/trip/Trip 15.wav'; Source = 'mods/custom_ghosts/assets/squatter/voice/trip/Trip 15.wav' }
        @{ Name = 'mods/spectral-flame/SpectralFlame.dll'; Source = 'build/SpectralFlame.dll' }
        @{ Name = 'mods/spectral-flame/manifest.json'; Source = 'mods/spectral_flame/manifest.json' }
        @{ Name = 'mods/spectral-flame/i18n/default.json'; Source = 'mods/spectral_flame/i18n/default.json' }
        @{ Name = 'mods/ghost-tells/GhostTells.dll'; Source = 'build/GhostTells.dll' }
        @{ Name = 'mods/ghost-tells/manifest.json'; Source = 'mods/ghost_tells/manifest.json' }
        @{ Name = 'mods/ghost-tells/i18n/default.json'; Source = 'mods/ghost_tells/i18n/default.json' }
    )
    Records = @{ Name = 'builds'; Source = 'sdk/builds' }
}
