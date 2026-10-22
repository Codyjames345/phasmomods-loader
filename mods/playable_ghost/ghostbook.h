#pragma once

#include <string>

#include "ability.h"
#include "session.h"

// What the ghost player's journal says in place of the investigators' pages (the pages themselves are
// src/journal.cpp's): a guide on the Home spread, the ghost's abilities on the Items spread, the
// ghost's own sheet on the Overview spread, and the evidence spread retitled as the investigators'
// findings. Pure text, kept free of game calls so it can be tested offline.
namespace pg::ghostbook {

struct Facts {
  std::string up, left, down, right;           // movement binding labels
  std::string interact, primary, secondary;    // action binding labels
  std::string fuse;                            // the fuse box binding label (the torch control)
  std::string journal, pause;                  // journal and pause binding labels
  std::string type;                            // the ghost's type name, empty until it spawns
  std::string room;                            // its favourite room's name, empty until known
  session::Status status;                      // charges, flags, sanity, hunt threshold and type from the host
  const ability::Ability* ability = nullptr;   // the ghost's own ability, if its type has one
};

enum PageId { kHomeLeft, kHomeRight, kItemsLeft, kItemsRight, kOverviewLeft, kOverviewRight, kPageCount };

struct Page {
  std::string title, body;  // TextMeshPro rich text
};

struct Book {
  Page pages[kPageCount];
  std::string evidenceTitle;  // the evidence page's heading
  std::string ghostsTitle;    // the ghost page's heading
  std::string ghostsLine;     // the line under it ("we believe the ghost is a")
};

Book write(const Facts& facts);

}  // namespace pg::ghostbook
