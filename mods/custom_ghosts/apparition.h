#pragma once

#include <cstdint>
#include <vector>

#include "il2cpp.h"

// A copy of a ghost model that only shows: its renderers and its Animator, with every script, collider,
// rigidbody and sound taken off, so it cannot touch, kill, be heard or be networked. It flickers and
// animates as the model it mirrors. Used for the Hydra's false heads and the Mesmer's forms. Local to
// the client that makes it. Main thread.
namespace pg::custom::apparition {

struct Parameter {
  int hash;
  int type;  // AnimatorControllerParameterType: 1 float, 3 int, 4 bool (triggers are not copied)
};

struct Copy {
  il2cpp::Held holder;  // the GameObject that owns the copy (destroyed with it)
  il2cpp::Held object;  // the copy's GameObject
  il2cpp::Held root;    // the copy's Transform
  il2cpp::Held animator;
  il2cpp::Held model;   // the GhostModel it was made from
  il2cpp::Held modelAnimator;
  std::vector<il2cpp::Held> renderers;       // the copy's, in hierarchy order
  std::vector<il2cpp::Held> modelRenderers;  // the model's, in the same order
  std::vector<Parameter> parameters;         // the model Animator's, copied each mirror()
  il2cpp::Held followed;                     // another model follow() shows it as, and what it reads of it
  il2cpp::Held followedAnimator;
  std::vector<il2cpp::Held> followedRenderers;
  std::vector<Parameter> shared;             // the parameters the copy's Animator and the followed one's share
};

bool resolve();
/** A copy of `model` (a GhostModel) at its place; empty (no root) when it cannot be made. */
Copy make(void* model);
/** Show the copy where `at` faces `yaw` degrees about the vertical. */
void place(Copy& copy, il2cpp::Vector3 at, float yaw);
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

}  // namespace pg::custom::apparition
