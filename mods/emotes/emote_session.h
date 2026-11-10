#pragma once

// What the Emotes mod decides each frame, free of game calls (docs/superpowers/specs/2026-11-10-emotes-design.md): on
// the emoter's client, an emote's start, its end (a move past half a metre, a listed action, death, a one-shot played
// out, the investigator gone), its song's restarts at its loop, the body's facing and what to tell the others; on every
// other client, each peer's dance as their messages say, the dead's rule, and the end of a peer who is gone. The mod's
// glue (emote.cpp) reads the frame from the game and carries the decisions out.

#include <array>
#include <cmath>
#include <map>
#include <optional>
#include <vector>

#include "emote_net.h"
#include "emote_rule.h"

namespace pm::emotes {

/// What the session knows of each emote once its clip is loaded.
struct EmoteInfo {
  float clipSeconds = 0.0f;  ///< the clip's length
  bool loops = false;        ///< the clip's Loop Time
  SongLoop song;             ///< where its song loops (song_loop_of)
};
using EmoteTable = std::array<EmoteInfo, kSegments>;

/// This client's investigator this frame, as the glue reads it.
struct LocalFrame {
  bool present = false;     ///< a local investigator in the game's player list (Playable Ghost's ghost player has none)
  bool dead = false;        ///< Player.isDead
  Point pos{};              ///< where it stands
  float viewYaw = 0.0f;     ///< the view's yaw: the head camera's on screen, the headset's in VR
  bool endAction = false;   ///< a game action that ends an emote was pressed this frame
  double clipTime = -1.0;   ///< the emote clip's own time on its body, -1 when none plays
};

/// What the glue does for this client's investigator this frame.
struct LocalOut {
  int start = -1;                 ///< play this emote: its clip, its song from the start, the view (cuts in over any)
  bool stop = false;              ///< end the emote: clip out, song off, view back
  bool restartSong = false;       ///< put the song back to its loop point
  std::optional<float> bodyYaw;   ///< the visible body's yaw now
  std::vector<EmoteMsg> send;     ///< messages to every other client, in order
};

/// The emoter's side.
class LocalSession {
 public:
  explicit LocalSession(const EmoteTable& table) : table_(table) {}

  /// The wheel played `emote`: start it, or cut it in over the one dancing.
  LocalOut pick(int emote, const LocalFrame& f) {
    LocalOut o;
    if (!f.present || emote < 0 || emote >= kSegments) return o;
    active_ = true;
    emote_ = emote;
    start_ = f.pos;
    yaw_ = sentYaw_ = f.viewYaw;
    prevClip_ = -1.0;
    wasDead_ = f.dead;
    o.start = emote;
    o.bodyYaw = yaw_;
    o.send.push_back(EmoteMsg{Kind::Start, static_cast<uint8_t>(emote), f.pos.x, f.pos.y, f.pos.z, yaw_});
    return o;
  }

  /// One frame of the emote: whether it ends, its song, its facing.
  LocalOut tick(const LocalFrame& f) {
    LocalOut o;
    if (!active_) return o;
    const EmoteInfo& info = table_[static_cast<size_t>(emote_)];
    const bool died = f.dead && !wasDead_;
    wasDead_ = f.dead;
    const bool playedOut = f.clipTime >= 0.0 && !info.loops && one_shot_done(f.clipTime, info.clipSeconds);
    if (!f.present || died || f.endAction || moved_too_far(start_, f.pos) || playedOut) {
      end(o);
      return o;
    }
    if (f.clipTime >= 0.0) {
      o.restartSong = song_jumped(prevClip_, f.clipTime, info.song);
      prevClip_ = f.clipTime;
    }
    yaw_ = body_yaw(yaw_, f.viewYaw);
    o.bodyYaw = yaw_;
    if (std::fabs(wrap_degrees(yaw_ - sentYaw_)) >= kFacingSendDegrees) {
      sentYaw_ = yaw_;
      o.send.push_back(EmoteMsg{Kind::Facing, static_cast<uint8_t>(emote_), 0, 0, 0, yaw_});
    }
    return o;
  }

  /// Ends the emote now, from outside (the mod stopping): what to undo and send.
  LocalOut end_now() {
    LocalOut o;
    if (active_) end(o);
    return o;
  }

  bool active() const { return active_; }
  int emote() const { return active_ ? emote_ : -1; }
  float yaw() const { return yaw_; }
  const EmoteInfo& info() const { return table_[static_cast<size_t>(emote_ < 0 ? 0 : emote_)]; }

 private:
  void end(LocalOut& o) {
    o.stop = true;
    o.send.push_back(EmoteMsg{Kind::End, static_cast<uint8_t>(emote_), 0, 0, 0, yaw_});
    active_ = false;
  }

  EmoteTable table_;
  bool active_ = false, wasDead_ = false;
  int emote_ = -1;
  Point start_{};
  float yaw_ = 0.0f, sentYaw_ = 0.0f;
  double prevClip_ = -1.0;
};

/// A peer's investigator this frame, as the glue reads it.
struct RemoteFrame {
  int actor = 0;            ///< their Photon actor number
  bool present = false;     ///< their investigator is in the player list
  bool dead = false;        ///< Player.isDead
  double clipTime = -1.0;   ///< their emote clip's time on their body here, -1 when none plays
};

/// What the glue does for a peer.
struct RemoteOut {
  enum What { Start, Stop, RestartSong, Yaw };
  What what = Start;
  int actor = 0;
  int emote = -1;
  float yaw = 0.0f;
};

/// Everyone else's side: one dance per peer, as their messages say.
class RemoteSessions {
 public:
  explicit RemoteSessions(const EmoteTable& table) : table_(table) {}

  /// A message from `actor` (the sender the loader gives, never the payload's word).
  std::vector<RemoteOut> receive(int actor, const EmoteMsg& m, bool senderDead, bool localDead) {
    std::vector<RemoteOut> out;
    auto it = peers_.find(actor);
    switch (m.kind) {
      case Kind::Start:
        if (!shows_to(senderDead, localDead)) return out;
        peers_[actor] = Peer{m.emote, -1.0};
        out.push_back({RemoteOut::Start, actor, m.emote, m.yaw});
        out.push_back({RemoteOut::Yaw, actor, m.emote, m.yaw});
        break;
      case Kind::Facing:
        if (it != peers_.end()) out.push_back({RemoteOut::Yaw, actor, it->second.emote, m.yaw});
        break;
      case Kind::End:
        if (it != peers_.end()) {
          out.push_back({RemoteOut::Stop, actor, it->second.emote, 0.0f});
          peers_.erase(it);
        }
        break;
    }
    return out;
  }

  /// One frame: each dancing peer's song restarts, and the end of a peer gone, dead to the living, or played out.
  std::vector<RemoteOut> tick(const std::vector<RemoteFrame>& frames, bool localDead) {
    std::vector<RemoteOut> out;
    for (auto it = peers_.begin(); it != peers_.end();) {
      const RemoteFrame* f = nullptr;
      for (const RemoteFrame& r : frames)
        if (r.actor == it->first) f = &r;
      const EmoteInfo& info = table_[it->second.emote];
      const bool playedOut = f && f->clipTime >= 0.0 && !info.loops && one_shot_done(f->clipTime, info.clipSeconds);
      if (!f || !f->present || !shows_to(f->dead, localDead) || playedOut) {
        out.push_back({RemoteOut::Stop, it->first, it->second.emote, 0.0f});
        it = peers_.erase(it);
        continue;
      }
      if (f->clipTime >= 0.0) {
        if (song_jumped(it->second.prevClip, f->clipTime, info.song))
          out.push_back({RemoteOut::RestartSong, it->first, it->second.emote, 0.0f});
        it->second.prevClip = f->clipTime;
      }
      ++it;
    }
    return out;
  }

  bool dancing(int actor) const { return peers_.count(actor) != 0; }
  /// The dancing peers' actors and emotes.
  std::vector<std::pair<int, int>> dancers() const {
    std::vector<std::pair<int, int>> out;
    for (const auto& [actor, p] : peers_) out.emplace_back(actor, p.emote);
    return out;
  }
  void clear() { peers_.clear(); }

 private:
  struct Peer {
    int emote;
    double prevClip;
  };
  EmoteTable table_;
  std::map<int, Peer> peers_;
};

}  // namespace pm::emotes
