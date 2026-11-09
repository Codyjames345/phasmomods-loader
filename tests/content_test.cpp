// The content registry (sdk/runtime/content_registry.h) through the content wrapper (sdk/api/content.h)
// on the fake backend: ghost and evidence registration and its refusals and limits, the registration
// window and commit, the numbering in byte order of the ids, vanilla ids, the check of the game's
// enums, and the lobby rule (active, pending, off; resends; an emptied manifest), with the room's
// peers faked by registries of their own whose manifests are delivered as the host's channel would
// deliver them. Then the game side of the ghost types (host/content_types.h) over a faked game: the
// collection, the name keys, identity and behaviour, the Mimic's list and copies, the acting-as
// dispatch of rules and its sync to other clients, the hunt speed's inputs, and withdrawal. Then custom
// evidence (host/content_evidence.h): given and withheld through the game's own pick and sync, faked as
// decompiled, and the contract ghost's look of an evidence.
// Run: build\content_test.exe
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Difficulty.h"
#include "GhostAI.h"
#include "GhostCollection.h"
#include "GhostController.h"
#include "GhostTypes.h"
#include "GhostHuntState.h"
#include "GhostInfo.h"
#include "LevelController.h"
#include "LocalisationSystem.h"
#include "check.h"
#include "content.h"
#include "content_acting.h"
#include "content_evidence.h"
#include "content_registry.h"
#include "content_types.h"
#include "fake_game.h"
#include "game_api.h"
#include "game_enums.h"
#include "ghost.h"
#include "log.h"
#include "text_fixture.h"
#include "runtime.h"

// Hooks' detours hold a hooks::Guard; a mod's glue counts them, a test does here.
namespace pm::hooks {
std::atomic<int> g_inflight{0};
}  // namespace pm::hooks

namespace {

namespace api = pm::api;
namespace content = pm::api::content;
namespace fake = pm::fake;
namespace registry = pm::content;
using registry::State;

std::vector<std::string> g_lines;
void capture(const char* line) { g_lines.emplace_back(line); }

bool logged(const std::string& text) {
  return std::any_of(g_lines.begin(), g_lines.end(), [&](const std::string& l) { return l.find(text) != std::string::npos; });
}

// A fresh fake game with the content wrapper bound to it; registration is open, as while a mod starts.
void start() {
  fake::reset();
  std::string missing;
  api::bind(fake::host(), missing);
  g_lines.clear();
  pm::log::set_sink(&capture, "");
}

content::GhostDef ghost(const std::string& id, const std::string& firstEvidence = "emf") {
  content::GhostDef g;
  g.id = id;
  g.name = content::Text("en", "Ghost " + id);
  g.description = content::Text("en", "It haunts.");
  g.strength = content::Text("en", "It is strong.");
  g.weakness = content::Text("en", "It is weak.");
  g.tells = {content::Text("en", "It tells.")};
  g.evidence = {firstEvidence, "orbs", "freezing"};
  return g;
}

content::EvidenceDef evidence(const std::string& id) {
  content::EvidenceDef e;
  e.id = id;
  e.name = content::Text("en", "Evidence " + id);
  e.journalLine = content::Text("en", id);
  return e;
}

// A registry with the audited build's enums, as the host gives its own at startup.
registry::Registry fresh() {
  registry::Registry r;
  registry::GameEnums e;
  CHECK(registry::read_game_enums(fake::audited_ghost_types(), fake::audited_evidence(), fake::kAuditedNoGhost,
                                  fake::kAuditedNoEvidence, e)
            .empty());
  r.set_game_enums(std::move(e));
  return r;
}

// A peer in the room: a registry of its own, in the room as `actor`, holding the content one mod of
// its registered. Its manifest reaches the fake host as the host's channel would carry it.
struct Peer {
  int actor;
  registry::Registry registry = fresh();

  void runs(const std::vector<content::EvidenceDef>& evidence, const std::vector<content::GhostDef>& ghosts) {
    registry.begin_group();
    for (const content::EvidenceDef& e : evidence) CHECK(registry.add_evidence(&e.c_def()) > 0);
    for (const content::GhostDef& g : ghosts) CHECK(registry.add_ghost(&g.c_def()) > 0);
    registry.end_group(true);
    registry.commit();
  }
};

// Every client sees the room as `actors` at `now`, with the master `master` (else the lowest actor), and each peer's due
// manifest reaches this client.
void room(int local, const std::vector<int>& actors, const std::vector<Peer*>& peers, uint64_t now = 0,
          int master = registry::Registry::kLowestActor) {
  fake::room(local, actors, now, master);
  for (Peer* p : peers) {
    p->registry.observe(p->actor, actors, now, master);
    for (const std::vector<uint8_t>& message : p->registry.take_messages())
      fake::host_message(p->actor, message.data(), message.size());
  }
}

void ghost_ids_number_in_byte_order() {
  start();
  CHECK(content::register_ghost(ghost("b.x")) > 0);
  CHECK(content::register_ghost(ghost("a.y")) > 0);
  CHECK(content::register_ghost(ghost("a.z")) > 0);
  room(1, {1}, {});
  CHECK(!content::active());  // nothing is in play before every mod has started
  fake::commit_content();
  CHECK(content::active());  // alone in a private room: this client is the host
  CHECK(content::type_of("a.y") == 31);
  CHECK(content::type_of("a.z") == 32);
  CHECK(content::type_of("b.x") == 33);
  CHECK(content::id_of(31) == "a.y" && content::id_of(32) == "a.z" && content::id_of(33) == "b.x");
  CHECK(content::id_of(34).empty());
  CHECK(content::type_of("c.w") == -1);
}

void evidence_numbers_from_8() {
  start();
  CHECK(content::register_evidence(evidence("z.flame")) > 0);
  CHECK(content::register_evidence(evidence("a.salt")) > 0);
  CHECK(content::register_ghost(ghost("a.ghost", "z.flame")) > 0);
  fake::commit_content();
  room(1, {1}, {});
  CHECK(content::evidence_of("a.salt") == 8);
  CHECK(content::evidence_of("z.flame") == 9);
  CHECK(content::evidence_of("emf") == 1 && content::evidence_of("dots") == 7);  // the game's own
  CHECK(content::evidence_of("y.none") == -1);
  CHECK(content::type_of("a.ghost") == 31);
}

void duplicate_id_is_refused() {
  start();
  const int32_t first = content::register_ghost(ghost("a.x"));
  CHECK(first > 0);
  CHECK(content::register_ghost(ghost("a.x")) == 0);
  CHECK(logged("a.x") && logged("already registered"));
  CHECK(content::register_evidence(evidence("a.e")) > 0);
  CHECK(content::register_evidence(evidence("a.e")) == 0);
  CHECK(content::register_ghost(ghost("a.y")) != first);  // every handle is its own
}

void missing_english_is_refused() {
  start();
  content::GhostDef g = ghost("a.x");
  g.name = content::Text("fr", "Fantome");
  CHECK(content::register_ghost(g) == 0);
  CHECK(logged("a.x") && logged("English"));
  g = ghost("a.x");
  g.tells.push_back(content::Text("de", "Es verrät sich."));
  CHECK(content::register_ghost(g) == 0);
  g = ghost("a.x");
  g.abilityName = content::Text("fr", "Parler");  // an ability needs English too
  g.abilityDescription = content::Text("en", "It speaks.");
  CHECK(content::register_ghost(g) == 0);
  content::EvidenceDef e = evidence("a.e");
  e.journalLine = content::Text("fr", "Flamme");
  CHECK(content::register_evidence(e) == 0);
  // English with other languages beside it, and no ability at all, are accepted.
  g = ghost("a.x");
  g.name.add("fr", "Fantome");
  g.abilityName = {};
  g.abilityDescription = {};
  CHECK(content::register_ghost(g) > 0);
}

void over_the_limits_is_refused() {
  start();
  content::GhostDef g = ghost("a.tells");
  g.tells.assign(registry::kMaxTells + 1, content::Text("en", "It tells."));
  CHECK(content::register_ghost(g) == 0 && logged("more than 64 tells"));
  g = ghost("a.languages");
  for (uint32_t i = 0; i < registry::kMaxLanguages; ++i) g.name.add("l" + std::to_string(i), "Name");
  CHECK(content::register_ghost(g) == 0 && logged("more than 64 languages"));
  g = ghost("a.long");
  g.description = content::Text("en", std::string(registry::kMaxTextBytes + 1, 'x'));
  CHECK(content::register_ghost(g) == 0 && logged("longer than 64 KB"));
  // At the limits it is accepted.
  g = ghost("a.full");
  g.tells.assign(registry::kMaxTells, content::Text("en", "It tells."));
  g.description = content::Text("en", std::string(registry::kMaxTextBytes, 'x'));
  for (uint32_t i = 1; i < registry::kMaxLanguages; ++i) g.name.add("l" + std::to_string(i), "Name");
  CHECK(content::register_ghost(g) > 0);
}

void unknown_evidence_is_refused() {
  start();
  CHECK(content::register_ghost(ghost("a.x", "salt")) == 0);  // neither vanilla nor a content id
  CHECK(logged("a.x") && logged("salt"));
  // A content id nobody registers: accepted for now, dropped once every mod has started.
  CHECK(content::register_ghost(ghost("a.y", "b.flame")) > 0);
  content::GhostDef none = ghost("a.z", "");
  none.evidence = {"", "", ""};  // a ghost may give no evidence at all
  CHECK(content::register_ghost(none) > 0);
  fake::commit_content();
  room(1, {1}, {});
  CHECK(logged("the ghost \"a.y\" is dropped") && logged("b.flame"));
  CHECK(content::type_of("a.y") == -1 && content::type_of("a.z") == 31);
}

// A ghost may give an evidence a mod that starts after its own registers, and one that started before.
void evidence_from_any_started_mod() {
  for (bool evidenceFirst : {false, true}) {
    g_lines.clear();
    registry::Registry r = fresh();
    const content::EvidenceDef flame = evidence("flames.flame");
    const content::GhostDef hydra = ghost("ghosts.hydra", "flames.flame");
    for (int mod = 0; mod < 2; ++mod) {
      r.begin_group();
      if ((mod == 0) == evidenceFirst) CHECK(r.add_evidence(&flame.c_def()) > 0);
      else CHECK(r.add_ghost(&hydra.c_def()) > 0);
      r.end_group(true);
    }
    r.commit();
    r.observe(1, {1}, 0);
    CHECK(r.active() && r.type_of("ghosts.hydra") == 31 && r.evidence_of("flames.flame") == 8);
    CHECK(!logged("dropped"));
  }
}

void registration_outside_a_start_is_refused() {
  start();
  fake::commit_content();
  CHECK(content::register_ghost(ghost("a.late")) == 0);
  CHECK(content::register_evidence(evidence("a.late")) == 0);
  CHECK(logged("the ghost \"a.late\" is refused: content is registered only while its mod starts"));
  registry::Registry r = fresh();  // no mod starting
  const content::GhostDef g = ghost("a.x");
  CHECK(r.add_ghost(&g.c_def()) == 0);
}

// Content follows this client's match with the host. Two content mods, each with a ghost, on this client, and a player with
// only the first. Under that player as host, all of it is off. With this client as host, that same player changes nothing:
// a joiner's content is never the room's, and the join guard deals with the joiner.
void content_follows_the_host_and_no_other_player() {
  start();
  CHECK(content::register_ghost(ghost("brownies.brownie")) > 0);
  CHECK(content::register_ghost(ghost("echoes.echo")) > 0);
  fake::commit_content();
  Peer peer{2};
  peer.runs({}, {ghost("brownies.brownie")});
  room(1, {1, 2}, {&peer}, 0, 2);
  CHECK(!content::active() && fake::content_state() == State::Off);
  CHECK(content::type_of("brownies.brownie") == -1 && content::type_of("echoes.echo") == -1);
  CHECK(content::id_of(31).empty());
  CHECK(content::type_of("vanilla.mare") == 6);  // vanilla ghosts stay
  room(1, {1, 2}, {&peer}, 0, 1);
  CHECK(content::active() && fake::content_state() == State::Active);
  CHECK(content::type_of("brownies.brownie") == 31 && content::type_of("echoes.echo") == 32);
}

void manifests_equal_activates() {
  start();
  CHECK(content::register_evidence(evidence("flames.flame")) > 0);
  CHECK(content::register_ghost(ghost("brownies.brownie", "flames.flame")) > 0);
  fake::commit_content();
  Peer peer{2};
  peer.runs({evidence("flames.flame")}, {ghost("brownies.brownie", "flames.flame")});
  room(1, {1, 2}, {&peer}, 0, 2);
  CHECK(content::active() && fake::content_state() == State::Active);
  CHECK(content::type_of("brownies.brownie") == 31 && content::evidence_of("flames.flame") == 8);
  // The host's own manifest goes out when somebody joins, for them to judge the same way.
  CHECK(!fake::host_messages().empty());
  // Same ids but another text (another build of the mod): not equal.
  start();
  CHECK(content::register_ghost(ghost("brownies.brownie")) > 0);
  fake::commit_content();
  Peer other{2};
  content::GhostDef changed = ghost("brownies.brownie");
  changed.weakness = content::Text("en", "Salt.");
  other.runs({}, {changed});
  room(1, {1, 2}, {&other}, 0, 2);
  CHECK(!content::active());
  // Outside a room nothing is in play.
  room(0, {}, {});
  CHECK(!content::active() && fake::content_state() == State::Off);
}

// A host not heard from yet keeps content pending, not off, for kGraceMs; then it is off. Another player's silence or content
// changes nothing, and a host who disagrees turns it off at once.
void pending_until_heard_then_off() {
  start();
  CHECK(content::register_ghost(ghost("brownies.brownie")) > 0);
  fake::commit_content();
  room(1, {1, 2}, {}, 1000, 2);
  CHECK(fake::content_state() == State::Pending && !content::active());
  CHECK(content::type_of("brownies.brownie") == -1);
  room(1, {1, 2}, {}, 1000 + registry::kGraceMs - 1, 2);
  CHECK(fake::content_state() == State::Pending);
  room(1, {1, 2}, {}, 1000 + registry::kGraceMs, 2);
  CHECK(fake::content_state() == State::Off);
  // Heard from late, and agreeing: active after all.
  Peer peer{2};
  peer.runs({}, {ghost("brownies.brownie")});
  room(1, {1, 2}, {&peer}, 1000 + registry::kGraceMs, 2);
  CHECK(fake::content_state() == State::Active);
  // A third player joins with other content, and a fourth is unheard: nothing changes for this client.
  Peer third{3};
  third.runs({}, {ghost("echoes.echo")});
  room(1, {1, 2, 3, 4}, {&peer, &third}, 40000, 2);
  CHECK(fake::content_state() == State::Active);
  // Now the third takes over as host with that other content: this client's is off, at once.
  room(1, {1, 3, 4}, {&third}, 40000, 3);
  CHECK(fake::content_state() == State::Off);
}

// A host whose content goes empty says so, so the others do not keep its old manifest and stay on.
void emptied_content_is_announced() {
  start();
  CHECK(content::register_ghost(ghost("brownies.brownie")) > 0);
  fake::commit_content();
  Peer peer{2};
  peer.runs({}, {ghost("brownies.brownie")});
  room(1, {1, 2}, {&peer}, 0, 2);
  CHECK(content::active());
  peer.registry.clear();  // its mods stopped
  room(1, {1, 2}, {&peer}, 0, 2);
  CHECK(!content::active() && fake::content_state() == State::Off);
  // A client that never had content says nothing.
  registry::Registry silent = fresh();
  silent.observe(3, {1, 3}, 0);
  CHECK(silent.take_messages().empty());
}

// The manifest goes out at once, then every kAnnounceMs, and kRetryMs after a send that failed.
void manifest_is_resent_and_retried() {
  Peer p{2};
  p.runs({}, {ghost("brownies.brownie")});
  p.registry.observe(2, {1, 2}, 1000);
  CHECK(!p.registry.take_messages().empty());
  CHECK(p.registry.take_messages().empty());
  p.registry.observe(2, {1, 2}, 1000 + registry::kAnnounceMs - 1);
  CHECK(p.registry.take_messages().empty());
  p.registry.observe(2, {1, 2}, 1000 + registry::kAnnounceMs);
  CHECK(!p.registry.take_messages().empty());
  p.registry.send_failed();
  p.registry.observe(2, {1, 2}, 1000 + registry::kAnnounceMs + registry::kRetryMs - 1);
  CHECK(p.registry.take_messages().empty());
  p.registry.observe(2, {1, 2}, 1000 + registry::kAnnounceMs + registry::kRetryMs);
  CHECK(!p.registry.take_messages().empty());
  p.registry.observe(2, {1, 2, 3}, 1000 + registry::kAnnounceMs + registry::kRetryMs);  // a joiner: at once
  CHECK(!p.registry.take_messages().empty());
}

// A manifest too long for one message arrives in parts and is judged whole.
void long_manifest_arrives_in_parts() {
  start();
  std::vector<content::GhostDef> defs;
  for (int i = 0; i < 40; ++i) defs.push_back(ghost("a-rather-long-mod-identifier.ghost_number_" + std::to_string(i)));
  for (const content::GhostDef& d : defs) CHECK(content::register_ghost(d) > 0);
  fake::commit_content();
  Peer peer{2};
  peer.runs({}, defs);
  peer.registry.observe(2, {1, 2}, 0);
  const std::vector<std::vector<uint8_t>> parts = peer.registry.take_messages();
  CHECK(parts.size() > 1);
  fake::room(1, {1, 2}, 0, 2);
  for (size_t i = 0; i + 1 < parts.size(); ++i) fake::host_message(2, parts[i].data(), parts[i].size());
  CHECK(!content::active());  // not every part yet
  fake::host_message(2, parts.back().data(), parts.back().size());
  CHECK(content::active() && content::type_of("a-rather-long-mod-identifier.ghost_number_9") == 31 + 39);
}

// A mod that fails to start takes the content it registered with it, as it does its hooks.
void failed_start_withdraws_its_content() {
  registry::Registry r = fresh();
  const content::EvidenceDef flame = evidence("flames.flame");
  const content::GhostDef kept = ghost("brownies.brownie"), dropped = ghost("echoes.echo", "flames.flame");
  r.begin_group();
  CHECK(r.add_ghost(&kept.c_def()) > 0);
  r.end_group(true);
  r.begin_group();
  CHECK(r.add_evidence(&flame.c_def()) > 0 && r.add_ghost(&dropped.c_def()) > 0);
  r.end_group(false);
  r.commit();
  r.observe(1, {1}, 0);
  CHECK(r.active() && r.type_of("brownies.brownie") == 31);
  CHECK(r.type_of("echoes.echo") == -1 && r.evidence_of("flames.flame") == -1);
}

void vanilla_ids_answer() {
  start();
  CHECK(content::id_of(6) == "vanilla.mare");
  CHECK(content::id_of(0) == "vanilla.spirit" && content::id_of(17) == "vanilla.thetwins" &&
        content::id_of(30) == "vanilla.deildegast");
  CHECK(content::id_of(24).empty());  // None is no ghost
  CHECK(content::type_of("vanilla.mare") == 6);
  CHECK(content::register_ghost(ghost("vanilla.mare")) == 0);  // the vanilla ids are the game's
}

// Custom numbers begin one past the game's own largest values, and the vanilla ids come from the
// values' names: a game update that adds a ghost type or an evidence moves them with no change here.
void game_enums_number_content() {
  registry::GameEnums e;
  CHECK(registry::read_game_enums(fake::audited_ghost_types(), fake::audited_evidence(), fake::kAuditedNoGhost,
                                  fake::kAuditedNoEvidence, e).empty());
  CHECK(e.firstGhostType == 31 && e.firstEvidence == 8 && e.ghost_id(6) == std::string("vanilla.mare"));
  CHECK(!e.ghost_id(24) && e.evidence_value("emf") == 1 && e.evidence_value("dots") == 7 && e.evidence_value("none") == -1);
  // An update adds a ghost type and an evidence.
  registry::EnumValues ghostValues = fake::audited_ghost_types(), evidenceValues = fake::audited_evidence();
  ghostValues.emplace_back("Wisp", 31);
  evidenceValues.emplace_back("Salt", 8);
  CHECK(registry::read_game_enums(ghostValues, evidenceValues, fake::kAuditedNoGhost, fake::kAuditedNoEvidence, e).empty());
  CHECK(e.firstGhostType == 32 && e.firstEvidence == 9);
  CHECK(e.ghost_type("vanilla.wisp") == 31 && e.evidence_value("salt") == 8);
  // A value whose name is obfuscated gets no id, and is listed, rather than being guessed at.
  ghostValues = fake::audited_ghost_types();
  ghostValues[6].first = "\xe0\xb4\x9e\xe0\xb4\xa7";
  CHECK(registry::read_game_enums(ghostValues, fake::audited_evidence(), fake::kAuditedNoGhost, fake::kAuditedNoEvidence, e)
            .empty());
  CHECK(!e.ghost_id(6) && e.ghost_id(7) == std::string("vanilla.revenant") && e.unusable.size() == 1);
  CHECK(e.firstGhostType == 31);
  // Enums that cannot be read.
  CHECK(!registry::read_game_enums({}, fake::audited_evidence(), 24, 0, e).empty() && !e.read());
  CHECK(!registry::read_game_enums(fake::audited_ghost_types(), {{"Huge", 5000000000}}, 24, 0, e).empty());
}

// Through the fake's enums: an update with an extra value numbers custom content one higher, and its
// new vanilla id answers.
void extra_enum_value_moves_numbering() {
  start();
  registry::EnumValues ghostValues = fake::audited_ghost_types(), evidenceValues = fake::audited_evidence();
  ghostValues.emplace_back("Wisp", 31);
  evidenceValues.emplace_back("Salt", 8);
  fake::game_enums(ghostValues, evidenceValues, fake::kAuditedNoGhost, fake::kAuditedNoEvidence);
  CHECK(content::register_evidence(evidence("flames.flame")) > 0);
  CHECK(content::register_ghost(ghost("brownies.brownie", "salt")) > 0);  // the new vanilla evidence
  fake::commit_content();
  room(1, {1}, {});
  CHECK(content::active());
  CHECK(content::type_of("brownies.brownie") == 32 && content::id_of(32) == "brownies.brownie");
  CHECK(content::evidence_of("flames.flame") == 9 && content::evidence_of("salt") == 8);
  CHECK(content::id_of(31) == "vanilla.wisp" && content::type_of("vanilla.wisp") == 31);
}

// The host reads the enums whole from the resolved map's model, as the offline check reads the dump's.
void game_enums_read_from_the_model() {
  pm::sdk::GameModel model;
  const auto enum_type = [&](const registry::EnumValues& values) {
    pm::sdk::ModelType t;
    t.isEnum = true;
    t.fields.push_back({.name = "value__", .type = "int"});
    for (const auto& [name, value] : values)
      t.fields.push_back({.name = name, .type = "E", .isStatic = true, .hasLiteral = true, .literal = value});
    model.types.push_back(t);
  };
  enum_type(fake::audited_ghost_types());
  enum_type(fake::audited_evidence());
  const pm::sdk::EntryDesc ghostType{.key = "GhostType"}, evidenceType{.key = "EvidenceType"},
      noGhost{.key = "GhostType.None"}, noEvidence{.key = "EvidenceType.None"};
  pm::sdk::Resolution resolution;
  resolution.results.push_back({.desc = &ghostType, .status = pm::sdk::Status::Resolved, .type = 0});
  resolution.results.push_back({.desc = &evidenceType, .status = pm::sdk::Status::Resolved, .type = 1});
  resolution.results.push_back({.desc = &noGhost, .status = pm::sdk::Status::Resolved, .value = 24});
  resolution.results.push_back({.desc = &noEvidence, .status = pm::sdk::Status::Resolved, .value = 0});
  registry::GameEnums e;
  CHECK(registry::read_game_enums(model, resolution, e).empty());  // value__ is no value
  CHECK(e.firstGhostType == 31 && e.firstEvidence == 8 && !e.ghost_id(24) && e.ghost_id(6) == std::string("vanilla.mare"));
  resolution.results[1].status = pm::sdk::Status::Missing;
  CHECK(registry::read_game_enums(model, resolution, e) == "the map's EvidenceType enum did not resolve");
  resolution.results[1].status = pm::sdk::Status::Resolved;
  resolution.results[2].status = pm::sdk::Status::Missing;
  CHECK(!registry::read_game_enums(model, resolution, e).empty() && !e.read());
}

// When the game's enums cannot be read, content never activates, vanilla ids do not answer, the log says
// why, and a custom ghost is refused, so its mod stays off and this client has no content to tell the others of.
void unreadable_enums_keep_content_off() {
  start();
  fake::game_enums({}, fake::audited_evidence(), fake::kAuditedNoGhost, fake::kAuditedNoEvidence);
  CHECK(logged("custom content stays off: the game's GhostType enum has no values"));
  CHECK(content::register_ghost(ghost("brownies.brownie")) == 0);  // refused: its mod stays off (host/mods.h)
  CHECK(logged("the ghost \"brownies.brownie\" is refused: custom content is off: the game's GhostType enum has no values"));
  fake::commit_content();
  room(1, {1}, {});
  CHECK(!content::active() && fake::content_state() == State::Off);
  CHECK(content::type_of("brownies.brownie") == -1 && content::id_of(6).empty() && content::evidence_of("emf") == -1);
  // It registered nothing, so it says nothing, as a host with no content: its peer waits, then turns off too.
  Peer other{1};
  other.runs({}, {ghost("brownies.brownie")});
  other.registry.observe(1, {1, 2}, 0, 2);
  fake::room(2, {1, 2}, 0, 2);
  CHECK(fake::host_messages().empty());
  CHECK(other.registry.state() == State::Pending);
  other.registry.observe(1, {1, 2}, registry::kGraceMs, 2);
  CHECK(other.registry.state() == State::Off);
}

void player_controlled_reads_playable_ghost() {
  start();
  CHECK(!content::player_controlled());
  fake::player_controlled(true);
  CHECK(content::player_controlled());
}

// The contract's room_agrees answers from the host, so 2.x mods load. A mod has no need of it.
void room_agreement_is_the_hosts() {
  start();
  CHECK(!content::room_agrees());
  fake::room_agrees(true);
  CHECK(content::room_agrees());
}

void before_bind_answers_inactive() {
  fake::reset();
  std::string missing;
  api::bind(nullptr, missing);
  CHECK(content::register_ghost(ghost("a.x")) == 0);
  CHECK(content::type_of("vanilla.mare") == -1 && content::id_of(6).empty() && !content::active());
  CHECK(content::attach_rules("vanilla.mare", {}) == 0 && content::acting_as() == -1 && content::identity() == -1);
  CHECK(!content::room_agrees() && !content::held_back(0) && !content::target() && content::flags() == 0);
}

// Two clients whose games read different enums would number the same content differently: the enums'
// first custom numbers travel in the manifest, so they never agree.
void manifests_from_other_enums_disagree() {
  start();
  CHECK(content::register_ghost(ghost("brownies.brownie")) > 0);
  fake::commit_content();
  Peer other{2};
  registry::EnumValues ghostValues = fake::audited_ghost_types();
  ghostValues.emplace_back("Wisp", 31);
  registry::GameEnums e;
  CHECK(registry::read_game_enums(ghostValues, fake::audited_evidence(), fake::kAuditedNoGhost,
                                  fake::kAuditedNoEvidence, e).empty());
  other.registry.set_game_enums(std::move(e));
  other.runs({}, {ghost("brownies.brownie")});
  room(1, {1, 2}, {&other}, 0, 2);
  CHECK(fake::content_state() == State::Off);
  // With the same enums the same content agrees.
  start();
  CHECK(content::register_ghost(ghost("brownies.brownie")) > 0);
  fake::commit_content();
  Peer same{2};
  same.runs({}, {ghost("brownies.brownie")});
  room(1, {1, 2}, {&same}, 0, 2);
  CHECK(fake::content_state() == State::Active);
}

// A value name that would give the id of another value names neither for certain: the contract's own
// evidence keeps its id, and the newcomer gets none, logged.
void colliding_enum_names_are_unusable() {
  registry::EnumValues evidenceValues = fake::audited_evidence(), ghostValues = fake::audited_ghost_types();
  evidenceValues.emplace_back("Orbs", 8);
  ghostValues.emplace_back("MARE", 31);
  registry::GameEnums e;
  CHECK(registry::read_game_enums(ghostValues, evidenceValues, fake::kAuditedNoGhost, fake::kAuditedNoEvidence, e)
            .empty());
  CHECK(e.evidence_value("orbs") == 4);
  CHECK(e.ghost_type("vanilla.mare") == -1 && !e.ghost_id(6) && !e.ghost_id(31));  // two Mares: neither
  CHECK(std::count(e.unusable.begin(), e.unusable.end(), "Orbs") == 1);
  CHECK(std::count(e.unusable.begin(), e.unusable.end(), "MARE") == 1 &&
        std::count(e.unusable.begin(), e.unusable.end(), "Mare") == 1);
  CHECK(e.firstEvidence == 9 && e.firstGhostType == 32);
  start();
  fake::game_enums(ghostValues, evidenceValues, fake::kAuditedNoGhost, fake::kAuditedNoEvidence);
  CHECK(logged("the game's enum value \"Orbs\" has no usable name"));
}

void enum_ranges_are_logged() {
  start();
  fake::game_enums(fake::audited_ghost_types(), fake::audited_evidence(), fake::kAuditedNoGhost,
                   fake::kAuditedNoEvidence);
  CHECK(logged("content: GhostType 0..30, EvidenceType 0..7"));
}

// Rules attach while a mod starts, to a custom id or one of the game's vanilla ids; rules on a custom
// ghost no started mod registered are dropped once every mod has started.
void rules_attach_and_are_refused() {
  start();
  content::GhostRules rules;
  CHECK(content::attach_rules("vanilla.mare", rules) > 0);
  CHECK(content::attach_rules("brownies.brownie", rules) > 0);
  CHECK(content::attach_rules("echoes.echo", rules) > 0);
  CHECK(content::attach_rules("vanilla.wisp", rules) == 0);  // the game has no such type
  CHECK(content::attach_rules("nodot", rules) == 0);
  CHECK(logged("rules on \"vanilla.wisp\" are refused: the game has no such ghost type"));
  CHECK(content::register_ghost(ghost("brownies.brownie")) > 0);
  fake::commit_content();
  CHECK(logged("rules on \"echoes.echo\" are dropped"));
  CHECK(fake::content_registry().rules_for("brownies.brownie").size() == 1);
  CHECK(fake::content_registry().rules_for("echoes.echo").empty());
  CHECK(content::attach_rules("vanilla.mare", rules) == 0);  // the start is over
}

// The SDK's worked example (sdk/examples/example_ghost, sdk/guides/adding-a-ghost.md) registers its ghost with its journal
// page from its table, and its rules keep the Knell's beat: a surge of a second, a crawl of a second, and a fresh beat for
// the next hunt.
void the_example_ghost_registers_and_keeps_its_beat() {
  start();
  CHECK(content::register_evidence(evidence("phasmomods.spectral-flame.flame")) > 0);
  example::Beat beat;
  CHECK(example::register_knell(beat));
  fake::commit_content();
  const registry::Ghost* g = fake::content_registry().registered(example::kGhostId);
  CHECK(g != nullptr);
  if (g) {
    CHECK((g->name == registry::Text{{"en", "Knell"}}));
    CHECK(g->evidence[0] == "spirit_box" && g->evidence[1] == "orbs" && g->evidence[2] == "phasmomods.spectral-flame.flame");
    CHECK(g->abilityName.empty());
  }
  const std::vector<PmGhostRules> rules = fake::content_registry().rules_for(example::kGhostId);
  CHECK(rules.size() == 1 && rules[0].huntSpeed == &example::hunt_speed && rules[0].tick == &example::tick &&
        rules[0].stop == &example::stop && rules[0].user == &beat);
  // The beat itself, on a clock of the test's own: the host asks the speed and ticks once a frame through a hunt.
  example::Beat b;
  auto frame = [&b](double now) {
    const float v = b.speed(1.7f, now);
    b.frame();
    return v;
  };
  CHECK(frame(10.0) == 1.7f * example::kFast);  // a hunt starts with a surge
  CHECK(frame(10.4) == 1.7f * example::kFast);
  CHECK(frame(10.8) == 1.7f * example::kFast);
  CHECK(frame(11.2) == 1.7f * example::kSlow);  // then a crawl
  CHECK(frame(11.6) == 1.7f * example::kSlow);
  CHECK(frame(12.0) == 1.7f * example::kFast);  // and over again
  CHECK(frame(12.4) == 1.7f * example::kFast);
  b.frame();                                    // a frame with no hunt ends it
  CHECK(b.start < 0);
  CHECK(frame(31.1) == 1.7f * example::kFast);  // so the next hunt starts its own beat
  CHECK(frame(32.2) == 1.7f * example::kSlow);
  example::stop(&b);                            // the rules stop: nothing of this beat is left for their next start
  CHECK(b.start < 0 && !b.asked);
  // The rule holds the line-of-sight speed-up off.
  float base = 1.7f, los = 1.4f;
  CHECK(example::hunt_speed(&base, &los, &b) && los == 1.0f && base > 0);
}

// Whether this client's mods equal the host's list (sdk/runtime/mod_agreement.h): the one answer the host gates every mod's
// rules, journal lines and content on, held through a contract. This client is a joiner, player 2 its host, unless a test says
// otherwise.
namespace agreement {

const std::vector<registry::ModBuild> kOurs{{"ghost-tells", "00000000000000aa"}, {"flames", "00000000000000bb"}};

// Presence's revision as each observe below passes it: each hears something new.
unsigned g_heard = 0;

// What each other player announces now: a player missing from `lists` is not heard from now.
registry::Agreement::ModsOf announced(std::map<int, std::vector<registry::ModBuild>> lists) {
  return [lists](int actor) -> std::optional<std::vector<registry::ModBuild>> {
    const auto it = lists.find(actor);
    if (it == lists.end()) return std::nullopt;
    return it->second;
  };
}

int g_starts = 0, g_ticks = 0, g_stops = 0;
void on_start(void*) { ++g_starts; }
void on_tick(void*) { ++g_ticks; }
void on_stop(void*) { ++g_stops; }

// A client running two mods, one of which gave the Mare rules and a journal line and a custom ghost, under
// the host player 2, whose content manifest is the same: only the answer decides.
struct Gated {
  registry::Agreement agreement;
  Peer peer{2};

  Gated() {
    start();
    g_starts = g_ticks = g_stops = 0;
    content::GhostRules rules;
    rules.start = &on_start;
    rules.tick = &on_tick;
    rules.stop = &on_stop;
    CHECK(content::attach_rules("vanilla.mare", rules) > 0);
    CHECK(content::add_journal_lines("vanilla.mare", {content::Text("en", "Tells: dark.")}) > 0);
    CHECK(content::register_ghost(ghost("brownies.brownie")) > 0);
    fake::commit_content();
    fake::content_registry().use_agreement(&agreement);
    agreement.set_local(kOurs);
    peer.runs({}, {ghost("brownies.brownie")});
    room(1, {1, 2}, {&peer}, 0, 2);
  }
  bool rules() const { return !fake::content_registry().rules_for("vanilla.mare").empty(); }
  bool lines() const { return !fake::content_registry().lines_for("vanilla.mare").empty(); }
};

// A host without one of the mods: no lines, no rules, and the content waits; the rules of a contract's
// Mare never start. (The join guard takes such a joiner out of the room: this is the moment before.)
void a_host_without_a_mod_gets_nothing_run() {
  Gated g;
  g.agreement.observe(true, false, 1, 2, {2}, ++g_heard, announced({{2, {kOurs[1]}}}));  // only the second mod
  CHECK(!g.agreement.agreed());
  CHECK(!g.rules() && !g.lines() && fake::content_state() == State::Pending);
  g.agreement.observe(true, true, 1, 2, {2}, ++g_heard, announced({{2, {kOurs[1]}}}));  // the contract loads
  fake::contract_ghost(6, 6);
  fake::tick_rules();
  CHECK(g_starts == 0 && g_ticks == 0);
  // A host not heard from at all is not agreed with either.
  Gated quiet;
  quiet.agreement.observe(true, false, 1, 2, {2}, ++g_heard, announced({}));
  CHECK(!quiet.agreement.agreed() && !quiet.rules() && !quiet.lines());
}

// The host runs the same mods (in any order): lines, rules and content all run.
void the_hosts_mods_here_run() {
  Gated g;
  g.agreement.observe(true, false, 1, 2, {2}, ++g_heard, announced({{2, {kOurs[1], kOurs[0]}}}));
  CHECK(g.agreement.agreed());
  CHECK(g.rules() && g.lines() && fake::content_state() == State::Active);
  g.agreement.observe(true, true, 1, 2, {2}, ++g_heard, announced({{2, {kOurs[1], kOurs[0]}}}));
  fake::contract_ghost(6, 6);
  fake::tick_rules();
  CHECK(g_starts == 1 && g_ticks == 1);
  // Alone in a private room: this client is the host.
  registry::Agreement alone;
  alone.set_local(kOurs);
  alone.observe(true, false, 1, 1, {}, ++g_heard, announced({}));
  CHECK(alone.agreed());
  // Outside a private room nothing is agreed.
  alone.observe(false, false, 0, 0, {}, ++g_heard, announced({}));
  CHECK(!alone.agreed());
}

// The answer a contract loads with holds until it ends: a player leaving, or the host's announcements
// lapsing, changes nothing mid-contract; the lobby after it decides afresh.
void a_peer_leaving_changes_nothing_mid_contract() {
  Gated g;
  const auto both = announced({{2, kOurs}, {3, kOurs}});
  g.agreement.observe(true, false, 1, 2, {2, 3}, ++g_heard, both);
  g.agreement.observe(true, true, 1, 2, {2, 3}, ++g_heard, both);  // the contract loads, agreed
  fake::contract_ghost(6, 6);
  fake::tick_rules();
  CHECK(g_starts == 1);
  const unsigned decisions = g.agreement.decisions();
  g.agreement.observe(true, true, 1, 2, {2}, ++g_heard, announced({}));  // player 3 leaves; player 2's announcements lapse
  fake::tick_rules();
  CHECK(g.agreement.agreed() && g.agreement.decisions() == decisions);
  CHECK(g.rules() && g.lines() && g_starts == 1 && g_ticks == 2 && g_stops == 0);
  // A contract that loaded not agreed stays so, however the room changes.
  Gated off;
  off.agreement.observe(true, true, 1, 2, {2}, ++g_heard, announced({{2, {kOurs[0]}}}));
  off.agreement.observe(true, true, 1, 1, {}, ++g_heard, announced({}));  // the host leaves, and this client takes over
  CHECK(!off.agreement.agreed() && !off.rules());
  // The lobby after the contract decides afresh.
  off.agreement.observe(true, false, 1, 1, {}, ++g_heard, announced({}));
  CHECK(off.agreement.agreed());
  // An announcement that lapses while a client loads the contract is not held against it.
  Gated loading;
  loading.agreement.observe(true, false, 1, 2, {2}, ++g_heard, announced({{2, kOurs}}));
  loading.agreement.observe(true, true, 1, 2, {2}, ++g_heard, announced({}));
  CHECK(loading.agreement.agreed());
}

// Another build of a mod is another mod: not agreed.
void a_mismatched_build_is_not_agreed() {
  Gated g;
  g.agreement.observe(true, false, 1, 2, {2}, ++g_heard, announced({{2, {kOurs[0], {"flames", "00000000000000cc"}}}}));
  CHECK(!g.agreement.agreed() && !g.rules() && !g.lines() && fake::content_state() == State::Pending);
  // A host with one mod more is a mismatch too.
  g.agreement.observe(true, false, 1, 2, {2}, ++g_heard,
                      announced({{2, {kOurs[0], kOurs[1], {"extra", "00000000000000dd"}}}}));
  CHECK(!g.agreement.agreed());
  g.agreement.observe(true, false, 1, 2, {2}, ++g_heard, announced({{2, kOurs}}));
  CHECK(g.agreement.agreed() && g.rules() && g.lines());
}

// The players' lists are read, copied and sorted only when presence changed what it heard, the room
// changed, or this client's own mods changed; on any other frame the decision stands as it was.
void the_lists_are_read_only_on_a_change() {
  registry::Agreement a;
  a.set_local(kOurs);
  int asked = 0;
  const registry::Agreement::ModsOf counting = [&](int) -> std::optional<std::vector<registry::ModBuild>> {
    ++asked;
    return kOurs;
  };
  a.observe(true, false, 1, 2, {2}, 7, counting);
  CHECK(asked == 1 && a.agreed());
  for (int frame = 0; frame < 100; ++frame) a.observe(true, false, 1, 2, {2}, 7, counting);
  CHECK(asked == 1 && a.agreed());
  a.observe(true, false, 1, 2, {2}, 8, counting);  // presence heard something
  CHECK(asked == 2);
  a.observe(true, false, 1, 2, {2, 3}, 8, counting);  // player 3 joined
  CHECK(asked == 4 && a.agreed());
  a.set_local({kOurs[0]});  // this client's mods changed
  a.observe(true, false, 1, 2, {2, 3}, 8, counting);
  CHECK(asked == 6 && !a.agreed());
  a.observe(false, false, 0, 0, {}, 8, counting);  // out of the room
  CHECK(asked == 6 && !a.agreed());
}

// A host whose mods stop while it stays in the room announces no mods (host/presence.h): the list
// kept for it is replaced, so this client no longer matches it.
void a_host_whose_mods_stop_is_no_longer_agreed() {
  Gated g;
  g.agreement.observe(true, false, 1, 2, {2}, ++g_heard, announced({{2, kOurs}}));
  CHECK(g.agreement.agreed() && g.rules());
  g.agreement.observe(true, false, 1, 2, {2}, ++g_heard, announced({}));  // not heard for a while: kept
  CHECK(g.agreement.agreed());
  g.agreement.observe(true, false, 1, 2, {2}, ++g_heard, announced({{2, {}}}));  // heard: no mods now
  CHECK(!g.agreement.agreed() && !g.rules() && !g.lines() && fake::content_state() == State::Pending);
}

}  // namespace agreement

// The game as the host's content code reads it, faked: the level with its ghost, the ghost's GhostInfo,
// the controller with its collection of the game's ghosts (sorted by the names in their name keys, as
// the game's are) and the Mimic's list, and the hunt state. Offsets are the fake's own.
struct GameWorld {
  int levelClass = 0, ghostClass = 0, infoClass = 0, controllerClass = 0, collectionClass = 0, dataClass = 0,
      huntClass = 0;
  void* levelInstance = nullptr;
  void* controllerInstance = nullptr;
  void *level, *ghostAi, *info, *controller, *collection, *hunt;
  std::vector<std::string> events;  // what the rules did, in order

  GameWorld() {
    namespace map = pm::map;
    fake::type(map::LevelController, &levelClass);
    fake::type(map::GhostAI, &ghostClass);
    fake::type(map::GhostInfo, &infoClass);
    fake::type(map::GhostController, &controllerClass);
    fake::type(map::GhostCollection, &collectionClass);
    fake::type(map::GhostData, &dataClass);
    fake::type(map::GhostHuntState, &huntClass);
    fake::static_field(map::LevelController_instance, &levelInstance, sizeof levelInstance, true);
    fake::static_field(map::GhostController_instance, &controllerInstance, sizeof controllerInstance, true);
    fake::field(map::LevelController_currentGhost, 0x40);
    fake::field(map::GhostAI_info, 0x38);
    fake::field(map::GhostInfo_traits, 0x28);
    fake::field(map::GhostInfo_identityType, 0x28);
    fake::field(map::GhostInfo_behaviourType, 0x2C);
    fake::field(map::GhostController_allGhostsCollection, 0x90);
    fake::field(map::GhostController_mimicCopies, 0xC8);
    fake::field(map::GhostCollection_ghosts, 0x18);
    fake::field(map::GhostData_type, 0x18);
    fake::field(map::GhostData_ghostNameLocalisationKey, 0x20);
    fake::field(map::GhostData_evidence1, 0x28);
    fake::field(map::GhostData_evidence2, 0x2C);
    fake::field(map::GhostData_evidence3, 0x30);
    fake::field(map::GhostHuntState_ghost, 0x10);
    fake::field(map::GhostHuntState_baseSpeed, 0xB0);
    fake::field(map::GhostHuntState_sightSpeedUp, 0xB4);
    fake::field(map::GhostController_behaviourType, 0x2C);
    fake::value(map::GhostType_Goryo, 14);
    // The hooked methods: their fakes are never called here, the hooks' bodies are.
    for (const pm::sdk::EntryDesc* m :
         {&map::LocalisationSystem_TryGetLocalisedValue, &map::GhostAI_Init, &map::GhostInfo_SyncValues,
          &map::GhostInfo_SyncValuesNetworked, &map::GhostController_BuildMimicCopies, &map::GhostController_MimicCopy,
          &map::GhostHuntState_UpdateSpeed, &map::GhostController_PickEvidence, &map::GhostData_GetEvidenceList})
      fake::method(*m, reinterpret_cast<void*>(&never_called));
    std::string missing;
    api::bind(fake::host(), missing);
    level = phasmo::LevelController::type.make();
    ghostAi = phasmo::GhostAI::type.make();
    info = phasmo::GhostInfo::type.make();
    controller = phasmo::GhostController::type.make();
    collection = phasmo::GhostCollection::type.make();
    hunt = phasmo::GhostHuntState::type.make();
    levelInstance = level;
    controllerInstance = controller;
    phasmo::LevelController::currentGhost.set(level, ghostAi);
    phasmo::GhostAI::info.set(ghostAi, info);
    phasmo::GhostController::allGhostsCollection.set(controller, collection);
    phasmo::GhostHuntState::ghost.set(hunt, ghostAi);
    // The game's ghosts as its collection holds them, sorted by the names in their keys.
    std::vector<void*> ghosts;
    for (const auto& [type, name] : std::vector<std::pair<int32_t, std::string>>{
             {4, "Banshee"}, {6, "Mare"}, {0, "Spirit"}, {17, "TheTwins"}, {10, "Yurei"}}) {
      void* data = phasmo::GhostData::type.make();
      phasmo::GhostData::ghostType.set(data, type);
      phasmo::GhostData::ghostNameLocalisationKey.set(data, api::make_string("Journal_" + name + "Title"));
      ghosts.push_back(data);
    }
    phasmo::GhostCollection::ghosts.set(collection, fake::list(ghosts));
  }
  static void never_called() {}

  // The contract's ghost is `identity`, acting by `behaviour`.
  void ghost_is(int32_t identity, int32_t behaviour) {
    phasmo::GhostInfo::identityType.set(info, identity);
    phasmo::GhostInfo::behaviourType.set(info, behaviour);
  }
  // The names in the collection's keys, in order.
  std::vector<std::string> keys() const {
    std::vector<std::string> out;
    for (void* data : fake::list_items_of(phasmo::GhostCollection::ghosts.get(collection)))
      out.push_back(api::to_utf8(phasmo::GhostData::ghostNameLocalisationKey.get(data)));
    return out;
  }
  void* entry_of(int32_t type) const {
    for (void* data : fake::list_items_of(phasmo::GhostCollection::ghosts.get(collection)))
      if (phasmo::GhostData::ghostType.get(data) == type) return data;
    return nullptr;
  }
};

GameWorld* g_world = nullptr;

// Rules that write what they do to the world's events, named by `user` (a C string).
void rule_start(void* user) { g_world->events.push_back(std::string("start ") + static_cast<const char*>(user)); }
void rule_tick(void* user) { g_world->events.push_back(std::string("tick ") + static_cast<const char*>(user)); }
void rule_stop(void* user) { g_world->events.push_back(std::string("stop ") + static_cast<const char*>(user)); }
content::GhostRules recording(const char* name) {
  content::GhostRules r;
  r.user = const_cast<char*>(name);
  r.start = &rule_start;
  r.tick = &rule_tick;
  r.stop = &rule_stop;
  return r;
}

content::GhostDef named(const std::string& id, const std::string& name) {
  content::GhostDef g = ghost(id);
  g.name = content::Text("en", name).add("de", name + " (de)");
  return g;
}

pm::host::ListOps fake_lists() { return {&fake::list_insert, &fake::list_remove_at, &fake::list_add_value}; }

// A world with three custom ghosts registered (types 31 to 33 in byte order of their ids: Asterion,
// Lethe, Zephyr), rules on the Lethe and the Mare, content active for this client alone in a room.
struct Placed {
  GameWorld world;
  pm::host::ContentTypes types{fake::content_registry(), fake::acting(), fake_lists()};
  Placed() {
    g_world = &world;
    CHECK(content::register_ghost(named("a.asterion", "Asterion")) > 0);
    CHECK(content::register_ghost(named("a.lethe", "Lethe")) > 0);
    CHECK(content::register_ghost(named("a.zephyr", "Zephyr")) > 0);
    CHECK(content::attach_rules("a.lethe", recording("lethe")) > 0);
    CHECK(content::attach_rules("vanilla.mare", recording("mare")) > 0);
    fake::commit_content();
    room(1, {1}, {});
    CHECK(types.missing().empty());
  }
  void tick() { types.tick(fake::content_state(), true, true); }
};

void custom_type_joins_collection_in_alphabetical_place() {
  start();
  Placed p;
  p.tick();
  CHECK((p.world.keys() == std::vector<std::string>{"Journal_31Title", "Journal_BansheeTitle", "Journal_32Title",
                                                    "Journal_MareTitle", "Journal_SpiritTitle", "Journal_TheTwinsTitle",
                                                    "Journal_YureiTitle", "Journal_33Title"}));
  void* lethe = p.world.entry_of(32);
  CHECK(lethe && phasmo::GhostData::evidence1.get(lethe) == 1 && phasmo::GhostData::evidence2.get(lethe) == 4 &&
        phasmo::GhostData::evidence3.get(lethe) == 6);
  p.tick();  // placed once
  CHECK(p.world.keys().size() == 8);
}

// The name key's form is read from the game's own keys: when they show no one form, no custom entry
// is made up, and the log says so once.
void key_form_comes_from_the_game() {
  start();
  Placed p;
  void* list = phasmo::GhostCollection::ghosts.get(p.world.collection);
  phasmo::GhostData::ghostNameLocalisationKey.set(fake::list_items_of(list)[1], api::make_string("Ghost_Mare_Name"));
  p.tick();
  p.tick();
  CHECK(p.world.keys().size() == 5 && !p.types.answers_names());
  CHECK(std::count_if(g_lines.begin(), g_lines.end(), [](const std::string& l) {
          return l.find("show no one form") != std::string::npos;
        }) == 1);
}

void name_key_answers_custom_name() {
  start();
  Placed p;
  CHECK(!p.types.answers_names());
  p.tick();
  CHECK(p.types.answers_names());
  CHECK(p.types.name_for_key("Journal_32Title") == std::optional<std::string>("Lethe"));
  CHECK(p.types.name_for_key("Journal_33Title") == std::optional<std::string>("Zephyr"));
  CHECK(!p.types.name_for_key("Journal_MareTitle") && !p.types.name_for_key("Journal_34Title"));
}

void identity_is_custom_behaviour_is_none() {
  start();
  Placed p;
  p.tick();
  // GhostAI.Init: the host's traits name the custom type as both; the AI gets "no type".
  int32_t traits[16] = {32, 32};
  p.types.before_init(traits);
  CHECK(traits[0] == 32 && traits[1] == 24);
  // SyncValues sends the behaviour type as the ghost's type: the custom type goes, and the traits are
  // put back after.
  const int32_t put = p.types.before_sync_send(traits);
  CHECK(traits[1] == 32 && put == 24);
  p.types.after_sync_send(traits, put);
  CHECK(traits[1] == 24);
  // Every client: SyncValuesNetworked wrote the custom type to both; the behaviour goes back to "no type".
  p.world.ghost_is(32, 32);
  p.types.after_sync_values(p.world.info);
  CHECK(phasmo::GhostInfo::identityType.get(p.world.info) == 32 && phasmo::GhostInfo::behaviourType.get(p.world.info) == 24);
  // A vanilla ghost is left as the game set it.
  int32_t mare[16] = {6, 6};
  p.types.before_init(mare);
  CHECK(p.types.before_sync_send(mare) == -1 && mare[1] == 6);
  p.world.ghost_is(6, 6);
  p.types.after_sync_values(p.world.info);
  CHECK(phasmo::GhostInfo::behaviourType.get(p.world.info) == 6);
  // Its acting-as type is the custom one, and the identity too.
  p.world.ghost_is(32, 24);
  p.tick();
  CHECK(content::acting_as() == 32 && content::identity() == 32);
  // The hooks are made on each method's bodies; the localisation's is the host's shared one
  // (host/content_localised.h).
  CHECK(p.types.create_hooks());
  const std::vector<std::string> hooked = fake::hooked();
  for (const char* key : {"GhostAI.Init", "GhostInfo.SyncValues", "GhostInfo.SyncValuesNetworked",
                          "GhostController.BuildMimicCopies", "GhostController.MimicCopy", "GhostHuntState.UpdateSpeed"})
    CHECK(std::count(hooked.begin(), hooked.end(), key) == 1);
  CHECK(std::count(hooked.begin(), hooked.end(), "LocalisationSystem.TryGetLocalisedValue") == 0);
}

void mimic_copy_list_includes_custom_types() {
  start();
  Placed p;
  p.tick();  // the contract loads with content active: it has the custom ghosts
  std::vector<int32_t> built;  // as the game builds it: every type but None
  for (int32_t t = 0; t <= 30; ++t)
    if (t != 24) built.push_back(t);
  void* list = fake::value_list(built);
  phasmo::GhostController::mimicCopies.set(p.world.controller, list);
  p.types.after_build_mimic_copies(p.world.controller);
  std::vector<int32_t> expected = built;
  expected.insert(expected.end(), {31, 32, 33});
  CHECK(fake::list_values(list) == expected);
  p.tick();  // already there: nothing doubles
  CHECK(fake::list_values(list) == expected);
}

void mimic_copy_switches_rules() {
  start();
  Placed p;
  p.world.ghost_is(20, 20);  // a Mimic, before its first copy
  p.tick();
  CHECK(content::acting_as() == 20 && p.world.events.empty());
  // It copies the Mare: the Mare's rules start.
  phasmo::GhostInfo::behaviourType.set(p.world.info, 6);
  p.types.after_mimic_copy(p.world.controller);
  CHECK((p.world.events == std::vector<std::string>{"start mare"}));
  CHECK(content::acting_as() == 6 && content::identity() == 20);
  // It copies the Lethe: the Mare's rules stop and the Lethe's start in that same call, and the AI
  // runs the generic behaviour ("no type").
  p.world.events.clear();
  phasmo::GhostInfo::behaviourType.set(p.world.info, 32);
  p.types.after_mimic_copy(p.world.controller);
  CHECK((p.world.events == std::vector<std::string>{"stop mare", "start lethe"}));
  CHECK(phasmo::GhostInfo::behaviourType.get(p.world.info) == 24);
  CHECK(content::acting_as() == 32 && content::identity() == 20);
  // The next frames keep the copy, though the behaviour type reads "no type".
  p.world.events.clear();
  p.tick();
  CHECK((p.world.events == std::vector<std::string>{"tick lethe"}));
  CHECK(content::acting_as() == 32);
}

// Rules on a vanilla id run whenever the ghost acts as that type: a Mare, or a Mimic copying one.
void vanilla_rules_follow_behaviour_type() {
  start();
  Placed p;
  p.world.ghost_is(6, 6);
  p.tick();
  CHECK((p.world.events == std::vector<std::string>{"start mare", "tick mare"}));
  p.world.events.clear();
  p.world.ghost_is(20, 6);  // a Mimic that has copied the Mare, as its GhostInfo has it
  p.tick();
  CHECK((p.world.events == std::vector<std::string>{"tick mare"}));  // still a Mare: nothing restarts
  p.world.ghost_is(20, 20);
  p.tick();
  CHECK((p.world.events == std::vector<std::string>{"tick mare", "stop mare"}));
  // Outside a private room no rules run.
  p.world.events.clear();
  p.world.ghost_is(6, 6);
  p.types.tick(State::Off, false, false);
  CHECK(p.world.events.empty() && content::acting_as() == -1);
}

// The Mimic's copy is made on the master only, and the game sends nothing of it: the master sends the
// acting-as type on its channel and every client follows it, so a rule's client-side effects run
// everywhere.
void acting_as_reaches_every_client() {
  start();
  Placed p;
  registry::Registry& r = fake::content_registry();
  registry::Acting master(r);
  master.observe(20, 20, true);
  master.copied(20, 32);
  const std::vector<registry::ActingMessage> sent = master.take_messages(true);
  CHECK(sent.size() == 1 && sent[0].actor == 0 && sent[0].data.size() == registry::kActingBytes);
  p.world.events.clear();  // the master's own rules (its dispatch shares this registry) are not this client's
  // This client: its GhostInfo still says Mimic.
  p.world.ghost_is(20, 20);
  p.types.tick(fake::content_state(), true, false);
  CHECK(content::acting_as() == 20 && p.world.events.empty());
  const std::vector<uint8_t>& data = sent[0].data;
  fake::acting().receive(3, 2, data.data(), data.size());  // not from the master: ignored
  CHECK(content::acting_as() == 20);
  fake::acting().receive(2, 2, data.data(), data.size());
  CHECK(content::acting_as() == 32 && (p.world.events == std::vector<std::string>{"start lethe"}));
  p.types.tick(fake::content_state(), true, false);
  CHECK(content::acting_as() == 32);
  // A client never sends; once the contract ends what it heard is forgotten.
  CHECK(fake::acting().take_messages(true).empty());
  p.world.events.clear();
  p.world.levelInstance = nullptr;
  p.types.tick(fake::content_state(), true, false);
  CHECK(content::acting_as() == -1 && (p.world.events == std::vector<std::string>{"stop lethe"}));
}

// The master sends the acting-as type only when it changes and once to each player who joins: the
// channel is reliable, so nothing repeats however long it stays, but a send that failed is due again.
void acting_as_is_sent_on_change_and_to_joiners() {
  start();
  CHECK(content::attach_rules("vanilla.mare", {}) > 0);
  fake::commit_content();
  registry::Acting master(fake::content_registry());
  master.observe_room(1, {1, 2});
  master.observe(20, 20, true);
  CHECK(master.take_messages(true).size() == 1);  // the first: to everyone
  for (int frame = 0; frame < 1000; ++frame) {
    master.observe(20, 20, true);
    CHECK(master.take_messages(true).empty());
  }
  master.copied(20, 6);
  std::vector<registry::ActingMessage> m = master.take_messages(true);
  CHECK(m.size() == 1 && m[0].actor == 0);
  CHECK(master.take_messages(true).empty());
  // A player joins: exactly one message, to them.
  master.observe_room(1, {1, 2, 3});
  m = master.take_messages(true);
  CHECK(m.size() == 1 && m[0].actor == 3);
  master.observe_room(1, {1, 2, 3});
  CHECK(master.take_messages(true).empty());
  // A failed send is tried again, then not again.
  master.send_failed(3);
  m = master.take_messages(true);
  CHECK(m.size() == 1 && m[0].actor == 3);
  master.send_failed(0);
  m = master.take_messages(true);
  CHECK(m.size() == 1 && m[0].actor == 0);
  CHECK(master.take_messages(true).empty());
}

// A client that declined the mods, or one with no rules and no content in play, sends nothing.
void acting_as_send_is_gated() {
  start();
  fake::commit_content();  // nothing registered
  registry::Acting empty(fake::content_registry());
  empty.observe_room(1, {1, 2});
  empty.observe(6, 6, true);
  CHECK(empty.take_messages(true).empty());
  start();
  CHECK(content::attach_rules("vanilla.mare", {}) > 0);
  fake::commit_content();
  registry::Acting declined(fake::content_registry());
  declined.observe_room(1, {1, 2});
  declined.observe(6, 6, true);
  CHECK(declined.take_messages(false).empty());
  CHECK(declined.take_messages(true).size() == 1);  // agreed, with rules: it speaks
}

// A Mimic's copy made before the host first looks at a new contract's ghost is kept for it.
void early_copy_is_kept() {
  start();
  CHECK(content::attach_rules("vanilla.mare", {}) > 0);
  fake::commit_content();
  registry::Acting acting(fake::content_registry());
  acting.observe(-1, -1, true);  // the lobby
  acting.copied(20, 6);          // the Mimic copies the Mare before the next frame
  CHECK(acting.acting_as() == 6);
  acting.observe(20, 20, true);
  CHECK(acting.acting_as() == 6);
  acting.observe(-1, -1, true);  // the contract ends: the copy goes with it
  acting.observe(20, 20, true);
  CHECK(acting.acting_as() == 20);
}

// The rule writes the hunt's inputs, the base and the line-of-sight speed-up, and the game's own
// update makes the speed from them: no other field changes. Its base is the game's own each time, so
// a scaled rule never compounds, and the game's base comes back once the rule stops asking.
void hunt_speed_writes_inputs_not_result() {
  struct Halving {
    bool on = true;
    float seen = 0;
  } halving;
  content::GhostRules rules;
  rules.user = &halving;
  rules.huntSpeed = [](float* base, float* losScale, void* user) {
    auto* h = static_cast<Halving*>(user);
    h->seen = *base;
    if (!h->on) return false;
    *base *= 0.5f;
    *losScale = 1.0f;
    return true;
  };
  start();
  GameWorld world;
  g_world = &world;
  CHECK(content::register_ghost(named("a.zephyr", "Zephyr")) > 0);
  CHECK(content::attach_rules("a.zephyr", rules) > 0);
  fake::commit_content();
  room(1, {1}, {});
  pm::host::ContentTypes types{fake::content_registry(), fake::acting(), fake_lists()};
  world.ghost_is(31, 24);
  types.tick(fake::content_state(), true, true);
  CHECK(content::acting_as() == 31);
  phasmo::GhostHuntState::baseSpeed.set(world.hunt, 1.7f);
  phasmo::GhostHuntState::sightSpeedUp.set(world.hunt, 1.3f);
  std::vector<uint8_t> before(static_cast<uint8_t*>(world.hunt), static_cast<uint8_t*>(world.hunt) + 0x200);
  types.before_update_speed(world.hunt);
  CHECK(phasmo::GhostHuntState::baseSpeed.get(world.hunt) == 0.85f);
  CHECK(phasmo::GhostHuntState::sightSpeedUp.get(world.hunt) == 1.0f);
  std::vector<uint8_t> after(static_cast<uint8_t*>(world.hunt), static_cast<uint8_t*>(world.hunt) + 0x200);
  for (size_t i = 0; i < before.size(); ++i)
    if (i < 0xB0 || i >= 0xB8) CHECK(before[i] == after[i]);  // only the two inputs
  // The next frame the rule sees the game's base again, not its own.
  types.before_update_speed(world.hunt);
  CHECK(halving.seen == 1.7f && phasmo::GhostHuntState::baseSpeed.get(world.hunt) == 0.85f);
  // The game's two-second refresh writes its base: the rule sees the new one.
  phasmo::GhostHuntState::baseSpeed.set(world.hunt, 2.0f);
  types.before_update_speed(world.hunt);
  CHECK(halving.seen == 2.0f && phasmo::GhostHuntState::baseSpeed.get(world.hunt) == 1.0f);
  // The rule stops asking: the game's own base comes back at once.
  halving.on = false;
  types.before_update_speed(world.hunt);
  CHECK(phasmo::GhostHuntState::baseSpeed.get(world.hunt) == 2.0f);
  // Another ghost's hunt is left alone.
  void* other = phasmo::GhostHuntState::type.make();
  phasmo::GhostHuntState::baseSpeed.set(other, 1.7f);
  halving.on = true;
  types.before_update_speed(other);
  CHECK(phasmo::GhostHuntState::baseSpeed.get(other) == 1.7f);
}

// Whether a contract has custom content is decided when it loads, and the decision holds: content
// going pending or off mid-contract keeps the entries and the custom rules, and what another put in
// is never touched.
void contract_keeps_its_content() {
  start();
  Placed p;
  void* list = fake::value_list({0, 6, 20, 45});  // 45: none of this client's content, which numbers 31 to 33
  phasmo::GhostController::mimicCopies.set(p.world.controller, list);
  void* foreign = phasmo::GhostData::type.make();
  phasmo::GhostData::ghostType.set(foreign, 45);
  phasmo::GhostData::ghostNameLocalisationKey.set(foreign, api::make_string("Journal_45Title"));
  CHECK(fake::list_insert(phasmo::GhostCollection::ghosts.get(p.world.collection), 5, foreign));
  p.world.ghost_is(32, 24);
  p.tick();
  CHECK(p.world.keys().size() == 9 && fake::list_values(list).size() == 7);
  CHECK((p.world.events == std::vector<std::string>{"start lethe", "tick lethe"}));
  // This client as a joiner: the host not heard from yet, then with other content: off.
  room(1, {1, 2}, {}, 0, 2);
  CHECK(fake::content_state() == State::Pending);
  p.tick();
  Peer other{2};
  other.runs({}, {ghost("echoes.echo")});
  room(1, {1, 2}, {&other}, 0, 2);
  CHECK(fake::content_state() == State::Off);
  p.world.events.clear();
  p.tick();
  CHECK(p.world.keys().size() == 9 && fake::list_values(list).size() == 7);
  CHECK(p.types.name_for_key("Journal_32Title") == std::optional<std::string>("Lethe"));
  CHECK((p.world.events == std::vector<std::string>{"tick lethe"}));
  CHECK(content::acting_as() == 32);
}

// The contract's end takes its entries back out (a collection that outlives the contract keeps none),
// leaves another's, stops its custom rules and answers no name key.
void contract_end_withdraws() {
  start();
  Placed p;
  void* foreign = phasmo::GhostData::type.make();
  phasmo::GhostData::ghostType.set(foreign, 45);
  phasmo::GhostData::ghostNameLocalisationKey.set(foreign, api::make_string("Journal_45Title"));
  CHECK(fake::list_insert(phasmo::GhostCollection::ghosts.get(p.world.collection), 5, foreign));
  p.world.ghost_is(32, 24);
  p.tick();
  CHECK(p.world.keys().size() == 9);
  p.world.events.clear();
  p.world.controllerInstance = nullptr;  // the contract unloads
  p.world.levelInstance = nullptr;
  p.tick();
  CHECK((p.world.keys() == std::vector<std::string>{"Journal_BansheeTitle", "Journal_MareTitle", "Journal_SpiritTitle",
                                                    "Journal_TheTwinsTitle", "Journal_YureiTitle", "Journal_45Title"}));
  CHECK(!p.types.answers_names() && !p.types.name_for_key("Journal_32Title"));
  CHECK((p.world.events == std::vector<std::string>{"stop lethe"}));
  // Before the mods stop, the vanilla rules stop as well.
  p.world.levelInstance = p.world.level;
  p.world.controllerInstance = p.world.controller;
  p.world.ghost_is(6, 6);
  p.tick();
  p.world.events.clear();
  p.types.withdraw();
  CHECK((p.world.events == std::vector<std::string>{"stop mare"}));
}

// The room's mod agreement holds its decision on the signal the ghost types decide a contract on: the
// contract's GhostController alive (ContentTypes::contract_loaded), the LevelController or not. So when
// the GhostController comes a frame before the LevelController and the room changes in between, the
// agreement still holds the answer the contract's content was decided with.
void contract_decision_and_agreement_share_one_signal() {
  start();
  Placed p;
  registry::Agreement agreement;
  agreement.set_local(agreement::kOurs);
  fake::content_registry().use_agreement(&agreement);
  p.world.controllerInstance = nullptr;  // the lobby
  p.world.levelInstance = nullptr;
  CHECK(!p.types.contract_loaded());
  unsigned heard = 0;
  agreement.observe(true, false, 1, 2, {2}, ++heard, agreement::announced({{2, agreement::kOurs}}));  // verified in the lobby
  // Frame 1, as the host runs it, the agreement before the ghost types: the GhostController is there.
  p.world.controllerInstance = p.world.controller;
  CHECK(p.types.contract_loaded());
  agreement.observe(true, p.types.contract_loaded(), 1, 2, {2}, ++heard, agreement::announced({{2, agreement::kOurs}}));
  p.tick();
  CHECK(p.types.decided() && p.types.held().size() == 3);
  // Frame 2: player 2 turns out to run another build, and the LevelController arrives.
  p.world.levelInstance = p.world.level;
  agreement.observe(true, p.types.contract_loaded(), 1, 2, {2}, ++heard,
                    agreement::announced({{2, {agreement::kOurs[0]}}}));
  p.tick();
  CHECK(agreement.agreed() && fake::content_state() == State::Active && p.types.held().size() == 3);
  CHECK(!fake::content_registry().rules_for("vanilla.mare").empty());
  // The contract ends: the agreement follows the room again.
  p.world.controllerInstance = nullptr;
  p.world.levelInstance = nullptr;
  agreement.observe(true, p.types.contract_loaded(), 1, 2, {2}, ++heard,
                    agreement::announced({{2, {agreement::kOurs[0]}}}));
  CHECK(!agreement.agreed());
  fake::content_registry().use_agreement(nullptr);
}

// A contract's start waits until everyone is verified, but a player can join in the round trip after the start goes. They may
// not have the mods, so that contract has no custom ghosts, on the host too, and what the difficulty asked for is rolled.
void a_contract_an_unverified_player_reached_has_no_custom_ghosts() {
  start();
  Placed p;
  p.world.controllerInstance = p.world.controller;
  p.types.tick(fake::content_state(), true, true, false);
  CHECK(p.types.decided() && p.types.held().empty());
  fake::content_registry().hold_contract(false);
}

// A contract that loads while content is not active has no custom ghosts, even should content turn
// active during it, and any of this client's numbers a lobby left in its collection come out.
void contract_loaded_without_content_stays_vanilla() {
  start();
  Placed p;
  void* leftover = phasmo::GhostData::type.make();
  phasmo::GhostData::ghostType.set(leftover, 32);
  phasmo::GhostData::ghostNameLocalisationKey.set(leftover, api::make_string("Journal_32Title"));
  CHECK(fake::list_insert(phasmo::GhostCollection::ghosts.get(p.world.collection), 0, leftover));
  p.types.tick(State::Off, true, true);
  CHECK(p.world.keys().size() == 5);
  p.tick();  // active now: the contract stays as it loaded
  CHECK(p.world.keys().size() == 5 && !p.types.answers_names());
}

// List operations the game refused: each kind is logged once, and a withdrawal that could not finish is
// kept and tried again until it does, in the lobby, logged.
bool g_refuseRemove = false, g_refuseAdd = false, g_refuseInsert = false;
bool refusing_remove(void* list, int32_t index) { return !g_refuseRemove && fake::list_remove_at(list, index); }
bool refusing_add(void* list, int32_t value) { return !g_refuseAdd && fake::list_add_value(list, value); }
bool refusing_insert(void* list, int32_t index, void* item) {
  return !g_refuseInsert && fake::list_insert(list, index, item);
}

int count_logged(const std::string& text) {
  return static_cast<int>(std::count_if(g_lines.begin(), g_lines.end(),
                                        [&](const std::string& l) { return l.find(text) != std::string::npos; }));
}

void refused_list_operations_are_logged_and_retried() {
  start();
  GameWorld world;
  g_world = &world;
  CHECK(content::register_ghost(named("a.lethe", "Lethe")) > 0);
  fake::commit_content();
  room(1, {1}, {});
  pm::host::ContentTypes types{fake::content_registry(), fake::acting(), {&refusing_insert, &refusing_remove, &refusing_add}};
  void* mimic = fake::value_list({0, 6});
  phasmo::GhostController::mimicCopies.set(world.controller, mimic);
  // Insert and Add refused: logged once each, tried again every frame.
  g_refuseInsert = g_refuseAdd = true;
  types.tick(State::Active, true, true);
  types.tick(State::Active, true, true);
  CHECK(world.keys().size() == 5 && fake::list_values(mimic).size() == 2 && !types.answers_names());
  CHECK(count_logged("could not join the ghost collection") == 1);
  CHECK(count_logged("the Mimic's list could not take the custom ghosts") == 1);
  g_refuseInsert = g_refuseAdd = false;
  types.tick(State::Active, true, true);
  CHECK(world.keys().size() == 6 && fake::list_values(mimic).size() == 3 && types.answers_names());
  // The contract ends with RemoveAt refused: the entry stays, the host knows it, and tries again.
  g_refuseRemove = true;
  world.controllerInstance = nullptr;
  types.tick(State::Off, true, true);
  types.tick(State::Off, true, true);
  CHECK(world.keys().size() == 6);
  CHECK(count_logged("could not all leave the ghost collection") == 1);
  g_refuseRemove = false;
  types.tick(State::Off, true, true);  // in the lobby, content off: the withdrawal finishes
  CHECK(world.keys().size() == 5 && logged("a withdrawal that failed earlier is done"));
  // The mods stop mid-contract with RemoveAt refused on the Mimic's list: kept and tried again.
  world.controllerInstance = world.controller;
  types.tick(State::Active, true, true);
  CHECK(fake::list_values(mimic).size() == 3);
  g_refuseRemove = true;
  types.withdraw();
  // The earlier refusal was resolved, so the collection's is news and logged; the Mimic's list's comes
  // in the same withdrawal and is not.
  CHECK(count_logged("could not all leave the ghost collection") == 2);
  CHECK(fake::list_values(mimic).size() == 3 && count_logged("could not all leave the Mimic's list") == 0);
  g_refuseRemove = false;
  types.tick(State::Off, true, true);
  CHECK(fake::list_values(mimic).size() == 2);
}

// A refusal logged once is logged again when it comes back after the operation went through.
void refusals_are_logged_again_after_success() {
  start();
  GameWorld world;
  g_world = &world;
  CHECK(content::register_ghost(named("a.lethe", "Lethe")) > 0);
  fake::commit_content();
  room(1, {1}, {});
  pm::host::ContentTypes types{fake::content_registry(), fake::acting(), {&refusing_insert, &refusing_remove, &refusing_add}};
  void* mimic = fake::value_list({0, 6});
  phasmo::GhostController::mimicCopies.set(world.controller, mimic);
  for (int contract = 0; contract < 2; ++contract) {
    g_refuseInsert = g_refuseAdd = true;
    types.tick(State::Active, true, true);
    g_refuseInsert = g_refuseAdd = false;
    types.tick(State::Active, true, true);
    CHECK(world.keys().size() == 6 && fake::list_values(mimic).size() == 3);
    world.controllerInstance = nullptr;  // the contract ends, taking its entry out
    types.tick(State::Off, true, true);
    phasmo::GhostController::mimicCopies.set(world.controller, mimic = fake::value_list({0, 6}));
    world.controllerInstance = world.controller;
  }
  CHECK(count_logged("could not join the ghost collection") == 2);
  CHECK(count_logged("the Mimic's list could not take the custom ghosts") == 2);
}

// A leftover of this client's numbers (a lobby's withdrawal the game refused) comes out when the next
// contract is decided, before that contract's own entries go in: its data is never reused.
void leftover_entry_is_replaced_at_contract_start() {
  start();
  Placed p;
  void* leftover = phasmo::GhostData::type.make();
  phasmo::GhostData::ghostType.set(leftover, 32);
  phasmo::GhostData::ghostNameLocalisationKey.set(leftover, api::make_string("Journal_32Title"));
  phasmo::GhostData::evidence1.set(leftover, 7);  // another content's evidence
  CHECK(fake::list_insert(phasmo::GhostCollection::ghosts.get(p.world.collection), 0, leftover));
  p.tick();
  void* lethe = p.world.entry_of(32);
  CHECK(lethe && lethe != leftover && phasmo::GhostData::evidence1.get(lethe) == 1);
  CHECK(p.world.keys().size() == 8);
}

// The acting-as pair is news again for the next contract's ghost, even when it is the pair last sent.
void acting_as_resent_for_next_contract() {
  start();
  CHECK(content::attach_rules("vanilla.mare", {}) > 0);
  fake::commit_content();
  registry::Acting master(fake::content_registry());
  master.observe_room(1, {1, 2});
  master.observe(6, 6, true);
  CHECK(master.take_messages(true).size() == 1);
  master.observe(-1, -1, true);  // the contract ends
  CHECK(master.take_messages(true).empty());
  master.observe(6, 6, true);  // the next contract's ghost is a Mare too
  CHECK(master.take_messages(true).size() == 1);
}

// The game's evidence pick and its sync to every client, as decompiled (build 25811160), over the faked
// game: GhostController.PickEvidence (0x22AFCB0) makes the given evidence a new empty list, and when the
// difficulty's evidenceGiven is 1 or more, the GetEvidenceList of the behaviour type's GhostData in the
// contract's collection (its three evidence in order), less 3 - evidenceGiven of them, each removed at a
// random index below the count (below the count less one for the Hantu, Goryo, Obake, Moroi and Deogen);
// `rolls` are those indexes. GhostInfo.SyncValues (0x206C8B0) sends the first evidenceGiven of them as an
// int[], and SyncEvidence (0x206C340) makes GhostInfo's given evidence anew from it on every client.
// Returns what the clients' GhostInfo then holds.
std::vector<int32_t> game_pick_and_sync(GameWorld& w, int32_t behaviour, int32_t evidenceGiven,
                                        const std::vector<int32_t>& rolls) {
  void* d = phasmo::Difficulty::type.make();
  phasmo::Difficulty::evidenceGiven.set(d, evidenceGiven);
  std::vector<int32_t> given;
  if (phasmo::Difficulty::evidenceGiven.get(d) >= 1) {
    void* data = nullptr;  // GhostCollection.GetGhostOfType: the first of that type
    for (void* g : fake::list_items_of(phasmo::GhostCollection::ghosts.get(w.collection)))
      if (!data && phasmo::GhostData::ghostType.get(g) == behaviour) data = g;
    CHECK(data != nullptr);
    if (!data) return {};
    given = {phasmo::GhostData::evidence1.get(data), phasmo::GhostData::evidence2.get(data),
             phasmo::GhostData::evidence3.get(data)};
    const bool forced = behaviour == 13 || behaviour == 14 || behaviour == 19 || behaviour == 21 || behaviour == 22;
    for (int32_t i = 0; i < 3 - phasmo::Difficulty::evidenceGiven.get(d); ++i) {
      const int32_t count = static_cast<int32_t>(given.size()) - (forced ? 1 : 0);
      given.erase(given.begin() + rolls.at(static_cast<size_t>(i)) % count);
    }
  }
  phasmo::GhostController::givenEvidence.set(w.controller, fake::value_list(given));
  std::vector<int32_t> sent;  // SyncValues' int[] of evidenceGiven numbers
  const std::vector<int32_t> pick = fake::list_values(phasmo::GhostController::givenEvidence.get(w.controller));
  for (int32_t i = 0; i < evidenceGiven && i < static_cast<int32_t>(pick.size()); ++i)
    sent.push_back(pick[static_cast<size_t>(i)]);
  phasmo::GhostInfo::givenEvidence.set(w.info, fake::value_list(sent));
  return fake::list_values(phasmo::GhostInfo::givenEvidence.get(w.info));
}

// The world as content_test's, with custom evidence: the Hydra lists "z.flame" (numbered 8, the only
// custom evidence) beside EMF and Ghost Orbs, the Lethe the game's own three.
struct FlameWorld {
  int difficultyClass = 0;
  GameWorld world;
  pm::host::ContentTypes types{fake::content_registry(), fake::acting(), fake_lists()};
  int32_t hydra = -1, lethe = -1, flame = -1;
  FlameWorld(const content::GhostDef& hydraDef) {
    g_world = &world;
    namespace map = pm::map;
    fake::type(map::Difficulty, &difficultyClass);
    fake::field(map::Difficulty_evidenceGiven, 0x4C);
    fake::field(map::GhostController_givenEvidence, 0x30);
    fake::field(map::GhostInfo_givenEvidence, 0x30);
    std::string missing;
    api::bind(fake::host(), missing);
    CHECK(content::register_evidence(evidence("z.flame")) > 0);
    CHECK(content::register_ghost(hydraDef) > 0);
    CHECK(content::register_ghost(named("a.lethe", "Lethe")) > 0);
    fake::commit_content();
    room(1, {1}, {});
    hydra = content::type_of("a.hydra");
    lethe = content::type_of("a.lethe");
    flame = content::evidence_of("z.flame");
  }
  void tick() { types.tick(fake::content_state(), true, true); }
};

content::GhostDef hydra_def() {
  content::GhostDef g = named("a.hydra", "Hydra");
  g.evidence = {"emf", "z.flame", "orbs"};
  return g;
}

// A ghost that lists a custom evidence carries its number in its GhostData, so the game's own pick gives
// it; the contract holds that evidence while it holds a ghost that lists it.
void custom_evidence_given_when_listed() {
  start();
  FlameWorld w(hydra_def());
  CHECK(w.flame == 8 && w.hydra == 31 && w.lethe == 32);
  w.tick();
  void* data = w.world.entry_of(w.hydra);
  CHECK(data && phasmo::GhostData::evidence1.get(data) == 1 && phasmo::GhostData::evidence2.get(data) == w.flame &&
        phasmo::GhostData::evidence3.get(data) == 4);
  // Every evidence given, the custom one among them, on every client's GhostInfo.
  CHECK((game_pick_and_sync(w.world, w.hydra, 3, {}) == std::vector<int32_t>{1, 8, 4}));
  // The contract holds the custom evidence its ghosts list, by number.
  pm::host::ContentEvidence side{fake::content_registry(), w.types, {}};
  side.tick({});
  CHECK(side.held().size() == 1 && side.held().count(8) && side.held().at(8).id == "z.flame");
  // A second custom evidence numbers in byte order of the ids, and a ghost listing it gets that number.
  start();
  CHECK(content::register_evidence(evidence("a.salt")) > 0);
  content::GhostDef salted = hydra_def();
  salted.evidence = {"a.salt", "z.flame", "orbs"};
  FlameWorld s(salted);
  s.tick();
  void* salty = s.world.entry_of(s.hydra);
  CHECK(content::evidence_of("a.salt") == 8 && s.flame == 9);
  CHECK(salty && phasmo::GhostData::evidence1.get(salty) == 8 && phasmo::GhostData::evidence2.get(salty) == 9);
  CHECK((game_pick_and_sync(s.world, s.hydra, 3, {}) == std::vector<int32_t>{8, 9, 4}));
}

// On a difficulty that gives fewer evidence, the game's pick withholds a custom evidence exactly as it
// withholds its own in the same slot, and the host never puts a withheld one back: the given lists stay
// as the game made them while the host ticks.
void withheld_evidence_not_given() {
  start();
  FlameWorld w(hydra_def());
  pm::host::ContentEvidence side{fake::content_registry(), w.types, {}};
  const auto tick = [&] {
    w.tick();
    side.tick({});
  };
  tick();
  // Two given, the roll taking the second slot: the flame is withheld, as a game ghost's second is.
  CHECK((game_pick_and_sync(w.world, w.hydra, 2, {1}) == std::vector<int32_t>{1, 4}));
  for (int i = 0; i < 3; ++i) tick();
  CHECK((fake::list_values(phasmo::GhostInfo::givenEvidence.get(w.world.info)) == std::vector<int32_t>{1, 4}));
  CHECK((fake::list_values(phasmo::GhostController::givenEvidence.get(w.world.controller)) ==
         std::vector<int32_t>{1, 4}));
  CHECK((game_pick_and_sync(w.world, w.lethe, 2, {1}) == std::vector<int32_t>{1, 6}));  // its orbs, likewise
  // The roll taking another slot leaves it given.
  CHECK((game_pick_and_sync(w.world, w.hydra, 2, {0}) == std::vector<int32_t>{8, 4}));
  // One given, both rolls missing it: only the flame.
  CHECK((game_pick_and_sync(w.world, w.hydra, 1, {0, 1}) == std::vector<int32_t>{8}));
  // None given: nothing, the custom evidence as the game's own.
  CHECK(game_pick_and_sync(w.world, w.hydra, 0, {}).empty());
  for (int i = 0; i < 3; ++i) tick();
  CHECK(fake::list_values(phasmo::GhostInfo::givenEvidence.get(w.world.info)).empty());
  // The contract still holds the evidence, as the game's journal shows every evidence's row whatever
  // the difficulty gives.
  CHECK(side.held().count(8) == 1);
}

// The game's evidence pick (0x22AFCB0) with the host's hooks where its detours run: before the pick, after
// GetEvidenceList returns, and after the pick. It reads the controller's behaviour type for
// GetGhostOfType, then for each of the 3 - evidenceGiven removals again: for the Hantu (13), Goryo (14),
// Obake (19), Moroi (21) and Deogen (22) the removal's range ends one short. `rolls` are the removals'
// indexes. Returns the given evidence; the behaviour type must be the ghost's own again after.
std::vector<int32_t> game_pick_with_hooks(FlameWorld& w, int32_t behaviour, int32_t evidenceGiven,
                                          const std::vector<int32_t>& rolls) {
  void* const c = w.world.controller;
  phasmo::GhostController::behaviourType.set(c, behaviour);
  w.types.before_pick_evidence(c);
  std::vector<int32_t> given;
  if (evidenceGiven >= 1) {
    void* data = w.world.entry_of(phasmo::GhostController::behaviourType.get(c));
    CHECK(data != nullptr);
    if (data) {
      given = {phasmo::GhostData::evidence1.get(data), phasmo::GhostData::evidence2.get(data),
               phasmo::GhostData::evidence3.get(data)};
      w.types.after_evidence_list();
      for (int32_t i = 0; i < 3 - evidenceGiven; ++i) {
        const int32_t t = phasmo::GhostController::behaviourType.get(c);
        const bool shortRange = t == 13 || t == 14 || t == 19 || t == 21 || t == 22;
        const int32_t count = static_cast<int32_t>(given.size()) - (shortRange ? 1 : 0);
        given.erase(given.begin() + rolls.at(static_cast<size_t>(i)) % count);
      }
    }
  }
  w.types.after_pick_evidence();
  CHECK(phasmo::GhostController::behaviourType.get(c) == behaviour);
  return given;
}

content::GhostDef keeper_def() {
  content::GhostDef g = hydra_def();  // EMF, the custom flame, Ghost Orbs last
  g.flags = PM_GHOST_KEEPS_LAST_EVIDENCE;
  return g;
}

// A custom ghost with PM_GHOST_KEEPS_LAST_EVIDENCE: one evidence withheld, the game's own pick never takes
// its last, whatever it rolls; another custom ghost loses whichever the roll names.
void custom_ghost_keeps_last_evidence_when_one_withheld() {
  start();
  FlameWorld w(keeper_def());
  w.tick();
  for (int32_t roll = 0; roll < 6; ++roll) {
    const std::vector<int32_t> given = game_pick_with_hooks(w, w.hydra, 2, {roll});
    CHECK(given.size() == 2 && given.back() == 4);
  }
  CHECK((game_pick_with_hooks(w, w.lethe, 2, {2}) == std::vector<int32_t>{1, 4}));  // the Lethe's last goes
  CHECK(w.types.create_hooks());
  const std::vector<std::string> hooked = fake::hooked();
  CHECK(std::count(hooked.begin(), hooked.end(), "GhostController.PickEvidence") == 1);
  CHECK(std::count(hooked.begin(), hooked.end(), "GhostData.GetEvidenceList") == 1);
}

// Two withheld: only its last stays; without the flag the last may go.
void custom_ghost_keeps_last_evidence_when_two_withheld() {
  start();
  FlameWorld w(keeper_def());
  w.tick();
  for (int32_t a = 0; a < 3; ++a)
    for (int32_t b = 0; b < 3; ++b) CHECK((game_pick_with_hooks(w, w.hydra, 1, {a, b}) == std::vector<int32_t>{4}));
  start();
  FlameWorld plain(hydra_def());
  plain.tick();
  CHECK((game_pick_with_hooks(plain, plain.hydra, 1, {2, 1}) == std::vector<int32_t>{1}));
}

// None given: none at all, as for the Goryo; its type is never changed, since the pick stops before the
// list is made, and a list made outside a pick changes nothing.
void custom_ghost_keeps_none_on_zero_evidence() {
  start();
  FlameWorld w(keeper_def());
  w.tick();
  CHECK(game_pick_with_hooks(w, w.hydra, 0, {}).empty());
  w.types.after_evidence_list();
  CHECK(phasmo::GhostController::behaviourType.get(w.world.controller) == w.hydra);
}

// A ghost that gives no evidence (the Squatter's spec, docs/superpowers/specs/2026-11-04-squatter-design.md,
// "Technical unknowns" 6). Three empty slots register, null or "" alike. Its GhostData holds the game's own "no
// evidence" (EvidenceType.None) in each, and the game's pick and sync then give only that value on every difficulty:
// GetEvidenceList hands the pick all three slots, and SyncValues sends as many as the difficulty gives, so a list of
// that length must reach it (sdk/systems/evidence.md, "A ghost that gives no evidence"). No evidence a player can
// find is ever given.
void a_ghost_may_give_no_evidence() {
  start();
  // The C definition with three null slots, as a mod built on core/mod_api.h alone gives it.
  registry::Registry r = fresh();
  r.begin_group();
  content::GhostDef quiet = named("a.quiet", "Quiet");
  quiet.evidence = {"", "", ""};
  PmGhostDef raw = quiet.c_def();
  for (const char*& e : raw.evidence) e = nullptr;
  CHECK(r.add_ghost(&raw) > 0);
  r.end_group(true);
  r.commit();
  const registry::Ghost* held = r.registered("a.quiet");
  CHECK(held && held->evidence[0].empty() && held->evidence[1].empty() && held->evidence[2].empty());

  // Through the content wrapper, beside a ghost that gives evidence, on the faked game.
  start();
  g_world = nullptr;
  GameWorld world;
  g_world = &world;
  namespace map = pm::map;
  int difficultyClass = 0;
  fake::type(map::Difficulty, &difficultyClass);
  fake::field(map::Difficulty_evidenceGiven, 0x4C);
  fake::field(map::GhostController_givenEvidence, 0x30);
  fake::field(map::GhostInfo_givenEvidence, 0x30);
  std::string missing;
  api::bind(fake::host(), missing);
  CHECK(content::register_ghost(quiet) > 0);
  CHECK(content::register_ghost(named("a.lethe", "Lethe")) > 0);
  fake::commit_content();
  room(1, {1}, {});
  pm::host::ContentTypes types{fake::content_registry(), fake::acting(), fake_lists()};
  types.tick(fake::content_state(), true, true);
  const int32_t type = content::type_of("a.quiet"), none = fake::kAuditedNoEvidence;
  void* data = world.entry_of(type);
  CHECK(type > 0 && data);
  if (!data) return;
  CHECK(phasmo::GhostData::evidence1.get(data) == none && phasmo::GhostData::evidence2.get(data) == none &&
        phasmo::GhostData::evidence3.get(data) == none);
  CHECK(types.held().count(type) && types.held().at(type).evidence[0] == none);
  for (int32_t given = 0; given <= 3; ++given)
    for (int32_t roll = 0; roll < 3; ++roll) {
      const std::vector<int32_t> rolls(3, roll);
      const std::vector<int32_t> sent = game_pick_and_sync(world, type, given, rolls);
      CHECK(sent.size() == static_cast<size_t>(given));
      CHECK(std::all_of(sent.begin(), sent.end(), [&](int32_t e) { return e == none; }));
    }
  // The other ghost still gives its own.
  CHECK((game_pick_and_sync(world, content::type_of("a.lethe"), 3, {}) == std::vector<int32_t>{1, 4, 6}));
}

// The Mimic's copy list gets the contract's custom ghosts after the game's own, but never one an author registered with
// PM_GHOST_NO_MIMIC_COPY in its GhostDef's flags; one without the flag is added as before.
void the_mimic_leaves_out_what_it_may_not_copy() {
  start();
  GameWorld world;
  g_world = &world;
  content::GhostDef never = named("a.lethe", "Lethe");
  never.flags = PM_GHOST_NO_MIMIC_COPY;
  CHECK(content::register_ghost(named("a.asterion", "Asterion")) > 0);
  CHECK(content::register_ghost(never) > 0);
  CHECK(content::register_ghost(named("a.zephyr", "Zephyr")) > 0);
  fake::commit_content();
  room(1, {1}, {});
  CHECK(fake::content_registry().registered("a.lethe")->flags == PM_GHOST_NO_MIMIC_COPY &&
        fake::content_registry().registered("a.zephyr")->flags == 0);
  pm::host::ContentTypes types{fake::content_registry(), fake::acting(), fake_lists()};
  types.tick(fake::content_state(), true, true);
  std::vector<int32_t> built;  // as the game builds it: every type but None
  for (int32_t t = 0; t <= 30; ++t)
    if (t != 24) built.push_back(t);
  void* list = fake::value_list(built);
  phasmo::GhostController::mimicCopies.set(world.controller, list);
  types.after_build_mimic_copies(world.controller);
  std::vector<int32_t> expected = built;
  expected.insert(expected.end(), {31, 33});  // the Asterion and the Zephyr, never the Lethe
  CHECK(fake::list_values(list) == expected);
  types.tick(fake::content_state(), true, true);  // each frame's top-up leaves it out too
  CHECK(fake::list_values(list) == expected);
}

// The evidence priority through the host's own detours (detour_pick_evidence, detour_evidence_list), as
// the game calls them: a test host captures each detour the host hooks (as tests/hooks_test.cpp's does),
// and a fake of the game's pick (0x22AFCB0) calls GetEvidenceList through its captured detour.
namespace through_detours {

PmHostApi g_host{};
bool (*g_fakeHookBody)(const char*, uint32_t, void*, void**, const char*) = nullptr;
std::map<std::string, void*> g_detour;  // the detour the host hooked, by entry key
FlameWorld* g_w = nullptr;
int32_t g_evidenceGiven = 3;
std::vector<int32_t> g_rolls, g_given;
std::vector<std::string> g_events;
bool g_throw = false;

bool capture_hook_body(const char* key, uint32_t index, void* detour, void** original, const char* name) {
  if (!g_fakeHookBody(key, index, detour, original, name)) return false;
  g_detour[key] = detour;
  return true;
}

// GhostData.GetEvidenceList, the game's own body: its three evidence in order.
void* game_evidence_list(void* data, const void*) {
  g_events.push_back("list");
  return fake::value_list({phasmo::GhostData::evidence1.get(data), phasmo::GhostData::evidence2.get(data),
                           phasmo::GhostData::evidence3.get(data)});
}

// GhostController.PickEvidence, the game's own body as decompiled: GetGhostOfType on the behaviour type,
// GetEvidenceList (through its hook), then each removal reads the behaviour type again for its range.
void game_pick(void* controller, const void*) {
  g_events.push_back("pick");
  g_given.clear();
  if (g_evidenceGiven < 1) return;
  void* data = g_w->world.entry_of(phasmo::GhostController::behaviourType.get(controller));
  using List = void* (*)(void*, const void*);
  void* list = reinterpret_cast<List>(g_detour.at("GhostData.GetEvidenceList"))(data, nullptr);
  std::vector<int32_t> given = fake::list_values(list);
  if (g_throw) throw std::runtime_error("the game's pick threw");
  for (int32_t i = 0; i < 3 - g_evidenceGiven; ++i) {
    const int32_t t = phasmo::GhostController::behaviourType.get(controller);
    g_events.push_back("removal reads " + std::to_string(t));
    const bool shortRange = t == 13 || t == 14 || t == 19 || t == 21 || t == 22;
    const int32_t count = static_cast<int32_t>(given.size()) - (shortRange ? 1 : 0);
    given.erase(given.begin() + g_rolls.at(static_cast<size_t>(i)) % count);
  }
  g_given = given;
}

// The world with the host's hooks made through the capturing host.
void hook(FlameWorld& w) {
  g_w = &w;
  fake::method(pm::map::GhostController_PickEvidence, reinterpret_cast<void*>(&game_pick));
  fake::method(pm::map::GhostData_GetEvidenceList, reinterpret_cast<void*>(&game_evidence_list));
  g_host = *fake::host();
  g_fakeHookBody = g_host.hook_body;
  g_host.hook_body = &capture_hook_body;
  g_detour.clear();
  std::string missing;
  api::bind(&g_host, missing);
  CHECK(w.types.create_hooks());
  CHECK(g_detour.count("GhostController.PickEvidence") == 1 && g_detour.count("GhostData.GetEvidenceList") == 1);
}

// The game calls the pick through its hook.
void pick(FlameWorld& w, int32_t behaviour) {
  phasmo::GhostController::behaviourType.set(w.world.controller, behaviour);
  g_events.clear();
  using Pick = void (*)(void*, const void*);
  reinterpret_cast<Pick>(g_detour.at("GhostController.PickEvidence"))(w.world.controller, nullptr);
}

}  // namespace through_detours

// Through the real detours: the pick's detour follows the pick, the list's detour switches the type only
// after the list is made, so GetGhostOfType has the custom ghost's own type and every removal reads the
// Goryo's; the custom type is back when the pick returns.
void evidence_priority_runs_through_the_host_detours() {
  using namespace through_detours;
  start();
  FlameWorld w(keeper_def());
  w.tick();
  hook(w);
  g_evidenceGiven = 1;
  g_rolls = {1, 1};
  g_throw = false;
  pick(w, w.hydra);
  CHECK((g_events == std::vector<std::string>{"pick", "list", "removal reads 14", "removal reads 14"}));
  CHECK((g_given == std::vector<int32_t>{4}));
  CHECK(phasmo::GhostController::behaviourType.get(w.world.controller) == w.hydra);
  pick(w, w.lethe);  // another custom ghost keeps its own type, and the roll may take its last
  CHECK((g_events == std::vector<std::string>{"pick", "list", "removal reads 32", "removal reads 32"}));
  CHECK((g_given == std::vector<int32_t>{1}));
}

// A pick that throws after its list still finds the custom type put back, by the pick's detour, and a
// list made later outside any pick switches nothing.
void evidence_priority_restored_when_pick_throws() {
  using namespace through_detours;
  start();
  FlameWorld w(keeper_def());
  w.tick();
  hook(w);
  g_evidenceGiven = 2;
  g_rolls = {0};
  g_throw = true;
  bool threw = false;
  try {
    pick(w, w.hydra);
  } catch (const std::runtime_error&) {
    threw = true;
  }
  g_throw = false;
  CHECK(threw && phasmo::GhostController::behaviourType.get(w.world.controller) == w.hydra);
  using List = void* (*)(void*, const void*);
  reinterpret_cast<List>(g_detour.at("GhostData.GetEvidenceList"))(w.world.entry_of(w.hydra), nullptr);
  CHECK(phasmo::GhostController::behaviourType.get(w.world.controller) == w.hydra);
}

// While a contract holds content, its custom numbers and ids answer even when the room's content goes
// off, as its entries keep them; once it ends they do not, and a contract that loads while content is
// off holds none.
void contract_numbers_answer_while_content_is_off() {
  start();
  FlameWorld w(hydra_def());
  w.tick();  // the contract loads with content active
  room(1, {1, 2}, {}, 100000, 2);
  room(1, {1, 2}, {}, 200000, 2);  // a host without the content, silent past the grace: off
  CHECK(fake::content_state() == State::Off && !content::active());
  w.tick();
  CHECK(content::type_of("a.hydra") == 31 && content::id_of(31) == "a.hydra");
  CHECK(content::evidence_of("z.flame") == 8 && content::evidence_of("emf") == 1);
  w.world.controllerInstance = nullptr;  // the contract ends
  w.tick();
  CHECK(content::type_of("a.hydra") == -1 && content::id_of(31).empty() && content::evidence_of("z.flame") == -1);
  w.world.controllerInstance = w.world.controller;  // the next loads with content off
  w.tick();
  CHECK(content::type_of("a.hydra") == -1 && content::evidence_of("z.flame") == -1);
}

// How the contract's ghost changes an evidence's look: its own definition's evidenceLook, by the ghost's
// identity, for the whole contract.
void* g_lookUser = nullptr;
const char* hydra_look(const char* evidenceId, void* user) {
  g_lookUser = user;
  return std::string_view(evidenceId) == "z.flame" ? "green" : nullptr;
}

void evidence_look_answers_ghost_choice() {
  start();
  CHECK(content::evidence_look("z.flame").empty());  // before any content
  int user = 0;
  content::GhostDef hydra = hydra_def();
  hydra.evidenceLook = &hydra_look;
  hydra.user = &user;
  FlameWorld w(hydra);
  w.tick();
  CHECK(content::evidence_look("z.flame").empty());  // no ghost yet
  w.world.ghost_is(w.hydra, 24);
  w.tick();
  CHECK(content::evidence_look("z.flame") == "green" && g_lookUser == &user);
  CHECK(content::evidence_look("emf").empty());  // its look of another evidence: the evidence's own
  w.world.ghost_is(w.lethe, 24);                 // a ghost without a look
  w.tick();
  CHECK(content::evidence_look("z.flame").empty());
  w.world.ghost_is(20, 20);  // a Mimic: the game's ghosts have no look of their own
  w.tick();
  CHECK(content::evidence_look("z.flame").empty());
  // Copying the Hydra it acts as one, but it is still a Mimic, whose look it keeps.
  phasmo::GhostInfo::behaviourType.set(w.world.info, w.hydra);
  w.types.after_mimic_copy(w.world.controller);
  CHECK(content::acting_as() == w.hydra && content::evidence_look("z.flame").empty());
  // Content going off mid-contract keeps the contract's ghost and its look.
  w.world.ghost_is(w.hydra, 24);
  w.tick();
  room(1, {1, 2}, {}, 100000, 2);
  room(1, {1, 2}, {}, 200000, 2);
  CHECK(fake::content_state() == State::Off);
  w.tick();
  CHECK(content::evidence_look("z.flame") == "green");
  CHECK(content::evidence_look("").empty());
}

// A player controlling the ghost reaches its definition through the content layer, by the type it acts as,
// for any registered ghost: its ability, its HUD state and line, its held-back check, its player view and
// its target. A vanilla type answers only its rules' held-back check (Ghost Tells' tells).
struct Controlled {
  int abilities = 0;
  int32_t lastState = -1;
};
Controlled* g_controlled = nullptr;
int g_target = 0;  // stands for a Player
bool g_mareHeld = false;
int g_lostAsks = 0;  // how often the controlled ghost's interactionLost was asked

content::GhostDef controlled_def(Controlled& c) {
  content::GhostDef g = hydra_def();
  g.user = &c;
  g.abilityName = content::Text("en", "Split").add("de", "Teilen");
  g.abilityDescription = content::Text("en", "Become two.");
  g.useAbility = [](void* user) { return ++static_cast<Controlled*>(user)->abilities > 0; };
  g.hudState = [](void*) { return int32_t{7}; };
  g.hudLine = [](int32_t state, void* user) {
    static_cast<Controlled*>(user)->lastState = state;
    return state == 7 ? "Seven heads" : static_cast<const char*>(nullptr);
  };
  g.heldBack = [](int32_t kind, void*) { return kind == 1; };
  g.playerView = [](float* xyz, uint32_t max, void*) {
    for (uint32_t i = 0; i < 3 * std::min<uint32_t>(max, 2); ++i) xyz[i] = static_cast<float>(i);
    return uint32_t{5};  // more than it was given room for: the host keeps to `max`
  };
  g.target = [](const void* const* excluded, uint32_t count, void*) -> void* {
    for (uint32_t i = 0; i < count; ++i)
      if (excluded[i] == &g_target) return nullptr;
    return &g_target;
  };
  g.playerMove = [](const float* at, float* dx, float* dz, void*) {  // held to +x, and still from x > 5
    *dz = 0;
    if (at[0] > 5) *dx = 0;
  };
  g.interactionLost = [](void*) { return ++g_lostAsks % 2 == 1; };
  g.flags = PM_GHOST_BLIND_IN_HUNTS;
  return g;
}

void controlled_ghost_answers_through_its_definition() {
  start();
  CHECK(!content::use_ability() && content::hud_state() == -1 && !content::held_back(1));  // before bind
  std::string missing;
  api::bind(fake::host(), missing);
  content::GhostRules mare;
  mare.heldBack = [](int32_t kind, void*) { return g_mareHeld && kind == 2; };
  CHECK(content::attach_rules("vanilla.mare", mare) > 0);
  Controlled c;
  g_controlled = &c;
  FlameWorld w(controlled_def(c));
  w.tick();
  float xyz[6] = {};
  // No ghost yet: nothing answers.
  CHECK(!content::use_ability() && content::ability_name().empty() && content::hud_state() == -1 &&
        content::hud_line(7).empty() && !content::held_back(1) && content::player_view(xyz, 2) == 0 &&
        !content::target());
  w.world.ghost_is(w.hydra, 24);
  w.tick();
  CHECK(content::use_ability() && c.abilities == 1);
  CHECK(content::ability_name() == "Split" && content::ability_description() == "Become two.");
  CHECK(content::hud_state() == 7 && content::hud_line(7) == "Seven heads" && c.lastState == 7);
  CHECK(content::hud_line(3).empty());
  CHECK(content::held_back(1) && !content::held_back(0) && !content::held_back(2));
  CHECK(content::player_view(xyz, 2) == 2 && xyz[5] == 5.0f);
  CHECK(content::target() == &g_target && content::flags() == PM_GHOST_BLIND_IN_HUNTS);
  const void* const excluded[] = {nullptr, &g_target};
  CHECK(!content::target(excluded, 2));  // the players excluded are passed to the definition
  CHECK(content::target(excluded, 1) == &g_target);
  const float near[3] = {1, 0, 0}, far[3] = {6, 0, 0};
  float dx = 0.6f, dz = 0.8f;
  content::player_move(near, dx, dz);  // the definition's limit on where its player steers
  CHECK(dx == 0.6f && dz == 0.0f);
  content::player_move(far, dx, dz);
  CHECK(dx == 0.0f && dz == 0.0f);
  g_lostAsks = 0;
  CHECK(content::interaction_lost() && !content::interaction_lost() && g_lostAsks == 2);  // asked each time
  CHECK(!content::roll_taken());
  fake::roll_taken(true);  // what Playable Ghost reports, for any ghost
  CHECK(content::roll_taken());
  fake::roll_taken(false);
  // A ghost without the callbacks answers nothing.
  w.world.ghost_is(w.lethe, 24);
  w.tick();
  CHECK(!content::use_ability() && content::ability_name().empty() && content::hud_state() == -1 &&
        !content::target() && content::player_view(xyz, 2) == 0 && content::flags() == 0);
  dx = 0.6f;
  dz = 0.8f;
  content::player_move(far, dx, dz);  // no limit: the direction stays the player's
  CHECK(dx == 0.6f && dz == 0.8f);
  CHECK(!content::interaction_lost());
  // A Mimic that copies it acts as it, so it is the copy's definition that answers.
  w.world.ghost_is(20, 20);
  w.tick();
  CHECK(!content::target());
  phasmo::GhostInfo::behaviourType.set(w.world.info, w.hydra);
  w.types.after_mimic_copy(w.world.controller);
  CHECK(content::acting_as() == w.hydra && content::target() == &g_target && content::hud_state() == 7);
  // A vanilla type: only its rules' held-back check.
  w.world.ghost_is(6, 6);
  w.tick();
  CHECK(!content::held_back(2) && content::hud_state() == -1 && content::ability_name().empty());
  g_mareHeld = true;
  CHECK(content::held_back(2) && !content::held_back(0));
  g_mareHeld = false;
  g_controlled = nullptr;
}

// Every entry the game side needs must resolve, or the host withholds custom content.
void missing_entries_withhold_content() {
  start();
  pm::host::ContentTypes types{fake::content_registry(), fake::acting(), fake_lists()};
  CHECK(types.missing().find("GhostHuntState.UpdateSpeed") != std::string::npos);
  CHECK(content::register_ghost(ghost("brownies.brownie")) > 0);
  fake::content_registry().withhold("these game entries did not resolve: " + types.missing());
  fake::commit_content();
  room(1, {1}, {});
  CHECK(!content::active() && content::type_of("vanilla.mare") == 6);
  CHECK(logged("custom content stays off: these game entries did not resolve"));
}

// Content the host has switched off before the mods start is refused at registration, so a mod whose ghost or evidence
// could never come into play fails its own start and is off, with the reason logged, instead of running without it.
// Rules and journal lines on the game's own ghosts are not content the host puts into the game, so they still register.
void content_switched_off_refuses_custom_registrations() {
  start();
  fake::content_registry().withhold("its hooks on the game could not be made");
  CHECK(content::register_ghost(ghost("brownies.brownie")) == 0);
  CHECK(logged("the ghost \"brownies.brownie\" is refused: custom content is off: its hooks on the game could not be made"));
  CHECK(content::register_evidence(evidence("flames.flame")) == 0);
  CHECK(logged("the evidence \"flames.flame\" is refused: custom content is off: its hooks on the game could not be made"));
  CHECK(content::attach_rules("vanilla.mare", {}) > 0);
  CHECK(content::add_journal_lines("vanilla.mare", {content::Text("en", "Tells: dark.")}) > 0);

  // The game's enums unreadable: the same.
  g_lines.clear();
  registry::Registry r;
  r.refuse("the game's GhostType enum could not be read");
  r.begin_group();
  const content::GhostDef hydra = ghost("ghosts.hydra");
  const content::EvidenceDef flame = evidence("flames.flame");
  CHECK(r.add_ghost(&hydra.c_def()) == 0 && r.add_evidence(&flame.c_def()) == 0);
  CHECK(logged("the ghost \"ghosts.hydra\" is refused: custom content is off: the game's GhostType enum could not be read"));
  r.end_group(false);
}

}  // namespace

int main() {
  use_text({"sdk/examples/example_ghost/i18n/default.json"});
  the_example_ghost_registers_and_keeps_its_beat();
  ghost_ids_number_in_byte_order();
  evidence_numbers_from_8();
  duplicate_id_is_refused();
  missing_english_is_refused();
  over_the_limits_is_refused();
  unknown_evidence_is_refused();
  evidence_from_any_started_mod();
  registration_outside_a_start_is_refused();
  content_follows_the_host_and_no_other_player();
  manifests_equal_activates();
  pending_until_heard_then_off();
  emptied_content_is_announced();
  manifest_is_resent_and_retried();
  long_manifest_arrives_in_parts();
  failed_start_withdraws_its_content();
  vanilla_ids_answer();
  game_enums_number_content();
  extra_enum_value_moves_numbering();
  game_enums_read_from_the_model();
  unreadable_enums_keep_content_off();
  player_controlled_reads_playable_ghost();
  room_agreement_is_the_hosts();
  before_bind_answers_inactive();
  manifests_from_other_enums_disagree();
  colliding_enum_names_are_unusable();
  enum_ranges_are_logged();
  rules_attach_and_are_refused();
  custom_type_joins_collection_in_alphabetical_place();
  key_form_comes_from_the_game();
  name_key_answers_custom_name();
  identity_is_custom_behaviour_is_none();
  mimic_copy_list_includes_custom_types();
  mimic_copy_switches_rules();
  vanilla_rules_follow_behaviour_type();
  acting_as_reaches_every_client();
  acting_as_is_sent_on_change_and_to_joiners();
  acting_as_send_is_gated();
  early_copy_is_kept();
  hunt_speed_writes_inputs_not_result();
  contract_keeps_its_content();
  contract_end_withdraws();
  contract_decision_and_agreement_share_one_signal();
  a_contract_an_unverified_player_reached_has_no_custom_ghosts();
  contract_loaded_without_content_stays_vanilla();
  refused_list_operations_are_logged_and_retried();
  refusals_are_logged_again_after_success();
  leftover_entry_is_replaced_at_contract_start();
  acting_as_resent_for_next_contract();
  custom_evidence_given_when_listed();
  withheld_evidence_not_given();
  custom_ghost_keeps_last_evidence_when_one_withheld();
  custom_ghost_keeps_last_evidence_when_two_withheld();
  custom_ghost_keeps_none_on_zero_evidence();
  a_ghost_may_give_no_evidence();
  the_mimic_leaves_out_what_it_may_not_copy();
  evidence_priority_runs_through_the_host_detours();
  evidence_priority_restored_when_pick_throws();
  evidence_look_answers_ghost_choice();
  controlled_ghost_answers_through_its_definition();
  contract_numbers_answer_while_content_is_off();
  missing_entries_withhold_content();
  content_switched_off_refuses_custom_registrations();
  agreement::a_host_without_a_mod_gets_nothing_run();
  agreement::the_hosts_mods_here_run();
  agreement::a_peer_leaving_changes_nothing_mid_contract();
  agreement::a_mismatched_build_is_not_agreed();
  agreement::the_lists_are_read_only_on_a_change();
  agreement::a_host_whose_mods_stop_is_no_longer_agreed();
  pm::log::set_sink(nullptr, nullptr);
  return g_failures ? 1 : 0;
}
