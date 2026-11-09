#pragma once

#include <cstdint>
#include <optional>
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
 *         renderers and skeleton and its Animator, with no controller: the game's own is taken off while the copy is
 *         still inactive, since its state machine behaviours live on the controller asset, where no strip reaches
 *         them. Give it one with use_controller. Every MonoBehaviour, AudioSource, AudioListener, Camera, Light,
 *         Collider and Rigidbody is taken off while the copy is inactive too, so none of the game's scripts ever
 *         wakes on it, and the rigs and targets that draw nothing are removed. When any of those cannot be taken off,
 *         nothing is kept and it fails. Destroy the copy with Object.Destroy; a scene load destroys it too.
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
 * @brief Blends into a state of the Animator's controller from the one playing, from the new state's start, on the
 *        first layer, as Animator.CrossFade does: the two poses mix over the blend, so the model never jumps.
 * @param state The state's name as the controller has it, case sensitive.
 * @param blend The blend's length as a fraction of the playing state's length (Animator.CrossFade's own unit), 0 to 1:
 *        a blend of `s` seconds out of a state `length` seconds long is s / length.
 * @return False, playing nothing, as play_state.
 * @since Contract 2.3 (a helper in the mod's own code)
 */
bool fade_to_state(void* animator, const char* state, float blend);

/**
 * @brief Poses the Animator's model in the state it plays now, without moving the state on (Animator.Update with no
 *        time): after play_state, the bones stand as the state's first frame has them in this same frame.
 * @return False, doing nothing, when the Animator is null, destroyed or not initialised.
 * @since Contract 2.3 (a helper in the mod's own code)
 */
bool pose_now(void* animator);

/**
 * @brief Stops drawing every Renderer under a GameObject, inactive ones included, keeping its bones, Animator and the
 *        rest: what games do with a body a costume covers, so no part of it can show through the clothing.
 *        Renderers added afterwards (a sheet's own) still draw.
 * @param keep GameObject names whose renderers keep drawing, at any depth: the parts the costume leaves showing (the
 *        investigator body's names are in sdk/systems/scene.md). Empty to hide them all.
 * @return How many renderers it switched off; 0 for null or a destroyed GameObject.
 * @since Contract 2.3 (a helper in the mod's own code); `keep` since 2.3 as well
 */
int hide_renderers(void* gameObject, std::span<const char* const> keep = {});

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
 *         null, material_available() is false, or its shader has not the Lit shader's base colour and cull properties.
 * @since Contract 2.3 (a helper in the mod's own code)
 */
void* two_sided_material(void* like, phasmo::Color colour, std::string* why = nullptr);

/**
 * @brief Sets a Lit material's smoothness (ShaderProperty.Lit.Smoothness): 0 is matte, as the game's own untextured
 *        fabric, and 1 a mirror; the shader's default is 0.5. Main thread.
 * @param material A material on the game's Lit shader, two_sided_material's for one.
 * @param smoothness 0 to 1; a value outside is brought within it.
 * @return False, changing nothing, for no material, or a shader without the property (or a build without the entry).
 * @since Contract 2.3 (a helper in the mod's own code)
 */
bool set_smoothness(void* material, float smoothness);

/**
 * @brief Whether this game build has the optional entries two_sided_material needs: the Lit shader's base colour and
 *        culling properties (ShaderProperty.Lit.BaseColor and ShaderProperty.Lit.Cull). Call it in pmmod_start and
 *        fail the start when it is false; the material's other entries are required, so a mod using it does not start
 *        without them anyway.
 * @param why Set to "<key> is not available in this game build" for the first missing entry, if not null.
 * @return True when every one resolved. Never logs.
 * @pre Main thread, once the mod is bound. The host resolves these before any mod starts.
 * @since Contract 2.3 (a helper in the mod's own code)
 */
bool material_available(std::string* why = nullptr);

/**
 * @brief Whether this game build has the optional entries add_cloth needs: the Cloth type, its capsule colliders,
 *        coefficients, gravity, stiffnesses and damping (sdk/map/UnityCloth.h), and the Ignore Raycast layer its
 *        spheres go on. Call it in pmmod_start and fail the start when it is false; add_cloth's other entries are
 *        required, so a mod using it does not start without them anyway.
 * @param why Set to "<key> is not available in this game build" for the first missing entry, if not null.
 * @return True when every one resolved. Never logs.
 * @pre Main thread, once the mod is bound. The host resolves these before any mod starts.
 * @since Contract 2.3 (a helper in the mod's own code)
 */
bool cloth_available(std::string* why = nullptr);

/**
 * @brief The shared material of the first renderer under a GameObject, inactive ones included: on
 *        copy_investigator_body's copy, a material on the game's Lit shader for two_sided_material.
 * @return The material, or null when nothing under it renders.
 * @since Contract 2.3 (a helper in the mod's own code)
 */
void* lit_material_of(void* gameObject);

/**
 * Which of ClothMesh::bones carry a vertex, and how much: up to four, as Unity's BoneWeight holds them, heaviest first,
 * their weights adding up to 1. The game's quality settings may skin with fewer; then the heaviest count.
 */
struct ClothWeight {
  int32_t bone[4] = {0, 0, 0, 0};             ///< Indices into ClothMesh::bones, heaviest first.
  float weight[4] = {1.0f, 0.0f, 0.0f, 0.0f};  ///< Each bone's share, 0 to 1, adding up to 1.
};

/**
 * @brief A mesh for add_cloth, in the space of the GameObject that will hold it. Every span but `triangles` has one
 *        element per vertex.
 */
struct ClothMesh {
  std::span<const phasmo::Vector3> vertices;  ///< Each vertex's position, metres.
  std::span<const phasmo::Vector3> normals;   ///< Each vertex's normal, of length 1.
  std::span<const phasmo::Vector2> uv;        ///< Each vertex's texture coordinates.
  std::span<const int32_t> triangles;         ///< Three vertex indices each, clockwise as seen from the front.
  /// Each vertex's constraint: x how far it may move from where the skinning carries it (0 pins it), y the depth of
  /// the sphere it may not enter. A large value means no limit.
  std::span<const phasmo::Vector2> coefficients;
  /// The Transforms that carry the mesh, as worn cloth in games is skinned to a body: empty, and every vertex follows
  /// the GameObject itself. Each bone's bind pose is where it stands when add_cloth is called.
  std::span<void* const> bones;
  /// One per vertex when `bones` is not empty, naming bones by their index in it.
  std::span<const ClothWeight> weights;
};

/**
 * Unity's translate-rotate-scale matrix (Matrix4x4.TRS) for `position`, `rotation` and `scale`, by column as Matrix4x4
 * holds it.
 */
inline phasmo::Matrix4x4 trs_matrix(phasmo::Vector3 position, phasmo::Quaternion rotation, phasmo::Vector3 scale) {
  const float x = rotation.x, y = rotation.y, z = rotation.z, w = rotation.w;
  const float r[3][3] = {{1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)},
                         {2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)},
                         {2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)}};
  const float s[3] = {scale.x, scale.y, scale.z};
  phasmo::Matrix4x4 m{};
  for (int col = 0; col < 3; ++col)
    for (int row = 0; row < 3; ++row) m.m[col * 4 + row] = r[row][col] * s[col];
  m.m[12] = position.x;
  m.m[13] = position.y;
  m.m[14] = position.z;
  m.m[15] = 1;
  return m;
}

/** The product `a` times `b` of two matrices held by column. */
inline phasmo::Matrix4x4 multiply(const phasmo::Matrix4x4& a, const phasmo::Matrix4x4& b) {
  phasmo::Matrix4x4 out{};
  for (int col = 0; col < 4; ++col)
    for (int row = 0; row < 4; ++row) {
      float sum = 0;
      for (int k = 0; k < 4; ++k) sum += a.m[k * 4 + row] * b.m[col * 4 + k];
      out.m[col * 4 + row] = sum;
    }
  return out;
}

/**
 * A bone's bind pose for a mesh: the inverse of the bone's matrix `bone` (a trs_matrix with no shear) times the mesh's
 * own matrix `mesh`, so the bone, where it stands, carries the mesh where it is.
 */
inline phasmo::Matrix4x4 bind_pose(const phasmo::Matrix4x4& bone, const phasmo::Matrix4x4& mesh) {
  phasmo::Matrix4x4 inverse{};
  for (int col = 0; col < 3; ++col) {  // the rotation's transpose, each row divided by its column's squared length
    float lengthSq = 0;
    for (int row = 0; row < 3; ++row) lengthSq += bone.m[col * 4 + row] * bone.m[col * 4 + row];
    for (int row = 0; row < 3; ++row) inverse.m[row * 4 + col] = lengthSq > 0 ? bone.m[col * 4 + row] / lengthSq : 0;
  }
  for (int row = 0; row < 3; ++row) {
    float t = 0;
    for (int k = 0; k < 3; ++k) t += inverse.m[k * 4 + row] * bone.m[12 + k];
    inverse.m[12 + row] = -t;
  }
  inverse.m[15] = 1;
  return multiply(inverse, mesh);
}

/** How the cloth behaves. Unity's own ranges: stiffnesses and damping 0 to 1. */
struct ClothSettings {
  bool useGravity = true;             ///< Whether gravity pulls the cloth.
  float bendingStiffness = 0.1f;      ///< How stiffly it resists bending.
  float stretchingStiffness = 1.0f;   ///< How stiffly it resists stretching.
  float damping = 0.2f;               ///< How much its motion is damped.
  /// How much it answers its object's motion through the world, 0 to 1 (Cloth.set_worldVelocityScale): lower hangs
  /// heavier. None keeps Unity's own (0.5).
  std::optional<float> worldVelocityScale;
  /// How much it answers its object's acceleration, 0 to 1 (Cloth.set_worldAccelerationScale): lower whips less. None
  /// keeps Unity's own (1).
  std::optional<float> worldAccelerationScale;
  /// Whether its colliders press it in smooth bulges rather than at its points (Cloth.set_useVirtualParticles). None
  /// keeps Unity's own.
  std::optional<bool> virtualParticles;
};

/** A sphere the cloth rests on and cannot pass through, following a bone. */
struct ClothSphere {
  void* bone;              ///< A Transform (Animator.GetBoneTransform); a null one fails add_cloth.
  float radius;            ///< Metres.
  phasmo::Vector3 offset;  ///< From the bone, in world space when add_cloth is called.
};

/** What add_cloth made, for destroy_cloth. */
struct ClothParts {
  Held mesh;                    ///< The Mesh it built.
  Held cloth;                   ///< The Cloth component.
  std::vector<Held> colliders;  ///< The spheres' GameObjects.
};

/**
 * @brief Makes `gameObject` draw `mesh` with `material` and simulate it as Unity Cloth: a new Mesh, a
 *        SkinnedMeshRenderer that carries every vertex with the GameObject itself, then a Cloth with `settings`, the
 *        mesh's coefficients and a sphere collider per `spheres` entry.
 * @param gameObject An active GameObject with no renderer, usually parented to a bone so the pinned vertices follow it.
 * @param spheres Each becomes a GameObject on the Ignore Raycast layer (Layer.IgnoreRaycast), parented to its bone. The
 *        game's physics matrix lets that layer collide with nothing the game names, so no body bumps into a sphere;
 *        a ray or overlap query of the game's whose layer mask includes the layer can still find one (see the
 *        layer's note).
 * @param out What was made; on failure it is left empty and nothing made stays.
 * @param why Set to the reason when it fails, if not null.
 * @return False when cloth_available() is false, a sphere has no bone, the mesh's spans disagree in length or index
 *         out of range, or the engine refused a part, a sphere included: nothing runs in part.
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
