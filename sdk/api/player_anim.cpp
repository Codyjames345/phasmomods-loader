#include "player_anim.h"

#include <algorithm>
#include <vector>

#include "Playables.h"
#include "Player.h"
#include "held.h"
#include "log.h"
#include "runtime.h"

namespace pm::api::anim {

namespace {

// One clip on a body: its own graph, output and clip playable, and its weight's blend.
struct Layer {
  phasmo::PlayableGraph graph;
  phasmo::PlayableOutputHandle output;
  phasmo::PlayableHandle clip;
  float weight = 0.0f;
  float target = 1.0f;
  float rate = 0.0f;  // weight per second; 0 is at once
  int order = 0;      // its output's sorting order
};

struct Body {
  Held player;
  Held animator;
  std::vector<Layer> layers;  // oldest first; the last is the newest, over the others
};

std::vector<Body> g_bodies;

void destroy(Layer& l) {
  if (phasmo::PlayableGraph::IsValid(&l.graph)) phasmo::PlayableGraph::Destroy(&l.graph);
}

void set_weight(Layer& l, float w) {
  l.weight = std::clamp(w, 0.0f, 1.0f);
  phasmo::PlayableOutputHandle::SetWeight(&l.output, l.weight);
}

Body* find(void* player) {
  for (Body& b : g_bodies)
    if (b.player.get() == player) return &b;
  return nullptr;
}

void end_body(Body& b) {
  for (Layer& l : b.layers) destroy(l);
  b.layers.clear();
}

}  // namespace

bool play_on_player(void* player, void* clip, float blendIn) {
  if (!player || !clip || !alive(player) || !alive(clip)) return false;
  void* animator = phasmo::Player::animator.get(player);
  if (!animator || !alive(animator)) return false;
  if (!phasmo::PlayableGraph::Create.available() || !phasmo::AnimationPlayables::CreateOutput.available() ||
      !phasmo::AnimationPlayables::CreateClipPlayable.available()) {
    PM_LOG("anim: the Playables entries did not resolve in this game build: no clip plays");
    return false;
  }
  Body* body = find(player);
  if (!body) {
    g_bodies.push_back(Body{player, animator, {}});
    body = &g_bodies.back();
  }
  // At most two clips: a third ends the older of the two at once.
  while (body->layers.size() >= 2) {
    destroy(body->layers.front());
    body->layers.erase(body->layers.begin());
  }
  Layer l;
  phasmo::PlayableGraph::Create(make_string("pm emote"), &l.graph);
  if (!phasmo::PlayableGraph::IsValid(&l.graph)) return false;
  if (!phasmo::AnimationPlayables::CreateOutput(&l.graph, make_string("pm emote"), &l.output) ||
      !phasmo::AnimationPlayables::CreateClipPlayable(&l.graph, clip, &l.clip)) {
    destroy(l);
    return false;
  }
  phasmo::AnimationPlayables::SetApplyFootIK(&l.clip, true);
  phasmo::AnimationPlayables::SetTarget(&l.output, animator);
  // Over the clip still on the body, if any; a lone clip is at kSortingOrder (tick_player_anims), so at most two
  // above it.
  l.order = body->layers.empty() ? kSortingOrder : body->layers.back().order + 1;
  phasmo::AnimationPlayables::SetSortingOrder(&l.output, l.order);
  phasmo::PlayableOutputHandle::SetSourcePlayable(&l.output, &l.clip, 0);
  l.target = 1.0f;
  l.rate = blendIn > 0.0f ? 1.0f / blendIn : 0.0f;
  set_weight(l, l.rate > 0.0f ? 0.0f : 1.0f);
  phasmo::PlayableGraph::Play(&l.graph);
  // The older clip, if any, stays at its weight under the new one until the new one is in.
  for (Layer& old : body->layers) old.target = old.weight;
  body->layers.push_back(l);
  return true;
}

void stop_on_player(void* player, float blendOut) {
  Body* b = player ? find(player) : nullptr;
  if (!b) return;
  for (Layer& l : b->layers) {
    l.target = 0.0f;
    l.rate = blendOut > 0.0f ? 1.0f / blendOut : 0.0f;
    if (l.rate == 0.0f) set_weight(l, 0.0f);
  }
}

bool playing_on(void* player) {
  const Body* b = player ? find(player) : nullptr;
  return b && !b->layers.empty();
}

double clip_time(void* player) {
  Body* b = player ? find(player) : nullptr;
  if (!b || b->layers.empty()) return -1.0;
  return phasmo::PlayableHandle::GetTime(&b->layers.back().clip);
}

float clip_seconds(void* clip) { return clip ? phasmo::AnimationClip::get_length(clip) : 0.0f; }

bool clip_loops(void* clip) { return clip && phasmo::AnimationClip::get_isLooping(clip); }

void tick_player_anims(float dt) {
  for (Body& b : g_bodies) {
    if (!alive(b.player) || !alive(b.animator)) {
      end_body(b);
      continue;
    }
    for (Layer& l : b.layers) {
      if (l.weight == l.target) continue;
      const float step = l.rate > 0.0f ? l.rate * dt : 1.0f;
      set_weight(l, l.weight < l.target ? std::min(l.target, l.weight + step) : std::max(l.target, l.weight - step));
    }
    // The newest in fully ends the ones under it; a stop that reached 0 ends them all.
    if (!b.layers.empty() && b.layers.back().target == 1.0f && b.layers.back().weight >= 1.0f)
      while (b.layers.size() > 1) {
        destroy(b.layers.front());
        b.layers.erase(b.layers.begin());
      }
    // A lone clip goes back to the first order, so the orders never climb.
    if (b.layers.size() == 1 && b.layers.front().order != kSortingOrder) {
      b.layers.front().order = kSortingOrder;
      phasmo::AnimationPlayables::SetSortingOrder(&b.layers.front().output, kSortingOrder);
    }
    if (!b.layers.empty() && std::all_of(b.layers.begin(), b.layers.end(),
                                         [](const Layer& l) { return l.target == 0.0f && l.weight <= 0.0f; }))
      end_body(b);
  }
  std::erase_if(g_bodies, [](const Body& b) { return b.layers.empty(); });
}

void stop_all_player_anims() {
  for (Body& b : g_bodies) end_body(b);
  g_bodies.clear();
}

}  // namespace pm::api::anim
