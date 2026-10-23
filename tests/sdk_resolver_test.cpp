#include <format>
#include <string>
#include <vector>

#include "check.h"
#include "entry.h"
#include "model.h"
#include "resolver.h"
#include "sdk_fixture.h"

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
  return g_failures ? 1 : 0;
}
