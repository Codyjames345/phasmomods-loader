# Example Emote Pack

The SDK's worked example of an emote pack: a content pack for the Emotes mod (`phasmomods.emotes`), data and no DLL.
It shows the files a pack holds and what its catalogue looks like; the guide is [an emote
pack](../../guides/emotes.md#an-emote-pack). The pack id is `example.emote-pack`, so its bundle is
`example.emote-pack.bundle`: naming the bundle after the pack id keeps it clear of other packs' bundles.

## The folder

| File | What it is |
| --- | --- |
| `manifest.json` | the pack: its id, and `contentPackFor` naming the Emotes mod and its minimum version. A pack has no `dll`. |
| `assets/emotes.json` | the catalogue: the bundle's file name and one emote, `example-dance`, with its clip, song, icon, label and loudness path, and the tool-only `clipSource`, `clipLoopGuess`, `songSource` and `songLoopGuess` fields. |
| `assets/loudness/example-dance.txt` | not included: the Build step writes it, the song's loudness measured from the song. |
| `assets/example.emote-pack.bundle` | not included: the Build step writes it, the bundle of this pack's clips, songs and icons. |
| `i18n/default.json` | the emote's name under the label key the catalogue names, `PM_TEXT_TODO` until the author writes it. |

The clip, song and icon are not here either: a pack's assets are its bundle, and an author builds one from their own
animation clip, song and icon. The catalogue's `Assets/...` paths are paths in the author's Unity project, where they
keep their own assets; nothing is read from this repository.

## Building it

The emote pack tool (`tools/emote_pack.ps1` and the `com.phasmomods.emote-tools` package) runs inside a Unity 2022.3
project that holds your clip, song and icon at the paths the catalogue names. Install the package into that project by
adding a line to its `Packages/manifest.json`, pointing at the package folder in an SDK copy:

```json
"com.phasmomods.emote-tools": "file:C:/<wherever you keep the SDK>/sdk/unity/com.phasmomods.emote-tools"
```

Then run the two steps, in this order. Each runs its own Unity; the pack folder you pass is this pack's `assets`
folder, which holds `emotes.json` and receives the loudness file and the bundle.

```powershell
pwsh -File tools\emote_pack.ps1 -Unity <Unity.exe> -Project <Unity project folder> -Pack <pack's assets folder> -FindLoops
pwsh -File tools\emote_pack.ps1 -Unity <Unity.exe> -Project <Unity project folder> -Pack <pack's assets folder> -Build
```

`-FindLoops` cuts the song and clip to their measured loops and writes the measured `songLoop` into the catalogue.
`-Build` measures the loudness file, builds the bundle and copies it into the pack folder. The finished pack is a
folder holding `manifest.json`, `assets/` and `i18n/`; the loader reads packs from its `mods` folder and lists them in
the Mods panel.
