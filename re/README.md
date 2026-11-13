# Reverse-engineering tools

The scripts in this folder read the game's binaries to map it. What they find becomes map entries and notes
(`sdk/internals/reverse-engineering.md` says how, and lists the pitfalls). Never run them in, or write to, the game's
folder: everything below works on copies.

Everything these scripts read or write here is derived from the game, so it is git-ignored (`.gitignore`):
`re/dump/`, `re/names.tsv`, `re/map-addresses.tsv`, `re/ghidra/`, `re/out/` and `re/import.log`. A fresh clone has
none of them; the steps below make them.

## 1. The dump and the tables: `tools/gamecheck.ps1`

```powershell
.\tools\gamecheck.ps1 -GameDir "<the game's folder>" -Cpp2Il "<Cpp2IL.exe>"
```

(or set `PM_GAME_DIR` and `PM_CPP2IL` once and run it with no arguments). It copies the files Cpp2IL needs out of the
game's folder, dumps them, and writes:

- `re/dump/`: the Cpp2IL DiffableCs dump, one C# file per class with each field's offset as a comment;
- `re/names.tsv`: each GameAssembly method's RVA and signature;
- `re/map-addresses.tsv`: each map key's resolved value and length.

`re/names.mjs <DiffableCs dir>` rebuilds `re/names.tsv` from a dump on its own. `re/map.mjs` is the shared reader of the
two tables; a script that needs them stops with the command to run when either is missing.

## 2. Ghidra, for `decomp.mjs`

`re/decomp.mjs` runs Ghidra headless (`analyzeHeadless.bat`, found on PATH) on a project it never
changes:

- the project folder `re/ghidra`, or the folder `PM_GHIDRA_PROJECT` names, project name `GA`;
- the program `GameAssembly.dll` in it, imported once from a copy of the game's `GameAssembly.dll`;
- opened read-only with `-noanalysis`, so it decompiles from whatever analysis the import left;
- with `re/Decomp.java` as its post-script.

Ghidra rejects a project path with a folder whose name starts with '.', so a git worktree kept under `.claude` cannot
open this checkout's `re/ghidra`: set `PM_GHIDRA_PROJECT` (in the process environment only, never in a file, like
`PM_GAME_DIR`) to a project outside the dotted folder, for example the main checkout's `re\ghidra`. `decomp.mjs` checks
the folder before running and stops with the same advice.

Put the Ghidra installation's `support` folder, which holds `analyzeHeadless.bat`, on PATH; `decomp.mjs` searches PATH
entries only, never the folder it runs from. The one-time import, on Ghidra 12.1.4, ran no analysis at all (its
log, `re/import.log`, reports the import and nothing after it), and took the language from the file
(`x86:LE:64:default:windows`). From the repository's root, with `re\ghidra` an empty folder and `re\dump\game` already made
by `tools/gamecheck.ps1`:

```powershell
analyzeHeadless.bat re\ghidra GA -import re\dump\game\GameAssembly.dll -noanalysis *> re\import.log
```

A new game build needs this once more: empty `re\ghidra` first.

Full analysis of the 113 MB binary does not finish within an interactive Ghidra session's time limit, so for interactive
work use `re/slimpe.mjs` to build a slim copy holding only the functions you need.

## 3. The scripts

| Script | Usage | Needs |
| --- | --- | --- |
| `decomp.mjs` | `node re/decomp.mjs <out.c> <rva or map key> ...` | the tables, `re/ghidra` (or `PM_GHIDRA_PROJECT` from a worktree under `.claude`), Ghidra's `support` folder on PATH |
| `callers.mjs` | `node re/callers.mjs <GameAssembly.dll> <rva> ...` | `re/names.tsv` |
| `slimpe.mjs` | `node re/slimpe.mjs <GameAssembly.dll> <out.dll> <from>-<to>\|<map key> ...` | `re/names.tsv` |
| `strings.mjs` | `node re/strings.mjs <global-metadata.dat> [regex]` | nothing |
| `names.mjs` | `node re/names.mjs <DiffableCs dir>` | a dump |

Pass a copy of `GameAssembly.dll` and `global-metadata.dat`, never the game's own files. Raw numbers these scripts print
belong in git-ignored notes and `re/dump/expect/`, never in a committed file; committed notes cite map keys.
