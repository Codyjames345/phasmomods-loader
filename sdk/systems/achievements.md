# Steam achievements

How the game unlocks a Steam achievement. The loader leaves achievements alone: with mods on or off, they work as the
game's own. The entries are in `sdk/map/Steam.h`; their `///` notes are the authority. Facts were read from decompiled
code of Steam build 25811160 (`re/decomp.mjs <key>`), and this page names map keys, never addresses.

## The only way to Steam

The game's copy of Steamworks.NET is trimmed to what the game calls. `SteamUserStats` has five members:
`SteamUserStats.SetAchievement`, `SteamUserStats.StoreStats`, `SteamUserStats.GetAchievement`,
`SteamUserStats.RequestCurrentStats` and a reader of an achievement's display text. There is no call that sets a stat, so
no stat can unlock an achievement by itself, and no other Steamworks class in the build writes an achievement.

Every unlock takes one path, from the game's achievement list, a static class whose name is obfuscated:

1. Something in the game asks for an achievement by its number, with an optional amount. The reward screen, the main
   menu's start, `GhostOS.CheckTierUnlockAchievements`, `LevelStats.SaveMoneySpent`, `LevelStats.SendData`, the kill and
   witness RPCs and the training room all ask.
2. An achievement that is already marked unlocked in memory ends here.
3. An achievement with a progress key adds the amount to its counter, clamped to its target, saved under that key in the
   game's save file (EasySave3). Below the target it ends here.
4. On the Steam platform the game's Steam class calls `SetAchievement` with the achievement's name, logs whether it
   worked, and calls `StoreStats`. The two are called nowhere else.
5. The achievement is marked unlocked in memory, whatever `SetAchievement` answered, and a static event is raised. This
   writes nothing to disk.

## What else the game does with them

- **An achievement at its target is asked for again.** A counter at its target stays there, and the next time the game
  asks for that achievement, step 3 passes and step 4 runs. The bronze, silver and gold hunter badges are asked for at
  every start of the main menu, from a saved flag.
- **The in-memory mark lasts until the game restarts.** In the rest of that run the game counts the achievement as
  unlocked and asks Steam no more, whatever Steam answered. The game's trophies (`TieredAchievementCollectable`),
  collectables and event leaflet read the same list; what each does with it was not followed.
- **The next run reads Steam again.** The game's answer to `RequestCurrentStats` rereads every achievement's mark from
  `GetAchievement`.
- **Nothing is queued.** An unlock that Steam refused is not sent again later by itself.
