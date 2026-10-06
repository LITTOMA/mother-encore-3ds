#include "encore/field_dialogue_audio.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
namespace encore::upstream {
namespace {
uint32_t word(const uint8_t *p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 |
         uint32_t(p[3]) << 24;
}
struct Reader {
  const uint8_t *p;
  size_t n, at = 128;
  bool ok = true;
  uint32_t u() {
    if (!ok || at > n || n - at < 4) {
      ok = false;
      return 0;
    }
    auto v = word(p + at);
    at += 4;
    return v;
  }
  int32_t i() {
    auto v = u();
    int32_t s;
    std::memcpy(&s, &v, 4);
    return s;
  }
  float f() {
    auto v = u();
    float s;
    std::memcpy(&s, &v, 4);
    if (!std::isfinite(s) || std::abs(s) >= 1000000)
      ok = false;
    return s;
  }
  double d() {
    uint64_t v = u();
    v |= uint64_t(u()) << 32;
    double out;
    std::memcpy(&out, &v, 8);
    if (!std::isfinite(out) || std::abs(out) >= 1000000)
      ok = false;
    return out;
  }
  bool b() {
    auto v = u();
    if (v > 1)
      ok = false;
    return v != 0;
  }
  std::string t() {
    auto count = u();
    if (!ok || count > 65536 || at > n || count > n - at) {
      ok = false;
      return {};
    }
    std::string s(reinterpret_cast<const char *>(p + at), count);
    at += count;
    size_t c = 0;
    if (s.find('\0') != s.npos || !utf8_count(s, c))
      ok = false;
    return s;
  }
  std::array<uint8_t, 32> h() {
    std::array<uint8_t, 32> h{};
    if (!ok || at > n || n - at < 32) {
      ok = false;
      return h;
    }
    std::copy_n(p + at, 32, h.begin());
    at += 32;
    if (std::all_of(h.begin(), h.end(), [](uint8_t v) { return !v; }))
      ok = false;
    return h;
  }
};
bool path(std::string_view p) {
  if (p.empty() || p.front() == '/' || p.back() == '/' ||
      p.find('\\') != p.npos || p.find(':') != p.npos)
    return false;
  size_t begin = 0;
  while (begin < p.size()) {
    auto end = p.find('/', begin);
    if (end == p.npos)
      end = p.size();
    auto q = p.substr(begin, end - begin);
    if (q.empty() || q == "." || q == "..")
      return false;
    begin = end + 1;
  }
  return true;
}
} // namespace
const FieldDialogueAudioNode *FieldDialogueAudioData::node(uint32_t id) const {
  for (const auto &n : nodes_)
    if (n.id == id)
      return &n;
  return nullptr;
}
const FieldDialogueAudioAsset *
FieldDialogueAudioData::asset(uint32_t id) const {
  for (const auto &a : assets_)
    if (a.id == id)
      return &a;
  return nullptr;
}
const FieldDialogueAudioAsset *
FieldDialogueAudioData::asset(std::string_view source) const {
  for (const auto &a : assets_)
    if (a.source == source)
      return &a;
  return nullptr;
}
bool FieldDialogueAudioData::verify_bank(const AudioBank &bank,
                                         std::string &e) const {
  if (!valid_) {
    e = "Dialogue audio checked data missing";
    return false;
  }
  for (const auto &binding : assets_) {
    AudioAsset a;
    if (!bank.find(binding.id, a) || a.source_path != binding.source ||
        a.source_sha256 != binding.sha) {
      e = "Dialogue audio actual checked bank binding differs";
      return false;
    }
  }
  e.clear();
  return true;
}
bool FieldDialogueAudioData::load_file(const char *p, const FieldIdentity &id,
                                       std::string &e) {
  if (!p) {
    e = "Dialogue audio path missing";
    return false;
  }
  FILE *f = std::fopen(p, "rb");
  if (!f) {
    e = "Cannot open dialogue audio";
    return false;
  }
  if (std::fseek(f, 0, SEEK_END)) {
    std::fclose(f);
    e = "Dialogue audio seek rejected";
    return false;
  }
  auto n = std::ftell(f);
  if (n < 128 || n > 4 * 1024 * 1024 || std::fseek(f, 0, SEEK_SET)) {
    std::fclose(f);
    e = "Dialogue audio size rejected";
    return false;
  }
  std::vector<uint8_t> b(static_cast<size_t>(n));
  auto got = std::fread(b.data(), 1, b.size(), f);
  bool close = std::fclose(f) == 0;
  if (got != b.size() || !close) {
    e = "Dialogue audio read rejected";
    return false;
  }
  return load(b.data(), b.size(), id, e);
}
bool FieldDialogueAudioData::load(const uint8_t *p, size_t n,
                                  const FieldIdentity &id, std::string &e) {
  auto reject = [&](const char *s) {
    e = s;
    return false;
  };
  if (!p || n < 128 || n > 4 * 1024 * 1024)
    return reject("Dialogue audio size rejected");
  if (std::memcmp(p, "ENCFDAU1", 8) || word(p + 8) != 1 ||
      word(p + 12) != 128 || word(p + 16) != n || word(p + 24) != 0x454e0049 ||
      word(p + 28) != 1 || word(p + 32) != 3 || word(p + 36) != id.scene_id ||
      !id.scene_id || std::memcmp(p + 40, id.upstream_commit.data(), 20) ||
      std::memcmp(p + 60, id.source_sha256.data(), 32) || word(p + 124) ||
      audio_crc32(p + 128, n - 128) != word(p + 20) ||
      std::all_of(p + 92, p + 124, [](uint8_t v) { return !v; }))
    return reject("Dialogue audio identity/CRC/version/capability rejected");
  FieldDialogueAudioData d;
  d.identity_ = id;
  Reader r{p, n};
  d.recipe_ = r.h();
  double x = r.d(), y = r.d();
  d.pitch_ = {x, y};
  d.silence_ = r.f();
  d.fade_stop_ = r.u();
  d.fade_replace_ = r.u();
  d.prefix_ = r.t();
  d.extension_ = r.t();
  d.finished_ = r.t();
  if (!r.ok || x <= 0 || y < x || y > 4 || d.silence_ < -120 ||
      d.silence_ > 0 || !d.fade_stop_ || d.fade_stop_ > 4096 ||
      !d.fade_replace_ || d.fade_replace_ > 4096 ||
      d.prefix_.substr(0, 6) != "res://" || d.prefix_.back() != '/' ||
      !path(std::string_view(d.prefix_).substr(6, d.prefix_.size() - 7)) ||
      d.extension_.empty() || d.extension_.size() > 16 ||
      d.extension_.front() != '.' || d.finished_.empty())
    return reject("Dialogue audio source tuning rejected");
  std::set<uint32_t> ids;
  std::set<std::string> paths;
  uint32_t previous = 0;
  for (uint32_t i = 0; i < 3; ++i) {
    FieldDialogueAudioNode v;
    v.id = r.u();
    v.parent = r.u();
    v.ready = r.u();
    v.pause = r.u();
    v.stream = r.u();
    v.autoplay = r.b();
    v.paused = r.b();
    v.mix_target = r.u();
    v.priority = r.i();
    v.volume = r.f();
    v.pitch = r.f();
    v.path = r.t();
    v.bus = r.t();
    if (!r.ok || !v.id || !v.parent || v.id == v.parent ||
        v.parent != id.scene_id || !ids.insert(v.id).second ||
        !paths.insert(v.path).second || !path(v.path) ||
        v.path.find('/') != v.path.npos || v.pause > 2 || v.ready >= 47 ||
        (i && v.ready <= previous) || v.volume < -120 || v.volume > 24 ||
        v.pitch <= 0 || v.pitch > 4 || v.mix_target != 0 || v.bus.empty() ||
        v.autoplay || v.paused)
      return reject("Dialogue audio source node rejected");
    previous = v.ready;
    d.nodes_.push_back(v);
  }
  auto assets = r.u();
  if (!r.ok || !assets || assets > 64)
    return reject("Dialogue audio asset roster rejected");
  std::set<uint32_t> asset_ids;
  std::set<std::string> source_names;
  for (uint32_t i = 0; i < assets; ++i) {
    FieldDialogueAudioAsset a;
    a.id = r.u();
    a.source = r.t();
    a.sha = r.h();
    if (!r.ok || !a.id || !asset_ids.insert(a.id).second ||
        !source_names.insert(a.source).second ||
        a.source.substr(0, 6) != "res://" ||
        !path(std::string_view(a.source).substr(6)) ||
        a.source.size() < d.extension_.size() ||
        a.source.substr(a.source.size() - d.extension_.size()) != d.extension_)
      return reject("Dialogue audio source asset rejected");
    d.assets_.push_back(a);
  }
  for (const auto &v : d.nodes_)
    if (!d.asset(v.stream))
      return reject("Dialogue audio default stream missing");
  auto proofs = r.u();
  if (!r.ok || proofs < 4 || proofs > 256)
    return reject("Dialogue audio source proof count rejected");
  std::map<std::string, std::array<uint8_t, 32>> sources;
  for (uint32_t i = 0; i < proofs; ++i) {
    auto path_ = r.t();
    auto hash = r.h();
    if (!r.ok || !path(path_) || !sources.emplace(path_, hash).second)
      return reject("Dialogue audio source proof rejected");
  }
  for (const auto &a : d.assets_) {
    auto it = sources.find(a.source.substr(6));
    if (it == sources.end() || it->second != a.sha)
      return reject("Dialogue audio source asset proof differs");
  }
  r.h();
  r.h();
  r.h();
  if (!r.ok || r.at != n ||
      std::none_of(sources.begin(), sources.end(),
                   [&](const auto &v) { return v.second == id.source_sha256; }))
    return reject("Dialogue audio source/trailing bytes rejected");
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
} // namespace encore::upstream
