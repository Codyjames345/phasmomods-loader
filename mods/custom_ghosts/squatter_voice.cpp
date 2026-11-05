#include "squatter_voice.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

#include "UnityAudio.h"
#include "game_api.h"
#include "held.h"
#include "log.h"
#include "runtime.h"
#include "sound.h"
#include "wav.h"

namespace pm::custom::squatter::voice {
namespace {

namespace fs = std::filesystem;

constexpr int kSetCount = static_cast<int>(Lines::Friendly) + 1;
constexpr std::uintmax_t kMaxFileBytes = 64u * 1024u * 1024u;  // a longer file is not a spoken line

struct Line {
  api::Held clip;  // null when the file could not become a clip: the slot stays, silent
};

struct Set {
  std::vector<Line> lines;
  int last = -1;  // the index picked last, so a line is not repeated
};

Set g_sets[kSetCount];

Set* set_of(Lines set) {
  const int i = static_cast<int>(set);
  return i >= 0 && i < kSetCount ? &g_sets[i] : nullptr;
}

bool is_wav(const fs::path& p) {
  std::string ext = p.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return ext == ".wav";
}

/** The clip for one file, or null with the reason logged. */
void* build(const fs::path& file) {
  const std::string name = file.filename().string();
  std::error_code ec;
  const std::uintmax_t size = fs::file_size(file, ec);
  if (ec || size > kMaxFileBytes) {
    PM_LOG("squatter: voice file %s skipped: %s", name.c_str(), ec ? "unreadable" : "too large");
    return nullptr;
  }
  std::ifstream in(file, std::ios::binary);
  const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (bytes.size() != size) {
    PM_LOG("squatter: voice file %s skipped: could not be read", name.c_str());
    return nullptr;
  }
  wav::Pcm pcm;
  std::string why;
  if (!wav::decode(bytes, &pcm, &why)) {
    PM_LOG("squatter: voice file %s skipped: %s", name.c_str(), why.c_str());
    return nullptr;
  }
  void* samples = phasmo::Single::type.make_array(pcm.samples.size());
  const api::Array<float> data{samples};
  if (!samples || data.size() < pcm.samples.size()) {
    PM_LOG("squatter: voice file %s skipped: no sample array", name.c_str());
    return nullptr;
  }
  void* clip = phasmo::AudioClip::Create(api::make_string("Squatter voice"), static_cast<int>(pcm.frames()),
                                         pcm.channels, pcm.frequency, false);
  if (!clip) {
    PM_LOG("squatter: voice file %s skipped: the game would not make a clip", name.c_str());
    return nullptr;
  }
  std::memcpy(&data[0], pcm.samples.data(), pcm.samples.size() * sizeof(float));
  phasmo::AudioClip::SetData(clip, samples, 0);
  return clip;
}

}  // namespace

std::vector<fs::path> list_wavs(const fs::path& folder) {
  std::vector<fs::path> files;
  std::error_code ec;
  for (fs::directory_iterator it(folder, ec), end; !ec && it != end; it.increment(ec)) {
    std::error_code kind;
    if (it->is_regular_file(kind) && is_wav(it->path())) files.push_back(it->path());
  }
  std::sort(files.begin(), files.end(), [](const fs::path& a, const fs::path& b) { return a.filename().string() < b.filename().string(); });
  return files;
}

bool load(const fs::path& folder) {
  bool any = false;
  for (int i = 0; i < kSetCount; ++i) {
    Set& set = g_sets[i];
    set.lines.clear();
    set.last = -1;
    for (const fs::path& file : list_wavs(folder / folder_name(static_cast<Lines>(i)))) {
      Line line;
      line.clip = build(file);
      any = any || line.clip.get() != nullptr;
      set.lines.push_back(std::move(line));
    }
  }
  return any;
}

int count(Lines set) {
  const Set* s = set_of(set);
  return s ? static_cast<int>(s->lines.size()) : 0;
}

int play(Lines set, phasmo::Vector3 at, uint32_t roll) {
  Set* s = set_of(set);
  if (!s) return -1;
  const int index = pick_line(static_cast<int>(s->lines.size()), s->last, roll);
  if (index < 0) return -1;
  s->last = index;
  play_index(set, index, at);
  return index;
}

void play_index(Lines set, int index, phasmo::Vector3 at) {
  const Set* s = set_of(set);
  if (!s || index < 0 || index >= static_cast<int>(s->lines.size())) return;
  void* clip = s->lines[static_cast<size_t>(index)].clip;
  if (!clip) return;
  sound::play_at(clip, at, 1.0f);
}

}  // namespace pm::custom::squatter::voice
