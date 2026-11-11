#pragma once

namespace pm::loader {

/** How large the shared native log (mods.log) may grow before the next load rotates it. */
inline constexpr long long kNativeLogCapBytes = 20ll * 1024 * 1024;

/**
 * Clear the loader's data folder (%LOCALAPPDATA%\PhasmoMods) of what earlier sessions left, and cap the shared
 * native log. The launcher calls it before staging a load's fresh copies (tools/inject.cpp, copy_native) and at
 * Steam startup (tools/launcher.cpp, launch_steam), and nothing in it can fail a load.
 *
 * In bin, every name that carries a process id goes when that game process is gone: the copy names tools/inject.cpp
 * makes (host and mod DLLs, their manifest and string-table sidecars, and each mod's .assets folder, all
 * "<stem>-<pid>-<tick>-<attempt>") and the session file "session-<pid>.txt". They stay while a Phasmophobia.exe
 * owns the pid, so a game still running from an earlier launch keeps everything it has loaded. In logs, every
 * "game-<pid>-<tick>.log" that tools/launcher.cpp reserved goes when its writer's process is gone. Once mods.log
 * passes kNativeLogCapBytes the load moves it to mods.log.old, replacing the previous copy there, and the session
 * that follows opens a fresh mods.log; while a host is still writing, Windows refuses the move and the next load
 * tries again.
 *
 * A name carrying no process id belongs to something else and is never touched. A file or folder that cannot be
 * deleted, because another process holds it, is left for the next load; an incomplete process inventory stops the
 * sweep entirely, so nothing is removed on a partial view.
 */
void sweep_previous_sessions();

}  // namespace pm::loader
