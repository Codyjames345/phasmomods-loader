#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>

// A ghost type's own state, as its player feels it: the host reads it from the game and sends two
// bytes (Status::trait); this says what they mean. The ghost player learns only about the ghost
// itself, never about the investigators or their equipment. Pure text, testable offline.
namespace pg::typestate {

constexpr int kRaiju = 18, kThaye = 23, kGallu = 25, kDayan = 26, kObambo = 27, kDeildegast = 30;
constexpr int kEcho = 31, kDullahan = 32, kKikimora = 33, kDraugr = 34;  // the Custom Ghosts mod's

/** Whose state a ghost shows: a custom ghost's own type, otherwise the type whose rules it follows. */
inline int subject(int behaviourType, int identity) { return identity >= kEcho ? identity : behaviourType; }

/** The Deildegast's speed for how many things it has moved (the game's rule: 3 m/s, 0.1 less per thing). */
inline float deildegast_speed(int moved) { return std::max(0.4f, 3.0f - 0.1f * static_cast<float>(moved)); }

/** The HUD line for a ghost's state; empty when its type has none to show. */
inline std::string describe(int subjectType, const uint8_t trait[2]) {
  char buf[160];
  switch (subjectType) {
    case kGallu: {
      static const char* states[] = {"calm", "<color=#E04030>enraged</color>", "exhausted"};
      return std::string("Rage: ") + states[std::min<int>(trait[0], 2)];
    }
    case kDayan: {
      static const char* states[] = {"steady", "<color=#E04030>empowered</color> (they move near you)",
                                     "weakened (they stand still near you)"};
      return std::string("Strength: ") + states[std::min<int>(trait[0], 2)];
    }
    case kObambo:
      std::snprintf(buf, sizeof buf, "Mood: %s for %d:%02d", trait[0] ? "<color=#E04030>aggressive</color>" : "calm",
                    trait[1] / 60, trait[1] % 60);
      return buf;
    case kThaye:
      std::snprintf(buf, sizeof buf, "Age: %d of 10 (hunts at %d%% sanity)", trait[0], trait[1]);
      return buf;
    case kDeildegast:
      std::snprintf(buf, sizeof buf, "Speed: %.1f m/s (%d things moved)", deildegast_speed(trait[0]), trait[0]);
      return buf;
    case kEcho:
      return trait[0] ? "<alpha=#77>Silence: no voice near you; you are dormant and cannot act" : "Voices: you hear the living near you";
    case kDullahan:
      std::snprintf(buf, sizeof buf, "Charge: %d%% speed", trait[0]);
      return trait[0] ? std::string(buf) : std::string();
    case kRaiju:
      return trait[0] ? "<color=#FFE070>Electronics hum near you</color>: you are faster" : std::string();
    case kKikimora:
      return trait[0] ? "<alpha=#77>The lights are on here: you cannot touch anything" : std::string();
    case kDraugr:
      return trait[0] ? "<color=#E04030>Something has left your room</color>: you may hunt from 65% sanity"
                      : "Your room is whole";
    default:
      return {};
  }
}

}  // namespace pg::typestate
