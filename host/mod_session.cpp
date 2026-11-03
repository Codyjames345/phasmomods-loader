#include "mod_session.h"

#include "log.h"

namespace pm::mod_session {

bool begin(consent::Answer answer, const Steps& steps, size_t& modHooks) {
  if (answer != consent::Answer::Agreed) return false;
  if (steps.required && !steps.required()) {
    PM_LOG("suite: the hooks the lobby's rules need could not be made, so no mod starts");
    return false;
  }
  if (!steps.install()) PM_LOG("suite: some of the host's hooks for the mods could not be made; the mods start without them where they can");
  modHooks = steps.mark();
  steps.start();
  return true;
}

consent::Answer opening(bool modsFound, const std::string& messages, const std::string& lobby, std::string& cause) {
  cause.clear();
  if (!modsFound) {
    PM_LOG("suite: no mods to start");
    return consent::Answer::Declined;  // nothing to agree to: never block the game with the notice
  }
  if (messages.empty() && lobby.empty()) return consent::Answer::Pending;
  std::string lacking = messages;
  if (!lobby.empty()) lacking += (lacking.empty() ? "" : ", ") + lobby;
  PM_LOG("suite: this game build lacks what the lobby's rules need; the mods stay off. These game entries did not "
         "resolve: %s", lacking.c_str());
  cause = lacks_the_rules();
  return consent::Answer::Declined;
}

}  // namespace pm::mod_session
