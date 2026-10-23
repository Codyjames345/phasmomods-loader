#include <format>
#include <string>
#include <utility>
#include <vector>

#include "check.h"
#include "entry.h"
#include "image.h"
#include "model.h"
#include "resolver.h"
#include "sdk_fixture.h"
#include "sdk_image_fixture.h"

using namespace pg::sdk;

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

void method_by_name_and_params() {
  Game g;
  const GameModel model = g.b.build();
  const Resolution r = run({&kPhysics, &kRaycastRay}, model);
  const Result* ray = r.find("Physics.Raycast");
  CHECK(ray && ray->status == Status::Resolved);
  CHECK(ray && ray->rva == 0x2000);
  CHECK(ray && ray->type == g.physics && ray->member == 0);
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
  CHECK(n && n->detail == "a type found by shape needs values or field types");
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
    code.function(0x1000, join({lead, prologue, call, kHuntingBody,
                                body == Hunting::OtherObject ? other : std::vector<uint8_t>{}, {0x5F},
                                kRet}));  // pop rdi; ret
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

  // A positive fact counts any object: the other store still makes 0xF3 a candidate of StoresConst 1.
  static constexpr Fact cooldown[] = {{.is = Fact::Is::StoresConst, .method = "GhostAI.Hunting", .value = 1}};
  const Result* c = g.flag(cooldown);
  CHECK(c && c->status == Status::Ambiguous && c->candidates.size() == 2);
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

static constexpr const char* kPlayer[] = {"Player"};
constexpr EntryDesc kLookAtPlayer{.key = "GhostAI.LookAtPlayer", .kind = Kind::Method, .owner = "GhostAI",
                                  .name = "LookAtPlayer", .params = kPlayer};

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
  static constexpr Fact neither[] = {{.is = Fact::Is::CalledBy, .method = "GhostAI.Hunting", .negate = true},
                                     {.is = Fact::Is::Calls, .method = "GhostAI.LookAtPlayer", .negate = true}};
  constexpr EntryDesc idle{.key = "GhostAI.Idle", .kind = Kind::Method, .owner = "GhostAI", .type = "int",
                           .facts = neither};
  const Resolution r = code.run({&kGhostAI, &kLookAtPlayer, &kHuntingMethod, &looker, &step, &idle}, m.build());
  CHECK(r.find("GhostAI.LookAtPlayer")->twins == std::vector<uint64_t>{0x1100});
  CHECK(r.find("GhostAI.Looker")->status == Status::Resolved && r.find("GhostAI.Looker")->rva == 0x1300);
  CHECK(r.find("GhostAI.Step")->status == Status::Resolved && r.find("GhostAI.Step")->rva == 0x1400);
  CHECK(r.find("GhostAI.Idle")->status == Status::Resolved && r.find("GhostAI.Idle")->rva == 0x1500);
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

  constexpr EntryDesc imm{.key = "GhostAI.storeCooldown", .kind = Kind::Code, .pattern = "c6 87 ?? ?? ?? ?? 01",
                          .within = "GhostAI.Hunting", .patternOffset = 6};
  CHECK(g.code.run({&kGhostAI, &kHuntingMethod, &imm}, model).find("GhostAI.storeCooldown")->rva == 0x1016);

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

}  // namespace

int main() {
  type_by_name();
  method_by_name_and_params();
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
  type_shape_needs_values_or_field_types();
  compiler_generated_types_are_not_shape_candidates();
  field_by_unique_type();
  field_shape_with_many_candidates_is_ambiguous();
  stale_hint_never_decides();
  hint_cannot_break_a_tie();
  method_by_shape();
  stores_const_picks_cooldown();
  negated_fact_picks_flag();
  negated_fact_needs_this();
  negated_fact_needs_the_owners_instance_method();
  fact_on_unresolved_method_blocks();
  fact_kinds_must_fit();
  field_facts_on_a_struct_fail_closed();
  unreadable_code_never_decides();
  folded_alias_is_one_behaviour();
  identical_twins_are_one_behaviour();
  twins_calling_one_helper();
  decoy_with_other_metadata_is_not_a_twin();
  unreadable_code_is_never_a_twin();
  calls_and_called_by_include_twins();
  pattern_inside_method();
  return g_failures ? 1 : 0;
}
