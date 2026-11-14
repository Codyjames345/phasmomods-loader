// Offline tests for playing a clip on a player's own body (sdk/api/player_anim.h) over the fake backend: one graph and
// one output per body, made once and kept for the body's life, above the game's own outputs (sorting order); clips on
// a mixer under it, each connected once; the output's weight blended in and out, and a new clip crossfaded over the
// old inside the mixer. The Animator's outputs never change after the first, since a body whose output was removed
// after each dance kept a wrong held-item pose afterwards (the user's live test, 2026-11-14). The game's side is in
// sdk/systems/players.md, "The body". Run: build\player_anim_test.exe
#include <algorithm>
#include <atomic>
#include <cmath>
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

// The fake engine: graphs, outputs and playables are numbered handles; an output and a playable live while their
// graph does.
struct Graph {
  bool alive = true, playing = false;
};
struct Output {
  uintptr_t graph = 0;
  void* target = nullptr;
  int order = 0;
  float weight = 1.0f;
  uintptr_t source = 0;
};
struct Input {
  uintptr_t from = 0;  // 0: nothing connected
  float weight = 0.0f;
};
struct Playable {
  uintptr_t graph = 0;
  bool mixer = false;
  void* clip = nullptr;
  double time = 0.0;
  bool footIK = false;
  bool connected = false;  // its output port feeds an input
  std::vector<Input> inputs;
};
std::map<uintptr_t, Graph> g_graphs;
std::map<uintptr_t, Output> g_outputs;
std::map<uintptr_t, Playable> g_playables;
std::map<void*, int> g_outputsMade;  // outputs ever pointed at each Animator
uintptr_t g_next = 1;

uintptr_t id(void* handle) { return reinterpret_cast<uintptr_t>(handle); }
bool graph_alive(uintptr_t g) { return g_graphs.count(g) && g_graphs[g].alive; }

void fake_create(void*, phasmo::PlayableGraph* out) {
  out->handle = reinterpret_cast<void*>(g_next);
  out->version = 1;
  g_graphs[g_next++] = Graph{};
}
void fake_destroy(phasmo::PlayableGraph* g) { g_graphs[id(g->handle)].alive = false; }
void fake_play(phasmo::PlayableGraph* g) { g_graphs[id(g->handle)].playing = true; }
bool fake_valid(phasmo::PlayableGraph* g) { return graph_alive(id(g->handle)); }
bool fake_create_output(phasmo::PlayableGraph* g, void*, phasmo::PlayableOutputHandle* out) {
  out->handle = reinterpret_cast<void*>(g_next);
  out->version = 1;
  g_outputs[g_next++] = Output{.graph = id(g->handle)};
  return true;
}
void fake_set_target(phasmo::PlayableOutputHandle* o, void* animator) {
  g_outputs[id(o->handle)].target = animator;
  ++g_outputsMade[animator];
}
void fake_set_order(phasmo::PlayableOutputHandle* o, int32_t order) { g_outputs[id(o->handle)].order = order; }
bool fake_create_clip(phasmo::PlayableGraph* g, void* clip, phasmo::PlayableHandle* out) {
  out->handle = reinterpret_cast<void*>(g_next);
  out->version = 1;
  g_playables[g_next++] = Playable{.graph = id(g->handle), .clip = clip};
  return true;
}
bool fake_create_mixer(phasmo::PlayableGraph* g, phasmo::PlayableHandle* out) {
  out->handle = reinterpret_cast<void*>(g_next);
  out->version = 1;
  g_playables[g_next++] = Playable{.graph = id(g->handle), .mixer = true};
  return true;
}
void fake_set_input_count(phasmo::PlayableHandle* p, int32_t n) {
  auto& ins = g_playables[id(p->handle)].inputs;
  for (size_t i = static_cast<size_t>(n); i < ins.size(); ++i)
    if (ins[i].from) g_playables[ins[i].from].connected = false;
  ins.resize(static_cast<size_t>(n));
}
bool fake_connect(phasmo::PlayableGraph* g, phasmo::PlayableHandle* src, int32_t port, phasmo::PlayableHandle* dst,
                  int32_t input) {
  Playable& s = g_playables[id(src->handle)];
  Playable& d = g_playables[id(dst->handle)];
  if (port != 0 || s.connected || s.graph != id(g->handle) || d.graph != id(g->handle)) return false;
  if (input < 0 || static_cast<size_t>(input) >= d.inputs.size() || d.inputs[input].from) return false;
  d.inputs[input].from = id(src->handle);
  s.connected = true;
  return true;
}
void fake_set_input_weight(phasmo::PlayableHandle* p, int32_t i, float w) {
  auto& ins = g_playables[id(p->handle)].inputs;
  if (i >= 0 && static_cast<size_t>(i) < ins.size()) ins[i].weight = w;
}
void fake_foot_ik(phasmo::PlayableHandle* p, bool on) { g_playables[id(p->handle)].footIK = on; }
void fake_set_source(phasmo::PlayableOutputHandle* o, phasmo::PlayableHandle* p, int32_t) {
  g_outputs[id(o->handle)].source = id(p->handle);
}
void fake_set_weight(phasmo::PlayableOutputHandle* o, float w) { g_outputs[id(o->handle)].weight = w; }
double fake_get_time(phasmo::PlayableHandle* p) { return g_playables[id(p->handle)].time; }
void fake_set_time(phasmo::PlayableHandle* p, double t) { g_playables[id(p->handle)].time = t; }
float fake_length(void*) { return 2.5f; }
bool g_loops = true;
bool fake_loops(void*) { return g_loops; }

Obj g_player, g_animator, g_clipA, g_clipB, g_clipC;

// The live outputs on an Animator.
std::vector<Output*> outputs_on(void* animator) {
  std::vector<Output*> out;
  for (auto& [h, o] : g_outputs)
    if (o.target == animator && graph_alive(o.graph)) out.push_back(&o);
  return out;
}

// How much of a clip shows on the Animator: its output's weight times its mixer input's weight.
float shown(void* animator, void* clip) {
  float total = 0.0f;
  for (Output* o : outputs_on(animator)) {
    const Playable& src = g_playables[o->source];
    if (!src.mixer) {
      if (src.clip == clip) total += o->weight;
      continue;
    }
    for (const Input& in : src.inputs)
      if (in.from && g_playables[in.from].clip == clip) total += o->weight * in.weight;
  }
  return total;
}

// The body's mixer: the source of its one output.
Playable* mixer_of(void* animator) {
  auto outs = outputs_on(animator);
  if (outs.size() != 1) return nullptr;
  Playable& p = g_playables[outs[0]->source];
  return p.mixer ? &p : nullptr;
}

// The sum of a mixer's input weights: 1 while a clip shows, so the mixer never blends toward a default pose.
float mix_sum(void* animator) {
  const Playable* m = mixer_of(animator);
  float sum = 0.0f;
  if (m)
    for (const Input& in : m->inputs) sum += in.weight;
  return sum;
}

int clip_playables() {
  int n = 0;
  for (auto& [h, p] : g_playables) n += p.mixer ? 0 : 1;
  return n;
}

bool near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

void give() {
  anim::stop_all_player_anims();
  fake::reset();
  g_graphs.clear();
  g_outputs.clear();
  g_playables.clear();
  g_outputsMade.clear();
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
  fake::icall(map::AnimationMixerPlayable_CreateHandleInternal_Injected, reinterpret_cast<void*>(&fake_create_mixer));
  fake::icall(map::PlayableGraph_ConnectInternal_Injected, reinterpret_cast<void*>(&fake_connect));
  fake::icall(map::PlayableHandle_SetInputCount_Injected, reinterpret_cast<void*>(&fake_set_input_count));
  fake::icall(map::PlayableHandle_SetInputWeightFromIndex_Injected, reinterpret_cast<void*>(&fake_set_input_weight));
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

void a_clip_blends_in_and_out() {
  give();
  CHECK(anim::play_on_player(&g_player, &g_clipA, 0.4f));
  auto outs = outputs_on(&g_animator);
  CHECK(outs.size() == 1);
  if (outs.size() != 1) return;
  CHECK(g_graphs[outs[0]->graph].playing);
  // Above the game's own Animator (0) and above Animation Rigging (1000): under the rig, its aims bent the dance
  // toward the camera (the head, the arms holding items; the fifth in-game run).
  CHECK(outs[0]->order >= anim::kSortingOrder && outs[0]->order > 1000);
  CHECK(near(shown(&g_animator, &g_clipA), 0.0f));
  CHECK(anim::playing_on(&g_player));
  anim::tick_player_anims(0.2f);
  CHECK(near(shown(&g_animator, &g_clipA), 0.5f));
  anim::tick_player_anims(0.3f);
  CHECK(near(shown(&g_animator, &g_clipA), 1.0f));
  anim::stop_on_player(&g_player, 0.5f);
  CHECK(anim::playing_on(&g_player));  // still blending out
  anim::tick_player_anims(0.25f);
  CHECK(near(shown(&g_animator, &g_clipA), 0.5f));
  anim::tick_player_anims(0.3f);
  CHECK(!anim::playing_on(&g_player));
  CHECK(near(shown(&g_animator, &g_clipA), 0.0f));
  // Out, the output stays on the Animator at weight 0, so the Animator's outputs are as they were during the dance.
  outs = outputs_on(&g_animator);
  CHECK(outs.size() == 1 && outs[0]->weight == 0.0f);
  for (auto& [h, p] : g_playables)
    if (p.clip == &g_clipA) CHECK(p.footIK);
}

// The fault the user saw (2026-11-14): after a dance the arms kept a wrong pose, a held item too high and an empty
// hand posed as holding. The Animator's outputs were added and removed per clip. Now a body gets its one output before
// any dance and keeps it through every dance, with each clip connected once.
void the_body_keeps_one_output_for_its_life() {
  give();
  CHECK(anim::prepare_player(&g_player));
  auto outs = outputs_on(&g_animator);
  CHECK(outs.size() == 1 && outs[0]->weight == 0.0f);
  CHECK(!anim::playing_on(&g_player));
  CHECK(anim::prepare_player(&g_player));  // again: nothing new
  for (int i = 0; i < 50; ++i) {
    anim::play_on_player(&g_player, i % 3 == 0 ? &g_clipA : i % 3 == 1 ? &g_clipB : &g_clipC, 0.2f);
    anim::tick_player_anims(0.1f);
    if (i % 2) anim::play_on_player(&g_player, &g_clipA, 0.2f);  // a phase change mid-dance
    anim::tick_player_anims(0.3f);
    anim::stop_on_player(&g_player, 0.35f);
    anim::tick_player_anims(0.5f);
    CHECK(!anim::playing_on(&g_player));
  }
  CHECK(g_outputsMade[&g_animator] == 1);
  CHECK(outputs_on(&g_animator).size() == 1);
  CHECK(clip_playables() == 3);  // one per clip, reused
  const Playable* m = mixer_of(&g_animator);
  CHECK(m && m->inputs.size() == 3);
}

// A play before any prepare gives the body its output then, once.
void a_play_alone_prepares_the_body() {
  give();
  anim::play_on_player(&g_player, &g_clipA, 0.0f);
  anim::stop_on_player(&g_player, 0.0f);
  anim::tick_player_anims(0.1f);
  anim::play_on_player(&g_player, &g_clipB, 0.0f);
  CHECK(g_outputsMade[&g_animator] == 1);
  CHECK(near(shown(&g_animator, &g_clipB), 1.0f));
}

void a_second_play_crossfades() {
  give();
  anim::play_on_player(&g_player, &g_clipA, 0.2f);
  anim::tick_player_anims(0.5f);
  CHECK(anim::play_on_player(&g_player, &g_clipB, 0.2f));
  CHECK(near(shown(&g_animator, &g_clipA), 1.0f) && near(shown(&g_animator, &g_clipB), 0.0f));
  anim::tick_player_anims(0.1f);
  CHECK(near(shown(&g_animator, &g_clipA), 0.5f) && near(shown(&g_animator, &g_clipB), 0.5f));
  anim::tick_player_anims(0.2f);
  CHECK(near(shown(&g_animator, &g_clipA), 0.0f) && near(shown(&g_animator, &g_clipB), 1.0f));
  // However many follow one another, the mixer's weights add to 1, and at most two clips show.
  for (int i = 0; i < 1200; ++i) {
    anim::play_on_player(&g_player, i % 3 == 0 ? &g_clipA : i % 3 == 1 ? &g_clipB : &g_clipC, 0.2f);
    CHECK(near(mix_sum(&g_animator), 1.0f));
    int showing = 0;
    for (void* c : {static_cast<void*>(&g_clipA), static_cast<void*>(&g_clipB), static_cast<void*>(&g_clipC)})
      showing += shown(&g_animator, c) > 0.0f ? 1 : 0;
    CHECK(showing <= 2);
    anim::tick_player_anims(i % 4 == 0 ? 0.05f : 0.3f);
    CHECK(near(mix_sum(&g_animator), 1.0f));
  }
  CHECK(g_outputsMade[&g_animator] == 1);
}

// Back to the clip still fading out: it carries on from where it showed, with no jump.
void back_to_the_fading_clip_has_no_jump() {
  give();
  anim::play_on_player(&g_player, &g_clipA, 0.2f);
  anim::tick_player_anims(0.5f);
  anim::play_on_player(&g_player, &g_clipB, 0.2f);
  anim::tick_player_anims(0.05f);  // A 0.75, B 0.25
  anim::play_on_player(&g_player, &g_clipA, 0.2f);
  CHECK(near(shown(&g_animator, &g_clipA), 0.75f) && near(shown(&g_animator, &g_clipB), 0.25f));
  anim::tick_player_anims(0.05f);
  CHECK(near(shown(&g_animator, &g_clipA), 1.0f) && near(shown(&g_animator, &g_clipB), 0.0f));
}

// A dance asked for while the last one blends out blends back in from where the weight is.
void a_play_while_blending_out_rises_from_there() {
  give();
  anim::play_on_player(&g_player, &g_clipA, 0.0f);
  anim::stop_on_player(&g_player, 0.5f);
  anim::tick_player_anims(0.25f);
  CHECK(near(shown(&g_animator, &g_clipA), 0.5f));
  anim::play_on_player(&g_player, &g_clipA, 0.5f);
  CHECK(near(shown(&g_animator, &g_clipA), 0.5f));
  anim::tick_player_anims(0.25f);
  CHECK(near(shown(&g_animator, &g_clipA), 1.0f));
}

void a_destroyed_body_is_forgotten() {
  give();
  anim::play_on_player(&g_player, &g_clipA, 0.2f);
  fake::destroy(&g_player);
  anim::tick_player_anims(0.1f);
  CHECK(!anim::playing_on(&g_player));
  CHECK(outputs_on(&g_animator).empty());  // its graph destroyed with it
}

void stopping_every_body_destroys_their_graphs() {
  give();
  anim::prepare_player(&g_player);
  anim::play_on_player(&g_player, &g_clipA, 0.2f);
  anim::stop_all_player_anims();
  CHECK(outputs_on(&g_animator).empty());
  CHECK(!anim::playing_on(&g_player));
}

void the_clip_reports_its_time_and_loop() {
  give();
  CHECK(anim::clip_seconds(&g_clipA) == 2.5f);
  CHECK(anim::clip_loops(&g_clipA));
  g_loops = false;
  CHECK(!anim::clip_loops(&g_clipA));
  anim::play_on_player(&g_player, &g_clipA, 0.2f);
  for (auto& [h, p] : g_playables)
    if (p.clip == &g_clipA) p.time = 3.75;
  CHECK(anim::clip_time(&g_player) == 3.75);
  CHECK(anim::clip_time(&g_clipB) < 0.0);  // not playing on it
  anim::stop_on_player(&g_player, 0.0f);
  anim::tick_player_anims(0.1f);
  CHECK(anim::clip_time(&g_player) < 0.0);
  // A clip played again starts from its start, though its playable is the same one.
  anim::play_on_player(&g_player, &g_clipA, 0.2f);
  CHECK(anim::clip_time(&g_player) == 0.0);
}

// A synced emote's second dancer joins part-way through the first dancer's dance: their clip starts there, wrapped to
// its length when it loops (2.5 s here), and at its start when it does not loop past its end.
void a_clip_starts_where_asked() {
  give();
  CHECK(anim::play_on_player(&g_player, &g_clipA, 0.2f, 6.0));
  CHECK(std::fabs(anim::clip_time(&g_player) - 1.0) < 1e-9);  // 6.0 wrapped to 2.5 s
  give();
  g_loops = false;
  CHECK(anim::play_on_player(&g_player, &g_clipA, 0.2f, 1.25));
  CHECK(std::fabs(anim::clip_time(&g_player) - 1.25) < 1e-9);
}

void nothing_plays_without_a_body() {
  give();
  CHECK(!anim::play_on_player(nullptr, &g_clipA, 0.2f));
  CHECK(!anim::play_on_player(&g_player, nullptr, 0.2f));
  CHECK(!anim::prepare_player(nullptr));
  Obj noAnimator;
  CHECK(!anim::play_on_player(&noAnimator, &g_clipA, 0.2f));
  CHECK(!anim::prepare_player(&noAnimator));
  CHECK(g_graphs.empty());
}

}  // namespace

int main() {
  a_clip_starts_where_asked();
  a_clip_blends_in_and_out();
  the_body_keeps_one_output_for_its_life();
  a_play_alone_prepares_the_body();
  a_second_play_crossfades();
  back_to_the_fading_clip_has_no_jump();
  a_play_while_blending_out_rises_from_there();
  a_destroyed_body_is_forgotten();
  stopping_every_body_destroys_their_graphs();
  the_clip_reports_its_time_and_loop();
  nothing_plays_without_a_body();
  anim::stop_all_player_anims();
  std::printf("%s\n", g_failures ? "player_anim tests FAILED" : "player_anim tests passed");
  return g_failures ? 1 : 0;
}
