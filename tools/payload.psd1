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
        @{ Name = 'mods/spectral-flame/SpectralFlame.dll'; Source = 'build/SpectralFlame.dll' }
        @{ Name = 'mods/spectral-flame/manifest.json'; Source = 'mods/spectral_flame/manifest.json' }
        @{ Name = 'mods/spectral-flame/i18n/default.json'; Source = 'mods/spectral_flame/i18n/default.json' }
        @{ Name = 'mods/ghost-tells/GhostTells.dll'; Source = 'build/GhostTells.dll' }
        @{ Name = 'mods/ghost-tells/manifest.json'; Source = 'mods/ghost_tells/manifest.json' }
        @{ Name = 'mods/ghost-tells/i18n/default.json'; Source = 'mods/ghost_tells/i18n/default.json' }
    )
    Records = @{ Name = 'builds'; Source = 'sdk/builds' }
}
