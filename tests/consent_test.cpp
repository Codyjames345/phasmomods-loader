// Offline tests for the notice players agree to (host/consent_text.h). Run: build\consent_test.exe
#include <windows.h>

#include "consent_text.h"

#include "check.h"

using namespace pg::consent;

namespace {

bool says(const Notice& n, const char* text) { return n.body.find(text) != std::string::npos; }

void covers_the_whole_suite() {
  const Notice n = notice({{"Playable Ghost", false}, {"Custom Ghosts", true}});
  CHECK(says(n, "ban"));
  CHECK(says(n, "private lobbies"));
  CHECK(says(n, "Voices are recorded"));
  CHECK(says(n, "Custom Ghosts keeps what other players say"));
  CHECK(says(n, "own risk"));
  CHECK(says(n, "Playable Ghost, Custom Ghosts"));
  CHECK(says(n, "Mods button"));  // every installed mod is named: any can be switched on later
  CHECK(n.body.find("\xE2\x80\x94") == std::string::npos);  // no em dash in player-facing copy
}

void mentions_recording_only_when_a_mod_records() {
  const Notice n = notice({{"Playable Ghost", false}});
  CHECK(!says(n, "recorded"));
  CHECK(says(n, "ban") && says(n, "own risk"));
}

void a_changed_notice_asks_again() {
  const Notice a = notice({{"Playable Ghost", false}});
  const Notice b = notice({{"Playable Ghost", false}, {"Custom Ghosts", true}});
  CHECK(fingerprint(a) == fingerprint(notice({{"Playable Ghost", false}})));
  CHECK(fingerprint(a) != fingerprint(b));
  CHECK(fingerprint(a).size() == 16);
}

void remembers_only_the_agreed_notice() {
  wchar_t temporary[MAX_PATH];
  CHECK(GetTempPathW(MAX_PATH, temporary) > 0);
  const std::wstring path = std::wstring(temporary) + L"pg-consent-" + std::to_wstring(GetCurrentProcessId()) + L".txt";
  DeleteFileW(path.c_str());
  const std::string a = fingerprint(notice({{"Playable Ghost", false}}));
  const std::string b = fingerprint(notice({{"Playable Ghost", false}, {"Custom Ghosts", true}}));
  CHECK(!remembered(path, a));  // nothing stored yet
  CHECK(remember(path, a));
  CHECK(remembered(path, a));
  CHECK(!remembered(path, b));  // a changed notice is asked again
  CHECK(!remembered(path, a.substr(0, 8)));  // a prefix is not an agreement
  CHECK(remember(path, b) && remembered(path, b) && !remembered(path, a));
  CHECK(DeleteFileW(path.c_str()));
}

}  // namespace

int main() {
  covers_the_whole_suite();
  mentions_recording_only_when_a_mod_records();
  a_changed_notice_asks_again();
  remembers_only_the_agreed_notice();
  return g_failures ? 1 : 0;
}
