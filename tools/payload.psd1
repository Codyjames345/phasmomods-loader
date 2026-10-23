# The release payload: each file's path in the release and its source in the project. Read by
# tools/stage.ps1 (the launchable copy in dist\PhasmoMods), package.ps1 (the release ZIP) and their
# tests. Each mod is one folder under mods\ holding its DLL and its mod.ini.
@{
    Payload = @(
        @{ Name = 'ModLauncher.exe'; Source = 'build/ModLauncher.exe' }
        @{ Name = 'ModHost.dll'; Source = 'build/ModHost.dll' }
        @{ Name = 'README.md'; Source = 'docs/installation.md' }
        @{ Name = 'MinHook-LICENSE.txt'; Source = 'third_party/minhook/LICENSE.txt' }
        @{ Name = 'mods/playable-ghost/PlayableGhost.dll'; Source = 'build/PlayableGhost.dll' }
        @{ Name = 'mods/playable-ghost/mod.ini'; Source = 'mods/playable_ghost/mod.ini' }
        @{ Name = 'mods/custom-ghosts/CustomGhosts.dll'; Source = 'build/CustomGhosts.dll' }
        @{ Name = 'mods/custom-ghosts/mod.ini'; Source = 'mods/custom_ghosts/mod.ini' }
        @{ Name = 'mods/spectral-flame/SpectralFlame.dll'; Source = 'build/SpectralFlame.dll' }
        @{ Name = 'mods/spectral-flame/mod.ini'; Source = 'mods/spectral_flame/mod.ini' }
        @{ Name = 'mods/ghost-tells/GhostTells.dll'; Source = 'build/GhostTells.dll' }
        @{ Name = 'mods/ghost-tells/mod.ini'; Source = 'mods/ghost_tells/mod.ini' }
    )
}
