# Loading investigation

The target is Steam app 739630, build 25811160, Unity 2022.3.40f1. Original game
executables and libraries remain unchanged on disk. This investigation concerns
loading; it does not establish that a complete multiplayer contract works.

## Files in the game folder

Every loader setup that left files beside the game executable ended the game at
startup, and the failure logs contained self-reporting and SIGABRT before the
main menu. That included leaving only the loader's directories and configuration,
with none of its modules loading, so the exact file trigger was never isolated:
treat any added file or folder in the game directory as detectable. With every
added file moved outside the game folder, Steam reached the menu. Static
analysis of this build shows a detection handler calling self-reporting and
deliberate crash/quit functions. This supports modification detection rather
than an ordinary mod crash, and it is why nothing is written to the game folder.

## PhasmoMenu comparison

Static inspection of the supplied PhasmoMenu v0.6.0.0 executable identified a
Cheat Engine trainer archive. Its Lua opens an already running Phasmophobia
process and starts MonoDataCollector. The embedded collector script loads its
native helper from outside the game folder. The inspected trainer scripts
contain no game-folder loader or detection-handler patch. This is a
static finding, not a claim that every embedded native component was audited.

The archive layout and trainer decoding were cross-checked against Cheat Engine's
[trainer generator](https://github.com/cheat-engine/cheat-engine/blob/master/Cheat%20Engine/frmExeTrainerGeneratorUnit.pas)
and [save/load implementation](https://github.com/cheat-engine/cheat-engine/blob/master/Cheat%20Engine/OpenSave.pas).
No PhasmoMenu code or binaries are bundled in PlayableGhost.

## Direct native loading

Loading PlayableGhost from outside the game directory into an already running
clean Steam session succeeded on 2026-10-19. The native log recorded supported
binary hashes, resolved bindings, installed steering, `PlayableGhost active`,
and creation of lobby controls. The process remained running. No detection
handler or game file was patched to obtain this result.

User testing confirmed that the controls were absent in a private lobby despite
native activation. Diagnostics showed active clones with no labels and
`interactable=false`. An absolute 8-unit gap placed clones far outside the native
board, where the source control is roughly 0.1 units wide. The generated native
metadata identifies `ServerManager.readyText` as a separate TextMeshProUGUI
reference, so searching below `readyToggle` cannot locate that label.

Visual controls, full round flow and two-client gameplay require separate
verification; a log message about creating controls is insufficient.

The portable launcher uses Unity's documented
[`-logFile` argument](https://docs.unity3d.com/2022.3/Documentation/Manual/PlayerCommandLineArguments.html)
to observe completion of menu loading when no explicit log path is provided.
Steam wrapping must be verified with the actual Steam Play button before being
reported as working.

## Development environment checks

The workspace has no Git repository or CI pipeline. Local validation does not
constitute a CI pass. Gameplay checks remain in FINDINGS entry 1.
