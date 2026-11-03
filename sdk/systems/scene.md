# Scene and asset facts

Some of what the mods depend on is not in the game's code: where a GameObject sits in a scene or a
prefab, which index a layer has, which parameters an Animator's controller has, which properties a
shader has. These facts are map entries of four kinds (`sdk/api/entry.h`): `Scene`, `Layer`,
`AnimatorParam` and `ShaderProperty`. The entries are in `sdk/map/Scene.h` (the flame shaders' properties
in `sdk/map/UnityRendering.h`), and their `///` notes hold what is known of each. This page says which of the game's
files hold such facts, how they are read, and how they are checked. It was written against Steam
build 25811160 and copies no game data: run the check to see it.

## How this was read

How these facts were read from the game, what the loader does with them and how the entries are found are in [the maintainers' page](../internals/scene.md).
