#include "room.h"

#include <windows.h>

#include <vector>

#include "alerts.h"
#include "body.h"
#include "control.h"
#include "equipment.h"
#include "form.h"
#include "game.h"
#include "hide.h"
#include "lobby.h"
#include "log.h"
#include "net.h"
#include "notice.h"
#include "outcome.h"
#include "leave.h"
#include "round.h"
#include "senses.h"

namespace pg::room {
namespace {

session::Session g_session;
std::vector<int> g_actors;
int g_loggedGhost = -1;

bool g_private = false;  // the mod only acts in private lobbies

void on_message(int sender, const uint8_t* data, size_t size) {
  if (g_private) g_session.receive(sender, data, size, GetTickCount64());
}

net::To to_net(session::To t) {
  switch (t) {
    case session::To::Master: return net::To::Master;
    case session::To::Actor: return net::To::Actor;
    default: return net::To::Others;
  }
}

}  // namespace

void init() { net::set_handler(&on_message); }

bool ghost_controlled() {
  void* level = game::level_controller();
  return g_private && g_session.ghost_actor() > 0 && level && game::alive(level);
}

int ghost_actor() { return g_private ? g_session.ghost_actor() : 0; }

void tick() {
  const uint64_t now = GetTickCount64();
  g_private = game::private_room();
  if (g_private) {
    game::room_actors(g_actors);
    g_session.observe(game::local_actor(), game::master_actor(), g_actors, now);
  } else {
    // Public room (or none): behave as if the mod were not installed. Observing an empty room
    // clears any ghost role, so control returns to the native AI and the lobby controls go.
    g_actors.clear();
    g_session.observe(0, 0, g_actors, now);
    g_session.take_outgoing();
  }
  lobby::tick(g_session);
  if (g_session.ghost_actor() != g_loggedGhost) {
    g_loggedGhost = g_session.ghost_actor();
    PG_LOG("room: ghost player is actor %d (local actor %d, master %d)", g_loggedGhost, game::local_actor(),
           game::master_actor());
  }
  equipment::tick(g_session);
  round::tick(g_session);
  body::tick(g_session);
  control::tick(g_session);
  senses::tick(g_session, g_session.ghost_actor() > 0 ? game::ghost_ai() : nullptr);
  hide::tick(ghost_controlled() && g_session.local_is_ghost());
  alerts::tick(g_session, g_session.ghost_actor() > 0 ? game::ghost_ai() : nullptr, ghost_controlled());
  form::tick(g_session, ghost_controlled() ? game::ghost_ai() : nullptr);
  outcome::tick(g_session, ghost_controlled());
  notice::tick();
  leave::tick_background();
  for (session::Outgoing& o : g_session.take_outgoing()) {
    if (!net::send(o.bytes.data(), o.bytes.size(), to_net(o.to), o.reliable, o.actor))
      PG_LOG("room: send of message type %d failed", o.bytes.size() > 3 ? o.bytes[3] : -1);
  }
}


void teardown() {
  lobby::teardown();
  round::teardown();
  control::teardown();
  senses::teardown();
  alerts::teardown();
  form::teardown();
  body::teardown();
  notice::hide();
}

}  // namespace pg::room
