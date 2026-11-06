#include "content_registry.h"

#include <algorithm>
#include <cstring>
#include <iterator>

#include "log.h"
#include "mod_ids.h"

namespace pm::content {
namespace {

// The size of each definition in version 2.2, its first: a smaller one is not a definition.
constexpr uint32_t kGhostDefSize = offsetof(PmGhostDef, evidenceLook) + sizeof(PmGhostDef::evidenceLook);
constexpr uint32_t kEvidenceDefSize = offsetof(PmEvidenceDef, journalLine) + sizeof(PmEvidenceDef::journalLine);
constexpr uint32_t kRulesSize = offsetof(PmGhostRules, heldBack) + sizeof(PmGhostRules::heldBack);

// Copies the first `size` bytes a mod gave of a definition (no more than this host knows) into a
// zeroed one, so a member past `size` reads as zero.
template <class Def>
Def copy_def(const Def* def) {
  Def out{};
  std::memcpy(&out, def, std::min<size_t>(def->size, sizeof out));
  return out;
}

// Why the id `id` cannot name content, or empty when it can.
std::string bad_id(const char* id) {
  if (!id || !*id) return "it has no id";
  const std::string_view v(id);
  if (v.size() > kMaxIdBytes) return "its id is longer than 255 bytes";
  if (!modid::split_content_id(v)) return "its id is not \"<mod id>.<name>\"";
  if (v.substr(0, kVanillaPrefix.size()) == kVanillaPrefix) return "ids beginning \"vanilla.\" are the game's own";
  return {};
}

// Copies the text set `set` into `out`, sorted by language. Empty `why` when it is valid: at most
// kMaxLanguages items, each with a language and a text of at most kMaxTextBytes, no language twice,
// and English among them when `required` or when any text is given. `what` names the text in `why`.
bool copy_text(const PmTextSet& set, bool required, const char* what, Text& out, std::string& why) {
  out.clear();
  if (set.count && !set.items) {
    why = std::string(what) + " has no items";
    return false;
  }
  if (set.count > kMaxLanguages) {
    why = std::string(what) + " has more than 64 languages";
    return false;
  }
  for (uint32_t i = 0; i < set.count; ++i) {
    const PmText& t = set.items[i];
    if (!t.lang || !*t.lang || !t.text) {
      why = std::string(what) + " has a text without its language or its text";
      return false;
    }
    if (std::strlen(t.text) > kMaxTextBytes || std::strlen(t.lang) > kMaxTextBytes) {
      why = std::string(what) + " has a text longer than 64 KB";
      return false;
    }
    out.emplace_back(t.lang, t.text);
  }
  std::sort(out.begin(), out.end());
  for (size_t i = 1; i < out.size(); ++i)
    if (out[i].first == out[i - 1].first) {
      why = std::string(what) + " names the language \"" + out[i].first + "\" twice";
      return false;
    }
  if ((required || !out.empty()) &&
      std::none_of(out.begin(), out.end(), [](const auto& t) { return t.first == "en"; })) {
    why = std::string(what) + " has no English text";
    return false;
  }
  return true;
}

// A 64-bit FNV-1a hash of a definition, fed every piece with its length first so that no two
// definitions read the same.
struct Hash {
  uint64_t value = 0xcbf29ce484222325ull;  // pm:not-game-data: the FNV-1a offset basis
  void bytes(const void* data, size_t size) {
    for (size_t i = 0; i < size; ++i) {
      value ^= static_cast<const uint8_t*>(data)[i];
      value *= 0x100000001b3ull;  // pm:not-game-data: the FNV-1a prime
    }
  }
  void number(uint64_t n) { bytes(&n, sizeof n); }
  void text(std::string_view s) {
    number(s.size());
    bytes(s.data(), s.size());
  }
  void text(const Text& t) {
    number(t.size());
    for (const auto& [lang, words] : t) {
      text(lang);
      text(words);
    }
  }
};

void put_hash(std::vector<uint8_t>& out, uint64_t hash) {
  for (int i = 0; i < 8; ++i) out.push_back(static_cast<uint8_t>(hash >> (8 * i)));
}

uint64_t get_hash(const uint8_t* at) {
  uint64_t hash = 0;
  for (int i = 0; i < 8; ++i) hash |= static_cast<uint64_t>(at[i]) << (8 * i);
  return hash;
}

// The index of `id` among the keys of `m`, in byte order, or -1.
template <class Map>
int32_t index_of(const Map& m, std::string_view id) {
  const auto it = m.find(id);
  return it == m.end() ? -1 : static_cast<int32_t>(std::distance(m.begin(), it));
}

}  // namespace

void Registry::begin_group() {
  groupStart_ = nextHandle_;
  open_ = true;
}

void Registry::end_group(bool started) {
  open_ = false;
  if (started) return;
  std::erase_if(rules_, [&](const Rules& r) { return r.handle >= groupStart_; });
  std::erase_if(lines_, [&](const Lines& l) { return l.handle >= groupStart_; });
  const auto erased = std::erase_if(ghosts_, [&](const auto& g) { return g.second.handle >= groupStart_; }) +
                      std::erase_if(evidence_, [&](const auto& e) { return e.second.handle >= groupStart_; });
  if (erased) changed();
}

void Registry::commit() {
  open_ = false;
  // Without the game's enums no evidence can be judged, and content stays off anyway.
  if (enums_.read()) std::erase_if(ghosts_, [&](const auto& entry) {
    for (const std::string& e : entry.second.evidence)
      if (!e.empty() && enums_.evidence_value(e) < 0 && !evidence_.count(e)) {
        PM_LOG("content: the ghost \"%s\" is dropped: no mod that started registered its evidence \"%s\"",
               entry.first.c_str(), e.c_str());
        return true;
      }
    return false;
  });
  std::erase_if(rules_, [&](const Rules& r) {
    if (r.id.substr(0, kVanillaPrefix.size()) == kVanillaPrefix || ghosts_.count(r.id)) return false;
    PM_LOG("content: rules on \"%s\" are dropped: no mod that started registered that ghost", r.id.c_str());
    return true;
  });
  committed_ = true;
  changed();
}

void Registry::clear() {
  ghosts_.clear();
  evidence_.clear();
  rules_.clear();
  lines_.clear();
  open_ = false;
  committed_ = false;
  changed();
}

void Registry::set_game_enums(GameEnums enums) {
  enums_ = std::move(enums);
  if (enums_.read())
    PM_LOG("content: GhostType %lld..%d, EvidenceType %lld..%d", static_cast<long long>(enums_.lowestGhostType),
           enums_.firstGhostType - 1, static_cast<long long>(enums_.lowestEvidence), enums_.firstEvidence - 1);
  for (const std::string& name : enums_.unusable)
    PM_LOG("content: the game's enum value \"%s\" has no usable name, so it gets no id", name.c_str());
  changed();
}

void Registry::refuse(std::string why) {
  if (why.empty() || !refused_.empty()) return;
  refused_ = std::move(why);
  enums_ = {};  // whatever was read before no longer holds
  PM_LOG("content: custom content stays off: %s", refused_.c_str());
  changed();
}

void Registry::withhold(std::string why) {
  if (why.empty() || !withheld_.empty()) return;
  withheld_ = std::move(why);
  PM_LOG("content: custom content stays off: %s", withheld_.c_str());
  changed();
}

void Registry::changed() {
  manifest_.clear();
  if (committed_ && refused_.empty() && withheld_.empty() && enums_.read() && (!ghosts_.empty() || !evidence_.empty())) {
    for (const auto& [id, g] : ghosts_) manifest_.push_back({'g', id, g.hash});
    for (const auto& [id, e] : evidence_) manifest_.push_back({'e', id, e.hash});
    // The numbers content takes: clients whose games read different enums never agree.
    manifest_.push_back({'n', "", static_cast<uint64_t>(static_cast<uint32_t>(enums_.firstGhostType)) << 32 |
                                      static_cast<uint32_t>(enums_.firstEvidence)});
    std::sort(manifest_.begin(), manifest_.end());
  }
  ++serial_;
  next_ = 0;  // due at once
}

bool Registry::open(const char* what, const char* id) const {
  if (open_) return true;
  PM_LOG("content: the %s \"%s\" is refused: content is registered only while its mod starts", what, id ? id : "");
  return false;
}

int32_t Registry::add_ghost(const PmGhostDef* def) {
  if (!def || def->size < kGhostDefSize) {
    PM_LOG("content: a ghost is refused: %s", def ? "its definition is smaller than contract 2.2's" : "it has no definition");
    return 0;
  }
  const PmGhostDef d = copy_def(def);
  if (!open("ghost", d.id)) return 0;
  std::string why = bad_id(d.id);
  Ghost g;
  if (why.empty() && ghosts_.count(std::string_view(d.id))) why = "it is already registered";
  if (why.empty() && d.tellCount > kMaxTells) why = "it has more than 64 tells";
  if (why.empty() && copy_text(d.name, true, "its name", g.name, why) &&
      copy_text(d.description, true, "its description", g.description, why) &&
      copy_text(d.strength, true, "its strength", g.strength, why) &&
      copy_text(d.weakness, true, "its weakness", g.weakness, why)) {
    if (d.tellCount && !d.tells) why = "its tells have no items";
    for (uint32_t i = 0; why.empty() && i < d.tellCount; ++i)
      copy_text(d.tells[i], true, "a tell", g.tells.emplace_back(), why);
    if (why.empty() && (d.abilityName.count || d.abilityDescription.count) &&
        copy_text(d.abilityName, true, "its ability's name", g.abilityName, why))
      copy_text(d.abilityDescription, true, "its ability's description", g.abilityDescription, why);
    // A custom evidence may be registered by a mod that starts later: commit checks those. Without the
    // game's enums no vanilla id can be told from a wrong one, and content stays off anyway.
    for (size_t i = 0; why.empty() && i < g.evidence.size(); ++i) {
      const std::string e = d.evidence[i] ? d.evidence[i] : "";
      if (e.empty()) continue;
      if (enums_.read() && enums_.evidence_value(e) < 0 && !bad_id(e.c_str()).empty())
        why = "the evidence \"" + e + "\" is unknown";
      else if (std::find(g.evidence.begin(), g.evidence.end(), e) != g.evidence.end())
        why = "it lists the evidence \"" + e + "\" twice";
      g.evidence[i] = e;
    }
  }
  if (!why.empty()) {
    PM_LOG("content: the ghost \"%s\" is refused: %s", d.id ? d.id : "", why.c_str());
    return 0;
  }
  g.id = d.id;
  g.user = d.user;
  g.useAbility = d.useAbility;
  g.hudState = d.hudState;
  g.heldBack = d.heldBack;
  g.playerView = d.playerView;
  g.evidenceLook = d.evidenceLook;
  g.flags = d.flags;
  g.target = d.target;
  g.hudLine = d.hudLine;
  g.playerMove = d.playerMove;
  g.interactionLost = d.interactionLost;
  Hash h;
  h.text(g.id);
  for (const Text* t : {&g.name, &g.description, &g.strength, &g.weakness}) h.text(*t);
  h.number(g.tells.size());
  for (const Text& t : g.tells) h.text(t);
  for (const std::string& e : g.evidence) h.text(e);
  h.text(g.abilityName);
  h.text(g.abilityDescription);
  h.number(g.flags);  // its evidence pick depends on them
  g.hash = h.value;
  g.handle = nextHandle_++;
  const int32_t handle = g.handle;
  ghosts_.emplace(g.id, std::move(g));
  return handle;
}

int32_t Registry::add_evidence(const PmEvidenceDef* def) {
  if (!def || def->size < kEvidenceDefSize) {
    PM_LOG("content: an evidence is refused: %s",
           def ? "its definition is smaller than contract 2.2's" : "it has no definition");
    return 0;
  }
  const PmEvidenceDef d = copy_def(def);
  if (!open("evidence", d.id)) return 0;
  std::string why = bad_id(d.id);
  Evidence e;
  if (why.empty() && evidence_.count(std::string_view(d.id))) why = "it is already registered";
  if (why.empty() && copy_text(d.name, true, "its name", e.name, why))
    copy_text(d.journalLine, true, "its journal line", e.journalLine, why);
  if (!why.empty()) {
    PM_LOG("content: the evidence \"%s\" is refused: %s", d.id ? d.id : "", why.c_str());
    return 0;
  }
  e.id = d.id;
  Hash h;
  h.text(e.id);
  h.text(e.name);
  h.text(e.journalLine);
  e.hash = h.value;
  e.handle = nextHandle_++;
  const int32_t handle = e.handle;
  evidence_.emplace(e.id, std::move(e));
  return handle;
}

int32_t Registry::add_rules(const char* id, const PmGhostRules* rules) {
  if (!rules || rules->size < kRulesSize) {
    PM_LOG("content: rules on \"%s\" are refused: %s", id ? id : "",
           rules ? "their definition is smaller than contract 2.2's" : "they have no definition");
    return 0;
  }
  if (!open_) {
    PM_LOG("content: rules on \"%s\" are refused: content is registered only while its mod starts", id ? id : "");
    return 0;
  }
  std::string why;
  if (!id || !*id) why = "they name no ghost";
  else if (std::string_view(id).substr(0, kVanillaPrefix.size()) == kVanillaPrefix)
    why = enums_.read() && enums_.ghost_type(id) < 0 ? "the game has no such ghost type" : "";
  else why = bad_id(id);
  if (!why.empty()) {
    PM_LOG("content: rules on \"%s\" are refused: %s", id ? id : "", why.c_str());
    return 0;
  }
  Rules r;
  r.handle = nextHandle_++;
  r.id = id;
  r.rules = copy_def(rules);
  rules_.push_back(std::move(r));
  return rules_.back().handle;
}

int32_t Registry::add_lines(const char* id, const PmTextSet* lines, uint32_t count) {
  if (!open_) {
    PM_LOG("content: journal lines for \"%s\" are refused: content is registered only while its mod starts",
           id ? id : "");
    return 0;
  }
  std::string why;
  if (!id || std::string_view(id).substr(0, kVanillaPrefix.size()) != kVanillaPrefix)
    why = "they name no vanilla ghost";
  else if (enums_.read() && enums_.ghost_type(id) < 0) why = "the game has no such ghost type";
  else if (!lines || count == 0) why = "they have no lines";
  else if (count > kMaxTells) why = "they are more than 64 lines";
  Lines l;
  for (uint32_t i = 0; why.empty() && i < count; ++i) copy_text(lines[i], true, "a line", l.lines.emplace_back(), why);
  if (!why.empty()) {
    PM_LOG("content: journal lines for \"%s\" are refused: %s", id ? id : "", why.c_str());
    return 0;
  }
  l.handle = nextHandle_++;
  l.id = id;
  lines_.push_back(std::move(l));
  return lines_.back().handle;
}

std::vector<Text> Registry::lines_for(std::string_view id) const {
  std::vector<Text> out;
  for (const Lines& l : lines_)
    if (l.id == id && agreed()) out.insert(out.end(), l.lines.begin(), l.lines.end());
  return out;
}

void Registry::observe(int localActor, const std::vector<int>& actors, uint64_t now, int master) {
  now_ = now;
  if (localActor == 0) {
    peers_.clear();
    firstSeen_.clear();
    actors_.clear();
    local_ = 0;
    master_ = 0;
    sentInRoom_ = false;
    return;
  }
  // Forget the players who left; a manifest from one not yet seen in the room is kept for when they are.
  for (int a : actors_)
    if (std::find(actors.begin(), actors.end(), a) == actors.end()) {
      peers_.erase(a);
      firstSeen_.erase(a);
    }
  for (int a : actors)
    if (a != localActor) firstSeen_.try_emplace(a, now);
  if (actors != actors_ || localActor != local_) next_ = 0;  // somebody joined or left: tell them at once
  actors_ = actors;
  local_ = localActor;
  master_ = master != kLowestActor ? master : actors.empty() ? localActor : *std::min_element(actors.begin(), actors.end());
}

void Registry::receive(int sender, const uint8_t* data, size_t size) {
  if (!data || size < kHeaderBytes || data[0] != 'H' || data[1] != 'C' || data[2] != kVersion) return;
  if (local_ != 0 && sender == local_) return;
  const uint8_t serial = data[3], part = data[4], parts = data[5];
  if (parts == 0 || part >= parts) return;
  std::vector<Entry> entries;
  for (size_t at = kHeaderBytes; at < size;) {
    if (at + 2 > size || (data[at] != 'g' && data[at] != 'e' && data[at] != 'n')) return;  // malformed: ignore it
    const size_t length = data[at + 1];
    if (at + 2 + length + 8 > size) return;
    entries.push_back({data[at], std::string(reinterpret_cast<const char*>(data + at + 2), length), get_hash(data + at + 2 + length)});
    at += 2 + length + 8;
  }
  Peer& p = peers_[sender];
  if (p.got.size() != parts || p.serial != serial) {
    p = Peer{};
    p.serial = serial;
    p.parts.resize(parts);
    p.got.resize(parts, false);
  }
  p.parts[part] = std::move(entries);
  p.got[part] = true;
  if (std::find(p.got.begin(), p.got.end(), false) != p.got.end()) return;
  p.manifest.clear();
  for (const std::vector<Entry>& e : p.parts) p.manifest.insert(p.manifest.end(), e.begin(), e.end());
  std::sort(p.manifest.begin(), p.manifest.end());
  p.complete = true;
}

std::vector<std::vector<uint8_t>> Registry::take_messages() {
  // A client with content says so, even when refused (its manifest is then empty), and one that said
  // so in this room keeps saying what it has, so an empty manifest tells the others its content went.
  const bool hasContent = committed_ && (!ghosts_.empty() || !evidence_.empty());
  if (local_ == 0 || now_ < next_ || !(hasContent || sentInRoom_)) return {};
  next_ = now_ + kAnnounceMs;
  std::vector<std::vector<uint8_t>> out{{'H', 'C', kVersion, serial_, 0, 0}};
  for (const Entry& e : manifest_) {
    if (out.back().size() + 2 + e.id.size() + 8 > PM_MAX_MESSAGE)
      out.push_back({'H', 'C', kVersion, serial_, static_cast<uint8_t>(out.size()), 0});
    std::vector<uint8_t>& m = out.back();
    m.push_back(e.kind);
    m.push_back(static_cast<uint8_t>(e.id.size()));
    m.insert(m.end(), e.id.begin(), e.id.end());
    put_hash(m, e.hash);
  }
  if (out.size() > 255) {
    if (!tooLongLogged_) PM_LOG("content: the content manifest needs more than 255 messages; it is not sent");
    tooLongLogged_ = true;
    return {};
  }
  for (std::vector<uint8_t>& m : out) m[5] = static_cast<uint8_t>(out.size());
  sentInRoom_ = true;
  return out;
}

void Registry::send_failed() { next_ = now_ + kRetryMs; }

State Registry::state() const {
  if (local_ == 0 || manifest_.empty()) return State::Off;
  // The host's content is the lobby's: nobody else's manifest counts, on the host or anywhere else.
  if (master_ != local_) {
    const auto it = peers_.find(master_);
    if (it != peers_.end() && it->second.complete) {
      if (it->second.manifest != manifest_) return State::Off;
    } else {
      const auto seen = firstSeen_.find(master_);
      if (seen != firstSeen_.end() && now_ - seen->second >= kGraceMs) return State::Off;
      return State::Pending;
    }
  }
  // The manifests agree, but this client's mods do not match the host's (yet): the content waits, as for a host not
  // heard from.
  return agreed() ? State::Active : State::Pending;
}

int32_t Registry::type_of(std::string_view id) const {
  if (id.substr(0, kVanillaPrefix.size()) == kVanillaPrefix) return enums_.ghost_type(id);
  const int32_t index = answering() ? index_of(ghosts_, id) : -1;
  return index < 0 ? -1 : enums_.firstGhostType + index;
}

const char* Registry::id_of(int32_t type) const {
  if (!enums_.read() || type < 0) return nullptr;
  if (type < enums_.firstGhostType) return enums_.ghost_id(type);
  const size_t index = static_cast<size_t>(type - enums_.firstGhostType);
  return answering() && index < ghosts_.size()
             ? std::next(ghosts_.begin(), static_cast<ptrdiff_t>(index))->first.c_str()
             : nullptr;
}

int32_t Registry::evidence_of(std::string_view id) const {
  const int32_t vanilla = enums_.evidence_value(id);
  if (vanilla >= 0) return vanilla;
  const int32_t index = answering() ? index_of(evidence_, id) : -1;
  return index < 0 ? -1 : enums_.firstEvidence + index;
}

const Ghost* Registry::ghost(std::string_view id) const {
  const auto it = active() ? ghosts_.find(id) : ghosts_.end();
  return it == ghosts_.end() ? nullptr : &it->second;
}

std::vector<int32_t> Registry::custom_types() const {
  std::vector<int32_t> out;
  if (!active()) return out;
  for (size_t i = 0; i < ghosts_.size(); ++i) out.push_back(enums_.firstGhostType + static_cast<int32_t>(i));
  return out;
}

std::vector<PmGhostRules> Registry::rules_for(std::string_view id) const {
  std::vector<PmGhostRules> out;
  for (const Rules& r : rules_)
    if (r.id == id && agreed()) out.push_back(r.rules);
  return out;
}

}  // namespace pm::content
