#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "check.h"
#include "code.h"
#include "entry.h"
#include "image.h"
#include "model.h"
#include "resolver.h"
#include "sdk_fixture.h"
#include "sdk_image_fixture.h"

using namespace pm::sdk;

namespace {

Resolution run(std::vector<const EntryDesc*> entries, const GameModel& model) {
  return resolve(entries, model, nullptr);
}

bool has(const std::vector<std::string>& list, const std::string& item) {
  for (const std::string& s : list)
    if (s == item) return true;
  return false;
}

// GhostAI with a NavMeshAgent at 0x40 and its nested GhostState enum, and Physics with two Raycast
// overloads.
struct Game {
  ModelBuilder b;
  int ghostAI, ghostState, physics;
  Game() {
    ghostAI = b.type("Assembly-CSharp", "", "GhostAI");
    b.field(ghostAI, "agent", "NavMeshAgent", 0x40);
    ghostState = b.type("Assembly-CSharp", "", "GhostState", ghostAI, true);
    b.literal(ghostState, "idle", 0);
    b.literal(ghostState, "wander", 1);
    b.literal(ghostState, "hunting", 2);
    b.method(ghostAI, "ChangeState", "void", {"GhostState", "PhotonObjectInteract", "PhotonObjectInteract[]"},
             0x1000);
    b.method(ghostAI, "Awake", "void", {}, 0x1100);
    physics = b.type("UnityEngine.PhysicsModule", "UnityEngine", "Physics");
    b.method(physics, "Raycast", "bool", {"Ray", "RaycastHit&", "float", "int", "QueryTriggerInteraction"}, 0x2000,
             true);
    b.method(physics, "Raycast", "bool", {"Vector3", "Vector3", "RaycastHit&", "float", "int"}, 0x2100, true);
  }
};

constexpr EntryDesc kGhostAI{.key = "GhostAI", .name = "GhostAI", .assembly = "Assembly-CSharp", .ns = ""};
constexpr EntryDesc kGhostState{.key = "GhostState", .owner = "GhostAI", .name = "GhostState"};
constexpr EntryDesc kHunting{.key = "GhostState.hunting", .kind = Kind::EnumValue, .owner = "GhostState",
                             .name = "hunting"};
constexpr EntryDesc kAgent{.key = "GhostAI.agent", .kind = Kind::Field, .owner = "GhostAI", .name = "agent"};
constexpr const char* kChangeStateParams[] = {"@GhostState", "PhotonObjectInteract", "PhotonObjectInteract[]"};
constexpr EntryDesc kChangeState{.key = "GhostAI.ChangeState", .kind = Kind::Method, .owner = "GhostAI",
                                 .name = "ChangeState", .params = kChangeStateParams};
constexpr EntryDesc kPhysics{.key = "Physics", .name = "Physics", .assembly = "UnityEngine.PhysicsModule",
                             .ns = "UnityEngine"};
constexpr const char* kRayParams[] = {"Ray", "RaycastHit&", "float", "int", "QueryTriggerInteraction"};
constexpr EntryDesc kRaycastRay{.key = "Physics.Raycast", .kind = Kind::Method, .flags = kStatic,
                                .owner = "Physics", .name = "Raycast", .params = kRayParams};
constexpr EntryDesc kRaycastAny{.key = "Physics.RaycastAny", .kind = Kind::Method, .flags = kStatic,
                                .owner = "Physics", .name = "Raycast"};

void type_by_name() {
  Game g;
  const GameModel model = g.b.build();
  const Resolution r = run({&kGhostAI}, model);
  const Result* ghost = r.find("GhostAI");
  CHECK(ghost && ghost->status == Status::Resolved);
  CHECK(ghost && ghost->type == g.ghostAI);
  CHECK(ghost && ghost->desc == &kGhostAI);
  CHECK(ghost && ghost->currentName == "GhostAI");
}

// The models name UnityEvent and UnityEvent<T0> alike: by name both match, and kNotGeneric keeps the one that is no
// generic definition. It applies to a top-level type entry only, since the dump knows no nested type's arity.
void not_generic_type_among_its_generic_kin() {
  ModelBuilder b;
  const int plain = b.type("UnityEngine.CoreModule", "UnityEngine.Events", "UnityEvent");
  for (int arity = 1; arity <= 2; ++arity) {
    const int kin = b.type("UnityEngine.CoreModule", "UnityEngine.Events", "UnityEvent");
    b.model.types[kin].genericParams = arity;
  }
  const GameModel model = b.build();
  constexpr EntryDesc byName{.key = "UnityEvent", .name = "UnityEvent", .assembly = "UnityEngine.CoreModule",
                             .ns = "UnityEngine.Events"};
  CHECK(run({&byName}, model).find("UnityEvent")->status == Status::Ambiguous);
  constexpr EntryDesc plainOnly{.key = "UnityEvent", .flags = kNotGeneric, .name = "UnityEvent",
                                .assembly = "UnityEngine.CoreModule", .ns = "UnityEngine.Events"};
  const Result* r = run({&plainOnly}, model).find("UnityEvent");
  CHECK(r && r->status == Status::Resolved && r->type == plain);
  constexpr EntryDesc nested{.key = "Nested", .flags = kNotGeneric, .owner = "UnityEvent", .name = "Nested"};
  CHECK(run({&plainOnly, &nested}, model).find("Nested")->status == Status::Missing);
  constexpr EntryDesc member{.key = "UnityEvent.Invoke", .kind = Kind::Method, .flags = kNotGeneric,
                             .owner = "UnityEvent", .name = "Invoke"};
  CHECK(run({&plainOnly, &member}, model).find("UnityEvent.Invoke")->status == Status::Missing);
}

void method_by_name_and_params() {
  Game g;
  const GameModel model = g.b.build();
  const Resolution r = run({&kPhysics, &kRaycastRay}, model);
  const Result* ray = r.find("Physics.Raycast");
  CHECK(ray && ray->status == Status::Resolved);
  CHECK(ray && ray->rva == 0x2000);
  CHECK(ray && ray->type == g.physics && ray->member == 0);
}

// The running game tells System.Object[] (object[]) from UnityEngine.Object[] (Object[]). The dump writes both as Object[], so only
// a model read from the dump takes one for the other: an entry written from the dump alone must not resolve there and fail live.
void an_object_array_is_exact_in_the_game_and_alike_in_the_dump() {
  ModelBuilder b;
  const int view = b.type("PhotonUnityNetworking", "Photon.Pun", "PhotonView");
  b.method(view, "RPC", "void", {"string", "Object[]"}, 0x3000);
  GameModel dump = b.build();
  static constexpr EntryDesc kView{.key = "PhotonView", .name = "PhotonView", .assembly = "PhotonUnityNetworking", .ns = "Photon.Pun"};
  static constexpr const char* kSystem[] = {"string", "object[]"};
  static constexpr EntryDesc kSystemRpc{.key = "PhotonView.RPC", .kind = Kind::Method, .owner = "PhotonView", .name = "RPC", .params = kSystem};
  // In the game: the method takes UnityEngine.Object[], which is not what the entry asks for.
  Resolution r = run({&kView, &kSystemRpc}, dump);
  CHECK(r.find("PhotonView.RPC") && r.find("PhotonView.RPC")->status == Status::Missing);
  // From the dump: the spelling could be either.
  dump.objectArraysLookAlike = true;
  r = run({&kView, &kSystemRpc}, dump);
  CHECK(r.find("PhotonView.RPC") && r.find("PhotonView.RPC")->status == Status::Resolved);
  // In the game, the type as it is.
  ModelBuilder live;
  const int liveView = live.type("PhotonUnityNetworking", "Photon.Pun", "PhotonView");
  live.method(liveView, "RPC", "void", {"string", "object[]"}, 0x3000);
  r = run({&kView, &kSystemRpc}, live.build());
  CHECK(r.find("PhotonView.RPC") && r.find("PhotonView.RPC")->status == Status::Resolved);
}

void method_by_name_without_params_is_ambiguous_with_overloads() {
  Game g;
  const GameModel model = g.b.build();
  const Resolution r = run({&kPhysics, &kRaycastAny}, model);
  const Result* ray = r.find("Physics.RaycastAny");
  CHECK(ray && ray->status == Status::Ambiguous);
  CHECK(ray && ray->candidates.size() == 2);
  CHECK(ray && has(ray->candidates, "Raycast(Ray,RaycastHit&,float,int,QueryTriggerInteraction)"));
  CHECK(ray && has(ray->candidates, "Raycast(Vector3,Vector3,RaycastHit&,float,int)"));
  CHECK(ray && ray->rva == 0);
}

// Leaving out kStatic must not choose between a static and an instance overload.
void method_by_name_ignores_static_among_overloads() {
  ModelBuilder b;
  const int ai = b.type("Assembly-CSharp", "", "GhostAI");
  b.method(ai, "Foo", "void", {"Player"}, 0x3000, true);
  b.method(ai, "Foo", "void", {}, 0x3100);
  const GameModel model = b.build();
  constexpr EntryDesc foo{.key = "GhostAI.Foo", .kind = Kind::Method, .owner = "GhostAI", .name = "Foo"};
  const Resolution res = run({&kGhostAI, &foo}, model);
  const Result* r = res.find("GhostAI.Foo");
  CHECK(r && r->status == Status::Ambiguous);
  CHECK(r && r->candidates.size() == 2);
  CHECK(r && r->rva == 0);

  // Listed params still let static-ness filter.
  static constexpr const char* kPlayer[] = {"Player"};
  constexpr EntryDesc fooPlayer{.key = "GhostAI.FooPlayer", .kind = Kind::Method, .owner = "GhostAI",
                                .name = "Foo", .params = kPlayer};
  CHECK(run({&kGhostAI, &fooPlayer}, model).find("GhostAI.FooPlayer")->status == Status::Missing);
}

void field_by_name() {
  Game g;
  const GameModel model = g.b.build();
  const Resolution r = run({&kGhostAI, &kAgent}, model);
  const Result* agent = r.find("GhostAI.agent");
  CHECK(agent && agent->status == Status::Resolved);
  CHECK(agent && agent->offset == 0x40);
}

void enum_value_by_name() {
  Game g;
  const GameModel model = g.b.build();
  const Resolution r = run({&kGhostAI, &kGhostState, &kHunting}, model);
  const Result* state = r.find("GhostState");
  CHECK(state && state->status == Status::Resolved && state->type == g.ghostState);
  const Result* hunting = r.find("GhostState.hunting");
  CHECK(hunting && hunting->status == Status::Resolved);
  CHECK(hunting && hunting->value == 2);

  // A value of a type that is not an enum is not a value.
  constexpr EntryDesc notEnum{.key = "GhostAI.agentValue", .kind = Kind::EnumValue, .owner = "GhostAI",
                              .name = "agent"};
  const Resolution n = run({&kGhostAI, &notEnum}, model);
  CHECK(n.find("GhostAI.agentValue")->status == Status::Missing);
}

void at_type_resolves_through_entry() {
  Game g;
  const GameModel model = g.b.build();
  const Resolution r = run({&kGhostAI, &kGhostState, &kChangeState}, model);
  const Result* change = r.find("GhostAI.ChangeState");
  CHECK(change && change->status == Status::Resolved);
  CHECK(change && change->rva == 0x1000);

  // Where the method takes another type than the one the GhostState entry resolved to, it does not
  // match.
  ModelBuilder other;
  const int ai = other.type("Assembly-CSharp", "", "GhostAI");
  const int state = other.type("Assembly-CSharp", "", "GhostState", ai, true);
  other.literal(state, "idle", 0);
  other.type("Assembly-CSharp", "", "GhostMood", ai, true);
  other.method(ai, "ChangeState", "void", {"GhostMood", "PhotonObjectInteract", "PhotonObjectInteract[]"}, 0x1000);
  const GameModel otherModel = other.build();
  const Resolution o = run({&kGhostAI, &kGhostState, &kChangeState}, otherModel);
  CHECK(o.find("GhostAI.ChangeState")->status == Status::Missing);
}

// "@GhostState" means the type the entry resolved to, not any type of that name.
void at_type_is_the_resolved_type_not_its_name() {
  // Two GhostStates: the one nested in GhostAI, and another in its own namespace.
  const auto model_with = [](bool knownIndex, bool pointsAtOther, bool shareName) {
    ModelBuilder b;
    const int ai = b.type("Assembly-CSharp", "", "GhostAI");
    const int state = b.type("Assembly-CSharp", "", "GhostState", ai, true);
    const int other = b.type("Assembly-CSharp", "Other", shareName ? "GhostState" : "OtherState");
    const int target = pointsAtOther ? other : state;
    const std::string name = pointsAtOther && !shareName ? "OtherState" : "GhostState";
    b.method(ai, "ChangeState", "void", {name, "PhotonObjectInteract", "PhotonObjectInteract[]"}, 0x1000, false,
             {knownIndex ? target : -1, -1, -1});
    return b.build();
  };

  // A known index on a different, same-named type does not match.
  const GameModel elsewhere = model_with(true, true, true);
  CHECK(run({&kGhostAI, &kGhostState, &kChangeState}, elsewhere).find("GhostAI.ChangeState")->status ==
        Status::Missing);

  // A known index on the resolved type matches, though the name is shared.
  const GameModel known = model_with(true, false, true);
  const Resolution kr = run({&kGhostAI, &kGhostState, &kChangeState}, known);
  const Result* k = kr.find("GhostAI.ChangeState");
  CHECK(k && k->status == Status::Resolved && k->rva == 0x1000);

  // An unknown index with a shared name cannot be trusted.
  const GameModel unsure = model_with(false, false, true);
  const Resolution ur = run({&kGhostAI, &kGhostState, &kChangeState}, unsure);
  const Result* u = ur.find("GhostAI.ChangeState");
  CHECK(u && u->status == Status::Ambiguous);
  CHECK(u && u->detail.find("GhostState") != std::string::npos);
  CHECK(u && u->rva == 0);

  // An unknown index with a unique name falls back to the name.
  const GameModel unique = model_with(false, false, false);
  const Resolution nr = run({&kGhostAI, &kGhostState, &kChangeState}, unique);
  const Result* n = nr.find("GhostAI.ChangeState");
  CHECK(n && n->status == Status::Resolved && n->rva == 0x1000);

  // Fields follow the same rule.
  ModelBuilder f;
  const int ai = f.type("Assembly-CSharp", "", "GhostAI");
  f.type("Assembly-CSharp", "", "GhostState", ai, true);
  const int other = f.type("Assembly-CSharp", "Other", "GhostState");
  f.field(ai, "state", "GhostState", 0x48, false, other);
  const GameModel fields = f.build();
  constexpr EntryDesc state{.key = "GhostAI.state", .kind = Kind::Field, .owner = "GhostAI", .name = "state",
                            .type = "@GhostState"};
  CHECK(run({&kGhostAI, &kGhostState, &state}, fields).find("GhostAI.state")->status == Status::Missing);
}

void missing_owner_blocks_members() {
  Game g;
  const GameModel model = g.b.build();
  constexpr EntryDesc ghostless{.key = "GhostAI", .name = "NoSuchGhost", .assembly = "Assembly-CSharp", .ns = ""};
  const Resolution r = run({&ghostless, &kAgent, &kGhostState, &kHunting}, model);
  CHECK(r.find("GhostAI")->status == Status::Missing);
  for (const char* key : {"GhostAI.agent", "GhostState"}) {
    const Result* member = r.find(key);
    CHECK(member && member->status == Status::Blocked);
    CHECK(member && member->detail.find("GhostAI") != std::string::npos);
  }
  // Blocked passes down the chain: the value's owner is itself blocked.
  const Result* hunting = r.find("GhostState.hunting");
  CHECK(hunting && hunting->status == Status::Blocked);
  CHECK(hunting && hunting->detail.find("GhostState") != std::string::npos);
}

void dependency_order_is_independent_of_declaration_order() {
  Game g;
  const GameModel model = g.b.build();
  const Resolution forward = run({&kGhostAI, &kGhostState, &kHunting, &kAgent, &kChangeState}, model);
  const Resolution backward = run({&kChangeState, &kAgent, &kHunting, &kGhostState, &kGhostAI}, model);
  CHECK(forward.results.size() == backward.results.size());
  for (const Result& f : forward.results) {
    const Result* b = backward.find(f.desc->key);
    CHECK(b != nullptr);
    if (!b) continue;
    CHECK(f.status == Status::Resolved);
    CHECK(b->status == f.status);
    CHECK(b->type == f.type && b->member == f.member);
    CHECK(b->rva == f.rva && b->offset == f.offset && b->value == f.value);
  }
  // Results keep the order the entries were given in.
  CHECK(backward.results.front().desc == &kChangeState);
}

void cycle_is_missing() {
  Game g;
  const GameModel model = g.b.build();
  constexpr EntryDesc a{.key = "A", .owner = "B", .name = "GhostAI"};
  constexpr EntryDesc b{.key = "B", .owner = "A", .name = "GhostState"};
  constexpr EntryDesc c{.key = "C", .kind = Kind::Field, .owner = "A", .name = "agent"};
  const Resolution r = run({&a, &b, &c}, model);
  CHECK(r.find("A")->status == Status::Missing && r.find("A")->detail == "cycle");
  CHECK(r.find("B")->status == Status::Missing && r.find("B")->detail == "cycle");
  CHECK(r.find("C")->status == Status::Blocked);
}

void code_needs_an_image() {
  Game g;
  const GameModel model = g.b.build();
  static constexpr Fact facts[] = {{.is = Fact::Is::Calls, .method = "GhostAI.ChangeState"}};
  constexpr EntryDesc awake{.key = "GhostAI.Awake", .kind = Kind::Method, .owner = "GhostAI", .name = "Awake",
                            .facts = facts};
  const Resolution r = run({&kGhostAI, &kGhostState, &kChangeState, &awake}, model);
  CHECK(r.find("GhostAI.Awake")->status == Status::Missing);
  CHECK(r.find("GhostAI.Awake")->detail == "no code image");
}

void required_resolved_ignores_optional() {
  Game g;
  const GameModel model = g.b.build();
  constexpr EntryDesc optional{.key = "GhostAI.gone", .kind = Kind::Field, .flags = kOptional, .owner = "GhostAI",
                               .name = "gone"};
  const Resolution r = run({&kGhostAI, &kAgent, &optional}, model);
  CHECK(r.find("GhostAI.gone")->status == Status::Missing);
  CHECK(r.required_resolved());

  constexpr EntryDesc required{.key = "GhostAI.gone", .kind = Kind::Field, .owner = "GhostAI", .name = "gone"};
  CHECK(!run({&kGhostAI, &required}, model).required_resolved());
}

constexpr const char* kStateValues[] = {"idle", "wander", "hunting"};
constexpr EntryDesc kStateByValues{.key = "GhostState", .owner = "GhostAI", .hint = "ദശസറ", .values = kStateValues};

void enum_type_by_values() {
  ModelBuilder b;
  const int ai = b.type("Assembly-CSharp", "", "GhostAI");
  const int mood = b.type("Assembly-CSharp", "", "ബഹജഭ", ai, true);
  b.literal(mood, "calm", 0);
  b.literal(mood, "angry", 1);
  const int state = b.type("Assembly-CSharp", "", "ദശസറ", ai, true);
  b.literal(state, "idle", 0);
  b.literal(state, "wander", 1);
  b.literal(state, "hunting", 2);
  b.literal(state, "favouriteRoom", 3);
  const GameModel model = b.build();
  constexpr EntryDesc hunting{.key = "GhostState.hunting", .kind = Kind::EnumValue, .owner = "GhostState",
                              .name = "hunting"};
  const Resolution r = run({&kGhostAI, &kStateByValues, &hunting}, model);
  const Result* s = r.find("GhostState");
  CHECK(s && s->status == Status::Resolved && s->type == state);
  CHECK(s && s->currentName == "ദശസറ");
  CHECK(s && s->detail.empty());
  CHECK(r.find("GhostState.hunting")->value == 2);

  // A class that is not an enum is not found by values, nor an enum without values.
  ModelBuilder c;
  const int ai2 = c.type("Assembly-CSharp", "", "GhostAI");
  const int notEnum = c.type("Assembly-CSharp", "", "ദശസറ", ai2);
  c.literal(notEnum, "idle", 0);
  c.literal(notEnum, "wander", 1);
  c.literal(notEnum, "hunting", 2);
  CHECK(run({&kGhostAI, &kStateByValues}, c.build()).find("GhostState")->status == Status::Missing);
  constexpr EntryDesc anyNested{.key = "Nested", .owner = "GhostAI"};
  const Resolution n = run({&kGhostAI, &anyNested}, model);
  CHECK(n.find("Nested")->status == Status::Missing);
}

void type_by_field_types() {
  ModelBuilder b;
  const int ai = b.type("Assembly-CSharp", "", "GhostAI");
  const int a = b.type("Assembly-CSharp", "", "ബഹജഭ");
  b.field(a, "x", "GhostAI", 0x10, false, ai);
  b.field(a, "y", "float", 0x18);
  const int c = b.type("Assembly-CSharp", "", "ദശസറ");
  b.field(c, "x", "GhostAI", 0x10, false, ai);
  b.field(c, "y", "float", 0x18);
  b.field(c, "z", "float", 0x1c);
  const int d = b.type("Assembly-CSharp", "", "സറദബ");
  b.field(d, "x", "GhostAI", 0x10, true, ai);  // static, so not an instance field
  b.field(d, "y", "float", 0x18);
  b.field(d, "z", "float", 0x1c);
  b.type("Assembly-CSharp", "Other", "ജഭദശ");
  const GameModel model = b.build();

  // Each listed type needs a field of its own.
  static constexpr const char* kTwoFloats[] = {"@GhostAI", "float", "float"};
  constexpr EntryDesc twoFloats{.key = "Brain", .assembly = "Assembly-CSharp", .fieldTypes = kTwoFloats};
  const Resolution r = run({&kGhostAI, &twoFloats}, model);
  const Result* brain = r.find("Brain");
  CHECK(brain && brain->status == Status::Resolved && brain->type == c);

  static constexpr const char* kOneFloat[] = {"@GhostAI", "float"};
  constexpr EntryDesc oneFloat{.key = "Brain", .assembly = "Assembly-CSharp", .fieldTypes = kOneFloat};
  const Resolution o = run({&kGhostAI, &oneFloat}, model);
  const Result* any = o.find("Brain");
  CHECK(any && any->status == Status::Ambiguous && any->candidates.size() == 2);
}

// With kExactFields a type declares exactly the listed instance fields: a look-alike that declares one
// more is no candidate, statics and constants do not count, two exact matches stay ambiguous, and the
// flag fits only a type found by its field types.
void type_by_exact_field_types() {
  static constexpr const char* kWander[] = {"@GhostAI", "NavMeshAgent", "float", "int",
                                            "bool",     "Vector3",      "bool",  "bool"};
  constexpr EntryDesc wander{.key = "Wander", .flags = kExactFields, .assembly = "Assembly-CSharp", .ns = "",
                             .fieldTypes = kWander};
  constexpr EntryDesc loose{.key = "Wander", .assembly = "Assembly-CSharp", .ns = "", .fieldTypes = kWander};
  const auto add = [](ModelBuilder& b, const char* name, int ai, bool extraFloat) {
    const int t = b.type("Assembly-CSharp", "", name);
    b.field(t, "ghost", "GhostAI", 0x10, false, ai);
    b.field(t, "agent", "NavMeshAgent", 0x18);
    b.field(t, "time", "float", 0x20);
    b.field(t, "radius", "int", 0x24);
    b.field(t, "moving", "bool", 0x28);
    b.field(t, "to", "Vector3", 0x2C);
    if (extraFloat) b.field(t, "wait", "float", 0x38);
    b.field(t, "a", "bool", extraFloat ? 0x3C : 0x38);
    b.field(t, "b", "bool", extraFloat ? 0x3D : 0x39);
    return t;
  };
  {  // the exact class among a superset look-alike; a static and a constant do not count
    ModelBuilder b;
    const int ai = b.type("Assembly-CSharp", "", "GhostAI");
    const int superset = add(b, "ഹഫതണ", ai, true);
    const int exact = add(b, "ഝസബള", ai, false);
    b.field(exact, "shared", "int", 0, true);
    b.literal(exact, "limit", 3);
    const GameModel model = b.build();
    const Resolution r = run({&kGhostAI, &wander}, model);
    const Result* w = r.find("Wander");
    CHECK(w && w->status == Status::Resolved && w->type == exact);
    const Resolution l = run({&kGhostAI, &loose}, model);  // without the flag both fit
    CHECK(l.find("Wander") && l.find("Wander")->status == Status::Ambiguous);
    CHECK(superset != exact);
  }
  {  // the superset alone is no match
    ModelBuilder b;
    const int ai = b.type("Assembly-CSharp", "", "GhostAI");
    add(b, "ഹഫതണ", ai, true);
    const Resolution r = run({&kGhostAI, &wander}, b.build());
    CHECK(r.find("Wander") && r.find("Wander")->status == Status::Missing);
  }
  {  // two exact matches
    ModelBuilder b;
    const int ai = b.type("Assembly-CSharp", "", "GhostAI");
    add(b, "ഝസബള", ai, false);
    add(b, "ദശസറ", ai, false);
    const Resolution r = run({&kGhostAI, &wander}, b.build());
    const Result* w = r.find("Wander");
    CHECK(w && w->status == Status::Ambiguous && w->candidates.size() == 2);
  }
  {  // only a type found by its field types takes the flag
    ModelBuilder b;
    const int ai = b.type("Assembly-CSharp", "", "GhostAI");
    b.field(ai, "agent", "NavMeshAgent", 0x40);
    const GameModel model = b.build();
    constexpr EntryDesc named{.key = "Named", .flags = kExactFields, .name = "GhostAI",
                              .assembly = "Assembly-CSharp", .ns = ""};
    constexpr EntryDesc field{.key = "GhostAI.agent", .kind = Kind::Field, .flags = kExactFields,
                              .owner = "GhostAI", .name = "agent"};
    const Resolution r = run({&kGhostAI, &named, &field}, model);
    for (const char* key : {"Named", "GhostAI.agent"}) {
      const Result* x = r.find(key);
      CHECK(x && x->status == Status::Missing &&
            x->detail == "an exact field list applies to a type found by its field types");
    }
  }
}

// With kFieldOrder the exact fields also lie in the listed order by offset: a look-alike with the same
// fields reordered (a different size) is no candidate, and the flag needs kExactFields.
void type_by_field_order() {
  static constexpr const char* kScore[] = {"int", "double", "short"};
  constexpr EntryDesc ordered{.key = "Score", .flags = kExactFields | kFieldOrder, .assembly = "Assembly-CSharp",
                              .ns = "", .fieldTypes = kScore};
  constexpr EntryDesc unordered{.key = "Score", .flags = kExactFields, .assembly = "Assembly-CSharp", .ns = "",
                                .fieldTypes = kScore};
  constexpr EntryDesc alone{.key = "Score", .flags = kFieldOrder, .assembly = "Assembly-CSharp", .ns = "",
                            .fieldTypes = kScore};
  {  // declaration order does not matter, offsets do
    ModelBuilder b;
    const int t = b.type("Assembly-CSharp", "", "ഥഩവഹ");
    b.field(t, "c", "short", 0x20);
    b.field(t, "a", "int", 0x10);
    b.field(t, "b", "double", 0x18);
    const GameModel model = b.build();
    const Resolution r_s = run({&ordered}, model);
    const Result* s = r_s.find("Score");
    CHECK(s && s->status == Status::Resolved && s->type == t);
    const Resolution r_a = run({&alone}, model);
    const Result* a = r_a.find("Score");
    CHECK(a && a->status == Status::Missing && a->detail == "a field order applies to an exact field list");
  }
  {  // reordered look-alike: same fields, the two small ones together
    ModelBuilder b;
    const int t = b.type("Assembly-CSharp", "", "ഥഩവഹ");
    b.field(t, "a", "int", 0x10);
    b.field(t, "c", "short", 0x14);
    b.field(t, "b", "double", 0x18);
    const GameModel model = b.build();
    const Resolution r_s = run({&ordered}, model);
    const Result* s = r_s.find("Score");
    CHECK(s && s->status == Status::Missing);
    const Resolution r_u = run({&unordered}, model);
    const Result* u = r_u.find("Score");  // without the order it still fits
    CHECK(u && u->status == Status::Resolved);
  }
}

// A type shape that declares nothing to look for does not take the lone type in scope.
void type_shape_needs_values_or_field_types() {
  ModelBuilder b;
  const int ai = b.type("Assembly-CSharp", "", "GhostAI");
  b.type("Assembly-CSharp", "", "ദശസറ", ai);
  const GameModel model = b.build();
  constexpr EntryDesc nested{.key = "Nested", .owner = "GhostAI"};
  const Resolution r = run({&kGhostAI, &nested}, model);
  const Result* n = r.find("Nested");
  CHECK(n && n->status == Status::Missing);
  CHECK(n && n->detail == "a type found by shape needs values, field types, a static method's shape or a type that declares it");
}

// A static class has no fields to go by: a static method it declares finds it, and an instance method of that
// shape, or another return type, does not.
void type_found_by_a_static_method() {
  ModelBuilder b;
  const int yes = b.type("Assembly-CSharp", "", "ദശ");
  b.method(yes, "ബഹ", "int", {"T[]"}, 0x1000, true);
  const int instanceOnly = b.type("Assembly-CSharp", "", "സറ");
  b.method(instanceOnly, "ജഭ", "int", {"T[]"}, 0x1100);
  const int otherReturn = b.type("Assembly-CSharp", "", "ദബ");
  b.method(otherReturn, "ജദ", "void", {"T[]"}, 0x1200, true);
  const GameModel model = b.build();
  static constexpr const char* kArray[] = {"T[]"};
  constexpr EntryDesc shape{.key = "Helpers", .hint = "ദശ", .assembly = "Assembly-CSharp", .ns = "", .type = "int",
                            .params = kArray};
  const Resolution r = run({&shape}, model);
  const Result* h = r.find("Helpers");
  CHECK(h && h->status == Status::Resolved && h->currentName == "ദശ");
}

// Coroutine and closure classes are never shape candidates, whether the compiler's name survives or
// only their fields' names do.
void compiler_generated_types_are_not_shape_candidates() {
  static constexpr const char* kOwnerField[] = {"@GhostAI"};
  constexpr EntryDesc helper{.key = "Helper", .owner = "GhostAI", .fieldTypes = kOwnerField};
  const auto model_with = [](bool renamed, bool named, bool target) {
    ModelBuilder b;
    const int ai = b.type("Assembly-CSharp", "", "GhostAI");
    if (renamed) {
      const int co = b.type("Assembly-CSharp", "", "ജഷധഷഡബടഷഺ", ai);
      b.field(co, "<>1__state", "int", 0x10);
      b.field(co, "<>4__this", "GhostAI", 0x18, false, ai);
    }
    if (named) {
      const int co = b.type("Assembly-CSharp", "", "<Hunt>d__1", ai);
      b.field(co, "state", "int", 0x10);
      b.field(co, "owner", "GhostAI", 0x18, false, ai);
    }
    if (target) {
      const int t = b.type("Assembly-CSharp", "", "ദശസറ", ai);
      b.field(t, "ghost", "GhostAI", 0x10, false, ai);
    }
    return b.build();
  };
  CHECK(run({&kGhostAI, &helper}, model_with(true, false, false)).find("Helper")->status == Status::Missing);
  CHECK(run({&kGhostAI, &helper}, model_with(false, true, false)).find("Helper")->status == Status::Missing);
  const GameModel all = model_with(true, true, true);
  const Resolution r = run({&kGhostAI, &helper}, all);
  const Result* h = r.find("Helper");
  CHECK(h && h->status == Status::Resolved && h->type == 3);
  CHECK(h && h->currentName == "ദശസറ");

  // With kCompilerGenerated only those types are candidates: the renamed state machine, not the plain look-alike.
  constexpr EntryDesc machine{.key = "Machine", .flags = kCompilerGenerated, .owner = "GhostAI",
                              .fieldTypes = kOwnerField};
  const Resolution g = run({&kGhostAI, &machine}, model_with(true, false, true));
  const Result* m = g.find("Machine");
  CHECK(m && m->status == Status::Resolved && m->currentName == "ജഷധഷഡബടഷഺ");
  CHECK(run({&kGhostAI, &machine}, model_with(false, false, true)).find("Machine")->status == Status::Missing);
}

void field_by_unique_type() {
  Game g;
  g.b.field(g.ghostAI, "ദശ", "float", 0x48);
  g.b.field(g.ghostAI, "സറ", "NavMeshAgent", 0x50, true);
  const GameModel model = g.b.build();
  constexpr EntryDesc agent{.key = "GhostAI.agent", .kind = Kind::Field, .owner = "GhostAI", .type = "NavMeshAgent"};
  const Resolution r = run({&kGhostAI, &agent}, model);
  const Result* a = r.find("GhostAI.agent");
  CHECK(a && a->status == Status::Resolved);
  CHECK(a && a->offset == 0x40);
  CHECK(a && a->currentName == "agent");

  // The static one is found only as static.
  constexpr EntryDesc shared{.key = "GhostAI.shared", .kind = Kind::Field, .flags = kStatic, .owner = "GhostAI",
                             .type = "NavMeshAgent"};
  const Resolution s = run({&kGhostAI, &shared}, model);
  CHECK(s.find("GhostAI.shared")->status == Status::Resolved && s.find("GhostAI.shared")->offset == 0x50);

  // A field found by shape needs its type.
  constexpr EntryDesc untyped{.key = "GhostAI.untyped", .kind = Kind::Field, .owner = "GhostAI"};
  CHECK(run({&kGhostAI, &untyped}, model).find("GhostAI.untyped")->status == Status::Missing);
}

// "@Key" inside a generic's arguments means the type that entry resolved to, as at the start of a type
// string: List<@GhostState> is a list of exactly that type, and the field depends on its entry.
void at_type_inside_generic_arguments() {
  const auto model_with = [](bool shareName) {
    ModelBuilder b;
    const int ai = b.type("Assembly-CSharp", "", "GhostAI");
    const int state = b.type("Assembly-CSharp", "", "GhostState", ai, true);
    b.literal(state, "idle", 0);
    b.type("Assembly-CSharp", "Other", shareName ? "GhostState" : "OtherState");
    b.field(ai, "states", "List<GhostState>", 0x48);
    b.field(ai, "others", "List<OtherState>", 0x50);
    b.field(ai, "next", "Nullable<GhostState>", 0x58);
    b.field(ai, "weights", "Dictionary<GhostState,int>", 0x60);
    return b.build();
  };
  constexpr EntryDesc states{.key = "GhostAI.states", .kind = Kind::Field, .owner = "GhostAI",
                             .type = "List<@GhostState>"};
  constexpr EntryDesc next{.key = "GhostAI.next", .kind = Kind::Field, .owner = "GhostAI",
                           .type = "Nullable<@GhostState>"};
  constexpr EntryDesc weights{.key = "GhostAI.weights", .kind = Kind::Field, .owner = "GhostAI",
                              .type = "Dictionary<@GhostState, int>"};
  const GameModel model = model_with(false);
  const Resolution r = run({&kGhostAI, &kGhostState, &states, &next, &weights}, model);
  CHECK(r.find("GhostAI.states")->status == Status::Resolved && r.find("GhostAI.states")->offset == 0x48);
  CHECK(r.find("GhostAI.next")->status == Status::Resolved && r.find("GhostAI.next")->offset == 0x58);
  CHECK(r.find("GhostAI.weights")->status == Status::Resolved && r.find("GhostAI.weights")->offset == 0x60);

  // The field depends on the entry inside the generic: without it in the map, it is blocked.
  const Resolution alone = run({&kGhostAI, &states}, model);
  CHECK(alone.find("GhostAI.states")->status == Status::Blocked);
  CHECK(alone.find("GhostAI.states")->detail.find("GhostState") != std::string::npos);

  // A generic argument's type is known only by name, so a name another type shares cannot decide.
  const GameModel shared = model_with(true);
  const Resolution s = run({&kGhostAI, &kGhostState, &states}, shared);
  CHECK(s.find("GhostAI.states")->status == Status::Ambiguous);
  CHECK(s.find("GhostAI.states")->offset == 0);

  // The same holds in a type's field types.
  ModelBuilder t;
  const int ai = t.type("Assembly-CSharp", "", "GhostAI");
  t.type("Assembly-CSharp", "", "GhostState", ai, true);
  const int traits = t.type("Assembly-CSharp", "", "ടഷള");
  t.field(traits, "a", "List<GhostState>", 0x10);
  const int decoy = t.type("Assembly-CSharp", "", "ഫഹദ");
  t.field(decoy, "a", "List<int>", 0x10);
  const GameModel typed = t.build();
  static constexpr const char* kTraitsFields[] = {"List<@GhostState>"};
  static constexpr EntryDesc traitsEntry{.key = "Traits", .assembly = "Assembly-CSharp", .ns = "", .fieldTypes = kTraitsFields};
  const Resolution tr = run({&kGhostAI, &kGhostState, &traitsEntry}, typed);
  CHECK(tr.find("Traits")->status == Status::Resolved && tr.find("Traits")->currentName == "ടഷള");
}

void field_shape_with_many_candidates_is_ambiguous() {
  ModelBuilder b;
  b.type("Assembly-CSharp", "", "GhostAI");
  for (int i = 0; i < 13; ++i) b.field(0, "b" + std::to_string(i), "bool", 0x60 + i);
  b.field(0, "f", "float", 0x70);
  const GameModel model = b.build();
  constexpr EntryDesc flag{.key = "GhostAI.flag", .kind = Kind::Field, .owner = "GhostAI", .type = "bool"};
  const Resolution r = run({&kGhostAI, &flag}, model);
  const Result* f = r.find("GhostAI.flag");
  CHECK(f && f->status == Status::Ambiguous);
  CHECK(f && f->candidates.size() == 13);
  for (int i = 0; i < 13; ++i)
    CHECK(f && has(f->candidates, std::format("b{}: bool at {:#x}", i, 0x60 + i)));
  CHECK(f && f->offset == 0);
}

void stale_hint_never_decides() {
  ModelBuilder b;
  b.type("Assembly-CSharp", "", "GhostAI");
  b.field(0, "ദശസറ", "float", 0x40);
  b.field(0, "ബഹജഭ", "bool", 0x44);
  const GameModel model = b.build();
  constexpr EntryDesc flag{.key = "GhostAI.flag", .kind = Kind::Field, .owner = "GhostAI", .hint = "ദശസറ",
                           .type = "bool"};
  const Resolution r = run({&kGhostAI, &flag}, model);
  const Result* f = r.find("GhostAI.flag");
  CHECK(f && f->status == Status::Resolved);
  CHECK(f && f->offset == 0x44);
  CHECK(f && f->currentName == "ബഹജഭ");
  CHECK(f && f->detail == "renamed from ദശസറ");

  // With no field of the wanted type, the hint is not taken either.
  constexpr EntryDesc agent{.key = "GhostAI.agent", .kind = Kind::Field, .owner = "GhostAI", .hint = "ദശസറ",
                            .type = "NavMeshAgent"};
  const Resolution m = run({&kGhostAI, &agent}, model);
  CHECK(m.find("GhostAI.agent")->status == Status::Missing);
  CHECK(m.find("GhostAI.agent")->currentName.empty());
}

void hint_cannot_break_a_tie() {
  ModelBuilder b;
  b.type("Assembly-CSharp", "", "GhostAI");
  b.field(0, "ദശസറ", "bool", 0x40);
  b.field(0, "ബഹജഭ", "bool", 0x41);
  const GameModel model = b.build();
  constexpr EntryDesc flag{.key = "GhostAI.flag", .kind = Kind::Field, .owner = "GhostAI", .hint = "ദശസറ",
                           .type = "bool"};
  const Resolution r = run({&kGhostAI, &flag}, model);
  const Result* f = r.find("GhostAI.flag");
  CHECK(f && f->status == Status::Ambiguous);
  CHECK(f && f->candidates.size() == 2);
  CHECK(f && f->offset == 0 && f->currentName.empty());
}

void method_by_shape() {
  ModelBuilder b;
  const int ai = b.type("Assembly-CSharp", "", "GhostAI");
  b.method(ai, "ദശ", "void", {}, 0x1000);
  b.method(ai, "സറ", "void", {"float"}, 0x1100);
  b.method(ai, "ബഹ", "bool", {"float"}, 0x1200);
  b.method(ai, "ജഭ", "bool", {"float"}, 0x1300, true);
  b.method(ai, "ദബ", "void", {"float", "int"}, 0x1400);
  const GameModel model = b.build();

  // Empty params: takes no parameters.
  constexpr EntryDesc noArgs{.key = "GhostAI.Tick", .kind = Kind::Method, .owner = "GhostAI", .hint = "ദശ",
                             .type = "void"};
  const Resolution r = run({&kGhostAI, &noArgs}, model);
  const Result* tick = r.find("GhostAI.Tick");
  CHECK(tick && tick->status == Status::Resolved && tick->rva == 0x1000);
  CHECK(tick && tick->currentName == "ദശ" && tick->detail.empty());

  // Return type, every param in order, and static-ness all count.
  static constexpr const char* kFloat[] = {"float"};
  constexpr EntryDesc voidFloat{.key = "GhostAI.Set", .kind = Kind::Method, .owner = "GhostAI", .type = "void",
                                .params = kFloat};
  CHECK(run({&kGhostAI, &voidFloat}, model).find("GhostAI.Set")->rva == 0x1100);
  constexpr EntryDesc boolFloat{.key = "GhostAI.Test", .kind = Kind::Method, .owner = "GhostAI", .type = "bool",
                                .params = kFloat};
  CHECK(run({&kGhostAI, &boolFloat}, model).find("GhostAI.Test")->rva == 0x1200);
  constexpr EntryDesc staticBool{.key = "GhostAI.Check", .kind = Kind::Method, .flags = kStatic,
                                 .owner = "GhostAI", .type = "bool", .params = kFloat};
  CHECK(run({&kGhostAI, &staticBool}, model).find("GhostAI.Check")->rva == 0x1300);
  static constexpr const char* kIntFloat[] = {"int", "float"};
  constexpr EntryDesc swapped{.key = "GhostAI.Swapped", .kind = Kind::Method, .owner = "GhostAI", .type = "void",
                              .params = kIntFloat};
  CHECK(run({&kGhostAI, &swapped}, model).find("GhostAI.Swapped")->status == Status::Missing);

  // A method found by shape needs its return type.
  constexpr EntryDesc noRet{.key = "GhostAI.NoRet", .kind = Kind::Method, .owner = "GhostAI", .params = kFloat};
  CHECK(run({&kGhostAI, &noRet}, model).find("GhostAI.NoRet")->status == Status::Missing);
}

// Machine code for the fingerprint tests: functions placed in an image, and the model's methods at
// the same RVAs.
struct Code {
  ImageBuilder b;
  std::vector<uint8_t> mapped;

  void function(uint32_t rva, std::vector<uint8_t> bytes) { b.function(rva, bytes); }
  Resolution run(std::vector<const EntryDesc*> entries, const GameModel& model) {
    mapped = b.build();
    const CodeImage image(mapped);
    return resolve(entries, model, &image);
  }
};

// An instruction at `at` with a rel32 to target after `head`: a call, or a rip-relative operand.
std::vector<uint8_t> rel(uint64_t at, std::vector<uint8_t> head, uint64_t target) {
  const auto v = static_cast<uint32_t>(static_cast<int32_t>(target - (at + head.size() + 4)));
  for (int i = 0; i < 4; ++i) head.push_back(static_cast<uint8_t>(v >> (8 * i)));
  return head;
}

std::vector<uint8_t> join(std::initializer_list<std::vector<uint8_t>> parts) {
  std::vector<uint8_t> out;
  for (const auto& p : parts) out.insert(out.end(), p.begin(), p.end());
  return out;
}

const std::vector<uint8_t> kRet{0xC3};
const std::vector<uint8_t> kVzeroupper{0xC5, 0xF8, 0x77};  // VEX: the reader cannot account for it
const std::vector<uint8_t> kLoad40{0x48, 0x8B, 0x41, 0x40};  // mov rax, [rcx+0x40]
// mov [rdi+0xF3], bpl; mov byte ptr [rdi+0xD8], 1; movzx eax, byte ptr [rdi+0xE0]
const std::vector<uint8_t> kHuntingBody{0x40, 0x88, 0xAF, 0xF3, 0x00, 0x00, 0x00, 0xC6, 0x87, 0xD8, 0x00,
                                        0x00, 0x00, 0x01, 0x0F, 0xB6, 0x87, 0xE0, 0x00, 0x00, 0x00};

constexpr EntryDesc kHuntingMethod{.key = "GhostAI.Hunting", .kind = Kind::Method, .owner = "GhostAI",
                                   .name = "Hunting"};

// How Hunting's body differs between the tests.
enum class Hunting {
  Plain,
  Unreadable,   // it starts with an instruction the reader cannot account for
  OtherObject,  // it also stores 1 at 0xF3 of another object, loaded from a field of this
  OtherOnly,    // it also stores 2 at 0xF4 of another object, which this never does
  GlobalClass,  // it also reads 0xF4 of a class just loaded from a fixed address (a class-init check)
  GlobalAfterBranch,  // the same, with a branch landing between the load and the read
};

// GhostAI with four bools, and Hunting at 0x1000 which writes two of them and reads a third through
// rdi, a copy of this that it saves and restores. It also calls out, so rcx no longer holds this.
struct Bools {
  ModelBuilder m;
  Code code;
  explicit Bools(Hunting body = Hunting::Plain) {
    const int ai = m.type("Assembly-CSharp", "", "GhostAI");
    for (const int32_t offset : {0xD8, 0xE0, 0xF3, 0xF4}) m.field(ai, std::format("b{:x}", offset), "bool", offset);
    m.field(ai, "speed", "float", 0xF8);
    m.method(ai, "Hunting", "void", {}, 0x1000);
    const std::vector<uint8_t> lead = body == Hunting::Unreadable ? kVzeroupper : std::vector<uint8_t>{};
    const std::vector<uint8_t> prologue{0x57, 0x48, 0x8B, 0xF9};  // push rdi; mov rdi, rcx
    const std::vector<uint8_t> call = rel(0x1000 + lead.size() + prologue.size(), {0xE8}, 0x9000);
    // mov rax, [rdi+0x40]; mov byte ptr [rax+0xF3], 1
    const std::vector<uint8_t> other{0x48, 0x8B, 0x47, 0x40, 0xC6, 0x80, 0xF3, 0x00, 0x00, 0x00, 0x01};
    // mov rax, [rdi+0x40]; mov byte ptr [rax+0xF4], 2
    const std::vector<uint8_t> otherOnly{0x48, 0x8B, 0x47, 0x40, 0xC6, 0x80, 0xF4, 0x00, 0x00, 0x00, 0x02};
    // mov rax, [rip+0x10000]; mov rdx, [rdi+0x40]; cmp dword ptr [rax+0xF4], 0
    const std::vector<uint8_t> global{0x48, 0x8B, 0x05, 0x00, 0x00, 0x01, 0x00, 0x48, 0x8B, 0x57, 0x40,
                                      0x83, 0xB8, 0xF4, 0x00, 0x00, 0x00, 0x00};
    // The same with jmp +0 before the cmp: a branch lands on it, so rax may hold anything there.
    const std::vector<uint8_t> globalAfterBranch{0x48, 0x8B, 0x05, 0x00, 0x00, 0x01, 0x00, 0xEB, 0x00,
                                                 0x83, 0xB8, 0xF4, 0x00, 0x00, 0x00, 0x00};
    const std::vector<uint8_t> tail = body == Hunting::OtherObject         ? other
                                      : body == Hunting::OtherOnly         ? otherOnly
                                      : body == Hunting::GlobalClass       ? global
                                      : body == Hunting::GlobalAfterBranch ? globalAfterBranch
                                                                           : std::vector<uint8_t>{};
    code.function(0x1000, join({lead, prologue, call, kHuntingBody, tail, {0x5F}, kRet}));  // pop rdi; ret
  }
  // The bool with these facts. The entry lives as long as the results that point at it.
  const Result* flag(std::span<const Fact> facts) {
    entry = {.key = "GhostAI.flag", .kind = Kind::Field, .owner = "GhostAI", .type = "bool", .facts = facts};
    results = code.run({&kGhostAI, hunting, &entry}, m.build());
    return results.find("GhostAI.flag");
  }
  const EntryDesc* hunting = &kHuntingMethod;
  EntryDesc entry;
  Resolution results;
};

void stores_const_picks_cooldown() {
  Bools g;
  static constexpr Fact cooldown[] = {{.is = Fact::Is::StoresConst, .method = "GhostAI.Hunting", .value = 1}};
  const Result* f = g.flag(cooldown);
  CHECK(f && f->status == Status::Resolved);
  CHECK(f && f->offset == 0xD8);
  CHECK(f && f->currentName == "bd8");

  static constexpr Fact two[] = {{.is = Fact::Is::StoresConst, .method = "GhostAI.Hunting", .value = 2}};
  CHECK(g.flag(two)->status == Status::Missing);

  static constexpr Fact loads[] = {{.is = Fact::Is::Loads, .method = "GhostAI.Hunting"}};
  const Result* l = g.flag(loads);
  CHECK(l && l->status == Status::Resolved && l->offset == 0xE0);

  // Without facts the shape alone cannot choose.
  CHECK(g.flag({})->status == Status::Ambiguous);
  CHECK(g.flag({})->candidates.size() == 4);
}

void negated_fact_picks_flag() {
  Bools g;
  static constexpr Fact facts[] = {
      {.is = Fact::Is::Stores, .method = "GhostAI.Hunting"},
      {.is = Fact::Is::StoresConst, .method = "GhostAI.Hunting", .value = 1, .negate = true}};
  const Result* f = g.flag(facts);
  CHECK(f && f->status == Status::Resolved);
  CHECK(f && f->offset == 0xF3);

  // Neither stored nor loaded: the one left untouched.
  static constexpr Fact untouched[] = {{.is = Fact::Is::Stores, .method = "GhostAI.Hunting", .negate = true},
                                       {.is = Fact::Is::Loads, .method = "GhostAI.Hunting", .negate = true}};
  const Result* u = g.flag(untouched);
  CHECK(u && u->status == Status::Resolved && u->offset == 0xF4);
}

// An access through a register just loaded from a fixed address reads a runtime class (the class-init
// check every method makes), never an object: it counts for no field fact. With a branch landing
// between the load and the access the register may hold anything, and the access counts as before.
void global_class_access_is_no_field() {
  static constexpr Fact untouched[] = {{.is = Fact::Is::Stores, .method = "GhostAI.Hunting", .negate = true},
                                       {.is = Fact::Is::Loads, .method = "GhostAI.Hunting", .negate = true}};
  {
    Bools g(Hunting::GlobalClass);
    const Result* u = g.flag(untouched);
    CHECK(u && u->status == Status::Resolved && u->offset == 0xF4);
  }
  {
    Bools g(Hunting::GlobalAfterBranch);
    const Result* u = g.flag(untouched);
    CHECK(u && u->status == Status::Ambiguous);
    CHECK(u && u->detail.find("not proven to hold this") != std::string::npos);
  }
}

// A negated fact removes a candidate only on an access through this. Another object's field at the
// same offset leaves the candidate undecided.
void negated_fact_needs_this() {
  Bools g(Hunting::OtherObject);
  static constexpr Fact facts[] = {
      {.is = Fact::Is::Stores, .method = "GhostAI.Hunting"},
      {.is = Fact::Is::StoresConst, .method = "GhostAI.Hunting", .value = 1, .negate = true}};
  const Result* f = g.flag(facts);
  CHECK(f && f->status == Status::Ambiguous);
  CHECK(f && f->candidates.size() == 1 && f->offset == 0);
  CHECK(f && f->detail.find("not proven to hold this") != std::string::npos);
  CHECK(f && f->candidates.size() == 1 && f->candidates[0] == "bf3: bool at 0xf3");

  // A positive fact counts only this as well: the other object's store leaves 0xF3 undecided beside
  // 0xD8, which this stores 1 to.
  static constexpr Fact cooldown[] = {{.is = Fact::Is::StoresConst, .method = "GhostAI.Hunting", .value = 1}};
  const Result* c = g.flag(cooldown);
  CHECK(c && c->status == Status::Ambiguous && c->candidates.size() == 2);
}

// In an instance method of the field's owner, a positive field fact counts only an access through
// this. A store at the offset in another object, through rax, decides nothing, so after an update it
// cannot make a lone wrong match.
void positive_fact_needs_this_in_the_owners_method() {
  Bools g(Hunting::OtherOnly);
  static constexpr Fact two[] = {{.is = Fact::Is::StoresConst, .method = "GhostAI.Hunting", .value = 2}};
  const Result* f = g.flag(two);
  CHECK(f && f->status == Status::Ambiguous && f->offset == 0);
  CHECK(f && f->candidates.size() == 1 && f->candidates[0] == "bf4: bool at 0xf4");
  CHECK(f && f->detail.find("not proven to hold this") != std::string::npos);

  // Through rdi, a copy of this, a store decides.
  static constexpr Fact one[] = {{.is = Fact::Is::StoresConst, .method = "GhostAI.Hunting", .value = 1}};
  const Result* c = g.flag(one);
  CHECK(c && c->status == Status::Resolved && c->offset == 0xD8);

  // A static method has no this: its positive facts count any register, as cross-owner facts do,
  // which is why such a fact needs a second, independent one.
  static constexpr EntryDesc staticHunting{.key = "GhostAI.Hunting", .kind = Kind::Method, .flags = kStatic,
                                           .owner = "GhostAI", .name = "Hunting"};
  Bools s(Hunting::OtherOnly);
  s.m.model.types[0].methods[0].isStatic = true;
  s.hunting = &staticHunting;
  const Result* weak = s.flag(two);
  CHECK(weak && weak->status == Status::Resolved && weak->offset == 0xF4);
}

// rcx is the field owner's `this` only in an instance method of that owner that returns nothing
// through a hidden buffer. Otherwise a negated fact has no access through this to remove on.
void negated_fact_needs_the_owners_instance_method() {
  static constexpr Fact facts[] = {
      {.is = Fact::Is::Stores, .method = "GhostAI.Hunting"},
      {.is = Fact::Is::StoresConst, .method = "GhostAI.Hunting", .value = 1, .negate = true}};
  static constexpr EntryDesc staticHunting{.key = "GhostAI.Hunting", .kind = Kind::Method, .flags = kStatic,
                                           .owner = "GhostAI", .name = "Hunting"};
  const auto with = [](auto change) {
    Bools g;
    change(g.m.model.types[0].methods[0]);
    if (g.m.model.types[0].methods[0].isStatic) g.hunting = &staticHunting;
    const Result* f = g.flag(facts);
    CHECK(f && g.results.find("GhostAI.Hunting")->status == Status::Resolved);
    return std::pair(f ? f->status : Status::Missing, f ? f->detail : std::string());
  };
  // A static method's rcx is its first argument.
  const auto [staticStatus, staticDetail] = with([](ModelMethod& m) { m.isStatic = true; });
  CHECK(staticStatus == Status::Ambiguous);
  CHECK(staticDetail.find("not proven to hold this") != std::string::npos);
  // A struct returned through a hidden buffer takes rcx, and this moves to rdx.
  CHECK(with([](ModelMethod& m) { m.ret = "Vector3"; }).first == Status::Ambiguous);
  CHECK(with([](ModelMethod& m) { m.ret = "decimal"; }).first == Status::Ambiguous);
  // Values that come back in a register leave rcx alone.
  CHECK(with([](ModelMethod& m) { m.ret = "int"; }).first == Status::Resolved);
  CHECK(with([](ModelMethod& m) { m.ret = "GameObject[]"; }).first == Status::Resolved);
  CHECK(with([](ModelMethod& m) { m.ret = "string"; }).first == Status::Resolved);

  // A method of another type has another layout.
  Bools g;
  g.m.model.types[0].methods[0].name = "Other";
  const int ev = g.m.type("Assembly-CSharp", "", "GhostEvent");
  g.m.method(ev, "Hunting", "void", {}, 0x1000);
  constexpr EntryDesc event{.key = "GhostEvent", .name = "GhostEvent", .assembly = "Assembly-CSharp", .ns = ""};
  constexpr EntryDesc hunting{.key = "GhostEvent.Hunting", .kind = Kind::Method, .owner = "GhostEvent",
                              .name = "Hunting"};
  static constexpr Fact eventFacts[] = {
      {.is = Fact::Is::Stores, .method = "GhostEvent.Hunting"},
      {.is = Fact::Is::StoresConst, .method = "GhostEvent.Hunting", .value = 1, .negate = true}};
  constexpr EntryDesc flag{.key = "GhostAI.flag", .kind = Kind::Field, .owner = "GhostAI", .type = "bool",
                           .facts = eventFacts};
  const Resolution r = g.code.run({&kGhostAI, &event, &hunting, &flag}, g.m.build());
  CHECK(r.find("GhostEvent.Hunting")->status == Status::Resolved);
  CHECK(r.find("GhostAI.flag")->status == Status::Ambiguous);
}

void fact_on_unresolved_method_blocks() {
  Bools g;
  static constexpr Fact facts[] = {{.is = Fact::Is::StoresConst, .method = "GhostAI.Gone", .value = 1}};
  constexpr EntryDesc gone{.key = "GhostAI.Gone", .kind = Kind::Method, .owner = "GhostAI", .name = "Gone"};
  constexpr EntryDesc flag{.key = "GhostAI.flag", .kind = Kind::Field, .owner = "GhostAI", .type = "bool",
                           .facts = facts};
  const Resolution r = g.code.run({&kGhostAI, &gone, &flag}, g.m.build());
  CHECK(r.find("GhostAI.Gone")->status == Status::Missing);
  const Result* f = r.find("GhostAI.flag");
  CHECK(f && f->status == Status::Blocked);
  CHECK(f && f->detail.find("GhostAI.Gone") != std::string::npos);
}

// A fact names a method entry, and its kind fits the entry: Stores, StoresConst and Loads are about
// fields, Calls and CalledBy about methods.
void fact_kinds_must_fit() {
  Bools g;
  static constexpr Fact onField[] = {{.is = Fact::Is::Stores, .method = "GhostAI.b"}};
  constexpr EntryDesc b{.key = "GhostAI.b", .kind = Kind::Field, .owner = "GhostAI", .name = "bd8"};
  constexpr EntryDesc flag{.key = "GhostAI.flag", .kind = Kind::Field, .owner = "GhostAI", .type = "bool",
                           .facts = onField};
  const Resolution r = g.code.run({&kGhostAI, &b, &flag}, g.m.build());
  CHECK(r.find("GhostAI.flag")->status == Status::Missing);
  CHECK(r.find("GhostAI.flag")->detail.find("not a method entry") != std::string::npos);

  static constexpr Fact calls[] = {{.is = Fact::Is::Calls, .method = "GhostAI.Hunting"}};
  CHECK(g.flag(calls)->status == Status::Missing);
  static constexpr Fact stores[] = {{.is = Fact::Is::Stores, .method = "GhostAI.Hunting"}};
  constexpr EntryDesc method{.key = "GhostAI.method", .kind = Kind::Method, .owner = "GhostAI", .type = "void",
                             .facts = stores};
  const Resolution m = g.code.run({&kGhostAI, &kHuntingMethod, &method}, g.m.build());
  CHECK(m.find("GhostAI.method")->status == Status::Missing);

  static constexpr Fact noMethod[] = {{.is = Fact::Is::StoresConst, .method = nullptr, .value = 1}};
  const Result* n = g.flag(noMethod);
  CHECK(n && n->status == Status::Missing && n->detail == "a fact needs a method");
}

// A struct's own methods address its fields at the offset minus 0x10, so a field fact on a struct
// decides nothing: the entry stays Ambiguous, and `this` is never taken to be the struct.
void field_facts_on_a_struct_fail_closed() {
  Bools g;
  g.m.model.types[0].isValueType = true;
  static constexpr Fact cooldown[] = {{.is = Fact::Is::StoresConst, .method = "GhostAI.Hunting", .value = 1}};
  const Result* f = g.flag(cooldown);
  CHECK(f && f->status == Status::Ambiguous);
  CHECK(f && f->offset == 0);
  CHECK(f && f->detail.find("field facts on a struct are not supported") != std::string::npos);

  static constexpr Fact untouched[] = {{.is = Fact::Is::Stores, .method = "GhostAI.Hunting", .negate = true}};
  const Result* u = g.flag(untouched);
  CHECK(u && u->status == Status::Ambiguous && u->offset == 0);

  // Without a fact the struct's field is chosen as before, by shape.
  ModelBuilder m;
  const int pair = m.type("Assembly-CSharp", "", "GhostAI", -1, false, true);
  m.field(pair, "agent", "NavMeshAgent", 0x40);
  constexpr EntryDesc agent{.key = "GhostAI.agent", .kind = Kind::Field, .owner = "GhostAI", .type = "NavMeshAgent"};
  const Resolution r = run({&kGhostAI, &agent}, m.build());
  CHECK(r.find("GhostAI.agent")->status == Status::Resolved);
}

// A class that embeds a struct at 0x28 whose first two fields are of one enum type, as GhostController
// embeds its traits: Attempt reads the first through this, Sync writes the second.
struct Embedded {
  ModelBuilder m;
  Code code;
  int controller = -1;
  explicit Embedded(const char* holderType = "Traits", bool holderIsStruct = true) {
    controller = m.type("Assembly-CSharp", "", "Controller");
    const int traits = m.type("Assembly-CSharp", "", "Traits", -1, false, holderIsStruct);
    const int kind = m.type("Assembly-CSharp", "", "Kind", -1, true, true);
    m.literal(kind, "None", 24);
    m.field(controller, "view", "PhotonView", 0x20);
    m.field(controller, "ഢധഢ", holderType, 0x28, false, std::string_view(holderType) == "Traits" ? traits : -1);
    m.field(controller, "count", "int", 0x68);
    m.field(traits, "ഫഠ഻", "Kind", 0x10, false, kind);  // the struct's own offsets include the boxed header
    m.field(traits, "നബട", "Kind", 0x14, false, kind);
    m.field(traits, "ഺഹഴ", "int", 0x18);
    m.field(controller, "shared", "Traits", 0x0, true, traits);
    m.field(controller, "mood", "Kind", 0x70, false, kind);
    const int info = m.type("Assembly-CSharp", "", "Info");
    const int other = m.type("Assembly-CSharp", "", "Other", -1, false, true);
    m.field(info, "ഢധഢ", "Traits", 0x30, false, traits);
    m.field(info, "ഴരന", "Other", 0x40, false, other);
    m.field(other, "ഫഠ഻", "Kind", 0x10, false, kind);
    m.field(other, "നബട", "Kind", 0x14, false, kind);
    m.method(controller, "Attempt", "void", {}, 0x1000);
    m.method(controller, "Sync", "void", {}, 0x1100);
    code.function(0x1000, join({{0x8B, 0x41, 0x28}, kRet}));  // mov eax, [rcx+0x28]; ret
    code.function(0x1100, join({{0x89, 0x51, 0x2C}, kRet}));  // mov [rcx+0x2C], edx; ret
  }
  const Result* field(const EntryDesc& entry, bool withHolder = true, std::vector<const EntryDesc*> more = {}) {
    std::vector<const EntryDesc*> entries{&kController, &kTraits, &kKind, &kInfo, &kOther, &kAttempt, &kSync, &entry};
    if (withHolder) entries.push_back(&kHolder);
    entries.insert(entries.end(), more.begin(), more.end());
    results = code.run(entries, m.build());
    return results.find(entry.key);
  }
  static constexpr EntryDesc kController{.key = "Controller", .name = "Controller", .assembly = "Assembly-CSharp",
                                         .ns = ""};
  static constexpr EntryDesc kTraits{.key = "Traits", .name = "Traits", .assembly = "Assembly-CSharp", .ns = ""};
  static constexpr EntryDesc kKind{.key = "Kind", .name = "Kind", .assembly = "Assembly-CSharp", .ns = ""};
  static constexpr EntryDesc kInfo{.key = "Info", .name = "Info", .assembly = "Assembly-CSharp", .ns = ""};
  static constexpr EntryDesc kOther{.key = "Other", .name = "Other", .assembly = "Assembly-CSharp", .ns = ""};
  static constexpr EntryDesc kHolder{.key = "Controller.traits", .kind = Kind::Field, .owner = "Controller",
                                     .type = "@Traits"};
  static constexpr EntryDesc kAttempt{.key = "Controller.Attempt", .kind = Kind::Method, .owner = "Controller",
                                      .name = "Attempt"};
  static constexpr EntryDesc kSync{.key = "Controller.Sync", .kind = Kind::Method, .owner = "Controller",
                                   .name = "Sync"};
  Resolution results;
};

// A struct's own instance field (an entry with a struct owner and no `within`) is published at its
// place in the struct, as a method that receives the struct by address reads it: the model's offset
// less the boxed header. A struct's static field keeps its offset.
void struct_field_is_published_at_its_place_in_the_struct() {
  static constexpr EntryDesc count{.key = "Traits.count", .kind = Kind::Field, .owner = "Traits", .name = "ഺഹഴ"};
  Embedded g;
  const Result* c = g.field(count, false);
  CHECK(c && c->status == Status::Resolved && c->offset == 0x8);
  ModelBuilder m;
  const int pair = m.type("Assembly-CSharp", "", "Traits", -1, false, true);
  m.field(pair, "shared", "int", 0x4, true);
  static constexpr EntryDesc shared{.key = "Traits.shared", .kind = Kind::Field, .flags = pm::sdk::kStatic,
                                    .owner = "Traits", .name = "shared"};
  const Resolution r = run({&Embedded::kTraits, &shared}, m.build());
  CHECK(r.find("Traits.shared")->status == Status::Resolved && r.find("Traits.shared")->offset == 0x4);
}

void field_within_an_embedded_struct() {
  // A fact in the owner's code, at the struct field's place in the owner, decides; the entry reports
  // that place, as a field of the owner with no member of its own.
  static constexpr Fact read[] = {{.is = Fact::Is::Loads, .method = "Controller.Attempt"}};
  static constexpr EntryDesc identity{.key = "Controller.identity", .kind = Kind::Field, .owner = "Controller",
                                      .type = "@Kind", .facts = read, .within = "Controller.traits"};
  Embedded g;
  const Result* i = g.field(identity);
  CHECK(i && i->status == Status::Resolved && i->offset == 0x28);
  CHECK(i && i->type == g.controller && i->member == -1);
  CHECK(i && i->currentName == "ഫഠ഻");
  static constexpr Fact written[] = {{.is = Fact::Is::Stores, .method = "Controller.Sync"}};
  static constexpr EntryDesc behaviour{.key = "Controller.behaviour", .kind = Kind::Field, .owner = "Controller",
                                       .type = "@Kind", .facts = written, .within = "Controller.traits"};
  const Result* b = g.field(behaviour);
  CHECK(b && b->status == Status::Resolved && b->offset == 0x2C);

  // Without a fact the struct's two fields of that type are both candidates, at their places in the
  // owner.
  static constexpr EntryDesc either{.key = "Controller.either", .kind = Kind::Field, .owner = "Controller",
                                    .type = "@Kind", .within = "Controller.traits"};
  const Result* e = g.field(either);
  CHECK(e && e->status == Status::Ambiguous && e->offset == 0);
  CHECK(e && has(e->candidates, "ഫഠ഻: Kind at 0x28") && has(e->candidates, "നബട: Kind at 0x2c"));

  // No field of that type in the struct: missing.
  static constexpr EntryDesc none{.key = "Controller.none", .kind = Kind::Field, .owner = "Controller",
                                  .type = "float", .within = "Controller.traits"};
  CHECK(g.field(none)->status == Status::Missing);

  // The holding field is not in the map: blocked. Not a struct: missing.
  CHECK(g.field(identity, false)->status == Status::Blocked);
  Embedded notStruct("Traits", false);
  const Result* n = notStruct.field(identity);
  CHECK(n && n->status == Status::Missing && n->detail.find("struct") != std::string::npos);

  // A fact that matches neither place leaves nothing.
  static constexpr Fact stored[] = {{.is = Fact::Is::Stores, .method = "Controller.Attempt"}};
  static constexpr EntryDesc stores{.key = "Controller.stores", .kind = Kind::Field, .owner = "Controller",
                                    .type = "@Kind", .facts = stored, .within = "Controller.traits"};
  CHECK(g.field(stores)->status == Status::Missing);
}

// Every holder a field within a struct cannot use fails closed, and a field within one may be named.
void within_fails_closed_on_any_other_holder() {
  Embedded g;
  static constexpr EntryDesc staticHolder{.key = "Controller.shared", .kind = Kind::Field, .flags = kStatic,
                                          .owner = "Controller", .type = "@Traits"};
  static constexpr EntryDesc inStatic{.key = "Controller.inStatic", .kind = Kind::Field, .owner = "Controller",
                                      .type = "@Kind", .within = "Controller.shared"};
  CHECK(g.field(inStatic, true, {&staticHolder})->status == Status::Missing);
  CHECK(g.results.find("Controller.shared")->status == Status::Resolved);

  // A holder that is itself a field within a struct (resolved here by its fact).
  static constexpr Fact read[] = {{.is = Fact::Is::Loads, .method = "Controller.Attempt"}};
  static constexpr EntryDesc first{.key = "Controller.first", .kind = Kind::Field, .owner = "Controller",
                                   .type = "@Kind", .facts = read, .within = "Controller.traits"};
  static constexpr EntryDesc nested{.key = "Controller.nested", .kind = Kind::Field, .owner = "Controller",
                                    .type = "@Kind", .within = "Controller.first"};
  CHECK(g.field(nested, true, {&first})->status == Status::Missing);
  CHECK(g.results.find("Controller.first")->status == Status::Resolved);

  static constexpr EntryDesc elsewhere{.key = "Info.elsewhere", .kind = Kind::Field, .owner = "Info",
                                       .type = "@Kind", .within = "Controller.traits"};
  const Result* e = g.field(elsewhere);
  CHECK(e && e->status == Status::Missing && e->detail.find("is not a field of Info") != std::string::npos);

  static constexpr EntryDesc enumHolder{.key = "Controller.mood", .kind = Kind::Field, .owner = "Controller",
                                        .type = "@Kind"};
  static constexpr EntryDesc inEnum{.key = "Controller.inEnum", .kind = Kind::Field, .owner = "Controller",
                                    .type = "@Kind", .within = "Controller.mood"};
  CHECK(g.field(inEnum, true, {&enumHolder})->status == Status::Missing);
  CHECK(g.results.find("Controller.mood")->status == Status::Resolved);

  // By name, the struct's own field name decides; a name it lacks is missing.
  static constexpr EntryDesc named{.key = "Controller.named", .kind = Kind::Field, .owner = "Controller",
                                   .name = "നബട", .within = "Controller.traits"};
  const Result* n = g.field(named);
  CHECK(n && n->status == Status::Resolved && n->offset == 0x2C);
  static constexpr EntryDesc unnamed{.key = "Controller.unnamed", .kind = Kind::Field, .owner = "Controller",
                                     .name = "nope", .within = "Controller.traits"};
  CHECK(g.field(unnamed)->status == Status::Missing);
}

// sameFieldAs: the struct member another within entry resolved to, in a struct of the same type held
// by another class, without a fact of its own.
void same_field_as_another_within_entry() {
  static constexpr Fact read[] = {{.is = Fact::Is::Loads, .method = "Controller.Attempt"}};
  static constexpr EntryDesc identity{.key = "Controller.identity", .kind = Kind::Field, .owner = "Controller",
                                      .type = "@Kind", .facts = read, .within = "Controller.traits"};
  static constexpr EntryDesc infoTraits{.key = "Info.traits", .kind = Kind::Field, .owner = "Info",
                                        .type = "@Traits"};
  static constexpr EntryDesc infoIdentity{.key = "Info.identity", .kind = Kind::Field, .owner = "Info",
                                          .type = "@Kind", .within = "Info.traits",
                                          .sameFieldAs = "Controller.identity"};
  Embedded g;
  const Result* i = g.field(infoIdentity, true, {&identity, &infoTraits});
  CHECK(i && i->status == Status::Resolved && i->offset == 0x30);
  CHECK(i && i->member == -1 && i->inner == 0 && i->currentName == "ഫഠ഻");
  CHECK(g.results.find("Controller.identity")->inner == 0);

  // A struct of another type, though its field has the same name and shape: missing.
  static constexpr EntryDesc infoOther{.key = "Info.other", .kind = Kind::Field, .owner = "Info", .type = "@Other"};
  static constexpr EntryDesc otherIdentity{.key = "Info.otherIdentity", .kind = Kind::Field, .owner = "Info",
                                           .type = "@Kind", .within = "Info.other",
                                           .sameFieldAs = "Controller.identity"};
  const Result* o = g.field(otherIdentity, true, {&identity, &infoOther});
  CHECK(o && o->status == Status::Missing && o->detail.find("another struct type") != std::string::npos);

  // While the target is unresolved (no fact decides it): blocked.
  static constexpr EntryDesc either{.key = "Controller.identity", .kind = Kind::Field, .owner = "Controller",
                                    .type = "@Kind", .within = "Controller.traits"};
  CHECK(g.field(infoIdentity, true, {&either, &infoTraits})->status == Status::Blocked);

  // A target that is not a field within a struct, or an entry that is not one itself: missing.
  static constexpr EntryDesc plainTarget{.key = "Info.plain", .kind = Kind::Field, .owner = "Info",
                                         .type = "@Kind", .within = "Info.traits", .sameFieldAs = "Controller.traits"};
  CHECK(g.field(plainTarget, true, {&infoTraits})->status == Status::Missing);
  static constexpr EntryDesc notWithin{.key = "Controller.count2", .kind = Kind::Field, .owner = "Controller",
                                       .type = "int", .sameFieldAs = "Controller.identity"};
  CHECK(g.field(notWithin, true, {&identity})->status == Status::Missing);
}

// Fail closed: facts read from code the reader could not fully account for never decide.
void unreadable_code_never_decides() {
  Bools g(Hunting::Unreadable);
  static constexpr Fact cooldown[] = {{.is = Fact::Is::StoresConst, .method = "GhostAI.Hunting", .value = 1}};
  const Result* f = g.flag(cooldown);
  CHECK(f && f->status == Status::Ambiguous);
  CHECK(f && f->detail.find("could not be fully read") != std::string::npos);
  CHECK(f && f->offset == 0);

  // A lone candidate whose own code is unreadable is not taken on a Calls fact either.
  ModelBuilder m;
  const int ai = m.type("Assembly-CSharp", "", "GhostAI");
  m.method(ai, "Helper", "int", {}, 0x1000);
  m.method(ai, "ദശ", "void", {}, 0x1100);
  m.method(ai, "സറ", "void", {}, 0x1200);
  Code code;
  code.function(0x1000, kRet);
  code.function(0x1100, join({kVzeroupper, rel(0x1103, {0xE8}, 0x1000), kRet}));
  code.function(0x1200, join({kLoad40, kRet}));
  constexpr EntryDesc helper{.key = "GhostAI.Helper", .kind = Kind::Method, .owner = "GhostAI", .name = "Helper"};
  static constexpr Fact calls[] = {{.is = Fact::Is::Calls, .method = "GhostAI.Helper"}};
  constexpr EntryDesc caller{.key = "GhostAI.Caller", .kind = Kind::Method, .owner = "GhostAI", .type = "void",
                             .facts = calls};
  const Resolution r = code.run({&kGhostAI, &helper, &caller}, m.build());
  const Result* c = r.find("GhostAI.Caller");
  CHECK(c && c->status == Status::Ambiguous);
  CHECK(c && c->detail.find("could not be fully read") != std::string::npos);
  CHECK(c && c->candidates.size() == 1 && c->rva == 0);
}

void folded_alias_is_one_behaviour() {
  ModelBuilder m;
  const int ai = m.type("Assembly-CSharp", "", "GhostAI");
  m.method(ai, "Init", "void", {"float"}, 0x1000);
  m.method(ai, "ഞത", "void", {"float"}, 0x1000);
  Code code;
  code.function(0x1000, join({kLoad40, kRet}));
  static constexpr const char* kFloat[] = {"float"};
  constexpr EntryDesc init{.key = "GhostAI.Init", .kind = Kind::Method, .owner = "GhostAI", .name = "Init",
                           .params = kFloat};
  const Resolution r = code.run({&kGhostAI, &init}, m.build());
  const Result* i = r.find("GhostAI.Init");
  CHECK(i && i->status == Status::Resolved && i->rva == 0x1000);
  CHECK(i && i->currentName == "Init");
  CHECK(i && i->aliases == std::vector<std::string>{"ഞത"});
  CHECK(i && i->twins.empty());

  // Found by shape, both are candidates, and they are one behaviour.
  constexpr EntryDesc shape{.key = "GhostAI.Init", .kind = Kind::Method, .owner = "GhostAI", .type = "void",
                            .params = kFloat};
  const Resolution s = code.run({&kGhostAI, &shape}, m.build());
  const Result* h = s.find("GhostAI.Init");
  CHECK(h && h->status == Status::Resolved && h->rva == 0x1000);
  CHECK(h && h->aliases.size() == 1 && h->twins.empty());
}

// rva 0 is no address (abstract, extern, or code one model places elsewhere): two methods there are
// no aliases, and the entry stays Ambiguous.
void zero_rva_is_never_an_alias() {
  ModelBuilder m;
  const int ai = m.type("Assembly-CSharp", "", "GhostAI");
  m.method(ai, "ഞത", "void", {"float"}, 0);
  m.method(ai, "ദശ", "void", {"float"}, 0);
  Code code;
  static constexpr const char* kFloat[] = {"float"};
  constexpr EntryDesc shape{.key = "GhostAI.Init", .kind = Kind::Method, .owner = "GhostAI", .type = "void",
                            .params = kFloat};
  const Resolution s = code.run({&kGhostAI, &shape}, m.build());
  const Result* r = s.find("GhostAI.Init");
  CHECK(r && r->status == Status::Ambiguous && r->candidates.size() == 2);
  CHECK(r && r->aliases.empty() && r->rva == 0);
}

static constexpr const char* kPlayer[] = {"Player"};
constexpr EntryDesc kLookAtPlayer{.key = "GhostAI.LookAtPlayer", .kind = Kind::Method, .owner = "GhostAI",
                                  .name = "LookAtPlayer", .params = kPlayer};

// A method with no code of its own (abstract, extern, a shared generic, or code outside the module)
// has rva 0. That is no address to call or hook, so the entry is Missing.
void method_without_code_is_missing() {
  ModelBuilder m;
  const int ai = m.type("Assembly-CSharp", "", "GhostAI");
  m.method(ai, "Abstract", "void", {}, 0);
  constexpr EntryDesc e{.key = "GhostAI.Abstract", .kind = Kind::Method, .owner = "GhostAI", .name = "Abstract"};
  Code code;
  for (const Resolution& r : {run({&kGhostAI, &e}, m.build()), code.run({&kGhostAI, &e}, m.build())}) {
    const Result* a = r.find("GhostAI.Abstract");
    CHECK(a && a->status == Status::Missing && a->rva == 0);
    CHECK(a && a->detail == "has no code of its own (abstract, extern or generic)");
  }
}

void identical_twins_are_one_behaviour() {
  ModelBuilder m;
  const int ai = m.type("Assembly-CSharp", "", "GhostAI");
  m.method(ai, "LookAtPlayer", "void", {"Player"}, 0x1000);
  m.method(ai, "ദശ", "void", {"Player"}, 0x1100);
  m.method(ai, "സറ", "void", {"Player"}, 0x1200);
  m.method(ai, "ബഹ", "void", {}, 0x1300);            // the same body, another signature
  m.method(ai, "ജഭ", "void", {"Player"}, 0x1400, true);  // the same body, static
  Code code;
  for (const uint32_t at : {0x1000, 0x1100, 0x1200, 0x1300, 0x1400}) code.function(at, join({kLoad40, kRet}));
  const Resolution r = code.run({&kGhostAI, &kLookAtPlayer}, m.build());
  const Result* l = r.find("GhostAI.LookAtPlayer");
  CHECK(l && l->status == Status::Resolved && l->rva == 0x1000);
  CHECK(l && l->twins == std::vector<uint64_t>({0x1100, 0x1200}));
  CHECK(l && l->aliases.empty());

  // Found by shape, the three are one behaviour, and the lowest RVA is canonical.
  constexpr EntryDesc shape{.key = "GhostAI.Look", .kind = Kind::Method, .owner = "GhostAI", .type = "void",
                            .params = kPlayer};
  const Resolution s = code.run({&kGhostAI, &shape}, m.build());
  const Result* h = s.find("GhostAI.Look");
  CHECK(h && h->status == Status::Resolved && h->rva == 0x1000);
  CHECK(h && h->currentName == "LookAtPlayer");
  CHECK(h && h->twins.size() == 2);
}

// Twins sit at other addresses, so a call to the same helper has other bytes: they are compared as
// read, with the target made absolute.
void twins_calling_one_helper() {
  ModelBuilder m;
  const int ai = m.type("Assembly-CSharp", "", "GhostAI");
  m.method(ai, "LookAtPlayer", "void", {"Player"}, 0x1000);
  m.method(ai, "ദശ", "void", {"Player"}, 0x1100);
  Code code;
  for (const uint32_t at : {0x1000, 0x1100}) code.function(at, join({rel(at, {0xE8}, 0x9000), kRet}));
  const Resolution r = code.run({&kGhostAI, &kLookAtPlayer}, m.build());
  const Result* l = r.find("GhostAI.LookAtPlayer");
  CHECK(l && l->status == Status::Resolved && l->twins == std::vector<uint64_t>{0x1100});
}

void decoy_with_other_metadata_is_not_a_twin() {
  ModelBuilder m;
  const int ai = m.type("Assembly-CSharp", "", "GhostAI");
  m.method(ai, "UnAppear", "void", {}, 0x1000);
  m.method(ai, "ദശ", "void", {}, 0x1100);
  // The same bytes at another address: the rip-relative operand names another metadata item.
  Code code;
  const std::vector<uint8_t> body = join({rel(0x1000, {0x48, 0x8D, 0x05}, 0x9000), kRet});
  code.function(0x1000, body);
  code.function(0x1100, body);
  constexpr EntryDesc unAppear{.key = "GhostAI.UnAppear", .kind = Kind::Method, .owner = "GhostAI",
                               .name = "UnAppear"};
  const Resolution r = code.run({&kGhostAI, &unAppear}, m.build());
  const Result* u = r.find("GhostAI.UnAppear");
  CHECK(u && u->status == Status::Resolved && u->rva == 0x1000);
  CHECK(u && u->twins.empty() && u->aliases.empty());

  // Found by shape they are two behaviours.
  constexpr EntryDesc shape{.key = "GhostAI.Un", .kind = Kind::Method, .owner = "GhostAI", .type = "void"};
  CHECK(code.run({&kGhostAI, &shape}, m.build()).find("GhostAI.Un")->status == Status::Ambiguous);
}

// Code the reader could not fully account for is never a twin, even byte for byte: what it could not
// decode may name an address, and at another address the same bytes name another target. Only a
// method at the very same address joins, as an alias.
void unreadable_code_is_never_a_twin() {
  ModelBuilder m;
  const int ai = m.type("Assembly-CSharp", "", "GhostAI");
  m.method(ai, "Same", "void", {"int"}, 0x1000);
  m.method(ai, "ദശ", "void", {"int"}, 0x1100);
  m.method(ai, "ഞത", "void", {"int"}, 0x1000);
  m.method(ai, "Load", "void", {"float"}, 0x1200);
  m.method(ai, "സറ", "void", {"float"}, 0x1300);
  Code code;
  for (const uint32_t at : {0x1000, 0x1100}) code.function(at, join({kVzeroupper, kLoad40, kRet}));
  // vmovss xmm0, dword ptr [rip+X]: the same bytes at both addresses read two different floats.
  const std::vector<uint8_t> load = join({rel(0x1200, {0xC5, 0xFA, 0x10, 0x05}, 0x9000), kRet});
  code.function(0x1200, load);
  code.function(0x1300, load);
  static constexpr const char* kInt[] = {"int"};
  static constexpr const char* kFloat[] = {"float"};
  constexpr EntryDesc same{.key = "GhostAI.Same", .kind = Kind::Method, .owner = "GhostAI", .name = "Same",
                           .params = kInt};
  constexpr EntryDesc named{.key = "GhostAI.Load", .kind = Kind::Method, .owner = "GhostAI", .name = "Load",
                            .params = kFloat};
  constexpr EntryDesc shape{.key = "GhostAI.LoadShape", .kind = Kind::Method, .owner = "GhostAI", .type = "void",
                            .params = kFloat};
  const Resolution r = code.run({&kGhostAI, &same, &named, &shape}, m.build());
  CHECK(r.find("GhostAI.Same")->status == Status::Resolved);
  CHECK(r.find("GhostAI.Same")->twins.empty());
  CHECK(r.find("GhostAI.Same")->aliases == std::vector<std::string>{"ഞത"});
  CHECK(r.find("GhostAI.Load")->status == Status::Resolved);
  CHECK(r.find("GhostAI.Load")->twins.empty());
  CHECK(r.find("GhostAI.LoadShape")->status == Status::Ambiguous);
  CHECK(r.find("GhostAI.LoadShape")->candidates.size() == 2);
}

void calls_and_called_by_include_twins() {
  ModelBuilder m;
  const int ai = m.type("Assembly-CSharp", "", "GhostAI");
  m.method(ai, "LookAtPlayer", "void", {"Player"}, 0x1000);
  m.method(ai, "ദശ", "void", {"Player"}, 0x1100);
  m.method(ai, "Hunting", "void", {}, 0x1200);
  m.method(ai, "സറ", "int", {}, 0x1300);
  m.method(ai, "ബഹ", "int", {}, 0x1400);
  m.method(ai, "ജഭ", "int", {}, 0x1500);
  Code code;
  for (const uint32_t at : {0x1000, 0x1100}) code.function(at, join({kLoad40, kRet}));
  code.function(0x1200, join({rel(0x1200, {0xE8}, 0x1400), kRet}));
  code.function(0x1300, join({rel(0x1300, {0xE8}, 0x1100), kRet}));  // calls the twin
  code.function(0x1400, join({kLoad40, kLoad40, kRet}));
  code.function(0x1500, join({{0x0F, 0x1F, 0x00}, kRet}));

  static constexpr Fact calls[] = {{.is = Fact::Is::Calls, .method = "GhostAI.LookAtPlayer"}};
  constexpr EntryDesc looker{.key = "GhostAI.Looker", .kind = Kind::Method, .owner = "GhostAI", .type = "int",
                             .facts = calls};
  static constexpr Fact calledBy[] = {{.is = Fact::Is::CalledBy, .method = "GhostAI.Hunting"}};
  constexpr EntryDesc step{.key = "GhostAI.Step", .kind = Kind::Method, .owner = "GhostAI", .type = "int",
                           .facts = calledBy};
  const Resolution r = code.run({&kGhostAI, &kLookAtPlayer, &kHuntingMethod, &looker, &step}, m.build());
  CHECK(r.find("GhostAI.LookAtPlayer")->twins == std::vector<uint64_t>{0x1100});
  CHECK(r.find("GhostAI.Looker")->status == Status::Resolved && r.find("GhostAI.Looker")->rva == 0x1300);
  CHECK(r.find("GhostAI.Step")->status == Status::Resolved && r.find("GhostAI.Step")->rva == 0x1400);
}

// A negated Calls or CalledBy fact cannot fail closed yet: the reader sees no indirect or virtual
// call and no conditional tail jump, so "does not call" may be wrong. Such an entry is Missing.
void negated_call_facts_are_refused() {
  ModelBuilder m;
  const int ai = m.type("Assembly-CSharp", "", "GhostAI");
  m.method(ai, "Hunting", "void", {}, 0x1200);
  m.method(ai, "ബഹ", "int", {}, 0x1400);
  m.method(ai, "ജഭ", "int", {}, 0x1500);
  Code code;
  code.function(0x1200, join({rel(0x1200, {0xE8}, 0x1400), kRet}));
  code.function(0x1400, join({kLoad40, kLoad40, kRet}));
  code.function(0x1500, join({{0x0F, 0x1F, 0x00}, kRet}));
  static constexpr Fact notCalledBy[] = {{.is = Fact::Is::CalledBy, .method = "GhostAI.Hunting", .negate = true}};
  static constexpr Fact notCalls[] = {{.is = Fact::Is::Calls, .method = "GhostAI.Hunting", .negate = true}};
  for (const std::span<const Fact> facts : {std::span<const Fact>(notCalledBy), std::span<const Fact>(notCalls)}) {
    const EntryDesc idle{.key = "GhostAI.Idle", .kind = Kind::Method, .owner = "GhostAI", .type = "int",
                         .facts = facts};
    const Resolution r = code.run({&kGhostAI, &kHuntingMethod, &idle}, m.build());
    const Result* i = r.find("GhostAI.Idle");
    CHECK(i && i->status == Status::Missing && i->rva == 0);
    CHECK(i && i->detail == "negated call facts are not supported yet");
  }
}

// The obfuscator's decoys of a method are never called. A Called fact keeps the candidate some other
// method of its type calls or jumps to, read in full: through a function the exception table lists,
// or a leaf function it does not (straight-line code up to a ret or a tail jump).
struct Decoys {
  ModelBuilder m;
  Code code;
  int ai;
  Decoys() {
    ai = m.type("Assembly-CSharp", "", "GhostAI");
    m.method(ai, "Helper", "int", {}, 0x1000);
    m.method(ai, "ദശ", "int", {}, 0x1100);  // the real one, called by 0x1300
    m.method(ai, "സറ", "int", {}, 0x1200);  // its decoy: the same call, and no caller
    m.method(ai, "ബഹ", "void", {}, 0x1300);
    m.method(ai, "ജഭ", "int", {}, 0x1400);  // a leaf getter: no entry in the exception table
    code.function(0x1000, kRet);
    code.function(0x1100, join({rel(0x1100, {0xE8}, 0x1000), kRet}));
    code.function(0x1200, join({rel(0x1200, {0xE8}, 0x1000), kLoad40, kRet}));
    code.function(0x1300, join({rel(0x1300, {0xE8}, 0x1100), kRet}));
    code.b.code(0x1400, join({kLoad40, kRet}));
  }
  const Result* real(std::span<const Fact> facts) {
    entry = {.key = "GhostAI.Real", .kind = Kind::Method, .owner = "GhostAI", .type = "int", .facts = facts};
    results = code.run({&kGhostAI, &kHelper, &entry}, m.build());
    return results.find("GhostAI.Real");
  }
  static constexpr EntryDesc kHelper{.key = "GhostAI.Helper", .kind = Kind::Method, .owner = "GhostAI",
                                     .name = "Helper"};
  EntryDesc entry;
  Resolution results;
};

constexpr Fact kCallsHelperAndCalled[] = {{.is = Fact::Is::Calls, .method = "GhostAI.Helper"},
                                          {.is = Fact::Is::Called}};

void called_fact_drops_uncalled_decoys() {
  {
    Decoys g;
    static constexpr Fact callsOnly[] = {{.is = Fact::Is::Calls, .method = "GhostAI.Helper"}};
    const Result* r = g.real(callsOnly);
    CHECK(r->status == Status::Ambiguous);  // the decoy matches as well
    // The leaf getter, which the exception table does not list, is read as a leaf and calls nothing:
    // the Calls fact drops it rather than leave it undecided.
    CHECK(r->candidates.size() == 2 && r->detail.find("could not be fully read") == std::string::npos);
  }
  {
    // A leaf the exception table does not list that tail-jumps to the fact method calls it: the Calls
    // fact says yes for it.
    Decoys g;
    g.m.method(g.ai, "ലള", "int", {}, 0x1480);
    g.code.b.code(0x1480, join({kLoad40, rel(0x1484, {0xE9}, 0x1000)}));
    static constexpr Fact callsOnly[] = {{.is = Fact::Is::Calls, .method = "GhostAI.Helper"}};
    const Result* r = g.real(callsOnly);
    CHECK(r && r->status == Status::Ambiguous && r->candidates.size() == 3);
  }
  {
    Decoys g;
    const Result* r = g.real(kCallsHelperAndCalled);
    CHECK(r && r->status == Status::Resolved && r->rva == 0x1100);
  }
  {
    // A leaf function that tail-jumps to the decoy calls it.
    Decoys g;
    g.m.method(g.ai, "ലള", "int", {}, 0x1480);
    g.code.b.code(0x1480, join({kLoad40, rel(0x1484, {0xE9}, 0x1200)}));
    const Result* r = g.real(kCallsHelperAndCalled);
    CHECK(r && r->status == Status::Ambiguous && r->candidates.size() == 2);
  }
}

// A field fact reads a leaf function the exception table does not list, straight-line code up to a ret
// or a tail jump, as it reads a listed one: a constructor that stores its defaults and jumps to its base
// tells two floats apart by the constants it stores through `this`.
void field_fact_reads_a_leaf() {
  constexpr EntryDesc rig{.key = "Rig", .name = "Rig", .assembly = "Assembly-CSharp", .ns = ""};
  constexpr EntryDesc ctor{.key = "Rig.ctor", .kind = Kind::Method, .owner = "Rig", .name = ".ctor"};
  static constexpr Fact walk[] = {{.is = Fact::Is::StoresConst, .method = "Rig.ctor", .value = 0x3FCCCCCD}};
  static constexpr Fact sprint[] = {{.is = Fact::Is::StoresConst, .method = "Rig.ctor", .value = 0x40400000}};
  const auto run = [&](std::vector<uint8_t> body, std::span<const Fact> facts) {
    ModelBuilder m;
    Code code;
    const int t = m.type("Assembly-CSharp", "", "Rig");
    m.field(t, "a", "float", 0x90);
    m.field(t, "b", "float", 0x94);
    m.field(t, "c", "float", 0x98);
    m.method(t, ".ctor", "void", {}, 0x1400);
    code.b.code(0x1400, std::move(body));
    const EntryDesc field{.key = "Rig.speed", .kind = Kind::Field, .owner = "Rig", .type = "float", .facts = facts};
    const Resolution r = code.run({&rig, &ctor, &field}, m.build());
    const Result* f = r.find("Rig.speed");
    return f ? std::pair{f->status, f->offset} : std::pair{Status::Missing, int32_t{-1}};
  };
  // mov dword [rcx+0x90], 1.6f; mov dword [rcx+0x94], 3.0f; jmp out
  const std::vector<uint8_t> stores{0xC7, 0x81, 0x90, 0x00, 0x00, 0x00, 0xCD, 0xCC, 0xCC, 0x3F,
                                    0xC7, 0x81, 0x94, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x40};
  const std::vector<uint8_t> tailJump = rel(0x1400 + stores.size(), {0xE9}, 0x9000);
  CHECK((run(join({stores, tailJump}), walk) == std::pair{Status::Resolved, int32_t{0x90}}));
  CHECK((run(join({stores, tailJump}), sprint) == std::pair{Status::Resolved, int32_t{0x94}}));
  CHECK(run(join({stores, kRet}), walk).first == Status::Resolved);
  // A leaf that branches cannot be read in full: the fact cannot decide, so nothing is picked.
  CHECK(run(join({stores, {0xEB, 0x00}, kRet}), walk).first == Status::Ambiguous);
  // Stores through rcx after the leaf wrote it are not through `this`: the fact cannot decide by them.
  const std::vector<uint8_t> reloaded{0x48, 0x8B, 0x49, 0x40};  // mov rcx, [rcx+0x40]
  CHECK(run(join({reloaded, stores, kRet}), walk).first == Status::Ambiguous);
}

// None found decides only when every other method of the type was read in full: one the reader
// cannot account for, listed or a leaf that branches, may hide the call.
void called_fact_fails_closed() {
  {
    Decoys g;
    g.m.method(g.ai, "ലള", "void", {}, 0x1500);
    g.code.function(0x1500, join({kVzeroupper, kRet}));
    const Result* r = g.real(kCallsHelperAndCalled);
    CHECK(r && r->status == Status::Ambiguous);
    CHECK(r && r->detail.find("could not be fully read for its Called fact") != std::string::npos);
  }
  {
    Decoys g;
    g.m.method(g.ai, "ലള", "int", {}, 0x1480);
    g.code.b.code(0x1480, join({{0x74, 0x01}, kRet, kRet}));  // je: not straight-line
    const Result* r = g.real(kCallsHelperAndCalled);
    CHECK(r && r->status == Status::Ambiguous);
  }
}

// A Called fact names no method, applies to a method only, and cannot be negated.
void called_fact_must_fit() {
  Decoys g;
  static constexpr Fact named[] = {{.is = Fact::Is::Called, .method = "GhostAI.Helper"}};
  const Result* r = g.real(named);
  CHECK(r && r->status == Status::Missing && r->detail == "a Called fact names no method");
  static constexpr Fact negated[] = {{.is = Fact::Is::Called, .negate = true}};
  r = g.real(negated);
  CHECK(r && r->status == Status::Missing && r->detail == "negated call facts are not supported yet");
  static constexpr Fact called[] = {{.is = Fact::Is::Called}};
  const EntryDesc field{.key = "GhostAI.f", .kind = Kind::Field, .owner = "GhostAI", .type = "int", .facts = called};
  const Resolution f = g.code.run({&kGhostAI, &field}, g.m.build());
  CHECK(f.find("GhostAI.f")->status == Status::Missing);
  CHECK(f.find("GhostAI.f")->detail == "a Called fact applies to a method");
}

// GhostAI with two bools and Hunting at 0x1000 made of `body`; the bool Hunting stores 1 into, as a
// StoresConst fact finds it.
Result stored_flag(const std::vector<uint8_t>& body) {
  ModelBuilder m;
  const int ai = m.type("Assembly-CSharp", "", "GhostAI");
  for (const int32_t offset : {0xD8, 0xE0}) m.field(ai, std::format("b{:x}", offset), "bool", offset);
  m.method(ai, "Hunting", "void", {}, 0x1000);
  Code code;
  code.function(0x1000, body);
  static constexpr Fact stores[] = {{.is = Fact::Is::StoresConst, .method = "GhostAI.Hunting", .value = 1}};
  constexpr EntryDesc e{.key = "GhostAI.flag", .kind = Kind::Field, .owner = "GhostAI", .type = "bool",
                        .facts = stores};
  const Resolution r = code.run({&kGhostAI, &kHuntingMethod, &e}, m.build());
  return *r.find("GhostAI.flag");
}

// A register saved to the stack and restored by an epilogue's mov (rather than a pop) still holds
// this wherever the function uses it: the value the restore loads goes back to the caller unused.
// A load from the stack the function goes on to use is another value.
void epilogue_restore_keeps_this() {
  const std::vector<uint8_t> save{0x48, 0x89, 0x5C, 0x24, 0x08};                 // mov [rsp+8], rbx
  const std::vector<uint8_t> frame{0x48, 0x83, 0xEC, 0x20};                      // sub rsp, 0x20
  const std::vector<uint8_t> copy{0x48, 0x8B, 0xD9};                             // mov rbx, rcx
  const std::vector<uint8_t> storeD8{0xC6, 0x83, 0xD8, 0x00, 0x00, 0x00, 0x01};  // mov byte [rbx+0xD8], 1
  const std::vector<uint8_t> storeE0{0xC6, 0x83, 0xE0, 0x00, 0x00, 0x00, 0x01};  // mov byte [rbx+0xE0], 1
  const std::vector<uint8_t> restore{0x48, 0x8B, 0x5C, 0x24, 0x28};              // mov rbx, [rsp+0x28]
  const std::vector<uint8_t> release{0x48, 0x83, 0xC4, 0x20};                    // add rsp, 0x20
  const std::vector<uint8_t> call = rel(0x1000 + 12, {0xE8}, 0x9000);
  const Result plain = stored_flag(join({save, frame, copy, call, storeD8, restore, release, kRet}));
  CHECK(plain.status == Status::Resolved && plain.offset == 0xD8);

  // Through r11, as a frame opened with mov rax, rsp is closed: mov rax, rsp; mov [rax+0x10], rbx;
  // push rdi, then lea r11, [rsp+0x20]; mov rbx, [r11+0x18]; mov rsp, r11; pop rdi.
  const std::vector<uint8_t> open{0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x10, 0x57};
  const std::vector<uint8_t> callR11 = rel(0x1000 + 15, {0xE8}, 0x9000);
  const std::vector<uint8_t> close{0x4C, 0x8D, 0x5C, 0x24, 0x20, 0x49, 0x8B, 0x5B, 0x18, 0x49, 0x8B, 0xE3, 0x5F};
  const Result viaR11 = stored_flag(join({open, frame, copy, callR11, storeD8, close, kRet}));
  CHECK(viaR11.status == Status::Resolved && viaR11.offset == 0xD8);

  // Loaded mid-function and used: rbx is no longer this, and the store decides nothing.
  const Result reused = stored_flag(join({save, frame, copy, call, storeD8, restore, storeE0, release, kRet}));
  CHECK(reused.status == Status::Ambiguous);
  CHECK(reused.detail.find("not proven to hold this") != std::string::npos);
}

// A `jmp rel32` back to the function's own start is a tail call of itself: the start is no branch
// target within the run, so the entry's copy of this still counts, and the restore before the jump is
// an epilogue's. A conditional jump to the start stays a branch.
void self_tail_jump_keeps_this() {
  const std::vector<uint8_t> save{0x48, 0x89, 0x5C, 0x24, 0x08};                 // mov [rsp+8], rbx
  const std::vector<uint8_t> frame{0x48, 0x83, 0xEC, 0x20};                      // sub rsp, 0x20
  const std::vector<uint8_t> copy{0x48, 0x8B, 0xD9};                             // mov rbx, rcx
  const std::vector<uint8_t> storeD8{0xC6, 0x83, 0xD8, 0x00, 0x00, 0x00, 0x01};  // mov byte [rbx+0xD8], 1
  const std::vector<uint8_t> restore{0x48, 0x8B, 0x5C, 0x24, 0x28};              // mov rbx, [rsp+0x28]
  const std::vector<uint8_t> release{0x48, 0x83, 0xC4, 0x20};                    // add rsp, 0x20
  const std::vector<uint8_t> call = rel(0x1000 + 12, {0xE8}, 0x9000);
  // je +n over the tail call to a plain return: mov rcx, [rbx+0x40]; restore; release; jmp start.
  const std::vector<uint8_t> next{0x48, 0x8B, 0x4B, 0x40};
  const size_t tailAt = 0x1000 + 12 + 5 + 7 + 2 + 4 + 5 + 4;
  const std::vector<uint8_t> tail = join({next, restore, release, rel(tailAt, {0xE9}, 0x1000)});
  const std::vector<uint8_t> skip{0x74, static_cast<uint8_t>(tail.size())};  // je over the tail call
  const Result looped = stored_flag(join({save, frame, copy, call, storeD8, skip, tail, restore, release, kRet}));
  CHECK(looped.status == Status::Resolved && looped.offset == 0xD8);

  // A jne back to the start is a branch to a place the run reaches again: the copy is no longer in
  // the instructions every call runs first, and the store decides nothing.
  const std::vector<uint8_t> back = rel(0x1000 + 12 + 5 + 7, {0x0F, 0x85}, 0x1000);  // jne start
  const Result branched = stored_flag(join({save, frame, copy, call, storeD8, back, restore, release, kRet}));
  CHECK(branched.status == Status::Ambiguous);
}

// An IsNot fact drops the field another entry of the owner resolved to, and fails closed: while that entry
// is unresolved this one is blocked, and it names only a field entry of the same owner.
void is_not_fact_narrows_by_a_resolved_field() {
  ModelBuilder m;
  const int ai = m.type("Assembly-CSharp", "", "GhostAI");
  m.field(ai, "ദശ", "bool", 0x40);
  m.field(ai, "സറ", "bool", 0x41);
  m.field(ai, "ബഹ", "int", 0x44);
  static constexpr EntryDesc owner{.key = "GhostAI", .name = "GhostAI", .assembly = "Assembly-CSharp", .ns = ""};
  static constexpr EntryDesc known{.key = "GhostAI.Known", .kind = Kind::Field, .owner = "GhostAI", .name = "ദശ"};
  static constexpr Fact notKnown[] = {{.is = Fact::Is::IsNot, .method = "GhostAI.Known"}};
  static constexpr EntryDesc left{.key = "GhostAI.Left", .kind = Kind::Field, .owner = "GhostAI", .type = "bool",
                                  .facts = notKnown};
  Code code;
  {
    Resolution results = code.run({&owner, &known, &left}, m.build());
    const Result* r = results.find("GhostAI.Left");
    CHECK(r && r->status == Status::Resolved && r->offset == 0x41);
  }
  {  // The other entry did not resolve: nothing is accepted.
    static constexpr EntryDesc gone{.key = "GhostAI.Known", .kind = Kind::Field, .owner = "GhostAI", .name = "നമ"};
    Resolution results = code.run({&owner, &gone, &left}, m.build());
    const Result* r = results.find("GhostAI.Left");
    CHECK(r && r->status == Status::Blocked);
  }
  {  // It names a field of the same owner, and only a field carries it.
    static constexpr Fact bad[] = {{.is = Fact::Is::IsNot, .method = "GhostAI"}};
    static constexpr EntryDesc wrong{.key = "GhostAI.Wrong", .kind = Kind::Field, .owner = "GhostAI", .type = "bool",
                                     .facts = bad};
    Resolution results = code.run({&owner, &wrong}, m.build());
    const Result* r = results.find("GhostAI.Wrong");
    CHECK(r && r->status == Status::Missing);
  }
}

// IsNot over real code, as GhostAI.interactionBoost uses it: a Loads fact matches every bool a method
// reads, and IsNot facts naming the ones already told apart leave the one meant. A bool the method comes
// to read after an update leaves the entry Ambiguous (sdk/README.md), never a silent pick.
void is_not_narrows_a_loads_fact_over_code() {
  const auto resolve_with = [](bool readsAnother, std::span<const Fact> facts) {
    ModelBuilder m;
    const int ai = m.type("Assembly-CSharp", "", "GhostAI");
    for (const int32_t offset : {0xD8, 0xD9, 0xDA, 0xDB}) m.field(ai, std::format("b{:x}", offset), "bool", offset);
    m.method(ai, "Execute", "void", {}, 0x1000);
    // movzx eax, byte ptr [rcx+disp32] for each bool it reads, through this
    const auto load = [](uint8_t disp) { return std::vector<uint8_t>{0x0F, 0xB6, 0x81, disp, 0x00, 0x00, 0x00}; };
    Code code;
    code.function(0x1000, readsAnother ? join({load(0xD8), load(0xD9), load(0xDA), load(0xDB), kRet})
                                       : join({load(0xD8), load(0xD9), load(0xDA), kRet}));
    static constexpr EntryDesc execute{.key = "GhostAI.Execute", .kind = Kind::Method, .owner = "GhostAI",
                                       .name = "Execute"};
    static constexpr EntryDesc first{.key = "GhostAI.First", .kind = Kind::Field, .owner = "GhostAI", .name = "bd8"};
    static constexpr EntryDesc second{.key = "GhostAI.Second", .kind = Kind::Field, .owner = "GhostAI",
                                      .name = "bd9"};
    const EntryDesc left{.key = "GhostAI.Left", .kind = Kind::Field, .owner = "GhostAI", .type = "bool",
                         .facts = facts};
    Resolution results = code.run({&kGhostAI, &execute, &first, &second, &left}, m.build());
    const Result* r = results.find("GhostAI.Left");
    return r ? std::pair{r->status, r->offset} : std::pair{Status::Missing, int32_t{-1}};
  };
  static constexpr Fact narrowed[] = {{.is = Fact::Is::Loads, .method = "GhostAI.Execute"},
                                      {.is = Fact::Is::IsNot, .method = "GhostAI.First"},
                                      {.is = Fact::Is::IsNot, .method = "GhostAI.Second"}};
  static constexpr Fact loadsOnly[] = {{.is = Fact::Is::Loads, .method = "GhostAI.Execute"}};
  const auto [status, offset] = resolve_with(false, narrowed);
  CHECK(status == Status::Resolved && offset == 0xDA);
  CHECK(resolve_with(false, loadsOnly).first == Status::Ambiguous);  // the Loads fact alone cannot choose
  CHECK(resolve_with(true, narrowed).first == Status::Ambiguous);    // a new bool it reads fails closed
}

// A Compares fact keeps the methods whose own code compares something with an enum value's number
// (cmp r/m, imm and cmp eax, imm alike), and fails closed on code it cannot read in full.
struct Compared {
  ModelBuilder m;
  Code code;
  int ai;
  Compared() {
    ai = m.type("Assembly-CSharp", "", "GhostAI");
    const int ghostType = m.type("Assembly-CSharp", "", "GhostType", -1, true);
    m.literal(ghostType, "Spirit", 0);
    m.literal(ghostType, "Yurei", 10);
    m.literal(ghostType, "Wide", 0x80);
    m.literal(ghostType, "Wider", 0x180);
    m.literal(ghostType, "Unset", -1);
    m.method(ai, "ദശ", "void", {}, 0x1100);  // compares the type with 10
    m.method(ai, "സറ", "void", {}, 0x1200);  // its look-alike: compares with 0x5F
    m.method(ai, "ബഹ", "void", {}, 0x1300);  // compares nothing
    code.function(0x1100, {0x48, 0x8B, 0x41, 0x38, 0x83, 0x78, 0x2C, 0x0A, 0xC3});  // cmp dword [rax+0x2C], 10
    code.function(0x1200, {0x48, 0x8B, 0x41, 0x38, 0x83, 0x78, 0x2C, 0x5F, 0xC3});
    code.function(0x1300, join({kLoad40, kRet}));
  }
  const Result* real(std::span<const Fact> facts) {
    entry = {.key = "GhostAI.Real", .kind = Kind::Method, .owner = "GhostAI", .type = "void", .facts = facts};
    results = code.run({&kGhostAI, &kGhostType, &kYurei, &kWide, &kWider, &kUnset, &entry}, m.build());
    return results.find("GhostAI.Real");
  }
  static constexpr EntryDesc kGhostType{.key = "GhostType", .name = "GhostType", .assembly = "Assembly-CSharp",
                                        .ns = ""};
  static constexpr EntryDesc kYurei{.key = "GhostType.Yurei", .kind = Kind::EnumValue, .owner = "GhostType",
                                    .name = "Yurei"};
  static constexpr EntryDesc kWide{.key = "GhostType.Wide", .kind = Kind::EnumValue, .owner = "GhostType",
                                   .name = "Wide"};
  static constexpr EntryDesc kWider{.key = "GhostType.Wider", .kind = Kind::EnumValue, .owner = "GhostType",
                                    .name = "Wider"};
  static constexpr EntryDesc kUnset{.key = "GhostType.Unset", .kind = Kind::EnumValue, .owner = "GhostType",
                                    .name = "Unset"};
  EntryDesc entry;
  Resolution results;
};

constexpr Fact kComparesYurei[] = {{.is = Fact::Is::Compares, .method = "GhostType.Yurei"}};

// Look-alikes that differ only in a constant: the one that moves the number is kept, whether into a register or a place.
void moves_const_fact_keeps_the_method_with_the_number() {
  static constexpr Fact kMoves100[] = {{.is = Fact::Is::MovesConst, .value = 100}};
  {
    Compared g;
    g.code.function(0x1100, {0xB8, 0x64, 0x00, 0x00, 0x00, 0xC3});  // mov eax, 100
    g.code.function(0x1200, {0xB8, 0x42, 0x00, 0x00, 0x00, 0xC3});  // mov eax, 66
    const Result* r = g.real(kMoves100);
    CHECK(r && r->status == Status::Resolved && r->rva == 0x1100);
  }
  {
    Compared g;
    g.code.function(0x1100, {0xB8, 0x42, 0x00, 0x00, 0x00, 0xC3});
    g.code.function(0x1200, {0xC7, 0x41, 0x2C, 0x64, 0x00, 0x00, 0x00, 0xC3});  // mov dword [rcx+0x2C], 100
    const Result* r = g.real(kMoves100);
    CHECK(r && r->status == Status::Resolved && r->rva == 0x1200);
  }
}

// Several methods that move the number stay Ambiguous; one that cannot be read in full may move it, so it is
// undecided, never dropped.
void moves_const_fact_fails_closed() {
  static constexpr Fact kMoves100[] = {{.is = Fact::Is::MovesConst, .value = 100}};
  {
    Compared g;
    g.code.function(0x1100, {0xB8, 0x64, 0x00, 0x00, 0x00, 0xC3});
    g.code.function(0x1200, {0xB9, 0x64, 0x00, 0x00, 0x00, 0xC3});  // mov ecx, 100
    const Result* r = g.real(kMoves100);
    CHECK(r && r->status == Status::Ambiguous);
  }
  {
    Compared g;
    g.code.function(0x1100, {0xB8, 0x64, 0x00, 0x00, 0x00, 0xC3});
    g.code.function(0x1200, join({kVzeroupper, kRet}));
    const Result* r = g.real(kMoves100);
    CHECK(r && r->status == Status::Ambiguous);
    CHECK(r && r->detail.find("could not be fully read for its MovesConst fact") != std::string::npos);
  }
}

// A MovesConst fact names only a number, applies to a method only, and cannot be negated.
void moves_const_fact_must_fit() {
  Compared g;
  static constexpr Fact named[] = {{.is = Fact::Is::MovesConst, .method = "GhostType.Yurei", .value = 100}};
  const Result* r = g.real(named);
  CHECK(r && r->status == Status::Missing && r->detail == "a MovesConst fact names only a number");
  static constexpr Fact negated[] = {{.is = Fact::Is::MovesConst, .value = 100, .negate = true}};
  r = g.real(negated);
  CHECK(r && r->status == Status::Missing && r->detail == "a MovesConst fact names only a number");
  static constexpr Fact moves[] = {{.is = Fact::Is::MovesConst, .value = 100}};
  const EntryDesc field{.key = "GhostAI.f", .kind = Kind::Field, .owner = "GhostAI", .type = "int", .facts = moves};
  const Resolution f = g.code.run({&kGhostAI, &field}, g.m.build());
  CHECK(f.find("GhostAI.f")->status == Status::Missing);
  CHECK(f.find("GhostAI.f")->detail == "a MovesConst fact applies to a method");
}

void compares_fact_keeps_the_method_that_tests_the_value() {
  {
    Compared g;
    const Result* r = g.real(kComparesYurei);
    CHECK(r && r->status == Status::Resolved && r->rva == 0x1100);
  }
  {
    // cmp eax, imm32 counts as well.
    Compared g;
    g.code.function(0x1100, {0x8B, 0x41, 0x2C, 0x3D, 0x0A, 0x00, 0x00, 0x00, 0xC3});
    const Result* r = g.real(kComparesYurei);
    CHECK(r && r->status == Status::Resolved && r->rva == 0x1100);
  }
  {
    // A byte compare with 0x80 compares with 128 (not with -128 sign-extended), and never with 0x180,
    // which a byte cannot hold; a dword compare with the sign-extended imm8 0x80 is one with -128.
    Compared g;
    g.code.function(0x1100, {0x48, 0x8B, 0x41, 0x38, 0x80, 0x78, 0x2C, 0x80, 0xC3});  // cmp byte [rax+0x2C], 0x80
    g.code.function(0x1200, {0x48, 0x8B, 0x41, 0x38, 0x83, 0x78, 0x2C, 0x80, 0xC3});  // cmp dword [rax+0x2C], -128
    static constexpr Fact wide[] = {{.is = Fact::Is::Compares, .method = "GhostType.Wide"}};
    const Result* r = g.real(wide);
    CHECK(r && r->status == Status::Resolved && r->rva == 0x1100);
    static constexpr Fact wider[] = {{.is = Fact::Is::Compares, .method = "GhostType.Wider"}};
    r = g.real(wider);
    CHECK(r && r->status == Status::Missing);
  }
  {
    // A negative value: a dword compare with the sign-extended imm8 0xFF is one with -1.
    Compared g;
    g.code.function(0x1100, {0x48, 0x8B, 0x41, 0x38, 0x83, 0x78, 0x2C, 0xFF, 0xC3});  // cmp dword [rax+0x2C], -1
    static constexpr Fact unset[] = {{.is = Fact::Is::Compares, .method = "GhostType.Unset"}};
    const Result* r = g.real(unset);
    CHECK(r && r->status == Status::Resolved && r->rva == 0x1100);
  }
  // The comparison itself: a value fits the width signed or unsigned and matches in its low bits.
  using Compare = CodeFacts::Compare;
  CHECK((Compare{-1, 32}.is(-1) && Compare{-1, 32}.is(0xFFFFFFFF) && !Compare{-1, 32}.is(0x1FFFFFFFF)));
  CHECK((Compare{-128, 8}.is(128) && Compare{-128, 8}.is(-128)));
  CHECK((!Compare{-128, 8}.is(384) && !Compare{-128, 8}.is(-129)));
  CHECK((Compare{-128, 32}.is(-128) && !Compare{-128, 32}.is(128)));
  CHECK((Compare{-5, 64}.is(-5) && !Compare{-5, 64}.is(5)));
}

void compares_fact_fails_closed() {
  {
    // A look-alike the reader cannot read in full may hold the cmp: undecided, never dropped.
    Compared g;
    g.code.function(0x1200, join({kVzeroupper, kRet}));
    const Result* r = g.real(kComparesYurei);
    CHECK(r && r->status == Status::Ambiguous);
    CHECK(r && r->detail.find("could not be fully read for its Compares fact") != std::string::npos);
  }
  {
    // A leaf the exception table does not list is read for its calls only: it cannot decide.
    Compared g;
    g.m.method(g.ai, "ലള", "void", {}, 0x1480);
    g.code.b.code(0x1480, join({kLoad40, kRet}));
    const Result* r = g.real(kComparesYurei);
    CHECK(r && r->status == Status::Ambiguous);
  }
}

// A Compares fact names an enum value entry, applies to a method only, and cannot be negated.
void compares_fact_must_fit() {
  Compared g;
  static constexpr Fact onType[] = {{.is = Fact::Is::Compares, .method = "GhostType"}};
  const Result* r = g.real(onType);
  CHECK(r && r->status == Status::Missing && r->detail == "a Compares fact names an enum value entry");
  // "Does not compare" cannot fail closed: the reader sees only cmp with an immediate.
  static constexpr Fact notYurei[] = {{.is = Fact::Is::Compares, .method = "GhostType.Yurei", .negate = true}};
  r = g.real(notYurei);
  CHECK(r && r->status == Status::Missing && r->detail == "negated Compares facts are not supported");
  const EntryDesc field{.key = "GhostAI.f", .kind = Kind::Field, .owner = "GhostAI", .type = "int",
                        .facts = kComparesYurei};
  const Resolution f = g.code.run({&kGhostAI, &Compared::kGhostType, &Compared::kYurei, &field}, g.m.build());
  CHECK(f.find("GhostAI.f")->status == Status::Missing);
  CHECK(f.find("GhostAI.f")->detail == "a Compares fact applies to a method");
}

// A field found by shape may name its accessibility: only fields declared so match, and a field the
// model knows no accessibility for never does.
void field_access_narrows_the_shape() {
  ModelBuilder m;
  const int ai = m.type("Assembly-CSharp", "", "GhostAI");
  m.field(ai, "followed", "Player", 0x48);
  m.model.types[ai].fields.back().access = Visibility::Internal;
  m.field(ai, "fallback", "Player", 0x50);
  m.model.types[ai].fields.back().access = Visibility::Private;
  m.field(ai, "unknown", "Player", 0x58);
  const GameModel model = m.build();
  const auto resolve = [&](Visibility access) {
    const EntryDesc e{.key = "GhostAI.p", .kind = Kind::Field, .owner = "GhostAI", .type = "Player",
                      .access = access};
    return *run({&kGhostAI, &e}, model).find("GhostAI.p");
  };
  const Result internal = resolve(Visibility::Internal);
  CHECK(internal.status == Status::Resolved && internal.offset == 0x48);
  const Result privateOne = resolve(Visibility::Private);
  CHECK(privateOne.status == Status::Resolved && privateOne.offset == 0x50);
  CHECK(resolve(Visibility::Public).status == Status::Missing);
  CHECK(resolve(Visibility::Any).status == Status::Ambiguous);  // not checked: all three
}

// An access through a register loaded from a field of this reaches an object of that field's type. When
// that type and the owner are of unrelated families (neither among the other's possible ancestors, both
// known), the access cannot be the owner's field and counts for no fact; otherwise it counts as before.
struct Chained {
  ModelBuilder m;
  Code code;
  int hunt, ghost;
  Chained() {
    hunt = m.type("Assembly-CSharp", "", "Hunt");
    ghost = m.type("Assembly-CSharp", "", "Ghost");
    m.field(hunt, "ghost", "Ghost", 0x10, false, ghost);
    for (const int32_t offset : {0x48, 0x50, 0x58}) m.field(hunt, std::format("p{:x}", offset), "Player", offset);
    m.field(ghost, "activity", "object", 0x58);
    m.method(hunt, "Update", "void", {}, 0x1000);
    // push rbx; mov rbx, rcx; mov rax, [rbx+48h]; mov rax, [rbx+50h]; mov rax, [rbx+10h]; mov rcx, [rax+58h];
    // pop rbx; ret
    code.function(0x1000, {0x53, 0x48, 0x8B, 0xD9, 0x48, 0x8B, 0x43, 0x48, 0x48, 0x8B, 0x43, 0x50, 0x48, 0x8B,
                           0x43, 0x10, 0x48, 0x8B, 0x48, 0x58, 0x5B, 0xC3});
  }
  Result loaded() {
    static constexpr EntryDesc update{.key = "Hunt.Update", .kind = Kind::Method, .owner = "Hunt", .name = "Update"};
    static constexpr EntryDesc type{.key = "Hunt", .name = "Hunt", .assembly = "Assembly-CSharp", .ns = ""};
    static constexpr Fact loads[] = {{.is = Fact::Is::Loads, .method = "Hunt.Update"}};
    static constexpr EntryDesc e{.key = "Hunt.p", .kind = Kind::Field, .owner = "Hunt", .type = "Player",
                                 .facts = loads};
    return *code.run({&type, &update, &e}, m.build()).find("Hunt.p");
  }
};

void access_through_another_types_field_is_no_hit() {
  {
    Chained g;
    const Result r = g.loaded();
    CHECK(r.status == Status::Ambiguous && r.candidates.size() == 2);  // 0x48 and 0x50 only, decided
    CHECK(r.detail.find("not proven to hold this") == std::string::npos);
  }
  {
    // Ghost may derive from Hunt: the object may be a Hunt, and the access leaves 0x58 undecided.
    Chained g;
    g.m.model.types[g.ghost].bases = {"Hunt"};
    const Result r = g.loaded();
    CHECK(r.status == Status::Ambiguous && r.detail.find("not proven to hold this") != std::string::npos);
  }
  {
    // A base the model does not know: no proof either.
    Chained g;
    g.m.model.types[g.ghost].bases = {"Elsewhere"};
    const Result r = g.loaded();
    CHECK(r.status == Status::Ambiguous && r.detail.find("not proven to hold this") != std::string::npos);
  }
  {
    // The field's type is an interface: a class deriving from Hunt may implement it though Hunt does
    // not, so the object may be a Hunt. No proof.
    Chained g;
    g.m.model.types[g.ghost].isInterface = true;
    const Result r = g.loaded();
    CHECK(r.status == Status::Ambiguous && r.detail.find("not proven to hold this") != std::string::npos);
  }
  {
    // The owner is an interface: a class implementing Hunt may derive from Ghost though Ghost does not
    // implement Hunt, so the object may be a Hunt. No proof either.
    Chained g;
    g.m.model.types[g.hunt].isInterface = true;
    const Result r = g.loaded();
    CHECK(r.status == Status::Ambiguous && r.detail.find("not proven to hold this") != std::string::npos);
  }
}

void pattern_inside_method() {
  Bools g;
  g.m.method(0, "Twice", "void", {"int"}, 0x1100);
  // Twice holds Hunting's stores twice over. A search of Hunting does not reach into it.
  g.code.function(0x1100, join({kHuntingBody, kHuntingBody, kRet}));
  const GameModel model = g.m.build();
  constexpr EntryDesc store{.key = "GhostAI.storeCooldown", .kind = Kind::Code, .pattern = "C6 87 ?? ?? ?? ?? 01",
                            .within = "GhostAI.Hunting"};
  const Resolution r = g.code.run({&kGhostAI, &kHuntingMethod, &store}, model);
  const Result* s = r.find("GhostAI.storeCooldown");
  CHECK(s && s->status == Status::Resolved);
  CHECK(s && s->rva == 0x1010);

  // The offset may move the result to another instruction's start: the movzx after the store.
  constexpr EntryDesc next{.key = "GhostAI.storeCooldown", .kind = Kind::Code, .pattern = "c6 87 ?? ?? ?? ?? 01",
                           .within = "GhostAI.Hunting", .patternOffset = 7};
  CHECK(g.code.run({&kGhostAI, &kHuntingMethod, &next}, model).find("GhostAI.storeCooldown")->rva == 0x1017);
  // Never inside an instruction (the store's immediate) or outside the method.
  for (const int32_t offset : {6, 0x100, -0x20}) {
    const EntryDesc moved{.key = "GhostAI.storeCooldown", .kind = Kind::Code, .pattern = "C6 87 ?? ?? ?? ?? 01",
                          .within = "GhostAI.Hunting", .patternOffset = offset};
    const Resolution m = g.code.run({&kGhostAI, &kHuntingMethod, &moved}, model);
    const Result* s2 = m.find("GhostAI.storeCooldown");
    CHECK(s2 && s2->status == Status::Missing && s2->rva == 0);
    CHECK(s2 && s2->detail.find(offset == 6 ? "not where an instruction starts" : "outside GhostAI.Hunting") !=
                    std::string::npos);
  }

  static constexpr const char* kInt[] = {"int"};
  constexpr EntryDesc twice{.key = "GhostAI.Twice", .kind = Kind::Method, .owner = "GhostAI", .name = "Twice",
                            .params = kInt};
  constexpr EntryDesc both{.key = "GhostAI.storeCooldown", .kind = Kind::Code, .pattern = "C6 87 ?? ?? ?? ?? 01",
                           .within = "GhostAI.Twice"};
  const Resolution t = g.code.run({&kGhostAI, &twice, &both}, model);
  const Result* a = t.find("GhostAI.storeCooldown");
  CHECK(a && a->status == Status::Ambiguous && a->candidates.size() == 2 && a->rva == 0);

  constexpr EntryDesc none{.key = "GhostAI.storeCooldown", .kind = Kind::Code, .pattern = "C6 87 ?? ?? ?? ?? 02",
                           .within = "GhostAI.Hunting"};
  CHECK(g.code.run({&kGhostAI, &kHuntingMethod, &none}, model).find("GhostAI.storeCooldown")->status ==
        Status::Missing);
  for (const char* bad : {"", "C6 8", "C6 87 ?", "C6 GG", "C687"}) {
    const EntryDesc malformed{.key = "GhostAI.storeCooldown", .kind = Kind::Code, .pattern = bad,
                              .within = "GhostAI.Hunting"};
    const Resolution b = g.code.run({&kGhostAI, &kHuntingMethod, &malformed}, model);
    CHECK(b.find("GhostAI.storeCooldown")->status == Status::Missing);
    CHECK(b.find("GhostAI.storeCooldown")->detail.find("pattern") != std::string::npos);
  }
}

// A code entry's place counts only in code read in full: elsewhere the instruction starts are not
// known.
void pattern_needs_readable_code() {
  Bools g(Hunting::Unreadable);
  constexpr EntryDesc store{.key = "GhostAI.storeCooldown", .kind = Kind::Code, .pattern = "C6 87 ?? ?? ?? ?? 01",
                            .within = "GhostAI.Hunting"};
  const Resolution r = g.code.run({&kGhostAI, &kHuntingMethod, &store}, g.m.build());
  const Result* s = r.find("GhostAI.storeCooldown");
  CHECK(s && s->status == Status::Missing && s->rva == 0);
  CHECK(s && s->detail.find("could not be fully read") != std::string::npos);
}

// A static field is a Field entry flagged kStatic: its shape counts static fields only.
void static_field_by_shape() {
  ModelBuilder b;
  const int level = b.type("Assembly-CSharp", "", "LevelController");
  b.field(level, "ബദഭ", "LevelController", 0x0, true, level);
  b.field(level, "ഷമദ", "LevelController", 0x18, false, level);  // an instance field of the same type
  constexpr EntryDesc type{.key = "LevelController", .name = "LevelController", .assembly = "Assembly-CSharp", .ns = ""};
  constexpr EntryDesc instance{.key = "LevelController.instance", .kind = Kind::Field, .flags = kStatic,
                               .owner = "LevelController", .type = "@LevelController"};
  const Resolution r = run({&type, &instance}, b.build());
  const Result* i = r.find("LevelController.instance");
  CHECK(i && i->status == Status::Resolved && i->member == 0 && i->currentName == "ബദഭ");
}

int g_setBoolID = 0;  // stands in for the engine function's code

// An icall resolves through the running runtime, by the icall's name.
void icall_resolves_by_name() {
  constexpr EntryDesc setBool{.key = "Animator.SetBoolID", .kind = Kind::Icall,
                              .name = "UnityEngine.Animator::SetBoolID"};
  std::vector<std::string> asked;
  const IcallLookup runtime = [&](const char* name) -> const void* {
    asked.emplace_back(name);
    return std::string_view(name) == "UnityEngine.Animator::SetBoolID" ? &g_setBoolID : nullptr;
  };
  const Resolution r = resolve(std::vector<const EntryDesc*>{&setBool}, GameModel{}, nullptr, runtime);
  const Result* s = r.find("Animator.SetBoolID");
  CHECK(s && s->status == Status::Resolved && s->address == reinterpret_cast<uintptr_t>(&g_setBoolID));
  CHECK((asked == std::vector<std::string>{"UnityEngine.Animator::SetBoolID"}));

  constexpr EntryDesc gone{.key = "Animator.Gone", .kind = Kind::Icall, .name = "UnityEngine.Animator::Gone"};
  const Resolution missing = resolve(std::vector<const EntryDesc*>{&gone}, GameModel{}, nullptr, runtime);
  CHECK(missing.find("Animator.Gone")->status == Status::Missing);
  CHECK(missing.find("Animator.Gone")->address == 0);
}

// Offline there is no runtime to ask: an icall stays unresolved, checked at startup only.
void icall_without_runtime_is_checked_at_startup() {
  constexpr EntryDesc setBool{.key = "Animator.SetBoolID", .kind = Kind::Icall,
                              .name = "UnityEngine.Animator::SetBoolID"};
  const Resolution r = run({&setBool}, GameModel{});
  CHECK(r.find("Animator.SetBoolID")->status == Status::Missing);
  CHECK(r.find("Animator.SetBoolID")->detail == "checked at startup only");
}

// A model of a wander and the interfaces it declares, for a type found as the one interface another declares.
GameModel declaring(std::vector<std::string> bases) {
  GameModel m;
  ModelType wander;
  wander.assembly = "A";
  wander.name = "Wander";
  wander.bases = std::move(bases);
  m.types.push_back(wander);
  for (const char* name : {"IState", "IOther", "Base"}) {
    ModelType t;
    t.assembly = "A";
    t.name = name;
    t.isInterface = name[0] == 'I';
    m.types.push_back(t);
  }
  return m;
}

constexpr EntryDesc kWander{.key = "Wander", .name = "Wander", .assembly = "A", .ns = ""};
constexpr EntryDesc kDeclared{.key = "IState", .assembly = "A", .ns = "", .within = "Wander"};

const Result* declared_in(Resolution& r, const GameModel& model) {
  r = run({&kWander, &kDeclared}, model);
  return r.find("IState");
}

void type_is_the_one_interface_another_declares() {
  Resolution r;
  GameModel m = declaring({"IState"});
  const Result* d = declared_in(r, m);
  CHECK(d->status == Status::Resolved && d->type == 1);
  m = declaring({"Base", "IState"});  // its base class is no interface
  d = declared_in(r, m);
  CHECK(d->status == Status::Resolved && d->type == 1);
  m = declaring({"IState", "IOther"});  // never picks among them
  d = declared_in(r, m);
  CHECK(d->status == Status::Ambiguous && d->candidates.size() == 2);
  m = declaring({"Base"});
  d = declared_in(r, m);
  CHECK(d->status == Status::Missing);
  m = declaring({"IState", "Elsewhere"});  // an interface the model lacks could be the other one
  d = declared_in(r, m);
  CHECK(d->status == Status::Missing && d->detail.find("Elsewhere") != std::string::npos);
  m = declaring({"IState"});
  m.types.push_back(m.types[1]);  // two types share the name: the model does not say which it declares
  d = declared_in(r, m);
  CHECK(d->status == Status::Ambiguous);
  constexpr EntryDesc overField{.key = "F", .kind = Kind::Field, .owner = "Wander", .name = "f"};
  constexpr EntryDesc notType{.key = "Bad", .assembly = "A", .ns = "", .within = "F"};
  ModelField f;
  f.name = "f";
  f.type = "int";
  m = declaring({"IState"});
  m.types[0].fields.push_back(f);
  r = run({&kWander, &overField, &notType}, m);
  CHECK(r.find("Bad")->status == Status::Missing);
}

// A model of a generic class with one method, whose code the definition lacks.
GameModel generic_model() {
  GameModel m;
  ModelType box;
  box.assembly = "A";
  box.name = "Box";
  ModelMethod put;
  put.name = "Put";
  put.ret = "T";
  put.params = {"T"};
  put.paramIndex = {-1};
  box.methods.push_back(put);
  m.types.push_back(box);
  ModelType arg;
  arg.assembly = "A";
  arg.name = "Arg";
  m.types.push_back(arg);
  return m;
}

constexpr EntryDesc kBox{.key = "Box", .name = "Box", .assembly = "A", .ns = ""};
constexpr EntryDesc kArg{.key = "Arg", .name = "Arg", .assembly = "A", .ns = ""};
inline constexpr const char* kTParam[] = {"T"};
inline constexpr const char* kArgByKey[] = {"@Arg"};
inline constexpr const char* kArgByName[] = {"Arg"};
inline constexpr const char* kArgMissing[] = {"Gone"};
inline constexpr const char* kArgGeneric[] = {"List<Arg>"};
constexpr EntryDesc kPut{.key = "Box<Arg>.Put", .kind = Kind::Inflated, .owner = "Box", .name = "Put", .type = "T",
                         .params = kTParam, .typeArgs = kArgByKey};

// The instance's code is the running game's: the offline check finds the definition and its arguments, and never
// claims more, though a dump writes a shared instantiation's address on the definition.
void inflated_is_checked_at_startup_only() {
  GameModel m = generic_model();
  Resolution r = run({&kBox, &kArg, &kPut}, m);
  const Result* p = r.find("Box<Arg>.Put");
  CHECK(p->status == Status::Missing && checked_at_startup(*p));
  CHECK(p->type == 0 && p->member == 0 && p->typeArgs == std::vector<int>{1} && p->address == 0 && p->rva == 0);
  m.types[0].methods[0].rva = 0x1234;
  r = run({&kBox, &kArg, &kPut}, m);
  p = r.find("Box<Arg>.Put");
  CHECK(p->status == Status::Missing && checked_at_startup(*p) && p->rva == 0);
  constexpr EntryDesc byName{.key = "ByName", .kind = Kind::Inflated, .owner = "Box", .name = "Put", .params = kTParam,
                             .typeArgs = kArgByName};
  r = run({&kBox, &byName}, generic_model());
  CHECK(checked_at_startup(*r.find("ByName")) && r.find("ByName")->typeArgs == std::vector<int>{1});
}

void inflated_fails_closed_on_any_other_shape() {
  const GameModel m = generic_model();
  const auto detail_of = [&](const EntryDesc& d) {
    const Resolution r = run({&kBox, &kArg, &d}, m);
    const Result* x = r.find(d.key);
    CHECK(x->status != Status::Resolved && !checked_at_startup(*x));
    return x->detail;
  };
  constexpr EntryDesc noArgs{.key = "N", .kind = Kind::Inflated, .owner = "Box", .name = "Put", .params = kTParam};
  CHECK(detail_of(noArgs).find("type arguments") != std::string::npos);
  constexpr EntryDesc noOwner{.key = "O", .kind = Kind::Inflated, .name = "Put", .typeArgs = kArgByKey};
  CHECK(detail_of(noOwner).find("owner") != std::string::npos);
  constexpr EntryDesc noName{.key = "M", .kind = Kind::Inflated, .owner = "Box", .typeArgs = kArgByKey};
  CHECK(detail_of(noName).find("name") != std::string::npos);
  constexpr EntryDesc gone{.key = "G", .kind = Kind::Inflated, .owner = "Box", .name = "Put", .typeArgs = kArgMissing};
  CHECK(detail_of(gone).find("Gone") != std::string::npos);
  constexpr EntryDesc compound{.key = "C", .kind = Kind::Inflated, .owner = "Box", .name = "Put", .typeArgs = kArgGeneric};
  CHECK(detail_of(compound).find("single type") != std::string::npos);
  constexpr EntryDesc noMethod{.key = "X", .kind = Kind::Inflated, .owner = "Box", .name = "Take", .typeArgs = kArgByKey};
  detail_of(noMethod);
  static constexpr const char* kTwo[] = {"T", "T"};
  constexpr EntryDesc wrongParams{.key = "W", .kind = Kind::Inflated, .owner = "Box", .name = "Put", .params = kTwo,
                                  .typeArgs = kArgByKey};
  detail_of(wrongParams);
  // Overloads named without params never decide, as for a Method.
  GameModel two = m;
  two.types[0].methods.push_back(two.types[0].methods[0]);
  constexpr EntryDesc anyArity{.key = "A", .kind = Kind::Inflated, .owner = "Box", .name = "Put", .typeArgs = kArgByKey};
  const Resolution over = run({&kBox, &kArg, &anyArity}, two);
  CHECK(over.find("A")->status == Status::Ambiguous);
  // A type argument whose name several types share is never picked.
  GameModel shared = m;
  shared.types.push_back(shared.types[1]);
  constexpr EntryDesc byName{.key = "S", .kind = Kind::Inflated, .owner = "Box", .name = "Put", .typeArgs = kArgByName};
  CHECK(run({&kBox, &byName}, shared).find("S")->status == Status::Ambiguous);
  // An instance's missing definition blocks like any other dependency.
  CHECK(run({&kPut}, m).find("Box<Arg>.Put")->status == Status::Blocked);
}

constexpr EntryDesc kShutdown{.key = "GameAssembly.il2cpp_shutdown", .kind = Kind::Export, .name = "il2cpp_shutdown"};

// An export resolves from the image's export table, as the offline check reads the copied
// GameAssembly.dll.
void export_resolves_offline_from_pe() {
  ImageBuilder ib;
  ib.function(0x2000, std::vector<uint8_t>{0xC3});
  ib.function(0x2010, std::vector<uint8_t>{0xC3});
  ib.exported = {{"il2cpp_init", 0x2010}, {"il2cpp_shutdown", 0x2000}};
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "pm_sdk_resolver_test";
  std::filesystem::create_directories(dir);
  const std::vector<uint8_t> file = ib.file();
  std::ofstream(dir / "GameAssembly.dll", std::ios::binary)
      .write(reinterpret_cast<const char*>(file.data()), static_cast<std::streamsize>(file.size()));
  const std::vector<uint8_t> mapped = map_pe_file(dir / "GameAssembly.dll");
  std::filesystem::remove_all(dir);
  const CodeImage image(mapped);
  CHECK(image.valid());
  CHECK(image.exported("il2cpp_shutdown") == 0x2000);
  CHECK(image.exported("il2cpp_init") == 0x2010);
  const Resolution r = resolve(std::vector<const EntryDesc*>{&kShutdown}, GameModel{}, &image);
  const Result* s = r.find("GameAssembly.il2cpp_shutdown");
  CHECK(s && s->status == Status::Resolved && s->rva == 0x2000 && s->currentName == "il2cpp_shutdown");

  // Without an image there is nothing to read the table from.
  CHECK(run({&kShutdown}, GameModel{}).find("GameAssembly.il2cpp_shutdown")->detail == "no code image");
}

void missing_export_is_missing() {
  ImageBuilder ib;
  ib.function(0x2000, std::vector<uint8_t>{0xC3});
  // A name that only begins with the one wanted, and an export forwarded to another module (its RVA
  // lies inside the export directory, where the forwarder's text is).
  ib.exported = {{"il2cpp_shutdown_now", 0x2000}, {"il2cpp_stop", ImageBuilder::kExports + 4}};
  const std::vector<uint8_t> mapped = ib.build();
  const CodeImage image(mapped);
  CHECK(image.exported("il2cpp_shutdown") == 0);
  CHECK(image.exported("il2cpp_stop") == 0);
  const Resolution r = resolve(std::vector<const EntryDesc*>{&kShutdown}, GameModel{}, &image);
  const Result* s = r.find("GameAssembly.il2cpp_shutdown");
  CHECK(s && s->status == Status::Missing && s->rva == 0);

  // An image without an export directory exports nothing.
  ImageBuilder none;
  none.function(0x2000, std::vector<uint8_t>{0xC3});
  const std::vector<uint8_t> bare = none.build();
  CHECK(CodeImage(bare).exported("il2cpp_shutdown") == 0);
}

// A name table that runs off the image's end, or two exports of one name, never give an address.
void malformed_export_table_exports_nothing() {
  ImageBuilder ib;
  ib.function(0x2000, std::vector<uint8_t>{0xC3});
  ib.exported = {{"il2cpp_shutdown", 0x2000}, {"il2cpp_shutdown", 0x2000}};
  std::vector<uint8_t> twice = ib.build();
  CHECK(CodeImage(twice).exported("il2cpp_shutdown") == 0);

  ib.exported = {{"il2cpp_shutdown", 0x2000}};
  std::vector<uint8_t> broken = ib.build();
  const uint32_t names = ImageBuilder::kExports + 32;
  const uint32_t past = ImageBuilder::kSize - 2;  // the name table's one entry would end past the image
  std::memcpy(broken.data() + names, &past, 4);
  CHECK(CodeImage(broken).exported("il2cpp_shutdown") == 0);
}

void assemblies_named_once_sorted() {
  constexpr EntryDesc kCamera{.key = "Camera", .name = "Camera", .assembly = "UnityEngine.CoreModule"};
  const EntryDesc* entries[] = {&kGhostAI, &kCamera, &kGhostState, &kAgent, &kGhostAI};
  CHECK((assemblies_of(entries) == std::vector<std::string>{"Assembly-CSharp", "UnityEngine.CoreModule"}));
}

// An assembly only optional type entries name may be absent from a build: the model leaves it out and those entries
// stay unresolved, where a required assembly that is absent fails the whole model.
void optional_assemblies_are_those_only_optional_types_name() {
  constexpr EntryDesc kCloth{.key = "Cloth", .flags = kOptional, .name = "Cloth", .assembly = "UnityEngine.ClothModule"};
  constexpr EntryDesc kRare{.key = "Rare", .flags = kOptional, .name = "Rare", .assembly = "UnityEngine.CoreModule"};
  constexpr EntryDesc kCamera{.key = "Camera", .name = "Camera", .assembly = "UnityEngine.CoreModule"};
  const EntryDesc* entries[] = {&kGhostAI, &kCloth, &kRare, &kCamera, &kCloth};
  CHECK((optional_assemblies_of(entries) == std::vector<std::string>{"UnityEngine.ClothModule"}));
  const EntryDesc* required[] = {&kGhostAI, &kCamera};
  CHECK(optional_assemblies_of(required).empty());
  // The ones to read: every assembly named, less an optional one that is absent.
  const std::vector<std::string> all = assemblies_of(entries);
  CHECK((all == std::vector<std::string>{"Assembly-CSharp", "UnityEngine.ClothModule", "UnityEngine.CoreModule"}));
  const auto none = [](const std::string&) { return false; };
  const auto every = [](const std::string&) { return true; };
  CHECK((present_assemblies(entries, none) == std::vector<std::string>{"Assembly-CSharp", "UnityEngine.CoreModule"}));
  CHECK(present_assemblies(entries, every) == all);
}

}  // namespace

int main() {
  type_by_name();
  not_generic_type_among_its_generic_kin();
  method_by_name_and_params();
  an_object_array_is_exact_in_the_game_and_alike_in_the_dump();
  method_by_name_without_params_is_ambiguous_with_overloads();
  method_by_name_ignores_static_among_overloads();
  field_by_name();
  enum_value_by_name();
  at_type_resolves_through_entry();
  at_type_is_the_resolved_type_not_its_name();
  missing_owner_blocks_members();
  dependency_order_is_independent_of_declaration_order();
  cycle_is_missing();
  code_needs_an_image();
  required_resolved_ignores_optional();
  enum_type_by_values();
  type_by_field_types();
  type_by_exact_field_types();
  type_by_field_order();
  type_shape_needs_values_or_field_types();
  type_found_by_a_static_method();
  compiler_generated_types_are_not_shape_candidates();
  field_by_unique_type();
  at_type_inside_generic_arguments();
  field_shape_with_many_candidates_is_ambiguous();
  stale_hint_never_decides();
  hint_cannot_break_a_tie();
  method_by_shape();
  stores_const_picks_cooldown();
  negated_fact_picks_flag();
  negated_fact_needs_this();
  positive_fact_needs_this_in_the_owners_method();
  negated_fact_needs_the_owners_instance_method();
  fact_on_unresolved_method_blocks();
  fact_kinds_must_fit();
  field_facts_on_a_struct_fail_closed();
  field_within_an_embedded_struct();
  struct_field_is_published_at_its_place_in_the_struct();
  within_fails_closed_on_any_other_holder();
  same_field_as_another_within_entry();
  unreadable_code_never_decides();
  folded_alias_is_one_behaviour();
  zero_rva_is_never_an_alias();
  method_without_code_is_missing();
  identical_twins_are_one_behaviour();
  twins_calling_one_helper();
  decoy_with_other_metadata_is_not_a_twin();
  unreadable_code_is_never_a_twin();
  calls_and_called_by_include_twins();
  negated_call_facts_are_refused();
  called_fact_drops_uncalled_decoys();
  field_fact_reads_a_leaf();
  called_fact_fails_closed();
  called_fact_must_fit();
  epilogue_restore_keeps_this();
  self_tail_jump_keeps_this();
  global_class_access_is_no_field();
  compares_fact_keeps_the_method_that_tests_the_value();
  moves_const_fact_keeps_the_method_with_the_number();
  moves_const_fact_fails_closed();
  moves_const_fact_must_fit();
  is_not_fact_narrows_by_a_resolved_field();
  is_not_narrows_a_loads_fact_over_code();
  compares_fact_fails_closed();
  compares_fact_must_fit();
  field_access_narrows_the_shape();
  access_through_another_types_field_is_no_hit();
  pattern_inside_method();
  pattern_needs_readable_code();
  assemblies_named_once_sorted();
  optional_assemblies_are_those_only_optional_types_name();
  static_field_by_shape();
  icall_resolves_by_name();
  icall_without_runtime_is_checked_at_startup();
  type_is_the_one_interface_another_declares();
  inflated_is_checked_at_startup_only();
  inflated_fails_closed_on_any_other_shape();
  export_resolves_offline_from_pe();
  missing_export_is_missing();
  malformed_export_table_exports_nothing();
  return g_failures ? 1 : 0;
}
