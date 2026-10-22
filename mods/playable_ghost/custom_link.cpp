#include "custom_link.h"

#include <map>

#include "mod_glue.h"
#include "services.h"

namespace pg::custom_link {
namespace {

const PgCustomGhostsService* service() {
  const auto* s = static_cast<const PgCustomGhostsService*>(mod::host().service(PG_SERVICE_CUSTOM_GHOSTS));
  return s && s->version >= 1 ? s : nullptr;
}

const ability::Ability* custom_ability(int identity) {
  static std::map<int, ability::Ability> known;  // the service's strings live for the process
  const PgCustomGhostsService* s = service();
  const char* name = s ? s->abilityName(identity) : nullptr;
  if (!name) return nullptr;
  ability::Ability& a = known[identity];
  a = {name, s->abilityDescription(identity)};
  return &a;
}

}  // namespace

const ability::Ability* own_ability(int behaviourType, int identity) {
  if (const ability::Ability* a = ability::of(behaviourType)) return a;
  return custom_ability(identity);
}

bool is_custom_ability(int behaviourType, int identity) {
  return !ability::of(behaviourType) && custom_ability(identity) != nullptr;
}

bool use_custom_ability(int identity) {
  const PgCustomGhostsService* s = service();
  return s && s->useAbility(identity);
}

bool held_back(int identity, int kind) {
  const PgCustomGhostsService* s = service();
  if (s && s->version >= 3 && s->heldBack && s->heldBack(identity, kind)) return true;
  const auto* tells = static_cast<const PgGhostTellsService*>(mod::host().service(PG_SERVICE_GHOST_TELLS));
  return tells && tells->version >= 1 && tells->heldBack && tells->heldBack(kind);
}

}  // namespace pg::custom_link
