#include "outcome.h"

#include <windows.h>

#include "game.h"
#include "il2cpp.h"
#include "log.h"
#include "notice.h"
#include "score.h"

namespace pg::outcome {
namespace {

// Build 25811160 (verified by the host at load); obfuscated names, so by offset.
constexpr size_t kPlayerDead = 0x28;          // Player: dead
constexpr size_t kPlayerJournal = 0xC8;       // Player: JournalController
constexpr size_t kJournalChosenType = 0x1E0;  // JournalController: the chosen ghost (Ghost.type first)
constexpr int kNoneType = 24;                 // GhostType.None: nothing chosen
constexpr ULONGLONG kSettleMs = 4000;         // after the end, for the last guesses to arrive
constexpr ULONGLONG kGuessMs = 1000;
constexpr unsigned kPopupMs = 9000;

bool g_inContract = false;
uint8_t g_identity = score::kNoGuess;  // what the ghost is (ghost player's client)
ULONGLONG g_scoreDue = 0, g_nextGuess = 0;

score::Guess local_guess() {
  score::Guess g;
  void* player = game::local_player();
  if (!player || !game::alive(player)) return g;
  g.dead = il2cpp::field<bool>(player, kPlayerDead);
  void* journal = il2cpp::field<void*>(player, kPlayerJournal);
  if (journal && game::alive(journal) && il2cpp::is_class(journal, "JournalController")) {
    const int type = il2cpp::field<int>(journal, kJournalChosenType);
    if (type != kNoneType && type >= 0 && type < score::kNoGuess) g.type = static_cast<uint8_t>(type);
  }
  return g;
}

}  // namespace

void contract_over() {
  g_scoreDue = GetTickCount64() + kSettleMs;
  PG_LOG("outcome: the contract is over; the deception score follows");
}

void tick(session::Session& s, bool inContract) {
  const ULONGLONG now = GetTickCount64();
  if (inContract && !g_inContract && s.local_is_ghost()) s.clear_guesses();  // a new contract
  g_inContract = inContract;
  if (s.local_is_ghost()) {
    if (inContract && s.status().identity != session::kNoType) g_identity = s.status().identity;
    if (g_scoreDue && now >= g_scoreDue) {
      g_scoreDue = 0;
      const score::Tally t = score::tally(s.guesses(), g_identity);
      notice::show(score::headline(t, true), score::details(t), kPopupMs);
      s.send_score(t);
      PG_LOG("outcome: deception score %d of %d (%d killed, %d misled)", t.points(), t.investigators, t.killed, t.misled);
    }
  } else if (inContract && s.ghost_actor() > 0 && now >= g_nextGuess) {
    g_nextGuess = now + kGuessMs;
    s.send_guess(local_guess());
  }
  score::Tally received;
  if (s.take_score(received)) notice::show(score::headline(received, false), score::details(received), kPopupMs);
}

}  // namespace pg::outcome
