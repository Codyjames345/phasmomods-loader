# Player-facing text

Everything a player reads lives in string tables, never in the code (the author's choice, 2026-11-03, following SMAPI's
`i18n` folders). Each component has one table, a flat JSON object of key to text:

| Table | Entries | `PM_TEXT_AI` | `PM_TEXT_TODO` |
| --- | --- | --- | --- |
| `i18n/default.json` | 239 | 0 | 1 |
| `mods/custom_ghosts/i18n/default.json` | 78 | 9 | 0 |
| `mods/emotes/i18n/default.json` | 9 | 0 | 1 |
| `mods/ghost_tells/i18n/default.json` | 6 | 0 | 0 |
| `mods/playable_ghost/i18n/default.json` | 142 | 140 | 2 |
| `mods/spectral_flame/i18n/default.json` | 2 | 0 | 0 |

- **`PM_TEXT_AI`** marks an entry AI wrote: before the AI policy (`AI_POLICY.md`), or as a draft the author asked for (the
  Squatter's journal page, 2026-11-09). It stays until the author rewrites it; then the mark comes off.
- **`PM_TEXT_TODO`** marks a placeholder waiting for the author's words. The game shows it as written until then.
- **`{{token}}`** is a value the code fills in, such as a player's name or a number. Keep each token a text names, or the
  value is lost; `tests/text_table_test.cpp` checks every key the code asks for exists with the tokens it gives, and that no
  key goes unused.
- **Emphasis** inside a sentence (`<b>`, `<color=...>`) is part of the text, so it can be restyled here. Tags that only place
  or dim a line stay in the code.

Edit a table in any text editor: one entry per line, `//` comments allowed. The launcher copies each table beside the
component's DLL as it loads, so a change shows the next time the game starts with the mods.

The entries with no mark are the author's own: the custom ghosts' journal pages and abilities, Ghost Tells' journal lines,
Spectral Flame's evidence name and journal line, and the consent notice's paragraph on mods being programs. Each mod's name
and description are in its `manifest.json`, which the Mods panel reads before the mod loads.
