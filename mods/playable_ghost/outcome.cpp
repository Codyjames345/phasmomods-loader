#include "outcome.h"

#include <windows.h>

#include <set>
#include <vector>

#include "game.h"
#include "guess.h"
#include "log.h"
#include "mod_glue.h"
#include "reward.h"
#include "score.h"

namespace pm::outcome {
namespace {

constexpr ULONGLONG kSettleMs = 4000;         // after the end, for the last guesses to arrive
constexpr ULONGLONG kGuessMs = 1000;

bool g_inContract = false;
uint8_t g_identity = score::kNoGuess;  // what the ghost is (ghost player's client)
ULONGLONG g_scoreDue = 0, g_nextGuess = 0;
std::set<int> g_investigators;  // ghost player's client: everyone who was one during this contract
std::vector<int> g_atEnd;       // ghost player's client: who was in the room as it ended
bool g_ended = false;

score::Guess local_guess() { return guess::of(game::local_player()); }

}  // namespace

void contract_over() {
  g_scoreDue = GetTickCount64() + kSettleMs;
  g_ended = true;
  game::room_actors(g_atEnd);  // one who leaves from here on did not quit the contract
  PM_LOG("outcome: the contract is over; the deception score follows");
}

void tick(session::Session& s, bool inContract) {
  const ULONGLONG now = GetTickCount64();
  if (inContract && !g_inContract && s.local_is_ghost()) {  // a new contract
    s.clear_guesses();
    g_investigators.clear();
    g_ended = false;
  }
  g_inContract = inContract;
  if (inContract) s.in_contract();
  if (s.local_is_ghost() && inContract) {
    if (s.status().identity != session::kNoType) g_identity = s.status().identity;
    if (!g_ended)
      for (const int actor : s.actors())
        if (actor != s.ghost_actor()) g_investigators.insert(actor);
  }
  // Not behind local_is_ghost: back in the lobby the rotation may already have named the next ghost player.
  if (g_scoreDue && now >= g_scoreDue) {
    g_scoreDue = 0;
    const score::Tally t = score::tally(g_investigators, g_atEnd, s.guesses(), g_identity);
    s.send_score(t);
    reward::settled(t);  // the ghost player's popup: the score, with the pay if it is paid now
    PM_LOG("outcome: deception score %d of %d (%d killed, %d misled, %d quit)", t.points(), t.investigators, t.killed,
           t.misled, t.quit);
  }
  if (!s.local_is_ghost() && inContract && s.ghost_actor() > 0 && now >= g_nextGuess) {
    g_nextGuess = now + kGuessMs;
    s.send_guess(local_guess());
  }
  score::Tally received;
  if (s.take_score(received)) mod::show_message(score::headline(received, false), score::details(received));
}

}  // namespace pm::outcome
