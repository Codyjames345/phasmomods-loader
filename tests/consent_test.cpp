// Offline tests for the notice players agree to (host/consent_text.h). Run: build\consent_test.exe
#include <windows.h>

#include <cstring>

#include "consent_text.h"

#include "check.h"
#include "text_fixture.h"

using namespace pm::consent;

namespace {

const pm::build::Hashes kBuild{"aaaa", "bbbb"};

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

// Every notice warns that a mod is a program, after the account's risk (and after the update paragraph, which stays straight
// after it), whatever mods are installed. The warning
// changed the notice, so a player who agreed to the one before it is asked again.
void notice_warns_about_community_mods() {
  for (const Notice& n : {notice({}), notice({{"Playable Ghost", false}}), notice({{"Custom Ghosts", true}}, false)}) {
    CHECK(says(n, programs_paragraph().c_str()));
    CHECK(n.body.find("Your account is at risk") < n.body.find("Mods are programs"));
    if (says(n, "The game has updated")) CHECK(n.body.find("The game has updated") < n.body.find("Mods are programs"));
    CHECK(n.body.find("Mods are programs") < n.body.find("Your progress is at your own risk"));
    Notice before = n;
    before.body.erase(before.body.find(programs_paragraph()), programs_paragraph().size());
    CHECK(fingerprint(before, kBuild) != fingerprint(n, kBuild));
  }
}

void a_changed_notice_asks_again() {
  const Notice a = notice({{"Playable Ghost", false}});
  const Notice b = notice({{"Playable Ghost", false}, {"Custom Ghosts", true}});
  CHECK(fingerprint(a, kBuild) == fingerprint(notice({{"Playable Ghost", false}}), kBuild));
  CHECK(fingerprint(a, kBuild) != fingerprint(b, kBuild));
  CHECK(fingerprint(a, kBuild).size() == 16);
}

void remembers_only_the_agreed_notice() {
  wchar_t temporary[MAX_PATH];
  CHECK(GetTempPathW(MAX_PATH, temporary) > 0);
  const std::wstring path = std::wstring(temporary) + L"pg-consent-" + std::to_wstring(GetCurrentProcessId()) + L".txt";
  DeleteFileW(path.c_str());
  const std::string a = fingerprint(notice({{"Playable Ghost", false}}), kBuild);
  const std::string b = fingerprint(notice({{"Playable Ghost", false}, {"Custom Ghosts", true}}), kBuild);
  CHECK(!remembered(path, a));  // nothing stored yet
  CHECK(remember(path, a));
  CHECK(remembered(path, a));
  CHECK(!remembered(path, b));  // a changed notice is asked again
  CHECK(!remembered(path, a.substr(0, 8)));  // a prefix is not an agreement
  CHECK(remember(path, b) && remembered(path, b) && !remembered(path, a));
  CHECK(DeleteFileW(path.c_str()));
}

constexpr char kExactParagraph[] =
    "<b>The game has updated.</b> These mods haven't been checked against this version of Phasmophobia. Mods that still "
    "find everything they need will run, and the rest stay off. The mods can't tell whether the update changed the "
    "anti-cheat, so playing now carries more risk than usual.\n\n";

// The paragraph is about a build the offline check has not passed, and appears on no other.
void known_build_runs_without_the_update_paragraph() {
  const Notice n = notice({{"Playable Ghost", false}}, true);
  CHECK(!says(n, "The game has updated"));
  CHECK(fingerprint(n, kBuild) == fingerprint(notice({{"Playable Ghost", false}}), kBuild));  // checked is the default
}

// Word for word, straight after the paragraph on the account's risk and before everything else.
void unseen_build_asks_again_with_the_update_paragraph() {
  const Notice n = notice({{"Playable Ghost", true}}, false);
  CHECK(says(n, kExactParagraph));
  const size_t at = n.body.find(kExactParagraph);
  const size_t risk = n.body.find("Your account is at risk");
  const size_t before = n.body.find("Play only with friends who also agreed to this.\n\n");
  CHECK(risk < at && before != std::string::npos && before + std::strlen("Play only with friends who also agreed to this.\n\n") == at);
  CHECK(at < n.body.find("Voices are recorded"));
  CHECK(n.body.find("\xE2\x80\x94") == std::string::npos);
  // It is another notice: an agreement to the known build's does not answer it, on this build or any.
  CHECK(fingerprint(n, kBuild) != fingerprint(notice({{"Playable Ghost", true}}, true), kBuild));
}

// An agreement holds for the build it was given on: another hash of either library asks again, and asking again on the
// same build does not. Moving characters from one hash to the other is another build.
void agreement_is_remembered_per_build() {
  wchar_t temporary[MAX_PATH];
  CHECK(GetTempPathW(MAX_PATH, temporary) > 0);
  const std::wstring path = std::wstring(temporary) + L"pg-consent-build-" + std::to_wstring(GetCurrentProcessId()) + L".txt";
  DeleteFileW(path.c_str());
  const Notice n = notice({{"Playable Ghost", false}}, false);
  const std::string agreed = fingerprint(n, kBuild);
  CHECK(remember(path, agreed));
  CHECK(remembered(path, fingerprint(n, kBuild)));
  CHECK(!remembered(path, fingerprint(n, {"aaab", "bbbb"})));
  CHECK(!remembered(path, fingerprint(n, {"aaaa", "bbbc"})));
  CHECK(fingerprint(n, {"aa", "aabbbb"}) != fingerprint(n, {"aaaa", "bb"}));
  CHECK(can_remember(kBuild) && !can_remember({"", "bbbb"}) && !can_remember({"aaaa", ""}));
  CHECK(DeleteFileW(path.c_str()));
}

// A notice with any text missing would show keys, and "I agree" would be remembered against words nobody read. The shipped table
// has them all; a broken table, or one lacking any of them, is refused by name.
void a_notice_that_cannot_be_read_is_never_asked() {
  CHECK(missing_notice_text(pm::i18n::table()).empty());
  pm::i18n::Table empty;
  CHECK(!missing_notice_text(empty).empty());
  std::string why;
  pm::i18n::Table broken;
  CHECK(!broken.parse("{ \"consent.title\": \"T\", oops }", why));
  CHECK(!missing_notice_text(broken).empty());
  pm::i18n::Table lacking;
  CHECK(lacking.parse("{ \"consent.title\": \"T\" }", why));
  CHECK(missing_notice_text(lacking) == "consent.intro");
}

}  // namespace

int main() {
  use_text({"i18n/default.json"});
  a_notice_that_cannot_be_read_is_never_asked();
  known_build_runs_without_the_update_paragraph();
  unseen_build_asks_again_with_the_update_paragraph();
  agreement_is_remembered_per_build();
  covers_the_whole_suite();
  mentions_recording_only_when_a_mod_records();
  a_changed_notice_asks_again();
  notice_warns_about_community_mods();
  remembers_only_the_agreed_notice();
  return g_failures ? 1 : 0;
}
