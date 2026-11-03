// Offline tests for which peers run which mods (host/presence.h). Run: build\presence_test.exe
#include "presence.h"

#include "check.h"

using namespace pm::presence;

namespace {

const Mod kGhost{"phasmomods.playable-ghost", "0123456789abcdef", "Playable Ghost", "1.0.0"};
const Mod kCustom{"phasmomods.custom-ghosts", "fedcba9876543210", "Custom Ghosts", "1.2.10"};

using Announcement = std::vector<std::vector<uint8_t>>;
// Every part of `hello`, as `listener` hears it from `sender`.
void hear(Presence& listener, int sender, const Announcement& hello, uint64_t now) {
  for (const auto& part : hello) listener.receive(sender, part.data(), part.size(), now);
}

void announces_and_learns() {
  Presence a, b;
  a.set_local({kGhost, kCustom});
  a.observe(1, {1, 2}, 1000);
  b.observe(2, {1, 2}, 1000);
  const auto hello = a.take_announcement();
  CHECK(hello.size() == 1 && hello[0].size() <= kMaxMessage);
  hear(b, 1, hello, 1000);
  CHECK(b.has(1, "phasmomods.playable-ghost") && b.has(1, "phasmomods.custom-ghosts") && !b.has(1, "replay"));
  CHECK(!b.has(2, "phasmomods.playable-ghost"));  // b itself started nothing
  CHECK(a.has(1, "phasmomods.custom-ghosts"));    // the local actor answers for itself
  CHECK(a.take_announcement().empty());  // not again until it is due
  a.observe(1, {1, 2}, 1000 + kAnnounceMs);
  CHECK(!a.take_announcement().empty());
}

void builds_arrive_intact() {
  Presence a, b;
  a.set_local({kGhost, kCustom});
  a.observe(1, {1, 2}, 0);
  b.observe(2, {1, 2}, 0);
  auto hello = a.take_announcement();
  hear(b, 1, hello, 0);
  const std::vector<Mod>* mods = b.mods_of(1);
  CHECK(mods && *mods == (std::vector<Mod>{kGhost, kCustom}));
  CHECK(!b.mods_of(3));  // never heard from
  a.set_local({kGhost});
  hello = a.take_announcement();
  hear(b, 1, hello, 1);
  CHECK(b.mods_of(1) && *b.mods_of(1) == std::vector<Mod>{kGhost});
  CHECK(a.local() == std::vector<Mod>{kGhost});
}

void nothing_before_consent_or_outside_a_room() {
  Presence a;
  a.observe(1, {1, 2}, 0);
  CHECK(a.take_announcement().empty());  // no mods started: say nothing
  a.set_local({kGhost});
  a.observe(0, {}, 0);
  CHECK(a.take_announcement().empty());  // not in a (private) room
}

void forgets_silent_and_departed_peers() {
  Presence a, b;
  a.set_local({kGhost});
  a.observe(1, {1, 2}, 0);
  b.observe(2, {1, 2}, 0);
  auto hello = a.take_announcement();
  hear(b, 1, hello, 0);
  b.observe(2, {1, 2}, kTimeoutMs + 1);
  CHECK(!b.has(1, kGhost.id));
  hear(b, 1, hello, kTimeoutMs + 1);
  CHECK(b.has(1, kGhost.id));
  b.observe(2, {2}, kTimeoutMs + 2);  // actor 1 left
  CHECK(!b.has(1, kGhost.id));
}

void announces_at_once_when_someone_joins() {
  Presence a;
  a.set_local({kGhost});
  a.observe(1, {1}, 0);
  CHECK(!a.take_announcement().empty());
  a.observe(1, {1, 3}, 10);
  CHECK(!a.take_announcement().empty());
}

void ignores_malformed_old_and_own_messages() {
  Presence b;
  b.observe(2, {1, 2}, 0);
  const uint8_t truncated[] = {'H', 'M', kVersion, 1, 0, 1, 1, 2, 'a', 'b', 0, 0, 0};  // build cut short
  b.receive(1, truncated, sizeof truncated, 0);
  CHECK(!b.mods_of(1));
  // v0.1.0's version 2, one mod "a": a client of that release is never heard, so counts as running none.
  const uint8_t versionTwo[] = {'H', 'M', 2, 1, 1, 'a', 0, 0, 0, 0, 0, 0, 0, 0};
  b.receive(1, versionTwo, sizeof versionTwo, 0);
  CHECK(!b.mods_of(1));
  const uint8_t own[] = {'H', 'M', kVersion, 1, 0, 1, 1, 1, 'a', 0, 0, 0, 0, 0, 0, 0, 0, 1, 'A', 0};
  b.receive(2, own, sizeof own, 0);
  CHECK(!b.has(2, "a"));
  b.receive(1, own, sizeof own, 0);
  CHECK(b.has(1, "a") && b.mods_of(1)->front().build == "0000000000000000" && b.mods_of(1)->front().name == "A" &&
        b.mods_of(1)->front().version.empty());
  // What another client says is drawn in the game's menus: a name the game could not draw, a version that is not
  // one, an id no mod can have, bytes past the last mod, or a part numbered past its count, and the message says nothing.
  Presence c;
  c.observe(2, {1, 2}, 0);
  const std::vector<std::vector<uint8_t>> bad = {
      {'H', 'M', kVersion, 1, 0, 1, 1, 1, 'a', 0, 0, 0, 0, 0, 0, 0, 0, 3, '<', 'b', '>', 0},
      {'H', 'M', kVersion, 1, 0, 1, 1, 1, 'a', 0, 0, 0, 0, 0, 0, 0, 0, 1, 'A', 3, '1', '.', '0'},
      {'H', 'M', kVersion, 1, 0, 1, 1, 1, 'A', 0, 0, 0, 0, 0, 0, 0, 0, 1, 'A', 0},
      {'H', 'M', kVersion, 1, 0, 1, 1, 1, 'a', 0, 0, 0, 0, 0, 0, 0, 0, 1, 'A', 0, 9},
      {'H', 'M', kVersion, 1, 1, 1, 1, 1, 'a', 0, 0, 0, 0, 0, 0, 0, 0, 1, 'A', 0},
      {'H', 'M', kVersion, 1, 0, 0, 0}};
  for (const auto& m : bad) c.receive(1, m.data(), m.size(), 0);
  CHECK(!c.mods_of(1));
}

// A client whose mods stop while it is in a room says so once, with an announcement of no mods, so the
// other players stop counting the mods it announced before; outside a room it says nothing.
void stopping_in_a_room_is_announced_once() {
  Presence a, b;
  a.set_local({kGhost});
  a.observe(1, {1, 2}, 0);
  b.observe(2, {1, 2}, 0);
  auto hello = a.take_announcement();
  hear(b, 1, hello, 0);
  CHECK(b.has(1, kGhost.id));
  a.set_local({});
  const auto bye = a.take_announcement();
  CHECK(bye.size() == 1 && bye[0].size() == kHeaderBytes && bye[0][5] == 1 && bye[0][6] == 0);
  hear(b, 1, bye, 1);
  CHECK(b.mods_of(1) && b.mods_of(1)->empty() && !b.has(1, kGhost.id));
  a.observe(1, {1, 2}, kAnnounceMs + 1);
  CHECK(a.take_announcement().empty());  // said once
  // Stopped outside a room: nobody to tell, and nothing owed for the next room.
  Presence c;
  c.set_local({kGhost});
  c.set_local({});
  c.observe(1, {1, 2}, 0);
  CHECK(c.take_announcement().empty());
}

// What another player is heard to run changes the revision; hearing the same again does not.
void revision_follows_what_peers_run() {
  Presence a, b;
  a.set_local({kGhost});
  a.observe(1, {1, 2}, 0);
  b.observe(2, {1, 2}, 0);
  const unsigned start = b.revision();
  auto hello = a.take_announcement();
  hear(b, 1, hello, 0);
  const unsigned heard = b.revision();
  CHECK(heard != start);
  hear(b, 1, hello, kAnnounceMs);  // the same list again
  b.observe(2, {1, 2}, kAnnounceMs);
  CHECK(b.revision() == heard);
  a.set_local({kGhost, kCustom});
  hello = a.take_announcement();
  hear(b, 1, hello, kAnnounceMs + 1);
  const unsigned changed = b.revision();
  CHECK(changed != heard);
  b.observe(2, {2}, kAnnounceMs + 2);  // player 1 left: forgotten
  CHECK(b.revision() != changed);
}

// The longest id there can be, distinct for each of `count` mods, with a long name and the longest version.
std::vector<Mod> longest(int count) {
  std::vector<Mod> mods;
  for (int i = 0; i < count; ++i) {
    std::string id = std::string(30, 'a') + "." + std::string(30, 'b') + std::to_string(100 + i);
    CHECK(id.size() == pm::modid::kMaxIdBytes && pm::modid::valid_id(id));
    mods.push_back({id, "00112233445566" + std::to_string(10 + i), std::string(60, 'N') + std::to_string(1000 + i),
                    "4294967295.4294967295.4294967295"});
  }
  return mods;
}

// Every mod is announced, however many: 40 of the longest ids, with names and versions, take several messages, each
// within what the host channel carries, and arrive whole and in order. (Version 2 stopped at 511 bytes: the rest
// went unannounced, and their player counted as lacking them.)
void presence_announces_every_mod_across_parts() {
  Presence a, b;
  const std::vector<Mod> mods = longest(40);
  a.set_local(mods);
  a.observe(1, {1, 2}, 0);
  b.observe(2, {1, 2}, 0);
  const Announcement hello = a.take_announcement();
  CHECK(hello.size() > 1);
  size_t listed = 0;
  for (size_t i = 0; i < hello.size(); ++i) {
    CHECK(hello[i].size() <= kMaxMessage && hello[i][4] == i && hello[i][5] == hello.size() && hello[i][3] == hello[0][3]);
    listed += hello[i][6];
  }
  CHECK(listed == 40);
  hear(b, 1, hello, 0);
  CHECK(b.mods_of(1) && *b.mods_of(1) == mods);
  for (const Mod& m : mods) CHECK(b.has(1, m.id));
}

// An announcement of more mods than any install could hold says nothing about its sender: it is ignored whole, and the list
// last heard stands. At the cap it is heard.
void presence_ignores_an_announcement_past_the_cap() {
  const auto many = [](size_t count) {
    std::vector<Mod> mods;
    for (size_t i = 0; i < count; ++i) mods.push_back({"m" + std::to_string(i), "0011223344556677", "N", "1.0.0"});
    return mods;
  };
  Presence a, b;
  a.observe(1, {1, 2}, 0);
  b.observe(2, {1, 2}, 0);
  a.set_local(many(pm::presence::kMaxMods));
  hear(b, 1, a.take_announcement(), 0);
  CHECK(b.mods_of(1) && b.mods_of(1)->size() == pm::presence::kMaxMods);
  a.set_local(many(pm::presence::kMaxMods + 1));
  const unsigned heard = b.revision();
  hear(b, 1, a.take_announcement(), 1);
  CHECK(b.mods_of(1) && b.mods_of(1)->size() == pm::presence::kMaxMods && b.revision() == heard);
  Presence c;
  c.observe(3, {1, 3}, 0);
  a.set_local(many(pm::presence::kMaxMods + 1));
  hear(c, 1, a.take_announcement(), 0);
  CHECK(!c.mods_of(1));
}

// A player keeps the list last heard in full until every part of a newer announcement has arrived, whatever order they
// come in: half an announcement never stands for the whole one, nor does a part from an older one complete it.
void presence_keeps_last_list_until_all_parts_arrive() {
  Presence a, b;
  a.set_local(longest(10));
  a.observe(1, {1, 2}, 0);
  b.observe(2, {1, 2}, 0);
  const Announcement first = a.take_announcement();
  CHECK(first.size() >= 3);
  // Nothing whole yet: not heard.
  for (size_t i = 0; i + 1 < first.size(); ++i) b.receive(1, first[i].data(), first[i].size(), 0);
  CHECK(!b.mods_of(1));
  b.receive(1, first.back().data(), first.back().size(), 0);
  CHECK(b.mods_of(1) && b.mods_of(1)->size() == 10);
  const unsigned heard = b.revision();

  std::vector<Mod> fewer = longest(6);
  fewer.push_back(kGhost);
  a.set_local(fewer);
  const Announcement second = a.take_announcement();
  CHECK(second.size() >= 2 && second[0][3] != first[0][3]);
  // The newer parts out of order, one missing, and a stale part of the first between them: the old list stands.
  b.receive(1, second.back().data(), second.back().size(), 1);
  b.receive(1, first[0].data(), first[0].size(), 1);
  for (size_t i = 1; i + 1 < second.size(); ++i) b.receive(1, second[i].data(), second[i].size(), 1);
  CHECK(b.mods_of(1) && b.mods_of(1)->size() == 10 && !b.has(1, kGhost.id));
  // Still heard from, so not forgotten while the parts come in.
  b.observe(2, {1, 2}, kTimeoutMs);
  CHECK(b.mods_of(1));
  b.receive(1, second[0].data(), second[0].size(), kTimeoutMs);
  // The stale part reset the gathering, so the second is whole only once its parts all arrive again.
  hear(b, 1, second, kTimeoutMs);
  CHECK(b.mods_of(1) && *b.mods_of(1) == fewer && b.revision() != heard);
}

}  // namespace

int main() {
  announces_and_learns();
  builds_arrive_intact();
  nothing_before_consent_or_outside_a_room();
  forgets_silent_and_departed_peers();
  announces_at_once_when_someone_joins();
  ignores_malformed_old_and_own_messages();
  stopping_in_a_room_is_announced_once();
  revision_follows_what_peers_run();
  presence_announces_every_mod_across_parts();
  presence_keeps_last_list_until_all_parts_arrive();
  presence_ignores_an_announcement_past_the_cap();
  std::printf(g_failures ? "%d check(s) failed\n" : "all presence tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
