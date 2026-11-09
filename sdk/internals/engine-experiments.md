# Engine experiments: a body copy, clips from a bundle, and Cloth

For whoever extends the SDK. A throwaway mod tested four engine features in a live private room before any shipped mod relied on
them: a local copy of the PC player's body, animation clips loaded from an AssetBundle, a bundle built for the game's Unity version,
and Unity Cloth added at runtime. This page records what that test showed and what it left open. How to read the code
behind it: [reverse engineering](reverse-engineering.md). Names are map keys and class names, never addresses.

What counts as shown here is what one in-game run showed: the model appeared, the three clips played, nothing crashed, and the log
reported the Cloth counts below. Anything the run did not show is marked open.

## A local copy of the PC player's body

The body is reached without any player being present, so every client gets the same model:

1. `Resources.Load` the `PCPlayer` prefab. The game spawns each investigator's body from it
   (`LocalPCPlayer.CreateNetworkedPlayer`; see [Journal](../systems/journal.md)).
2. Find `PlayerCosmetics` in its children.
3. Read `PlayerCosmetics.playerAnimator`. That Animator's GameObject, `Rigged PC Model Base`, is the body.
4. `Object.Instantiate` it under a holder that is inactive, so no component wakes while it is copied.
5. Remove every MonoBehaviour, AudioSource, Collider and Rigidbody from the copy, including the prefab's PhotonView, which was never
   registered. Check that no MonoBehaviour is left, and keep the copy inactive if one is.
6. Unparent the copy and activate it.

Do not copy a live player's body instead. The game sets shadow-only casting and the model layer on it (`Player.SetupPlayer`,
`Player.SetPlayerModelLayer`) and it carries that player's outfit, so clients would differ. The `Male_N_Body` and `Female_N_Body`
prefabs are templates for the cosmetic code, with most material slots empty.

- **The game's controller came along.** The Animator's controller, `PCPlayer (overhaul)`, holds StateMachineBehaviours on the
  controller asset, not on the copy's GameObjects, so the strip in step 5 cannot reach them. In the test they ran on the copy from
  activation until the mod replaced the controller. A code review walked them on the copy's default parameters and found nothing
  they reach that matters; the in-game run did not test this separately. The SDK's copy (`copy_investigator_body`,
  `sdk/api/models.h`) takes the controller off while the copy is still inactive, so none of them is made for it.
- **Humanoid rig.** The avatar maps 23 of 25 human bones; the two eyes are unmapped. Humanoid clips from outside the game played
  on it in the run.
- **Look.** The copy appeared correctly in the run.
- `Player Body Offset` is on `VRPlayer (OpenXR)`, not on the PC player.

## Loading clips from an AssetBundle

The managed `AssetBundle` class is stripped from the game, but the engine calls behind it survive in UnityPlayer, and they worked in
the run: the host loaded a bundle from a file, listed its asset names, loaded a controller from it and set it on the copy's Animator,
and the clips played. Map the calls as icall entries ([reverse engineering](reverse-engineering.md), "Stripped methods and engine
calls"). The bundle object the engine returns is only passed back into further engine calls.

- **Asset names inside a bundle are lower-case paths.** A controller built from `SquatterSpike.controller` is listed as
  `assets/squatterspike.controller`. Ask for that path, or for the plain asset name.
- **Where a mod's data file goes.** Do not read it from beside the running DLL. The launcher copies only the DLL, the manifest and
  the string table to its own folder and runs that copy, so a file next to the DLL is not found. The first version of the test
  looked there for this reason, and the file was moved to a folder under the user's local application data.
- The bundle load ran last in the test, so a native crash there would still have left the earlier results in the log. It did not
  crash.

## Building the bundle

The test bundle was built outside the repository:

- Unity **2022.3.40f1**, the game's version (the bundle header and the serialized file both read it), target
  **StandaloneWindows64**, built with `BuildPipeline.BuildAssetBundles` from an Editor script in batch mode.
- Import the reference model (a T-pose humanoid) as Humanoid with *Create From This Model*, and abort if its avatar is not valid.
  Import each clip as Humanoid with *Copy From Other Avatar*, pointing at the reference's avatar. All three clips took it and came in
  with 130 humanoid curves each.
- A **controller-only bundle** is enough: assign only the controller to the bundle. The clips ride inside it because the controller
  references them, and no mesh, avatar, material or script is included. The result was 171 KB with no dependencies.

**Root motion lesson.** The test baked Root Transform Position XZ into the pose on every clip. A run clip that is not in place then
carries the body about 2.8 m forward each cycle and snaps back when the clip loops, which showed in game as the body jumping to
positions instead of walking. The bake was the cause, not the engine or the walking code. Leave Root Transform Position XZ unbaked
with root motion off on the Animator, or use in-place clips. Neither fix has been run in game yet.

## Unity Cloth added at runtime

`Cloth` was added with `AddComponent` to a SkinnedMeshRenderer whose mesh the mod built (a flat square sheet weighted to one bone).
In the run:

- The Cloth simulated: it reported all 1089 vertices, 21 of them pinned as set, and a sheet corner moved with the body and settled.
- **`Cloth.set_coefficients` accepted an array of a two-float struct.** The managed coefficient struct is stripped from the game; an
  array of a struct with the same layout (two floats) worked, and the game did not crash. `get_coefficients` was not called, since it
  would have to build the stripped type.
- The game's own scenes carry many Cloth components, and `ClothAttachCharacters` sets capsule colliders, so Cloth is live in this
  build.
- A map that adds Cloth entries needs the Cloth module's assembly to resolve; a missing assembly fails the whole map, so check it
  first. This is a review finding, not something the run showed.

What the run showed went wrong, and what the Squatter's figure does instead (not yet run in game):

- **A plain sheet coloured from one texel of a borrowed texture rendered black.** The colour was taken from one point of the shirt's
  texture. The figure makes its own material on the body's lit shader with its colour set directly and both sides drawn
  (`two_sided_material`, `sdk/api/models.h`).
- **A flat square did not drape well** over the six body spheres it rested on. The figure's sheet is made already hanging over a
  head and shoulders, its head sphere the same size as the cap it is made over (`mods/custom_ghosts/sheet_mesh.h`).

The helpers that do each step of this page for any mod are in `sdk/api/bundles.h` and `sdk/api/models.h`, with a guide for
authors: [custom animation](../guides/custom-animation.md).

## Method note

The test logged one line per engine call, with the call named before it ran, so a native crash would have shown which call caused
it. Do the same for any new engine call that cannot be checked offline.
