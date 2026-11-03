#include "ghostbook.h"

#include "text_table.h"

namespace pm::ghostbook {
namespace {

constexpr int kMaxCharges = 2;  // actions::kMaxCharges

std::string key(const std::string& label) {
  return i18n::text("book.key", {{"key", label.empty() ? i18n::text("book.key-unbound") : label}});
}

std::string sanity_line(const session::Status& s) {
  if (s.sanity == session::kUnknown) return i18n::text("book.sanity-unknown");
  std::string line = (s.flags & session::kFocusSanity) ? i18n::text("book.sanity-target", {{"sanity", std::to_string(s.sanity)}})
                                                       : i18n::text("book.sanity-team", {{"sanity", std::to_string(s.sanity)}});
  // The hunt check measures the team when the target is not what it measures (a ghost's own target).
  if (s.huntSanity != session::kUnknown) line += "\n" + i18n::text("book.sanity-team", {{"sanity", std::to_string(s.huntSanity)}});
  if (s.huntBelow == session::kUnknown) return line;
  line += "\n" + i18n::text("book.hunts-at", {{"sanity", std::to_string(s.huntBelow)}});
  if (session::hunt_sanity(s) <= s.huntBelow) line += "  " + i18n::text("book.low-enough");
  return line;
}

std::string hunt_state(const session::Status& s) {
  if (s.flags & session::kHunting) return i18n::text("book.hunting");
  if (s.flags & session::kHuntAvailable) return i18n::text("book.hunt-available");
  return i18n::text("book.no-hunt");
}

std::string charges(const session::Status& s, bool hasAbility) {
  const std::string interactions = std::to_string(session::whole_charges(s.interact, kMaxCharges));
  if (!hasAbility)
    return i18n::text("book.ready", {{"interactions", interactions}, {"events", std::to_string(s.events)},
                                     {"fuses", std::to_string(s.fuses)}});
  return i18n::text("book.ready-with-ability", {{"interactions", interactions}, {"events", std::to_string(s.events)},
                                                {"abilities", std::to_string(s.abilities)}, {"fuses", std::to_string(s.fuses)}});
}

}  // namespace

Book write(const Facts& f) {
  Book b;
  const std::string move = key(f.up) + " " + key(f.left) + " " + key(f.down) + " " + key(f.right);
  b.pages[kHomeLeft].title = i18n::text("book.playing.title");
  b.pages[kHomeLeft].body = i18n::text("book.playing.body", {{"move", move}, {"journal", key(f.journal)}, {"pause", key(f.pause)}});
  b.pages[kHomeRight].title = i18n::text("book.haunting.title");
  b.pages[kHomeRight].body = i18n::text("book.haunting.body", {{"primary", key(f.primary)}});

  const session::Status& st = f.status;
  const ability::Ability* own = f.ability;
  b.pages[kItemsLeft].title = i18n::text("book.abilities.title");
  b.pages[kItemsLeft].body = i18n::text("book.abilities.body", {{"interact", key(f.interact)}, {"primary", key(f.primary)}});
  if (own)
    b.pages[kItemsLeft].body +=
        i18n::text("book.abilities.own", {{"key", key(f.secondary)}, {"name", own->name}, {"description", own->description}});
  b.pages[kItemsLeft].body += i18n::text("book.abilities.fuse-hunt", {{"fuse", key(f.fuse)}, {"primary", key(f.primary)}});
  if (!own && st.type != session::kNoType) b.pages[kItemsLeft].body += "\n\n" + i18n::text("book.abilities.none");

  b.pages[kItemsRight].title = i18n::text("book.rules.title");
  b.pages[kItemsRight].body = i18n::text("book.rules.body");

  b.pages[kOverviewLeft].title = i18n::text("book.overview.title");
  b.pages[kOverviewLeft].body =
      i18n::text("book.overview.body", {{"type", f.type.empty() ? i18n::text("book.overview.not-yet") : f.type},
                                        {"room", f.room.empty() ? i18n::text("book.overview.not-yet") : f.room}});

  b.pages[kOverviewRight].title = i18n::text("book.hunt.title");
  b.pages[kOverviewRight].body = sanity_line(st) + "\n\n" + hunt_state(st) + "\n\n" + charges(st, own != nullptr);
  if (st.flags & session::kBound) b.pages[kOverviewRight].body += "\n\n" + i18n::text("book.bound");

  b.evidenceTitle = i18n::text("book.evidence-title");
  b.ghostsTitle = i18n::text("book.verdict-title");
  b.ghostsLine = i18n::text("book.verdict-line");
  return b;
}

}  // namespace pm::ghostbook
