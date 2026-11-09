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
  the room. Its renderers carry the prefab's own outfit, never a player's. Each renderer is on a GameObject of its
  own under the body, by these names: `CC_Base_Body` (the head's mesh alone, with its eyelashes), `CC_Base_Hands`,
  `CC_Base_Eye`, `CC_Base_EyeOcclusion`, `CC_Base_TearLine`, `CC_Base_Teeth`, `CC_Base_Tongue`, `Hair` (with `Extra
  Hair Renderer 1` and `2`), `Boots`, `JacketSkinnedMesh`, `TrousersSkinnedMesh`, `ShirtSkinnedMesh` and
  `NecklaceRenderer`, besides the cosmetic slots (rings, bracelets, nose, mouth, eyebrows, headphones, a watch, an ID
  card holder). `pm::api::hide_renderers` keeps the ones you name drawing.
- **The shader.** Every material of that body uses URP's lit shader, `Universal Render Pipeline/Lit`, which the
  first scene's shared assets hold (sharedassets0). Its base colour (`ShaderProperty.Lit.BaseColor`) and face culling
  (`ShaderProperty.Lit.Cull`, a `CullMode`) are entries. The body's materials cull back faces; a material that sets
  `CullMode.Off` draws both sides. A material made from that shader with `Material.ctor(Shader)` starts from the
  shader's defaults, so it carries none of a body material's textures or keywords.
- **A layer for private colliders.** `Layer.IgnoreRaycast` is Unity's built-in layer of that name. The game's physics
  matrix lets it collide only with a layer the game leaves unnamed, so no player, prop or other body bumps into a
  collider placed on it. Cloth collides with the colliders it is given whatever their layer, which makes this the
  place for a cloth's private colliders. The matrix does not govern queries, though: Unity's default ray mask leaves
  the layer out, but a ray or overlap query of the game's whose mask includes it (all layers, say) can still hit such
  a collider, and the game's own masks have not been audited.
