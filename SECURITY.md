# Security policy

## Supported versions

Only the latest release gets security fixes. The loader updates itself, so a fix ships as a new
release, and older releases are not patched.

## Reporting a vulnerability

Report it privately through GitHub: open the repository's **Security** tab and choose **Report a
vulnerability**. Please do not open a public issue, pull request or discussion about it until a fix
is released.

Include what you can of:

- the loader version (shown in the launcher) and the game build;
- what an attacker can do, and what they need first (for example, being in the same lobby);
- the steps to reproduce it, or a proof of concept;
- any log lines from the PhasmoMods log.

This is a one-person project, so replies are best effort and there is no fixed timeline. You will
be credited in the release notes unless you ask not to be.

## Scope

In scope:

- `PhasmoMods Loader.exe`, `ModHost.dll` and the updater;
- the bundled mods (Playable Ghost, Custom Ghosts, Spectral Flame, Ghost Tells);
- the SDK and the host contract it exposes to mods.

For example: another player in a lobby running code on your computer or crashing your game through
the loader's network messages, or an update being replaced in transit.

Out of scope:

- **Community mods.** Report problems in a community mod to its author.
- **A mod running code on your computer.** A mod is a program and can do anything a program can. That
  is by design, and the consent notice says so: only install mods you trust.
- **The game itself and its anti-cheat**, and any account action taken for using mods.
- **The Windows SmartScreen warning.** Releases are not code-signed.
