#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "mod_ids.h"

// Which modded peers in the room run which mods, and which builds of them. Every host announces the
// mods it started on the host channel every 2 s and whenever somebody joins; a peer not heard from
// for 6 s, or no longer in the room, counts as running nothing. A host whose mods stop while it is in
// a room announces once that it runs none (no mods), so the other players stop counting what it
// announced before. Pure logic so it can be tested offline.
//
// Wire format, version 3: an announcement is one or more messages of at most kMaxMessage bytes, each
// 'H' 'M' version serial part parts count, then count times
//   (id length:u8, id, build:8 bytes, name length:u8, name, version length:u8, version).
// `serial` names the announcement, `part` is 0 to parts - 1, and every mod is in exactly one part. A
// receiver replaces its list for that peer only once every part of one announcement has arrived; until
// then it keeps the last whole list. The name and version are what players read; matching is by id and
// build. Version 2 (v0.1.0) sent ids and builds in one message, cut off at 511 bytes; a version 2 peer
// and this one never hear each other, so each counts the other as running no mod.
namespace pm::presence {

constexpr uint8_t kVersion = 3;
constexpr uint64_t kAnnounceMs = 2000;
constexpr uint64_t kTimeoutMs = 6000;
constexpr size_t kBuildBytes = 8;
constexpr size_t kHeaderBytes = 7;
constexpr size_t kMaxMessage = 511;  // PM_MAX_MESSAGE, the most the host channel carries
constexpr size_t kMaxMods = 256;     // far past any install: a client that announces more is ignored whole

// A started mod: its id, its build (the first bytes of its DLL's SHA-256, as 16 hex digits), and its
// name and version for the player ("" for an old-format mod's version, which is unknown; a name is never
// empty from a manifest, and a receiver shows the id for one that is).
struct Mod {
  std::string id;
  std::string build;
  std::string name;
  std::string version;
  bool operator==(const Mod&) const = default;
};

class Presence {
 public:
  /** The mods this client has running now: empty until its player agreed to the notice, and without those it suspended. */
  void set_local(std::vector<Mod> mods) {
    // Mods stopping in a room: the players there hear it once (take_announcement).
    farewell_ = localActor_ != 0 && mods.empty() && (!local_.empty() || farewell_);
    local_ = std::move(mods);
    next_ = 0;
    ++revision_;
  }
  const std::vector<Mod>& local() const { return local_; }

  /** Every frame with the room's actors (empty outside a room). */
  void observe(int localActor, const std::vector<int>& actors, uint64_t now) {
    if (localActor != localActor_) ++revision_;
    if (localActor == 0) farewell_ = false;  // out of the room: nobody left to tell
    localActor_ = localActor;
    now_ = now;
    for (auto it = peers_.begin(); it != peers_.end();) {
      bool inRoom = false;
      for (int a : actors) inRoom |= a == it->first;
      if (!inRoom || now - it->second.lastSeen > kTimeoutMs) {
        if (it->second.complete) ++revision_;
        it = peers_.erase(it);
      } else {
        ++it;
      }
    }
    if (actors != actors_) next_ = 0;  // somebody joined or left: tell them at once
    actors_ = actors;
  }

  /**
   * One message from `sender`. One that is not version 3, is the local actor's, or does not read
   * whole (a length past its end, an id no mod can have, a build cut short, a name or version the game
   * could not draw) is ignored entirely, and so it says nothing about the sender.
   */
  void receive(int sender, const uint8_t* data, size_t size, uint64_t now) {
    if (!data || size < kHeaderBytes || size > kMaxMessage || data[0] != 'H' || data[1] != 'M' || data[2] != kVersion ||
        sender == localActor_)
      return;
    const uint8_t serial = data[3], part = data[4], parts = data[5], count = data[6];
    if (parts == 0 || part >= parts) return;
    std::vector<Mod> mods;
    size_t at = kHeaderBytes;
    const auto text = [&](std::string& out) {
      if (at >= size || at + 1 + data[at] > size) return false;
      out.assign(reinterpret_cast<const char*>(data + at + 1), data[at]);
      at += 1 + data[at];
      return true;
    };
    for (int i = 0; i < count; ++i) {
      Mod m;
      if (!text(m.id) || !modid::valid_any_id(m.id) || at + kBuildBytes > size) return;
      m.build = to_hex(data + at);
      at += kBuildBytes;
      if (!text(m.name) || !text(m.version)) return;
      if ((!m.name.empty() && !modid::display_safe(m.name, modid::kMaxNameBytes)) ||
          (!m.version.empty() && !modid::parse_version(m.version)))
        return;
      mods.push_back(std::move(m));
    }
    if (at != size) return;
    Peer& peer = peers_[sender];
    peer.lastSeen = now;
    if (peer.serial != serial || peer.got.size() != parts) {  // another announcement: its parts start over
      peer.serial = serial;
      peer.parts.assign(parts, {});
      peer.got.assign(parts, false);
    }
    peer.parts[part] = std::move(mods);
    peer.got[part] = true;
    for (bool got : peer.got)
      if (!got) return;
    std::vector<Mod> whole;
    for (std::vector<Mod>& p : peer.parts) whole.insert(whole.end(), p.begin(), p.end());
    peer.got.assign(parts, false);  // the next copy of this announcement is gathered afresh
    if (whole.size() > kMaxMods) return;  // says nothing about the sender, as a message that does not read whole
    if (!peer.complete || peer.mods != whole) ++revision_;
    peer.mods = std::move(whole);
    peer.complete = true;
  }

  /**
   * Grows whenever what mods_of answers may have changed: another player heard with another list or
   * for the first time, or forgotten; this client's own mods or actor changed. A reader that compares
   * it reads the lists again only then.
   */
  unsigned revision() const { return revision_; }

  bool has(int actor, std::string_view id) const {
    const std::vector<Mod>* mods = mods_of(actor);
    if (!mods) return false;
    for (const Mod& m : *mods)
      if (m.id == id) return true;
    return false;
  }

  /** The mods `actor` announced in full (the local actor's own), or null when no whole list has been heard. */
  const std::vector<Mod>* mods_of(int actor) const {
    if (actor != 0 && actor == localActor_) return &local_;
    auto it = peers_.find(actor);
    return it == peers_.end() || !it->second.complete ? nullptr : &it->second.mods;
  }

  /**
   * The announcement due now: every mod started, in as many messages as it needs, each at most
   * kMaxMessage bytes; or nothing. With no mods started it is due only once after mods stopped in the
   * room, and lists none. A mod whose fields could not be sent whole (longer than a length byte) is
   * left out, never cut.
   */
  std::vector<std::vector<uint8_t>> take_announcement() {
    if (localActor_ == 0 || (local_.empty() && !farewell_) || now_ < next_) return {};
    farewell_ = false;
    next_ = now_ + kAnnounceMs;
    ++serial_;
    std::vector<std::vector<uint8_t>> out{header()};
    for (const Mod& m : local_) {
      if (m.id.size() > 255 || m.name.size() > 255 || m.version.size() > 255) continue;
      const size_t bytes = 3 + m.id.size() + kBuildBytes + m.name.size() + m.version.size();
      if (out.back().size() + bytes > kMaxMessage || out.back()[6] == 255) out.push_back(header());
      std::vector<uint8_t>& message = out.back();
      put(message, m.id);
      for (size_t i = 0; i < kBuildBytes; ++i) message.push_back(from_hex(m.build, i));
      put(message, m.name);
      put(message, m.version);
      ++message[6];
    }
    if (out.size() > 255) return {};  // cannot be numbered: 255 parts hold 255 * 2 mods of the longest fields
    for (size_t i = 0; i < out.size(); ++i) {
      out[i][4] = static_cast<uint8_t>(i);
      out[i][5] = static_cast<uint8_t>(out.size());
    }
    return out;
  }

 private:
  struct Peer {
    std::vector<Mod> mods;  // the last whole list
    bool complete = false;  // a whole list has been heard
    uint8_t serial = 0;
    std::vector<std::vector<Mod>> parts;  // the announcement being gathered
    std::vector<bool> got;
    uint64_t lastSeen = 0;
  };

  std::vector<uint8_t> header() const { return {'H', 'M', kVersion, serial_, 0, 0, 0}; }
  static void put(std::vector<uint8_t>& out, const std::string& s) {
    out.push_back(static_cast<uint8_t>(s.size()));
    out.insert(out.end(), s.begin(), s.end());
  }
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
  std::map<int, Peer> peers_;
  std::vector<int> actors_;
  int localActor_ = 0;
  uint64_t now_ = 0, next_ = 0;
  uint8_t serial_ = 0;
  bool farewell_ = false;  // mods stopped in this room: an announcement of none is owed
  unsigned revision_ = 0;
};

}  // namespace pm::presence
