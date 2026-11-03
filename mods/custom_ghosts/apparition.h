#pragma once

#include <cstdint>
#include <vector>

#include "held.h"
#include "Transform.h"

// A copy of a ghost model that only shows: its renderers and its Animator, with every script, collider,
// rigidbody and sound taken off, so it cannot touch, kill, be heard or be networked. It flickers and
// animates as the model it mirrors. Used for the Hydra's false heads, the Mesmer's forms and the Goryo's
// figure (Ghost Tells). It reaches the game only through the game API (sdk/map), whose entries the mod
// requires. Local to the client that makes it. Main thread.
namespace pm::custom::apparition {

struct Parameter {
  int hash;
  int type;  // its AnimatorControllerParameterType: Float, Int or Bool (triggers are not copied)
};

struct Copy {
  api::Held holder;  // the GameObject that owns the copy (destroyed with it)
  api::Held object;  // the copy's GameObject
  api::Held root;    // the copy's Transform
  api::Held animator;
  api::Held model;   // the GhostModel it was made from
  api::Held modelAnimator;
  std::vector<api::Held> renderers;       // the copy's, in hierarchy order
  std::vector<api::Held> modelRenderers;  // the model's, in the same order
  std::vector<Parameter> parameters;      // the model Animator's, copied each mirror()
  api::Held followed;                     // another model follow() shows it as, and what it reads of it
  api::Held followedAnimator;
  std::vector<api::Held> followedRenderers;
  std::vector<Parameter> shared;          // the parameters the copy's Animator and the followed one's share
};

bool resolve();  // true: its entries are the requirements of the mod that links it
/** A copy of `model` (a GhostModel) at its place; empty (no root) when it cannot be made. */
Copy make(void* model);
/** Show the copy where `at` faces `yaw` degrees about the vertical. */
void place(Copy& copy, phasmo::Vector3 at, float yaw);
/** Show it as the model shows now (each renderer on or off as the model's), animating as it does. */
void mirror(Copy& copy, void* model);
/**
 * Show it as another model shows now (a copy of one form standing in for another): seen while any of
 * that model's renderers is on, animating with the parameters the two Animators share.
 */
void follow(Copy& copy, void* model);
/** Put the whole copy on one rendering layer. */
void set_layer(Copy& copy, int layer);
/** Show it or hide it outright. */
void show(Copy& copy, bool visible);
void destroy(Copy& copy);

/** The model the ghost wears now (GhostAI's GhostModel), or null. */
void* model_of(void* ghostAi);

}  // namespace pm::custom::apparition
