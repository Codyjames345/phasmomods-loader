#pragma once

#include <algorithm>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// The one question a client asks about mods in a room: do the mods it runs equal the host's list? The master's list is the
// lobby's list. Nothing a joiner runs, says or fails to say changes it, or changes what any other player's mods do. So the host's
// mods run from the moment its lobby opens, and a joiner's run once it hears the host and matches (host/join_guard.h switches its
// extras off and takes it out of a room it cannot match). This one answer stands for every mod: the host gates everything it
// dispatches on the mods' behalf on it (their journal lines, their ghost rules, and the content their registrations put into
// the game), and no mod checks the other clients itself.
//
// No decision changes during a contract: a contract's start waits until everyone is verified (host/lobby_gate.h), so whatever
// a client's answer was as it loaded holds until it ends.
//
// A player is verified when what they announced equals the host's list. That says who may get a custom value or a mod's
// message; it never changes anyone's answer. Pure logic, run by the host and by the fake backend's tests. Main thread.
namespace pm::content {

// A started mod: its id and its build (the first bytes of its DLL's SHA-256, as presence names it).
struct ModBuild {
  std::string id;
  std::string build;
  bool operator==(const ModBuild&) const = default;
  bool operator<(const ModBuild& o) const { return id != o.id ? id < o.id : build < o.build; }
};

class Agreement {
 public:
  /// The mods this client started.
  void set_local(std::vector<ModBuild> mods) {
    std::sort(mods.begin(), mods.end());
    local_ = std::move(mods);
    agreed_ = false;
    seen_.reset();
    ++decisions_;
  }

  /// The mods the client of `actor` announces now, or none when it is not heard from now.
  using ModsOf = std::function<std::optional<std::vector<ModBuild>>(int actor)>;

  /// Every frame: whether this client is in a private room, whether a contract is loaded (the host passes
  /// ContentTypes::contract_loaded, the signal its content decides a contract on), this client's actor and the master's (0
  /// when unread), the other players in the room, `heard` (presence's revision, which changes whenever what a player announces
  /// may have) and what each announces now. The lists are read, copied and sorted only when one of these changed since the
  /// last call or set_local; otherwise the answer stands. What a player announced is kept while they stay in the room, as
  /// the content manifests are (content_registry.h), so an announcement that lapses while a client loads a contract does not
  /// count against it; one that lists no mods (a client whose mods stopped, host/presence.h) replaces it. `listRead` false says
  /// the player list could not be read, which forgets nobody and is no room with nobody else in it.
  void observe(bool privateRoom, bool contract, int local, int master, const std::vector<int>& others, unsigned heard,
               const ModsOf& modsOf, bool listRead = true) {
    if (seen_ && seen_->privateRoom == privateRoom && seen_->contract == contract && seen_->local == local &&
        seen_->master == master && seen_->heard == heard && seen_->listRead == listRead && seen_->others == others)
      return;
    seen_ = Seen{privateRoom, contract, local, master, heard, listRead, others};
    localActor_ = local;
    masterActor_ = master;
    if (listRead || !privateRoom)
      std::erase_if(heard_, [&](const auto& h) {
        return !privateRoom || std::find(others.begin(), others.end(), h.first) == others.end();
      });
    for (int actor : others)
      if (std::optional<std::vector<ModBuild>> mods = modsOf ? modsOf(actor) : std::nullopt) {
        std::sort(mods->begin(), mods->end());
        heard_[actor] = std::move(*mods);
      }
    if (privateRoom && contract) return;  // the answer holds as it stood when the contract loaded
    bool agreed = false;
    if (privateRoom) {
      const std::vector<ModBuild>* host = host_list();
      agreed = (host && *host == local_) || (listRead && others.empty());  // alone, this client is the host
    }
    if (agreed != agreed_) ++decisions_;
    agreed_ = agreed;
  }

  /// Whether this client's running mods equal the host's list, in a private room. Held through a contract.
  bool agreed() const { return agreed_; }

  /// Whether what `actor` announced equals the host's list. The host is verified by being it, and this client by its own
  /// answer. Unknown, while this client has not heard the host, for everyone else.
  bool verified(int actor) const {
    if (actor == 0) return false;
    if (actor == localActor_) return agreed_;
    const std::vector<ModBuild>* host = host_list();
    if (!host) return false;
    if (actor == masterActor_) return true;
    const auto it = heard_.find(actor);
    return it != heard_.end() && it->second == *host;
  }

  /// Which of the players in `actors`, a fresh reading of the room, are not verified; this client is never among them.
  std::vector<int> unverified(const std::vector<int>& actors) const {
    std::vector<int> out;
    for (int actor : actors)
      if (actor != localActor_ && !verified(actor)) out.push_back(actor);
    return out;
  }

  /// Whether a message on the channel of `mod`, one of this client's mods, is for that mod: this client matches the host, and
  /// `sender` is verified, so it runs that very build. A client never heard (an older suite, whose presence reads as nothing
  /// here) or a sender no longer in the room gets no message through. A client's own message, raised for itself in single
  /// player, passes. `localActor` is 0 when it cannot be read.
  bool delivers(int sender, int localActor, const ModBuild& mod) const {
    if (!agreed_ || !std::binary_search(local_.begin(), local_.end(), mod)) return false;
    if (localActor != 0 && sender == localActor) return true;
    return sender != localActor_ && verified(sender);
  }

  /// Whether `actor` is verified and the host's list has the mod `id`: a verified player runs just that list.
  bool peer_runs(int actor, std::string_view id) const {
    const std::vector<ModBuild>* host = host_list();
    return host && verified(actor) &&
           std::any_of(host->begin(), host->end(), [&](const ModBuild& m) { return m.id == id; });
  }

  /// Grows each time agreed() changes, so what was shown under the last answer can be made again.
  unsigned decisions() const { return decisions_; }

 private:
  // What observe was last given, but the lists: the same again changes nothing.
  struct Seen {
    bool privateRoom, contract;
    int local, master;
    unsigned heard;
    bool listRead;
    std::vector<int> others;
  };

  // The lobby's list: this client's on the host, what the host announced elsewhere; null while the host is unheard or unknown.
  const std::vector<ModBuild>* host_list() const {
    if (masterActor_ == 0) return nullptr;
    if (masterActor_ == localActor_) return &local_;
    const auto it = heard_.find(masterActor_);
    return it == heard_.end() ? nullptr : &it->second;
  }

  std::optional<Seen> seen_;  // none: decide on the next observe
  std::vector<ModBuild> local_;  // sorted
  std::map<int, std::vector<ModBuild>> heard_;  // what each other player in the room announced, sorted
  int localActor_ = 0, masterActor_ = 0;
  bool agreed_ = false;
  unsigned decisions_ = 0;
};

}  // namespace pm::content
