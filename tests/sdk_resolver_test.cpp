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
  return g_failures ? 1 : 0;
}
