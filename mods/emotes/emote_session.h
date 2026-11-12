#pragma once

// What the Emotes mod decides each frame, free of game calls (docs/superpowers/specs/2026-11-10-emotes-design.md): on
// the emoter's client, an emote's start, its end (a move past half a metre, a listed action, death, a one-shot played
// out, the investigator gone), the body's facing and what to tell the others; on every
// other client, each peer's dance as their messages say, the dead's rule, and the end of a peer who is gone. The mod's
// glue (emote.cpp) reads the frame from the game and carries the decisions out.

#include <cmath>
#include <map>
#include <optional>
#include <vector>

#include "emote_net.h"
#include "emote_rule.h"

namespace pm::emotes {

/// What the session knows of each emote once its clip is loaded, in the wheel's order (catalogue.h's wheel_order).
struct EmoteInfo {
  float clipSeconds = 0.0f;  ///< the clip's length
  bool loops = false;        ///< the clip's Loop Time
  SongLoop song;             ///< where its song loops (song_loop_of)
  uint32_t key = 0;          ///< the emote's catalogue key (catalogue.h's emote_key), what its messages name it by
  bool hasMove = false;      ///< the emote has a walking variant its pack's file names (moveClip)
};
/// The wheel's emotes: any number, shown kSegments to a page (wheel_pages.h). An emote index is an int into this.
using EmoteTable = std::vector<EmoteInfo>;

/// This client's investigator this frame, as the glue reads it.
struct LocalFrame {
  bool present = false;     ///< a local investigator in the game's player list (Playable Ghost's ghost player has none)
  bool dead = false;        ///< Player.isDead: the death is over and the body lies
  bool dying = false;       ///< the death has begun (Player.NotifyOfDeathStart) and is not marked dead yet: the game's
                            ///< death animation has the body, so nothing of ours may be over it (the user, 2026-11-10)
  Point pos{};              ///< where it stands
  float viewYaw = 0.0f;     ///< the view's yaw: the head camera's on screen, the headset's in VR
  bool endAction = false;   ///< a game action that ends an emote was pressed this frame
  double clipTime = -1.0;   ///< the emote clip's own time on its body, -1 when none plays
  float dt = 0.0f;          ///< seconds since the last frame; 0 turns the body at once
};

/// What the glue does for this client's investigator this frame.
struct LocalOut {
  int start = -1;                 ///< play this emote: its clip, its song from the start, the view (cuts in over any)
  bool stop = false;              ///< end the emote: clip out, song off, view back
  bool atOnce = false;            ///< this stop leaves nothing blending: the game's own animation has the body (death)
  std::optional<float> bodyYaw;   ///< the visible body's yaw now
  std::vector<EmoteMsg> send;     ///< messages to every other client, in order
};

/// The emoter's side.
class LocalSession {
 public:
  explicit LocalSession(const EmoteTable& table) : table_(table) {}

  /// The wheel played `emote`: start it, or cut it in over the one dancing. Nothing while the game's death animation
  /// has the body (the user, 2026-11-10): an investigator beginning to die does not dance.
  LocalOut pick(int emote, const LocalFrame& f) {
    LocalOut o;
    if (!f.present || f.dying || emote < 0 || emote >= static_cast<int>(table_.size())) return o;
    active_ = true;
    emote_ = emote;
    start_ = lastPos_ = f.pos;
    yaw_ = sentYaw_ = f.viewYaw;
    wasDown_ = f.dead || f.dying;
    o.start = emote;
    o.bodyYaw = yaw_;
    o.send.push_back(
        EmoteMsg{Kind::Start, table_[static_cast<size_t>(emote)].key, false, f.pos.x, f.pos.y, f.pos.z, yaw_});
    return o;
  }

  /// One frame of the emote: whether it ends, and its facing.
  LocalOut tick(const LocalFrame& f) {
    LocalOut o;
    if (!active_) return o;
    const EmoteInfo& info = table_[static_cast<size_t>(emote_)];
    // The death, or its beginning: the game's animation has the body from the first, so the emote is over then, not
    // when the mark comes a second or two later.
    const bool down = f.dead || f.dying;
    const bool died = down && !wasDown_;
    wasDown_ = down;
    const bool playedOut = f.clipTime >= 0.0 && !info.loops && one_shot_done(f.clipTime, info.clipSeconds);
    if (!f.present || died || f.endAction || moved_too_far(start_, f.pos) || playedOut) {
      end(o, died);  // a death takes the body at once: nothing of ours blends over the game's animation
      return o;
    }
    // Walking within the half metre turns the body to the way it goes (the user's ruling), so it does not drift
    // sideways or backwards; otherwise it keeps the view within its arc. Either way it turns at a rate.
    const float dx = f.pos.x - lastPos_.x, dz = f.pos.z - lastPos_.z;
    lastPos_ = f.pos;
    const bool walking = f.dt > 0.0f && std::hypot(dx, dz) / f.dt >= kWalkingSpeed;
    const float target = walking ? std::atan2(dx, dz) * 57.29577951f : body_yaw(yaw_, f.viewYaw);
    yaw_ = f.dt > 0.0f ? turn_toward(yaw_, target, kTurnDegreesPerSecond * f.dt) : target;
    o.bodyYaw = yaw_;
    if (std::fabs(wrap_degrees(yaw_ - sentYaw_)) >= kFacingSendDegrees) {
      sentYaw_ = yaw_;
      o.send.push_back(EmoteMsg{Kind::Facing, table_[static_cast<size_t>(emote_)].key, false, 0, 0, 0, yaw_});
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
  void end(LocalOut& o, bool atOnce = false) {
    o.stop = true;
    o.atOnce = atOnce;
    o.send.push_back(EmoteMsg{Kind::End, table_[static_cast<size_t>(emote_)].key, false, 0, 0, 0, yaw_});
    active_ = false;
  }

  EmoteTable table_;
  bool active_ = false, wasDown_ = false;
  int emote_ = -1;
  Point start_{}, lastPos_{};
  float yaw_ = 0.0f, sentYaw_ = 0.0f;
};

/// A peer's investigator this frame, as the glue reads it.
struct RemoteFrame {
  int actor = 0;            ///< their Photon actor number
  bool present = false;     ///< their investigator is in the player list
  bool dead = false;        ///< Player.isDead: their death is over
  bool dying = false;       ///< their death has begun (Player.NotifyOfDeathStart) and is not over: their body is the
                            ///< game's, so their dance comes off at once (2026-11-10)
  double clipTime = -1.0;   ///< their emote clip's time on their body here, -1 when none plays
};

/// What the glue does for a peer.
struct RemoteOut {
  enum What { Start, Stop, Yaw };
  What what = Start;
  int actor = 0;
  int emote = -1;
  float yaw = 0.0f;
  bool atOnce = false;  ///< this Stop leaves nothing blending: the game's own animation has the body (their death)
};

/// Everyone else's side: one dance per peer, as their messages say.
class RemoteSessions {
 public:
  explicit RemoteSessions(const EmoteTable& table) : table_(table) {}

  /// A message from `actor` (the sender the loader gives, never the payload's word), with whether that player and this
  /// client's investigator are down (dead, or dying with the game's death animation on the body).
  std::vector<RemoteOut> receive(int actor, const EmoteMsg& m, bool senderDown, bool receiverDown) {
    std::vector<RemoteOut> out;
    auto it = peers_.find(actor);
    switch (m.kind) {
      case Kind::Start: {
        if (!shows_to(senderDown, receiverDown)) return out;
        // A Start names its emote by key; this client's table gives the index. A key it does not hold (the packs
        // differ, or the wheel changed) is passed over: no output and no state change.
        const int index = index_of_key(m.key);
        if (index < 0) return out;
        peers_[actor] = Peer{index};
        out.push_back({RemoteOut::Start, actor, index, m.yaw});
        out.push_back({RemoteOut::Yaw, actor, index, m.yaw});
        break;
      }
      case Kind::Facing:
        if (it != peers_.end()) out.push_back({RemoteOut::Yaw, actor, it->second.emote, m.yaw});
        break;
      case Kind::End:
        if (it != peers_.end()) {
          out.push_back({RemoteOut::Stop, actor, it->second.emote, 0.0f});
          peers_.erase(it);
        }
        break;
      case Kind::Move:
        // Walking during an emote: nothing acts on it yet (a later task adds it).
        break;
    }
    return out;
  }

  /// One frame: the end of a peer gone, down (dead to the living, or dying), or played out. A peer whose death has
  /// begun comes off at once, so their body is the game's death animation and not the dance.
  std::vector<RemoteOut> tick(const std::vector<RemoteFrame>& frames, bool receiverDown) {
    std::vector<RemoteOut> out;
    for (auto it = peers_.begin(); it != peers_.end();) {
      const RemoteFrame* f = nullptr;
      for (const RemoteFrame& r : frames)
        if (r.actor == it->first) f = &r;
      const EmoteInfo& info = table_[it->second.emote];
      const bool playedOut = f && f->clipTime >= 0.0 && !info.loops && one_shot_done(f->clipTime, info.clipSeconds);
      const bool down = f && (f->dead || f->dying);
      if (!f || !f->present || !shows_to(down, receiverDown) || playedOut) {
        RemoteOut stop{RemoteOut::Stop, it->first, it->second.emote, 0.0f};
        stop.atOnce = down;
        out.push_back(stop);
        it = peers_.erase(it);
        continue;
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
  /// The index of the emote with this catalogue key, or -1 when the table does not hold it.
  int index_of_key(uint32_t key) const {
    for (size_t i = 0; i < table_.size(); ++i)
      if (table_[i].key == key) return static_cast<int>(i);
    return -1;
  }

  struct Peer {
    int emote;  ///< the index into table_, found by key when the Start arrived
  };
  EmoteTable table_;
  std::map<int, Peer> peers_;
};

}  // namespace pm::emotes
