#include "player_anim.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "Playables.h"
#include "Player.h"
#include "held.h"
#include "log.h"
#include "runtime.h"

namespace pm::api::anim {

namespace {

// One clip a body has played: its clip playable, connected once to the body's mixer at the slot's index and kept.
struct Slot {
  Held clip;
  phasmo::PlayableHandle playable;
  float shown = 0.0f;  // the mixer input's weight as last set
};

// A body's one graph: a mixer as the source of one animation output on the body's Animator, made once and kept while
// the body lives. The output's weight blends the dance in and out over the game's animation; the mixer's weights
// crossfade from the clip before (`previous`) to the newest (`current`). In live tests (the user, 2026-11-14), a body
// whose output was added and removed for each clip kept a wrong held-item pose after the dance: an empty hand posed
// as holding, a held item too high. The game gives a player's Animator its outputs once, as the body is made, so this
// API does the same and afterwards changes only weights.
struct Body {
  Held player;
  HeldRef<pm::map::Animator> animator;
  phasmo::PlayableGraph graph;
  phasmo::PlayableOutputHandle output;
  phasmo::PlayableHandle mixer;
  std::vector<Slot> slots;
  int current = -1;   // the newest clip's slot, -1 while none plays
  int previous = -1;  // the slot it crossfades from, -1 for none
  float mix = 1.0f;   // the newest clip's share of the mixer, 0 to 1; the previous one has the rest
  float mixRate = 0.0f;
  float weight = 0.0f;  // the output's weight
  float target = 0.0f;
  float rate = 0.0f;  // weight per second; 0 is at once
  bool gone = false;  // its graph destroyed with the body, to be forgotten
};

std::vector<Body> g_bodies;
bool g_loggedUnresolved = false;

bool entries_resolved() {
  if (phasmo::PlayableGraph::Create.available() && phasmo::AnimationPlayables::CreateOutput.available() &&
      phasmo::AnimationPlayables::CreateClipPlayable.available() &&
      phasmo::AnimationPlayables::CreateMixer.available() && phasmo::AnimationPlayables::Connect.available() &&
      phasmo::PlayableHandle::SetInputCount.available() && phasmo::PlayableHandle::SetInputWeight.available())
    return true;
  if (!g_loggedUnresolved) PM_LOG("anim: the Playables entries did not resolve in this game build: no clip plays");
  g_loggedUnresolved = true;
  return false;
}

void destroy(Body& b) {
  if (phasmo::PlayableGraph::IsValid(&b.graph)) phasmo::PlayableGraph::Destroy(&b.graph);
}

Body* find(void* player) {
  for (Body& b : g_bodies)
    if (b.player.get() == player) return &b;
  return nullptr;
}

// The body's graph, made now if it has none: a mixer with no inputs, the source of an output on the Animator above
// the game's own and Animation Rigging's, at weight 0. Null when it cannot be made.
Body* body_of(void* player) {
  if (!player || !alive(player)) return nullptr;
  const auto animator = phasmo::Player::animator.get(player);
  if (!animator || !alive(animator.raw())) return nullptr;
  if (Body* b = find(player)) return b;
  if (!entries_resolved()) return nullptr;
  Body b{player, animator};
  phasmo::PlayableGraph::Create(make_string("pm emote"), &b.graph);
  if (!phasmo::PlayableGraph::IsValid(&b.graph)) return nullptr;
  if (!phasmo::AnimationPlayables::CreateMixer(&b.graph, &b.mixer) ||
      !phasmo::AnimationPlayables::CreateOutput(&b.graph, make_string("pm emote"), &b.output)) {
    destroy(b);
    return nullptr;
  }
  phasmo::PlayableOutputHandle::SetWeight(&b.output, 0.0f);
  phasmo::AnimationPlayables::SetSortingOrder(&b.output, kSortingOrder);
  phasmo::PlayableOutputHandle::SetSourcePlayable(&b.output, &b.mixer, 0);
  phasmo::AnimationPlayables::SetTarget(&b.output, animator);
  phasmo::PlayableGraph::Play(&b.graph);
  g_bodies.push_back(std::move(b));
  return &g_bodies.back();
}

// The slot of `clip` on the body, connected to the next mixer input the first time. -1 when it cannot be made.
int slot_of(Body& b, void* clip) {
  for (size_t i = 0; i < b.slots.size(); ++i)
    if (b.slots[i].clip.get() == clip) return static_cast<int>(i);
  Slot s{clip};
  if (!phasmo::AnimationPlayables::CreateClipPlayable(&b.graph, clip, &s.playable)) return -1;
  phasmo::AnimationPlayables::SetApplyFootIK(&s.playable, true);
  const int input = static_cast<int>(b.slots.size());
  phasmo::PlayableHandle::SetInputCount(&b.mixer, input + 1);
  if (!phasmo::AnimationPlayables::Connect(&b.graph, &s.playable, 0, &b.mixer, input)) {
    phasmo::PlayableHandle::SetInputCount(&b.mixer, input);
    return -1;
  }
  phasmo::PlayableHandle::SetInputWeight(&b.mixer, input, 0.0f);
  b.slots.push_back(std::move(s));
  return input;
}

// Sets each mixer input's weight from the crossfade, and the output's weight, calling the engine only for a change.
void apply(Body& b) {
  for (size_t i = 0; i < b.slots.size(); ++i) {
    const int slot = static_cast<int>(i);
    const float w = slot == b.current ? b.mix : slot == b.previous ? 1.0f - b.mix : 0.0f;
    if (w == b.slots[i].shown) continue;
    b.slots[i].shown = w;
    phasmo::PlayableHandle::SetInputWeight(&b.mixer, slot, w);
  }
  phasmo::PlayableOutputHandle::SetWeight(&b.output, b.weight);
}

void step(float& value, float goal, float rate, float dt) {
  if (value == goal) return;
  const float by = rate > 0.0f ? rate * dt : 1.0f;
  value = value < goal ? std::min(goal, value + by) : std::max(goal, value - by);
}

}  // namespace

bool play_on_player(void* player, void* clip, float blendIn) {
  if (!player || !clip || !alive(player) || !alive(clip)) return false;
  Body* b = body_of(player);
  if (!b) return false;
  const int slot = slot_of(*b, clip);
  if (slot < 0) return false;
  phasmo::PlayableHandle::SetTime(&b->slots[slot].playable, 0.0);
  const float rate = blendIn > 0.0f ? 1.0f / blendIn : 0.0f;
  if (b->current < 0) {
    b->current = slot;
    b->previous = -1;
    b->mix = 1.0f;
  } else if (slot == b->previous) {
    // Back to the clip still fading out: it goes on from the share it has.
    std::swap(b->current, b->previous);
    b->mix = 1.0f - b->mix;
    b->mixRate = rate;
  } else if (slot != b->current) {
    // Over the newest; a clip still fading out under it stops showing at once, so two show at most.
    b->previous = b->current;
    b->current = slot;
    b->mix = 0.0f;
    b->mixRate = rate;
  }
  if (rate == 0.0f) {
    b->mix = 1.0f;
    b->previous = -1;
  }
  b->target = 1.0f;
  b->rate = rate;
  if (rate == 0.0f) b->weight = 1.0f;
  apply(*b);
  return true;
}

bool play_on_player(void* player, void* clip, float blendIn, double startSeconds) {
  if (!play_on_player(player, clip, blendIn)) return false;
  Body* b = find(player);
  if (!b || b->current < 0) return false;
  const double length = static_cast<double>(clip_seconds(clip));
  double at = std::max(0.0, startSeconds);
  if (length > 0.0) at = clip_loops(clip) ? std::fmod(at, length) : std::min(at, length);
  phasmo::PlayableHandle::SetTime(&b->slots[b->current].playable, at);
  return true;
}

void stop_on_player(void* player, float blendOut) {
  Body* b = player ? find(player) : nullptr;
  if (!b || b->current < 0) return;
  b->target = 0.0f;
  b->rate = blendOut > 0.0f ? 1.0f / blendOut : 0.0f;
  if (b->rate == 0.0f) {
    b->weight = 0.0f;
    b->current = b->previous = -1;
    b->mix = 1.0f;
    apply(*b);
  }
}

bool playing_on(void* player) {
  const Body* b = player ? find(player) : nullptr;
  return b && b->current >= 0;
}

double clip_time(void* player) {
  Body* b = player ? find(player) : nullptr;
  if (!b || b->current < 0) return -1.0;
  return phasmo::PlayableHandle::GetTime(&b->slots[b->current].playable);
}

float clip_seconds(void* clip) { return clip ? phasmo::AnimationClip::get_length(clip) : 0.0f; }

bool clip_loops(void* clip) { return clip && phasmo::AnimationClip::get_isLooping(clip); }

void tick_player_anims(float dt) {
  for (Body& b : g_bodies) {
    // A body gone, or given another Animator, takes its graph with it; a new one is made at its next prepare or play.
    const bool gone = !alive(b.player) || !alive(b.animator.get().raw()) ||
                      phasmo::Player::animator.get(b.player.get()).raw() != b.animator.get().raw();
    if (gone) {
      destroy(b);
      b.gone = true;
      continue;
    }
    if (b.current < 0) continue;
    step(b.weight, b.target, b.rate, dt);
    if (b.previous >= 0) {
      step(b.mix, 1.0f, b.mixRate, dt);
      if (b.mix >= 1.0f) b.previous = -1;
    }
    if (b.target == 0.0f && b.weight <= 0.0f) {
      b.current = b.previous = -1;
      b.mix = 1.0f;
    }
    apply(b);
  }
  std::erase_if(g_bodies, [](const Body& b) { return b.gone; });
}

bool prepare_player(void* player) { return body_of(player) != nullptr; }

void stop_all_player_anims() {
  for (Body& b : g_bodies) destroy(b);
  g_bodies.clear();
}

}  // namespace pm::api::anim
