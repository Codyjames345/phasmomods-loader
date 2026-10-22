#include "ghostbook.h"

namespace pg::ghostbook {
namespace {

constexpr int kMaxCharges = 2;  // actions::kMaxCharges

std::string key(const std::string& label) { return "<b>[" + (label.empty() ? std::string("?") : label) + "]</b>"; }

std::string sanity_line(const session::Status& s) {
  if (s.sanity == session::kUnknown) return "Their sanity: not known yet";
  const std::string who = (s.flags & session::kFocusSanity) ? "Your target's sanity" : "The team's sanity";
  std::string line = who + ": <b>" + std::to_string(s.sanity) + "%</b>";
  if (s.huntBelow == session::kUnknown) return line;
  line += "\nHunts possible at <b>" + std::to_string(s.huntBelow) + "%</b> or below";
  if (s.sanity <= s.huntBelow) line += "  <color=#8C1A10>(low enough)</color>";
  return line;
}

std::string hunt_state(const session::Status& s) {
  if (s.flags & session::kHunting) return "<color=#8C1A10><b>Hunting</b></color>";
  if (s.flags & session::kHuntAvailable) return "<b>A hunt is available</b>";
  return "No hunt available yet";
}

std::string charges(const session::Status& s, bool hasAbility) {
  std::string line = "Ready: <b>" + std::to_string(session::whole_charges(s.interact, kMaxCharges)) + "</b> interaction, <b>" +
                     std::to_string(s.events) + "</b> event";
  if (hasAbility) line += ", <b>" + std::to_string(s.abilities) + "</b> ability";
  return line + ", <b>" + std::to_string(s.fuses) + "</b> fuse box";
}

}  // namespace

Book write(const Facts& f) {
  Book b;
  const std::string move = key(f.up) + " " + key(f.left) + " " + key(f.down) + " " + key(f.right);

  b.pages[kHomeLeft].title = "Playing the ghost";
  b.pages[kHomeLeft].body =
      "You are the ghost. The investigators must work out what you are and get away; you win by "
      "killing every one of them.\n\n"
      "Move with " + move + " and look with the mouse. A ghost only moves when the urge to wander "
      "comes, a few metres at a time, and when it hunts.\n\n" +
      key(f.journal) + " opens this journal and " + key(f.pause) + " its pause menu.\n\n"
      "Your favourite room is where they will look for you. When the game moves it, you have a few "
      "seconds to choose another on your map.";

  b.pages[kHomeRight].title = "The haunting";
  b.pages[kHomeRight].body =
      "<b>Your urges.</b> Every few seconds the ghost feels an urge, as any ghost of your type does: "
      "to touch something, to cause an event, to use its ability or the fuse box. Each one is kept "
      "for you to act on, up to two of a kind.\n\n"
      "<b>Restlessness.</b> Do nothing for half a minute and the ghost acts on its urges by itself "
      "until you try to act again.\n\n"
      "<b>Your room.</b> Some urges draw you back to your favourite room, as a ghost always is. "
      "Some things bind you to it for a while.\n\n"
      "<b>Hunting.</b> When the urge to hunt comes, hold " + key(f.primary) + " before the next one "
      "would. Crucifixes still burn, smoke still blinds you and the cooldown still applies.\n\n"
      "<b>Evidence.</b> Your type leaves its evidence on its own, as it always does.";

  const session::Status& st = f.status;
  const ability::Ability* own = f.ability;
  b.pages[kItemsLeft].title = "Your abilities";
  b.pages[kItemsLeft].body =
      key(f.interact) + " <b>Interact</b>: look at a door, light switch or object within reach.\n\n" +
      key(f.primary) + " <b>Manifest</b> (tap): an event your type is allowed, such as an "
      "appearance or a sound.\n\n";
  if (own) b.pages[kItemsLeft].body += key(f.secondary) + " <b>" + own->name + "</b>: " + own->description + "\n\n";
  b.pages[kItemsLeft].body +=
      key(f.fuse) + " <b>Fuse box</b>: switch the power from anywhere. Some types never turn it off, "
      "others never on, and turning it on often fails.\n\n"
      "Hold " + key(f.primary) + " <b>Hunt</b>: when a hunt is available.";
  if (!own && st.type != session::kNoType) b.pages[kItemsLeft].body += "\n\nYour type has no ability of its own.";

  b.pages[kItemsRight].title = "The rules";
  b.pages[kItemsRight].body =
      "Everything you do goes through the game's own rules for your type, so your evidence and "
      "behaviour stay true to it.\n\n"
      "You can only do what your urges allow, and only when the game would let a ghost of your type "
      "do it: nothing touched before the front door opens, and a Shade keeps still while it is "
      "watched.\n\n"
      "An interaction with an object your type rarely touches uses up more than one urge.\n\n"
      "Hunts follow the game's checks: sanity, setup time, cooldown and smudging.";

  b.pages[kOverviewLeft].title = "Your ghost";
  b.pages[kOverviewLeft].body =
      "Type: <b>" + (f.type.empty() ? std::string("chosen as the haunting begins") : f.type) + "</b>\n\n"
      "Favourite room: <b>" + (f.room.empty() ? std::string("chosen as the haunting begins") : f.room) + "</b>\n\n"
      "Goal: kill every investigator before they leave.";

  b.pages[kOverviewRight].title = "The hunt";
  b.pages[kOverviewRight].body = sanity_line(st) + "\n\n" + hunt_state(st) + "\n\n" + charges(st, own != nullptr);
  if (st.flags & session::kBound) b.pages[kOverviewRight].body += "\n\nYou are bound to your favourite room for now.";

  b.evidenceTitle = "Their evidence";
  b.ghostsTitle = "Their verdict";
  b.ghostsLine = "The investigators believe the ghost is a";
  return b;
}

}  // namespace pg::ghostbook
