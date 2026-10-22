// Offline tests for which peers run which mods (host/presence.h). Run: build\presence_test.exe
#include "presence.h"

#include "check.h"

using namespace pg::presence;

namespace {

const Mod kGhost{"playable-ghost", "0123456789abcdef"};
const Mod kCustom{"custom-ghosts", "fedcba9876543210"};

void announces_and_learns() {
  Presence a, b;
  a.set_local({kGhost, kCustom}, false);
  a.observe(1, {1, 2}, 1000);
  b.observe(2, {1, 2}, 1000);
  const auto hello = a.take_announcement();
  CHECK(!hello.empty() && hello.size() <= 511);
  b.receive(1, hello.data(), hello.size(), 1000);
  CHECK(b.has(1, "playable-ghost") && b.has(1, "custom-ghosts") && !b.has(1, "replay"));
  CHECK(!b.has(2, "playable-ghost"));  // b itself started nothing
  CHECK(a.has(1, "custom-ghosts"));    // the local actor answers for itself
  CHECK(a.take_announcement().empty());  // not again until it is due
  a.observe(1, {1, 2}, 1000 + kAnnounceMs);
  CHECK(!a.take_announcement().empty());
}

void builds_arrive_intact() {
  Presence a, b;
  a.set_local({kGhost, kCustom}, true);
  a.observe(1, {1, 2}, 0);
  b.observe(2, {1, 2}, 0);
  auto hello = a.take_announcement();
  b.receive(1, hello.data(), hello.size(), 0);
  const std::vector<Mod>* mods = b.mods_of(1);
  CHECK(mods && *mods == (std::vector<Mod>{kGhost, kCustom}));
  CHECK(!b.mods_of(3));  // never heard from
  a.set_local({kGhost}, false);
  hello = a.take_announcement();
  b.receive(1, hello.data(), hello.size(), 1);
  CHECK(b.mods_of(1) && *b.mods_of(1) == std::vector<Mod>{kGhost});
  CHECK(a.local_vanilla_peers() == false && a.local() == std::vector<Mod>{kGhost});
}

void nothing_before_consent_or_outside_a_room() {
  Presence a;
  a.observe(1, {1, 2}, 0);
  CHECK(a.take_announcement().empty());  // no mods started: say nothing
  a.set_local({kGhost}, true);
  a.observe(0, {}, 0);
  CHECK(a.take_announcement().empty());  // not in a (private) room
}

void forgets_silent_and_departed_peers() {
  Presence a, b;
  a.set_local({kGhost}, true);
  a.observe(1, {1, 2}, 0);
  b.observe(2, {1, 2}, 0);
  auto hello = a.take_announcement();
  b.receive(1, hello.data(), hello.size(), 0);
  b.observe(2, {1, 2}, kTimeoutMs + 1);
  CHECK(!b.has(1, kGhost.id));
  b.receive(1, hello.data(), hello.size(), kTimeoutMs + 1);
  CHECK(b.has(1, kGhost.id));
  b.observe(2, {2}, kTimeoutMs + 2);  // actor 1 left
  CHECK(!b.has(1, kGhost.id));
}

void announces_at_once_when_someone_joins() {
  Presence a;
  a.set_local({kGhost}, true);
  a.observe(1, {1}, 0);
  CHECK(!a.take_announcement().empty());
  a.observe(1, {1, 3}, 10);
  CHECK(!a.take_announcement().empty());
}

void ignores_malformed_old_and_own_messages() {
  Presence b;
  b.observe(2, {1, 2}, 0);
  const uint8_t truncated[] = {'H', 'M', kVersion, 1, 2, 'a', 'b', 0, 0, 0};  // build cut short
  b.receive(1, truncated, sizeof truncated, 0);
  CHECK(!b.mods_of(1));
  const uint8_t versionOne[] = {'H', 'M', 1, 1, 1, 'a'};  // an older host: counts as unheard
  b.receive(1, versionOne, sizeof versionOne, 0);
  CHECK(!b.mods_of(1));
  const uint8_t own[] = {'H', 'M', kVersion, 1, 1, 'a', 0, 0, 0, 0, 0, 0, 0, 0};
  b.receive(2, own, sizeof own, 0);
  CHECK(!b.has(2, "a"));
  b.receive(1, own, sizeof own, 0);
  CHECK(b.has(1, "a") && b.mods_of(1)->front().build == "0000000000000000");
}

}  // namespace

int main() {
  announces_and_learns();
  builds_arrive_intact();
  nothing_before_consent_or_outside_a_room();
  forgets_silent_and_departed_peers();
  announces_at_once_when_someone_joins();
  ignores_malformed_old_and_own_messages();
  std::printf(g_failures ? "%d check(s) failed\n" : "all presence tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
