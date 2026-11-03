#pragma once

#include <cstddef>
#include <string>

#include "consent.h"
#include "text_table.h"

// The start of a mod session once the player has answered the consent notice. Only "I agree" starts one: until then the host
// has only its frame hook, which the notice needs (host/main.cpp), and "Play without mods" takes that off too.
// The host first makes the hooks the lobby's rules cannot do without (the suite's messages). Without them the rules could
// not hold, so no mod starts. Then its other hooks that serve the mods: the game methods it tracks for pm::api::inside, its
// watch on Photon's room cleanup (host/room_watch.h) and custom content's. They come before the mark the mods' hooks are
// counted from, so a restart of the mods keeps them and they sit next to the game's bodies in each chain.
// Then the mods start. One of these others the host cannot make is logged and leaves only that service off; it never stops
// the host, and only a mod that requires a method it could not track stays off (host/mods.h). Any other answer
// makes nothing, so a declined session keeps the game as it is. A declined client has no presence to send, so it reads as
// a vanilla player to a modded host, and the host's join guard removes it like one.
// Main thread.
namespace pm::mod_session {

struct Steps {
  bool (*install)();  // the host's hooks for the mods, made and switched on; false when one could not be
  size_t (*mark)();   // where the mods' own hooks begin (hooks::mark)
  void (*start)();    // starts the chosen mods
  bool (*required)() = nullptr;  // the hooks the lobby's rules need, first: false starts nothing. None when null
};

// Says why no mod runs when this build lacks what the lobby's rules need, for the Mods panel (host/mods_panel.h tick_off).
inline std::string lacks_the_rules() { return i18n::text("off.lacks-rules"); }

// Runs the steps for `answer`: true when it started the session, with the mark in `modHooks`.
bool begin(consent::Answer answer, const Steps& steps, size_t& modHooks);

// Whether the notice is asked at all, once the mods are found: Declined, with a log line, when none is (nothing to agree
// to) or when this build lacks what the lobby's rules need. Clients tell each other their mods through the suite's
// messages (`messages`, hostnet::missing), and the join guard acts on a mismatch through the lobby's own steps (`lobby`,
// room_actions::missing); without either no mod starts, the line names the keys that did not resolve, and `cause` says so
// for the Mods panel (host/mods_panel.h tick_off). Pending otherwise: the notice is asked.
consent::Answer opening(bool modsFound, const std::string& messages, const std::string& lobby, std::string& cause);

}  // namespace pm::mod_session
