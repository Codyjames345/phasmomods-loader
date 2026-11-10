// Offline tests for playing a clip on a player's own body (sdk/api/player_anim.h) over the fake backend: a graph of
// the mod's own per clip, its output on the body's Animator above the game's (sorting order), its weight blended in and
// out, a new clip blended over the old, and the graphs destroyed when it ends, so the Animator is the game's again. The
// game's side is in sdk/systems/players.md, "The body". Run: build\player_anim_test.exe
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "Playables.h"
#include "Player.h"
#include "check.h"
#include "fake_game.h"
#include "player_anim.h"
#include "runtime.h"

namespace pm::hooks {
std::atomic<int> g_inflight{0};
}

namespace {

namespace anim = pm::api::anim;
namespace map = pm::map;
namespace fake = pm::fake;

struct Obj {
  void* klass = nullptr;
  unsigned char bytes[0x40]{};
};
constexpr int32_t kPlayerAnimator = 0x08;

// The fake engine: graphs, outputs and clip playables are numbered handles.
struct Graph {
  bool alive = true, playing = false;
  void* target = nullptr;
  int order = 0;
  float weight = 1.0f;
  void* clip = nullptr;
  double time = 0.0;
  bool footIK = false;
};
std::map<uintptr_t, Graph> g_graphs;  // by the graph's handle; an output and a playable carry their graph's
uintptr_t g_next = 1;

void fake_create(void*, phasmo::PlayableGraph* out) {
  out->handle = reinterpret_cast<void*>(g_next);
  out->version = 1;
  g_graphs[g_next++] = Graph{};
}
void fake_destroy(phasmo::PlayableGraph* g) { g_graphs[reinterpret_cast<uintptr_t>(g->handle)].alive = false; }
void fake_play(phasmo::PlayableGraph* g) { g_graphs[reinterpret_cast<uintptr_t>(g->handle)].playing = true; }
bool fake_valid(phasmo::PlayableGraph* g) {
  auto it = g_graphs.find(reinterpret_cast<uintptr_t>(g->handle));
  return it != g_graphs.end() && it->second.alive;
}
bool fake_create_output(phasmo::PlayableGraph* g, void*, phasmo::PlayableOutputHandle* out) {
  out->handle = g->handle;
  out->version = 1;
  return true;
}
Graph& of(void* handle) { return g_graphs[reinterpret_cast<uintptr_t>(handle)]; }
void fake_set_target(phasmo::PlayableOutputHandle* o, void* animator) { of(o->handle).target = animator; }
void fake_set_order(phasmo::PlayableOutputHandle* o, int32_t order) { of(o->handle).order = order; }
bool fake_create_clip(phasmo::PlayableGraph* g, void* clip, phasmo::PlayableHandle* out) {
  out->handle = g->handle;
  out->version = 1;
  of(g->handle).clip = clip;
  return true;
}
void fake_foot_ik(phasmo::PlayableHandle* p, bool on) { of(p->handle).footIK = on; }
void fake_set_source(phasmo::PlayableOutputHandle*, phasmo::PlayableHandle*, int32_t) {}
void fake_set_weight(phasmo::PlayableOutputHandle* o, float w) { of(o->handle).weight = w; }
double fake_get_time(phasmo::PlayableHandle* p) { return of(p->handle).time; }
void fake_set_time(phasmo::PlayableHandle* p, double t) { of(p->handle).time = t; }
float fake_length(void*) { return 2.5f; }
bool g_loops = true;
bool fake_loops(void*) { return g_loops; }

Obj g_player, g_animator, g_clipA, g_clipB;

// The live graphs on the animator, lowest sorting order first.
std::vector<Graph*> live_on(void* animator) {
  std::vector<Graph*> out;
  for (auto& [h, gr] : g_graphs)
    if (gr.alive && gr.target == animator) out.push_back(&gr);
  std::sort(out.begin(), out.end(), [](Graph* a, Graph* b) { return a->order < b->order; });
  return out;
}

void give() {
  fake::reset();
  g_graphs.clear();
  g_next = 1;
  g_loops = true;
  fake::field(map::Player_animator, kPlayerAnimator);
  fake::icall(map::PlayableGraph_Create_Injected, reinterpret_cast<void*>(&fake_create));
  fake::icall(map::PlayableGraph_Destroy_Injected, reinterpret_cast<void*>(&fake_destroy));
  fake::icall(map::PlayableGraph_Play_Injected, reinterpret_cast<void*>(&fake_play));
  fake::icall(map::PlayableGraph_IsValid_Injected, reinterpret_cast<void*>(&fake_valid));
  fake::icall(map::AnimationPlayableGraphExtensions_InternalCreateAnimationOutput,
              reinterpret_cast<void*>(&fake_create_output));
  fake::icall(map::AnimationPlayableOutput_InternalSetTarget, reinterpret_cast<void*>(&fake_set_target));
  fake::icall(map::AnimationPlayableOutputExtensions_InternalSetSortingOrder_Injected,
              reinterpret_cast<void*>(&fake_set_order));
  fake::icall(map::AnimationClipPlayable_CreateHandleInternal_Injected, reinterpret_cast<void*>(&fake_create_clip));
  fake::icall(map::AnimationClipPlayable_SetApplyFootIKInternal, reinterpret_cast<void*>(&fake_foot_ik));
  fake::icall(map::PlayableOutputHandle_SetSourcePlayable_Injected, reinterpret_cast<void*>(&fake_set_source));
  fake::icall(map::PlayableOutputHandle_SetWeight_Injected, reinterpret_cast<void*>(&fake_set_weight));
  fake::icall(map::PlayableHandle_GetTime_Injected, reinterpret_cast<void*>(&fake_get_time));
  fake::icall(map::PlayableHandle_SetTime_Injected, reinterpret_cast<void*>(&fake_set_time));
  fake::icall(map::AnimationClip_get_length, reinterpret_cast<void*>(&fake_length));
  fake::icall(map::Motion_get_isLooping, reinterpret_cast<void*>(&fake_loops));
  std::string missing;
  pm::api::bind(fake::host(), missing);
  *reinterpret_cast<void**>(g_player.bytes + kPlayerAnimator - 8) = &g_animator;
}

void a_clip_blends_in_and_hands_back() {
  give();
  CHECK(anim::play_on_player(&g_player, &g_clipA, 0.4f));
  auto live = live_on(&g_animator);
  CHECK(live.size() == 1);
  if (live.size() != 1) return;
  CHECK(live[0]->playing && live[0]->clip == &g_clipA && live[0]->footIK);
  // Above the game's own Animator (0) and above Animation Rigging (1000): under the rig, its aims bent the dance
  // toward the camera (the head, the arms holding items; the fifth in-game run).
  CHECK(live[0]->order >= anim::kSortingOrder && live[0]->order > 1000);
  CHECK(live[0]->weight == 0.0f);
  CHECK(anim::playing_on(&g_player));
  anim::tick_player_anims(0.2f);
  CHECK(live[0]->weight > 0.45f && live[0]->weight < 0.55f);
  anim::tick_player_anims(0.3f);
  CHECK(live[0]->weight == 1.0f);
  anim::stop_on_player(&g_player, 0.5f);
  CHECK(anim::playing_on(&g_player));  // still blending out
  anim::tick_player_anims(0.25f);
  CHECK(live[0]->weight > 0.45f && live[0]->weight < 0.55f);
  anim::tick_player_anims(0.3f);
  CHECK(live_on(&g_animator).empty());  // destroyed: the Animator is the game's alone
  CHECK(!anim::playing_on(&g_player));
}

void a_second_play_replaces_the_first() {
  give();
  anim::play_on_player(&g_player, &g_clipA, 0.2f);
  anim::tick_player_anims(0.5f);
  CHECK(anim::play_on_player(&g_player, &g_clipB, 0.2f));
  auto live = live_on(&g_animator);
  CHECK(live.size() == 2);  // the new one over the old while it blends in
  if (live.size() == 2) {
    CHECK(live[0]->clip == &g_clipA && live[0]->weight == 1.0f);
    CHECK(live[1]->clip == &g_clipB && live[1]->weight == 0.0f);
  }
  if (live.size() == 2) CHECK(live[1]->order > live[0]->order);  // the newer over the older
  anim::tick_player_anims(0.3f);
  live = live_on(&g_animator);
  CHECK(live.size() == 1 && live[0]->clip == &g_clipB && live[0]->weight == 1.0f);
  // However many dances follow one another, the orders stay bounded, the newest over the one before.
  for (int i = 0; i < 1200; ++i) {
    anim::play_on_player(&g_player, i % 2 ? &g_clipA : &g_clipB, 0.2f);
    auto now = live_on(&g_animator);
    if (now.size() == 2) CHECK(now[1]->order > now[0]->order);
    for (const auto* l : now) CHECK(l->order > 1000 && l->order <= anim::kSortingOrder + 2);
    anim::tick_player_anims(0.3f);
  }
  // A third, mid-blend: never more than two graphs at once.
  anim::play_on_player(&g_player, &g_clipA, 0.2f);
  anim::tick_player_anims(0.05f);
  anim::play_on_player(&g_player, &g_clipB, 0.2f);
  CHECK(live_on(&g_animator).size() <= 2);
  anim::stop_all_player_anims();
  CHECK(live_on(&g_animator).empty());
}

void a_destroyed_body_is_forgotten() {
  give();
  anim::play_on_player(&g_player, &g_clipA, 0.2f);
  fake::destroy(&g_player);
  anim::tick_player_anims(0.1f);
  CHECK(!anim::playing_on(&g_player));
  CHECK(live_on(&g_animator).empty());
}

void the_clip_reports_its_time_and_loop() {
  give();
  CHECK(anim::clip_seconds(&g_clipA) == 2.5f);
  CHECK(anim::clip_loops(&g_clipA));
  g_loops = false;
  CHECK(!anim::clip_loops(&g_clipA));
  anim::play_on_player(&g_player, &g_clipA, 0.2f);
  auto live = live_on(&g_animator);
  if (live.size() == 1) live[0]->time = 3.75;
  CHECK(anim::clip_time(&g_player) == 3.75);
  CHECK(anim::clip_time(&g_clipB) < 0.0);  // not playing on it
  anim::stop_all_player_anims();
}

void nothing_plays_without_a_body() {
  give();
  CHECK(!anim::play_on_player(nullptr, &g_clipA, 0.2f));
  CHECK(!anim::play_on_player(&g_player, nullptr, 0.2f));
  Obj noAnimator;
  CHECK(!anim::play_on_player(&noAnimator, &g_clipA, 0.2f));
  CHECK(g_graphs.empty());
}

}  // namespace

int main() {
  a_clip_blends_in_and_hands_back();
  a_second_play_replaces_the_first();
  a_destroyed_body_is_forgotten();
  the_clip_reports_its_time_and_loop();
  nothing_plays_without_a_body();
  std::printf("%s\n", g_failures ? "player_anim tests FAILED" : "player_anim tests passed");
  return g_failures ? 1 : 0;
}
