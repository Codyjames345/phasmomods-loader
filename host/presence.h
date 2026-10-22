#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

// Which modded peers in the room run which mods, and which builds of them. Every host announces the
// mods it started on the host channel every 2 s and whenever somebody joins; a peer not heard from
// for 6 s, or no longer in the room, counts as running nothing. Pure logic so it can be tested
// offline.
//
// Wire format: 'H' 'M' version count, then count times (length:u8, id bytes, build:8 bytes).
namespace pg::presence {

constexpr uint8_t kVersion = 2;
constexpr uint64_t kAnnounceMs = 2000;
constexpr uint64_t kTimeoutMs = 6000;
constexpr size_t kBuildBytes = 8;

// A started mod: its id and its build (the first bytes of its DLL's SHA-256, as 16 hex digits).
struct Mod {
  std::string id;
  std::string build;
  bool operator==(const Mod&) const = default;
};

class Presence {
 public:
  /**
   * The mods this client started (empty until its player agreed to the notice), and whether every one
   * of them lets players without the mod share the lobby (PG_MOD_VANILLA_PEERS).
   */
  void set_local(std::vector<Mod> mods, bool vanillaPeers) {
    local_ = std::move(mods);
    localVanilla_ = vanillaPeers;
    next_ = 0;
  }
  const std::vector<Mod>& local() const { return local_; }
  bool local_vanilla_peers() const { return localVanilla_; }

  /** Every frame with the room's actors (empty outside a room). */
  void observe(int localActor, const std::vector<int>& actors, uint64_t now) {
    localActor_ = localActor;
    now_ = now;
    for (auto it = peers_.begin(); it != peers_.end();) {
      bool inRoom = false;
      for (int a : actors) inRoom |= a == it->first;
      if (!inRoom || now - it->second.lastSeen > kTimeoutMs) it = peers_.erase(it);
      else ++it;
    }
    if (actors != actors_) next_ = 0;  // somebody joined or left: tell them at once
    actors_ = actors;
  }

  void receive(int sender, const uint8_t* data, size_t size, uint64_t now) {
    if (size < 4 || data[0] != 'H' || data[1] != 'M' || data[2] != kVersion || sender == localActor_) return;
    Peer peer;
    size_t at = 4;
    for (int i = 0; i < data[3]; ++i) {
      if (at >= size || at + 1 + data[at] + kBuildBytes > size) return;  // malformed: ignore the whole message
      Mod m;
      m.id.assign(reinterpret_cast<const char*>(data + at + 1), data[at]);
      at += 1 + data[at];
      m.build = to_hex(data + at);
      at += kBuildBytes;
      peer.mods.push_back(std::move(m));
    }
    peer.lastSeen = now;
    peers_[sender] = std::move(peer);
  }

  bool has(int actor, std::string_view id) const {
    const std::vector<Mod>* mods = mods_of(actor);
    if (!mods) return false;
    for (const Mod& m : *mods)
      if (m.id == id) return true;
    return false;
  }

  /** The mods `actor` announced (the local actor's own), or null when it has not been heard from. */
  const std::vector<Mod>* mods_of(int actor) const {
    if (actor != 0 && actor == localActor_) return &local_;
    auto it = peers_.find(actor);
    return it == peers_.end() ? nullptr : &it->second.mods;
  }

  /** The announcement due now, or an empty vector. Never more than 511 bytes. */
  std::vector<uint8_t> take_announcement() {
    if (localActor_ == 0 || local_.empty() || now_ < next_) return {};
    next_ = now_ + kAnnounceMs;
    std::vector<uint8_t> out = {'H', 'M', kVersion, 0};
    for (const Mod& m : local_) {
      if (m.id.size() > 255 || out.size() + 1 + m.id.size() + kBuildBytes > 511 || out[3] == 255) break;
      out.push_back(static_cast<uint8_t>(m.id.size()));
      out.insert(out.end(), m.id.begin(), m.id.end());
      for (size_t i = 0; i < kBuildBytes; ++i) out.push_back(from_hex(m.build, i));
      ++out[3];
    }
    return out;
  }

 private:
  struct Peer {
    std::vector<Mod> mods;
    uint64_t lastSeen = 0;
  };

  static std::string to_hex(const uint8_t* bytes) {
    static const char kDigits[] = "0123456789abcdef";
    std::string hex;
    for (size_t i = 0; i < kBuildBytes; ++i) {
      hex += kDigits[bytes[i] >> 4];
      hex += kDigits[bytes[i] & 15];
    }
    return hex;
  }
  static uint8_t nibble(char c) {
    if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
    if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(c - 'A' + 10);
    return 0;
  }
  // Byte i of a build written as hex; a short or malformed build reads as zeros.
  static uint8_t from_hex(const std::string& hex, size_t i) {
    if (hex.size() < 2 * i + 2) return 0;
    return static_cast<uint8_t>(nibble(hex[2 * i]) << 4 | nibble(hex[2 * i + 1]));
  }

  std::vector<Mod> local_;
  bool localVanilla_ = false;
  std::map<int, Peer> peers_;
  std::vector<int> actors_;
  int localActor_ = 0;
  uint64_t now_ = 0, next_ = 0;
};

}  // namespace pg::presence
