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
        # repository (.github/workflows/build.yml). Custom Ghosts does not start without it, so a stage or release fails.
        @{ Name = 'mods/custom-ghosts/assets/squatter.bundle'; Source = 'mods/custom_ghosts/assets/squatter.bundle' }
        # The Squatter's voice folders (sdk/api/assets.h): the launcher copies each mod's assets folder beside its DLL copy,
        # and the mod does not start without these. A recording added to a folder is listed here too.
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/lurk/.gitkeep'; Source = 'mods/custom_ghosts/assets/squatter/voice/lurk/.gitkeep' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/chase/.gitkeep'; Source = 'mods/custom_ghosts/assets/squatter/voice/chase/.gitkeep' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/search/.gitkeep'; Source = 'mods/custom_ghosts/assets/squatter/voice/search/.gitkeep' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/hiding/.gitkeep'; Source = 'mods/custom_ghosts/assets/squatter/voice/hiding/.gitkeep' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/catch/.gitkeep'; Source = 'mods/custom_ghosts/assets/squatter/voice/catch/.gitkeep' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/door/.gitkeep'; Source = 'mods/custom_ghosts/assets/squatter/voice/door/.gitkeep' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/tired/.gitkeep'; Source = 'mods/custom_ghosts/assets/squatter/voice/tired/.gitkeep' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/mope/.gitkeep'; Source = 'mods/custom_ghosts/assets/squatter/voice/mope/.gitkeep' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/excited/.gitkeep'; Source = 'mods/custom_ghosts/assets/squatter/voice/excited/.gitkeep' }
        @{ Name = 'mods/custom-ghosts/assets/squatter/voice/friendly/.gitkeep'; Source = 'mods/custom_ghosts/assets/squatter/voice/friendly/.gitkeep' }
        @{ Name = 'mods/spectral-flame/SpectralFlame.dll'; Source = 'build/SpectralFlame.dll' }
        @{ Name = 'mods/spectral-flame/manifest.json'; Source = 'mods/spectral_flame/manifest.json' }
        @{ Name = 'mods/spectral-flame/i18n/default.json'; Source = 'mods/spectral_flame/i18n/default.json' }
        @{ Name = 'mods/ghost-tells/GhostTells.dll'; Source = 'build/GhostTells.dll' }
        @{ Name = 'mods/ghost-tells/manifest.json'; Source = 'mods/ghost_tells/manifest.json' }
        @{ Name = 'mods/ghost-tells/i18n/default.json'; Source = 'mods/ghost_tells/i18n/default.json' }
    )
    Records = @{ Name = 'builds'; Source = 'sdk/builds' }
}
