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

## The investigator's body, its shader and a layer for private colliders

Facts a mod that shows its own figure needs (`sdk/api/models.h`, `sdk/guides/custom-animation.md`), read from the asset
files:

- **The body.** The `PCPlayer` prefab in Resources holds the investigator's body, `Rigged PC Model Base`, which
  `PlayerCosmetics.playerAnimator` names. It is a humanoid with Unity's avatar, the same on every client, whoever is in
  the room. Its renderers carry the prefab's own outfit, never a player's.
- **The shader.** Every material of that body uses URP's lit shader, `Universal Render Pipeline/Lit`, which the
  first scene's shared assets hold (sharedassets0). Its base colour (`ShaderProperty.Lit.BaseColor`) and face culling
  (`ShaderProperty.Lit.Cull`, a `CullMode`) are entries. The body's materials cull back faces; a material that sets
  `CullMode.Off` draws both sides. A material made from that shader with `Material.ctor(Shader)` starts from the
  shader's defaults, so it carries none of a body material's textures or keywords.
- **A layer the game ignores.** `Layer.IgnoreRaycast` is Unity's built-in layer of that name. The game's physics matrix
  lets it collide only with a layer the game leaves unnamed, so a collider placed on it touches no player, prop or
  ray of the game. Cloth collides with the colliders it is given whatever their layer, which makes this the place for
  a cloth's private colliders.
