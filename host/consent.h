#pragma once

#include <string>

#include "consent_text.h"

// The notice that every player answers before any mod starts, drawn as the game's own popup over the
// main menu (host/menu_panel.h), or as the mod's overlay when the menu's popup has not appeared
// within 20 s. Both presentations wait for main-menu focus and hide when it is lost.
// It blocks the game's own UI beneath it until it is answered. "I agree" is remembered against the notice's
// fingerprint in `rememberedPath`; "Play without mods" is not remembered, so the next start asks
// again. Main thread only.
namespace pg::consent {

enum class Answer { Pending, Agreed, Declined };

void begin(const Notice& notice, const std::wstring& rememberedPath);
Answer tick();  // every frame: shows the notice (again, after a scene change) until it is answered
void hide();

}  // namespace pg::consent
