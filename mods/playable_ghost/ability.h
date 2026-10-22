#pragma once

// The ghost types with an ability of their own: the ones GhostAI's ability entry (state 15, build
// 25811160) starts a state for. Every other type's ability roll does nothing. What each one does is
// read from that state's code. Kept free of game calls so it can be tested offline.
namespace pg::ability {

struct Ability {
  const char* name;
  const char* description;
};

inline const Ability* of(int ghostType) {
  static constexpr Ability kWraith{"Teleport", "Appear beside a random investigator, leaving EMF where you arrive."};
  static constexpr Ability kPhantom{"Stalk", "Walk to a spot close to a random investigator."};
  static constexpr Ability kPoltergeist{"Burst", "Throw every object around you at once, leaving EMF."};
  static constexpr Ability kJinn{"Sanity drain",
                                 "Drain the sanity of the investigators near you. It only works while the power is on."};
  static constexpr Ability kDemon{"Early hunt", "Start a hunt whatever the investigators' sanity."};
  static constexpr Ability kYurei{"Door slam",
                                  "Slam a door of the room you are in. Investigators close to you on your floor lose sanity."};
  static constexpr Ability kObake{"Fading prints", "Make the fingerprints you have left fade twice as fast."};
  switch (ghostType) {
    case 1: return &kWraith;
    case 2: return &kPhantom;
    case 3: return &kPoltergeist;
    case 5: return &kJinn;
    case 9: return &kDemon;
    case 10: return &kYurei;
    case 19: return &kObake;
    default: return nullptr;
  }
}

// The Jinn's ability state leaves at once while the power is off.
inline bool needs_power(int ghostType) { return ghostType == 5; }

}  // namespace pg::ability
