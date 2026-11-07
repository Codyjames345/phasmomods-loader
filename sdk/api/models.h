#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "Transform.h"
#include "UnityRendering.h"
#include "held.h"

// A mod's own figures: a local copy of the investigator's body, a mod's animator controller and states played on it,
// and Unity Cloth on a mesh the mod builds. Everything here is local to the client that calls it: nothing is
// networked, so a mod that wants every player to see the same figure creates it on every client and tells them what
// to play through its own channel (sdk/README.md, "What the loader does for you"). The walk-through is
// sdk/guides/custom-animation.md; what the engine test showed of each part is in sdk/internals/engine-experiments.md.
//
// The names follow Unity's own (Animator.Play, Cloth, Object.Instantiate), as mods for other Unity loaders use them.
// Main thread, every function. Objects returned are managed: keep one past the frame only in a pm::api::Held, and
// check pm::api::alive before using it on a later frame, since a scene load destroys every figure.
namespace pm::api {

/**
 * @brief A local, script-free copy of the investigator's body: the humanoid `Rigged PC Model Base` inside the game's
 *        PCPlayer prefab (PlayerCosmetics.playerAnimator), the same model on every client whoever is in the room.
 * @param name The copy's GameObject name, for the game's hierarchy and logs.
 * @param why Set to the reason when it fails, if not null.
 * @return The copy's GameObject, active, at the world origin, with no parent; null on failure. It has the body's
 *         renderers and skeleton and its Animator, still on the game's own controller (whose state machine behaviours
 *         live on the controller asset, so they run until use_controller replaces it). Every MonoBehaviour,
 *         AudioSource, Collider and Rigidbody is taken off while the copy is inactive, so none of the game's scripts
 *         ever wakes on it, and the rigs and targets that draw nothing are removed. When a script cannot be taken
 *         off, nothing is kept and it fails. Destroy the copy with Object.Destroy; a scene load destroys it too.
 * @pre Main thread, in any scene: it needs no player.
 * @note This client only: a local object, never a networked one. The prefab's own outfit, never a player's.
 * @since Contract 2.3 (a helper in the mod's own code; the host contract itself does not change)
 */
void* copy_investigator_body(const char* name, std::string* why = nullptr);

/**
 * @brief Puts a controller on an Animator to play a mod's own states: replaces its controller, turns root motion off
 *        (the mod moves the model; clips move only its pose) and keeps it animating while unseen.
 * @param animator An Animator, such as Component.GetComponent(copy, Animator) of copy_investigator_body's copy.
 * @param controller A controller load_controller (bundles.h) gave.
 * @return False, changing nothing, for a null or destroyed argument.
 * @since Contract 2.3 (a helper in the mod's own code)
 */
bool use_controller(void* animator, void* controller);

/**
 * @brief Plays a state of the Animator's controller now, from its start, on the first layer, as Animator.Play does.
 * @param state The state's name as the controller has it, case sensitive.
 * @return False, playing nothing, when the Animator is null, destroyed, not initialised (inactive, or no controller),
 *         or its controller has no such state on the first layer.
 * @since Contract 2.3 (a helper in the mod's own code)
 */
bool play_state(void* animator, const char* state);

/**
 * @brief Whether a controller has every one of `states` on its first layer: a check for a mod's start, so a bundle
 *        built without a state fails the start instead of a figure standing still in a contract.
 * @param controller A controller load_controller gave.
 * @param missing Set to the first missing name, or the reason the controller could not be checked, if not null.
 * @return True when every state is there. It puts the controller on a short-lived Animator of its own, which it
 *         destroys before it returns; nothing else changes.
 * @since Contract 2.3 (a helper in the mod's own code)
 */
bool controller_has_states(void* controller, std::span<const char* const> states, std::string* missing = nullptr);

/**
 * @brief A material for a mod's own mesh: URP's Lit shader of `like`, with the shader's defaults, the colour
 *        `colour` and both sides of each triangle drawn. Not a copy of `like`: none of its textures or keywords.
 * @param like A material on URP's Lit shader, as every material of the investigator's body is (lit_material_of).
 * @param why Set to the reason when it fails, if not null.
 * @return The new material (keep it in a Held, and destroy it with Object.Destroy when done), or null when `like` is
 *         null or its shader has not the Lit shader's base colour and cull properties (ShaderProperty.Lit.*).
 * @since Contract 2.3 (a helper in the mod's own code)
 */
void* two_sided_material(void* like, phasmo::Color colour, std::string* why = nullptr);

/**
 * @brief The shared material of the first renderer under a GameObject, inactive ones included: on
 *        copy_investigator_body's copy, a material on the game's Lit shader for two_sided_material.
 * @return The material, or null when nothing under it renders.
 * @since Contract 2.3 (a helper in the mod's own code)
 */
void* lit_material_of(void* gameObject);

/**
 * @brief A mesh for add_cloth, in the space of the GameObject that will hold it. Every span but `triangles` has one
 *        element per vertex.
 */
struct ClothMesh {
  std::span<const phasmo::Vector3> vertices;
  std::span<const phasmo::Vector3> normals;
  std::span<const phasmo::Vector2> uv;
  std::span<const int32_t> triangles;  // three vertex indices each, clockwise as seen from the front
  /// Each vertex's constraint: x how far it may move from where its GameObject carries it (0 pins it), y the depth of
  /// the sphere it may not enter. A large value means no limit.
  std::span<const phasmo::Vector2> coefficients;
};

/** How the cloth behaves. Unity's own ranges: stiffnesses and damping 0 to 1. */
struct ClothSettings {
  bool useGravity = true;
  float bendingStiffness = 0.1f;
  float stretchingStiffness = 1.0f;
  float damping = 0.2f;
};

/** A sphere the cloth rests on and cannot pass through, following a bone. */
struct ClothSphere {
  void* bone;               // a Transform (Animator.GetBoneTransform); a null one is skipped
  float radius;             // metres
  phasmo::Vector3 offset;   // from the bone, in world space when add_cloth is called
};

/** What add_cloth made, for destroy_cloth. */
struct ClothParts {
  Held mesh;
  Held cloth;
  std::vector<Held> colliders;  // the spheres' GameObjects
};

/**
 * @brief Makes `gameObject` draw `mesh` with `material` and simulate it as Unity Cloth: a new Mesh, a
 *        SkinnedMeshRenderer that carries every vertex with the GameObject itself, then a Cloth with `settings`, the
 *        mesh's coefficients and a sphere collider per `spheres` entry.
 * @param gameObject An active GameObject with no renderer, usually parented to a bone so the pinned vertices follow it.
 * @param spheres Each becomes a GameObject on the Ignore Raycast layer (Layer.IgnoreRaycast), parented to its bone, so
 *        it touches nothing of the game's.
 * @param out What was made; on failure it is left empty and nothing made stays.
 * @param why Set to the reason when it fails, if not null.
 * @return False when the Cloth entries did not resolve (they are optional: sdk/map/UnityCloth.h), the layer did not
 *         resolve, the mesh's spans disagree in length or index out of range, or the engine refused a part.
 * @note The engine test showed a flat square resting on body spheres drapes badly; give the mesh the shape it should
 *       settle in, as the Squatter's sheet does (mods/custom_ghosts/sheet_mesh.h).
 * @since Contract 2.3 (a helper in the mod's own code)
 */
bool add_cloth(void* gameObject, const ClothMesh& mesh, void* material, const ClothSettings& settings,
               std::span<const ClothSphere> spheres, ClothParts& out, std::string* why = nullptr);

/**
 * @brief Destroys what add_cloth made that its GameObject does not take with it: the mesh and the sphere colliders.
 *        The renderer and the Cloth go with the GameObject. Safe on parts a scene load already destroyed.
 * @since Contract 2.3 (a helper in the mod's own code)
 */
void destroy_cloth(ClothParts& parts);

}  // namespace pm::api
