# AI policy

PhasmoMods Loader is built with heavy use of generative AI. This page says where it is used and where it is not, for
players, mod authors and contributors. The README's AI Disclosure is the author's own account of why.

## Where AI is used

- **Reverse engineering.** Reading the game's decompiled code, finding the methods and fields the loader and the mods rely
  on, and writing those facts down as map entries.
- **C++ programming.** The loader, the SDK, the bundled mods and their tests.
- **Documentation.** The SDK guide and reference, the system pages, code comments, and the project's plans and notes.

## Where AI is not used

- **Assets.** No textures, models, sounds, icons or other art.
- **UI.** No screens, windows, layouts or visual design. The mods show their content in the game's own screens and widgets,
  and any new UI is designed by the author.
- **Player-facing text.** Nothing a player reads in the game: journal entries, ghost descriptions, popups, notices and
  menu labels are written by the author.
- **Ideas.** Ghosts, evidence, tells, mods and their designs come from the author, not from AI.

AI may carry out an idea the author gave, and may put the author's own words into the code, but it does not supply them.
Where code needs player-facing text that the author has not written yet, it is left as a marked placeholder for the author.

## For contributors

The same rules apply to contributions. Say in your pull request where you used AI. Don't submit AI-generated assets, UI,
player-facing text or ideas.
