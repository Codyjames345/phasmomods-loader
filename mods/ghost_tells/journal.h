#pragma once

// The told ghosts' journal pages (journal_text.h): while the tells are on, the game's own description
// of each of the six gains a line naming its new tells. Every client. Main thread.
namespace pg::tells::journal {

bool create_hooks();  // before hooks::enable_all

}  // namespace pg::tells::journal
