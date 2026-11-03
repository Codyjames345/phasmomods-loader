#pragma once

#include <cstdint>
#include <string>

namespace pm::content {
class Registry;
}

// What the host keeps from going out of a lobby: a contract's start while a player is unverified, and a custom value that could
// reach such a player (sdk/systems/photon.md). The host's own mods stay on all the while, so this is the only thing
// standing between them and a player who may not have them. Main thread. Nothing here waits or blocks: a refused call simply
// does not happen, and the player goes on.
//
// A contract's start is refused twice over. At the press (Button.Press on ServerManager.startGameButton), before any of the game's
// start runs, so its state does not change and the host can press again. And at the send (PhotonView.RPC of
// ServerManager.LoadScene), since the start can wait on a task while someone joins, and since the press is not the only way a
// future build might start one. A refused send has run the game's start up to it: see photon.md for what that touches.
//
// An allowed send is held, not sent, while the room is closed for it (host/start_close.h): the host keeps it and lets it go
// later with release_held, exactly as the game sent it, or drops it. While a start is underway a press and another send start
// nothing.
//
// The game's own ready gate is left alone: it has no input for this, and no way to cover a click in the frame a join arrives.
//
// An RPC is judged by its name and arguments, never by who called it. A journal's page sync leaves from GoToPage(int), and from
// NextPage and PreviousPage, which each hold their own copy of that code, so nothing but the RPC itself sees all three.
//
// Any failure in a check of the start refuses it. Any failure telling what an RPC is lets it go as the game sent it: stopping the
// game's RPCs would break far more than a custom number could.
namespace pm::lobby_gate {

struct Config {
  bool (*isStartButton)(void* button) = nullptr;
  // Whether a contract may start now. False tells the host why, through the game's popup.
  bool (*startAllowed)() = nullptr;
  // Whether everyone in the room now is verified. False while it cannot be read.
  bool (*roomVerified)() = nullptr;
  // Whether a page number sent on this view means another page than in the book as the game made it (host/content_journal.h).
  bool (*customPage)(void* view, int32_t page) = nullptr;
  // The int a boxed value holds; false when it is no int.
  bool (*boxedInt)(const void* boxed, int32_t& out) = nullptr;
  // Where custom numbers begin, for a journal's marks. Null: nothing is custom.
  const content::Registry* registry = nullptr;
  // Whether a start is underway: its send is held or being let go (host/start_close.h). While true no press goes on, and a send
  // of the start is not checked again, but answered by startSend alone. Null: never.
  bool (*startPending)() = nullptr;
  // What becomes of the start's send once allowed: Held while the room is closed for it, AsSent for the held send let go, Refused
  // for another while one is underway. Null: it goes as sent. A throw refuses it.
  int (*startSend)() = nullptr;  // a Send value
  // Whether this is a room the lobby's rules hold in: a private lobby, or single player. Elsewhere the loader behaves as if it were
  // not installed, so a start goes as the game sent it and the room stays as it is. Null, or a throw: it is taken for one.
  bool (*privateRoom)() = nullptr;
};

// What this build lacks for it, for the start gate's log: the keys, ", "-separated. Without them the lobby's rules could not
// hold, so no mod starts (host/room_actions.h).
std::string missing();

// On "I agree", before hooks::enable_all. False when a hook could not be made.
bool create_hooks(const Config& config);

// What each hook does, on its own.

// Whether a click on `button` goes on.
bool press_goes_on(void* button);

enum class Send {
  AsSent,      // as the game sent it
  Refused,     // nothing at all
  NotBuffered,  // to everyone, and the server keeps nothing for a later joiner
  HereAlone,   // on this client, and nothing goes out
  Held,        // nothing goes out now: the call is kept, to be let go by release_held or dropped
};
// What happens to an RPC the game is sending on `view`.
Send decide_rpc(void* view, void* methodName, void* parameters);

// Whether a send is held now.
bool holding();
// Sends the held call, as the game made it, down the hook's chain below this hook; true when there was one. Main thread, from
// the host's tick: never from inside a detour.
bool release_held();
// Forgets the held call: nothing is sent.
void drop_held();

}  // namespace pm::lobby_gate
