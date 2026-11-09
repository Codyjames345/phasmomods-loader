# Custom animation: a figure of your own

A mod can show a figure the game never had: the investigator's body playing your own animation clips, with cloth on
it if you like. The bundled Custom Ghosts does this for the Squatter, a person in a bedsheet
(`mods/custom_ghosts/squatter_figure.cpp`). This page shows how, with the SDK's helpers in `sdk/api/bundles.h` and
`sdk/api/models.h`, and how to build the AssetBundle that carries your clips. No reverse engineering is needed.

What the engine test proved of each part, and what it left open, is in the maintainers' page
`sdk/internals/engine-experiments.md`.

## What you get

| Helper | What it does |
| --- | --- |
| `load_bundle(file)` | Loads an AssetBundle from a file, as Unity's `AssetBundle.LoadFromFile` does |
| `load_controller(bundle, name)` | An animator controller from the bundle by its name |
| `controller_has_states(controller, states)` | Whether the controller has every state you will play, for your start check |
| `copy_investigator_body(name)` | A local copy of the investigator's body with none of the game's scripts on it |
| `use_controller(animator, controller)` | Puts your controller on the copy, with root motion off |
| `play_state(animator, state)` | Plays one of your states by its name |
| `two_sided_material(like, colour)` | A plain material on the body's own lit shader, drawn on both sides |
| `add_cloth(gameObject, mesh, material, settings, spheres, parts)` | Unity Cloth on a mesh you build |

Everything they make is local to the client that calls them. Nothing is networked, so a figure every player sees is
one each client builds for itself. Tell the other clients what to play through your mod's own channel (`send` and
`set_receiver`, in [the SDK's overview](../README.md#what-the-loader-does-for-you)), never by creating a networked
object.

## Building the bundle

The game runs Unity **2022.3.40f1**. A bundle built with any other version, or for another platform, is refused by the
engine, which writes its reason to the game's own `Player.log`.

1. Install Unity 2022.3.40f1 with Windows build support and make an empty 3D project.
2. Import your clips (below) and make an Animator Controller. Give it one state per clip on its first layer (Base
   Layer). The state names are what your code plays, case sensitive. Transitions and parameters are optional: the
   helpers play states directly.
3. Select the controller and set its AssetBundle name in the Inspector, at the bottom (for example `mymod.bundle`).
   The controller alone is enough: the clips go into the bundle with it, because it refers to them. No mesh, avatar,
   material or script is needed.
4. Build with an Editor script, placed in `Assets/Editor/`:

   ```csharp
   using UnityEditor;

   public static class BuildBundles {
     [MenuItem("Assets/Build Bundles")]
     public static void Build() {
       System.IO.Directory.CreateDirectory("Bundles");
       BuildPipeline.BuildAssetBundles("Bundles", BuildAssetBundleOptions.None, BuildTarget.StandaloneWindows64);
     }
   }
   ```

5. Inside a bundle, an asset's name is its path in the project, lower case: a controller saved as
   `Assets/MyMod.controller` loads as `assets/mymod.controller`. `bundle_asset_names` lists them, which helps when a
   load finds nothing.

## Mixamo clips on the investigator

The investigator's body is a Unity humanoid, so humanoid clips play on it whatever skeleton they were made for. Mixamo
clips work this way:

1. From Mixamo, download a character in its T-pose as FBX for Unity, with skin. This is only your reference skeleton.
2. Download each animation as FBX for Unity. Pick **In Place** where Mixamo offers it, for anything that travels (a
   run or a roll): your mod moves the figure, and the clip should only move its pose.
3. In Unity, select the reference FBX. Under Rig, set Animation Type to **Humanoid** and Avatar Definition to **Create
   From This Model**, then Apply. Unity must report the avatar valid.
4. Select each clip FBX. Under Rig, set Animation Type to **Humanoid**, Avatar Definition to **Copy From Other
   Avatar**, and Source to the reference's avatar, then Apply.
5. Under Animation, for each clip: leave **Root Transform Position (XZ)** with Bake Into Pose **off**. Turn Loop Time on
   for a clip that repeats, such as a run.

Why the root matters: the engine test baked Root Transform Position (XZ) into the pose. A run that is not in place then
carried the body about three metres forward each cycle and snapped it back when the clip looped. `use_controller` turns
root motion off on the Animator, so with the position left unbaked, or an In Place clip, the body stays where your
mod puts it.

## The licence

Mixamo's FAQ, as Adobe's forum reposted it in 2022, says its characters and animations may be used in a game
royalty-free, but **the raw files may not be redistributed**. Adobe's live FAQ could not be read when this was written,
and nothing official covers mods, so check the current terms yourself. Never commit or ship the FBX files you download.
Ship only the built bundle, and keep `*.bundle` out of a public repository (this one ignores it). Crediting Mixamo is
optional and polite.

## Where the bundle goes

Put the bundle in your mod's `assets` folder, which the launcher copies beside your DLL for each game
([a mod's own files](mod-files.md)), and list it in the release's payload like any other asset file:

```
mods/<folder>/
  MyMod.dll
  manifest.json
  assets/
    mymod.bundle
```

## The code

Load the bundle and check its states in `pmmod_start`, before you register or hook anything. If anything is missing,
fail the start with a logged reason: a mod with an error is off, with no partial workaround.

```cpp
#include "assets.h"
#include "bundles.h"
#include "models.h"

constexpr const char* kStates[] = {"Wave", "Bow"};
pm::api::Held g_bundle;

bool start(const PmHostApi* api) {
  if (!pm::mod::bind(api, &kInfo)) return false;
  std::string why;
  g_bundle = pm::api::load_bundle(pm::api::assets_folder() / "mymod.bundle", &why);
  void* controller = g_bundle ? pm::api::load_controller(g_bundle, "assets/mymod.controller") : nullptr;
  if (!controller || !pm::api::controller_has_states(controller, kStates, &why)) {
    PM_LOG("MyMod inactive: its bundle cannot be used: %s", why.c_str());
    pm::api::unload_bundle(g_bundle, false);
    return false;
  }
  return true;
}
```

Build the figure when you need it, on every client, and play a state:

```cpp
void* copy = pm::api::copy_investigator_body("My figure", &why);
void* animator = phasmo::Component::GetComponent(phasmo::GameObject::get_transform(copy),
                                                 phasmo::Animator::type.type_object());
pm::api::use_controller(animator, pm::api::load_controller(g_bundle, "assets/mymod.controller"));
phasmo::Vector3 at{x, y, z};  // its feet
phasmo::Transform::set_position(phasmo::GameObject::get_transform(copy), &at);
pm::api::play_state(animator, "Wave");
```

Keep the copy and its Animator in `pm::api::Held`, and check `pm::api::alive` on a later frame: a scene load destroys
the copy. In `pmmod_stop`, destroy the copy with `Object.Destroy` and unload the bundle.

## Cloth

`add_cloth` turns a mesh you build into Unity Cloth on a GameObject, usually one parented to a bone so the vertices you
pin follow it. You give it, in that GameObject's space, each vertex's position, normal, uv and constraint (0 pins the
vertex; a large number leaves it free), the triangles, and the spheres the cloth rests on, each following a bone
(`Animator.GetBoneTransform`). Those spheres sit on the Ignore Raycast layer, which no body of the game's collides
with; a ray or overlap query of the game's whose mask includes that layer can still find one.

- **Make the mesh in the shape it should settle in.** A flat square dropped onto a body drapes badly. The Squatter's
  sheet is made already hanging over a head and shoulders (`mods/custom_ghosts/sheet_mesh.h`).
- **Use a material of your own.** `two_sided_material(lit_material_of(copy), colour)` makes one on the body's lit
  shader with your colour, drawn on both sides, which a sheet seen from inside needs. Taking a colour from a borrowed
  texture rendered black in the engine test.
- **Cloth is optional in the map.** Its Unity module could leave a later game build, so call
  `pm::api::cloth_available(&why)` and `pm::api::material_available(&why)` at your start and fail it, with `why` in
  the log, if either is false.
- In `pmmod_stop`, or when you remove the figure, call `destroy_cloth` for what the GameObject does not take with it,
  and destroy the material.

## Blending, pacing and root motion

- **Blend between states** with `pm::api::fade_to_state(animator, "State", blend)` instead of `play_state`: the
  poses mix over `blend`, a fraction of the playing state's length (Animator.CrossFade's own unit; this build strips
  the fixed-time CrossFadeInFixedTime). Unity's own default transition is a quarter of a second.
- **Pose the model now** with `pm::api::pose_now(animator)` after `play_state`, so its bones stand as the state's first
  frame in the same frame: do this before you measure the body or bind anything to its bones.
- **Keep feet in step with the ground.** Build a 1D blend tree in your controller on a float parameter (the Squatter
  uses `Speed`), each clip at the speed it travels on its own (`AnimationClip.averageSpeed` in the editor), and give
  the state a speed multiplier parameter (`PlayRate`). Each frame set both with `Animator.SetFloatID`: the ground
  speed, and the ground speed over the speed the clip travels. Download walks and runs with Mixamo's In Place
  unticked, so their speed can be read.
- **Let a one-off clip move the character** (a roll, a trip) by reading `Animator.get_deltaPosition` each frame, how
  far the clip moved the root, and moving your object by it; the Animator works it out even with Apply Root Motion off.
  A NavMeshAgent can be moved with `NavMeshAgent.Warp` (its `Move` is stripped).

## Inverse kinematics (Final IK)

The game ships RootMotion's Final IK, mapped in `sdk/map/FinalIK.h`. Its components bend bones after the Animator,
in LateUpdate, so a hand lands on a point or a head turns to look at one:

- Add `phasmo::LimbIK` to the model's GameObject, take its solver (`LimbIK::solver`), and set the arm with
  `IKSolverTrigonometric::SetChain(solver, upperArm, forearm, hand, root)` and `IKSolverLimb::goal` (an
  `AvatarIKGoal`). Set `IKSolverTrigonometric::IKRotationWeight` to 0 unless you also set the hand's rotation.
- Add `phasmo::LookAtIK`, and set its solver with `IKSolverLookAt::SetChain(solver, spine, head, eyes, root)` (two
  `Transform[]`, either may be empty) and its `headWeight`, `bodyWeight` and `clampWeight`.
- Each frame set the solver's `IKSolver::IKPosition` (world space) and `IKSolver::IKPositionWeight` (0 to 1), blending
  the weight in and out over a moment rather than jumping it.
