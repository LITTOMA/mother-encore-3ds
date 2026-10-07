#include "encore/audio_data.hpp"
#include "encore/field_scene_audio.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <set>
namespace encore::upstream {
namespace {
uint32_t u(const uint8_t *p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 |
         uint32_t(p[3]) << 24;
}
struct Reader {
  const uint8_t *p;
  size_t n, at = 128;
  bool ok = true;
  uint32_t integer() {
    if (!ok || at > n || n - at < 4) {
      ok = false;
      return 0;
    }
    auto v = u(p + at);
    at += 4;
    return v;
  }
  float scalar() {
    auto bits = integer();
    float v;
    std::memcpy(&v, &bits, 4);
    if (!std::isfinite(v))
      ok = false;
    return v;
  }
  std::string text() {
    auto len = integer();
    if (!ok || len > 2048 || at > n || len > n - at) {
      ok = false;
      return {};
    }
    std::string s(reinterpret_cast<const char *>(p + at), len);
    at += len;
    size_t i = 0;
    uint32_t cp;
    while (i < s.size())
      if (!utf8_next(s, i, cp) || cp < 32) {
        ok = false;
        break;
      }
    return s;
  }
  std::array<uint8_t, 32> hash() {
    std::array<uint8_t, 32> v{};
    if (!ok || at > n || n - at < 32) {
      ok = false;
      return v;
    }
    std::copy(p + at, p + at + 32, v.begin());
    at += 32;
    if (std::all_of(v.begin(), v.end(), [](auto b) { return !b; }))
      ok = false;
    return v;
  }
  bool boolean() {
    auto v = integer();
    if (v > 1)
      ok = false;
    return v != 0;
  }
};
bool path(std::string_view p) {
  return !p.empty() && p.front() != '/' && p.find("..") == p.npos &&
         p.find(':') == p.npos && p.find('\\') == p.npos;
}
bool same(const FieldIdentity &a, const FieldIdentity &b) {
  return a.scene_id == b.scene_id && a.upstream_commit == b.upstream_commit &&
         a.source_sha256 == b.source_sha256;
}
} // namespace
const FieldSceneAudioNode *FieldSceneAudioData::node(uint32_t id) const {
  for (const auto &x : nodes_)
    if (x.id == id)
      return &x;
  return nullptr;
}
const FieldSceneAudioStream *FieldSceneAudioData::stream(uint32_t id) const {
  for (const auto &x : streams_)
    if (x.id == id)
      return &x;
  return nullptr;
}
bool FieldSceneAudioData::load_file(const char *p, const FieldNodeTreeData &t,
                                    std::string &e) {
  if (!p) {
    e = "Scene audio path absent";
    return false;
  }
  std::ifstream f(p, std::ios::binary);
  if (!f) {
    e = "Scene audio file unavailable";
    return false;
  }
  f.seekg(0, std::ios::end);
  auto n = f.tellg();
  if (n < 128 || n > 2 * 1024 * 1024) {
    e = "Scene audio file size";
    return false;
  }
  f.seekg(0);
  std::vector<uint8_t> b(static_cast<size_t>(n));
  if (!f.read(reinterpret_cast<char *>(b.data()), n)) {
    e = "Scene audio file read";
    return false;
  }
  return load(b.data(), b.size(), t, e);
}
bool FieldSceneAudioData::load(const uint8_t *p, size_t n,
                               const FieldNodeTreeData &t, std::string &e) {
  auto fail = [&](const char *m) {
    e = m;
    return false;
  };
  if (!p || n < 128 || n > 2 * 1024 * 1024 || std::memcmp(p, "ENCSAUD1", 8) ||
      u(p + 8) != 1 || u(p + 12) != 128 || u(p + 16) != n ||
      u(p + 20) != audio_crc32(p + 128, n - 128) || u(p + 24) != 0x454e0067 ||
      u(p + 28) != 1 || u(p + 32) != 1 || !t.valid())
    return fail("Scene audio header/CRC/capability/source tree");
  FieldSceneAudioData d;
  d.identity_.scene_id = u(p + 36);
  std::copy(p + 40, p + 60, d.identity_.upstream_commit.begin());
  std::copy(p + 60, p + 92, d.identity_.source_sha256.begin());
  std::copy(p + 92, p + 124, d.ir_.begin());
  if (!same(d.identity_, t.identity()) || u(p + 124) ||
      std::all_of(d.ir_.begin(), d.ir_.end(), [](auto x) { return !x; }))
    return fail("Scene audio identity/reserved/IR");
  Reader r{p, n};
  d.scene_bank_ = r.text();
  d.bank_sha_ = r.hash();
  d.global_panning_ = r.scalar();
  auto nc = r.integer(), sc = r.integer();
  if (!path(d.scene_bank_) || d.global_panning_ < 0 || d.global_panning_ > 64 ||
      nc > 4096 || !nc || sc > 256 || !sc)
    return fail("Scene audio counts/bank/panning");
  std::set<uint32_t> ids, assets;
  for (uint32_t i = 0; i < sc; ++i) {
    FieldSceneAudioStream x;
    x.id = r.integer();
    x.asset_id = r.integer();
    x.bank = r.integer();
    x.source = r.text();
    x.native_class = r.text();
    x.source_sha = r.hash();
    if (!r.ok || !x.id || !x.asset_id || x.bank < 1 || x.bank > 2 ||
        !ids.insert(x.id).second || !assets.insert(x.asset_id).second ||
        !path(x.source) ||
        (x.native_class != "AudioStreamMP3" &&
         x.native_class != "AudioStreamSample"))
      return fail("Scene audio source stream");
    d.streams_.push_back(std::move(x));
  }
  ids.clear();
  std::set<uint32_t> actual;
  for (const auto &x : t.records()) {
    auto c =
        x.native_class.empty() ? t.classes().at(x.class_index) : x.native_class;
    if (c == "AudioStreamPlayer" || c == "AudioStreamPlayer2D")
      actual.insert(x.id);
  }
  for (uint32_t i = 0; i < nc; ++i) {
    FieldSceneAudioNode x;
    x.id = r.integer();
    x.kind = r.integer();
    x.stream = r.integer();
    x.mix_target = r.integer();
    x.area_mask = r.integer();
    x.autoplay = r.boolean();
    x.paused = r.boolean();
    x.path = r.text();
    x.bus = r.text();
    x.volume_db = r.scalar();
    x.pitch = r.scalar();
    x.max_distance = r.scalar();
    x.attenuation = r.scalar();
    x.panning = r.scalar();
    const auto *q = t.record(x.id);
    auto c = q ? (q->native_class.empty() ? t.classes().at(q->class_index)
                                          : q->native_class)
               : std::string{};
    if (!r.ok || !q || q->path != x.path || !q->script.empty() ||
        !ids.insert(x.id).second || x.kind < 1 || x.kind > 2 ||
        c != (x.kind == 1 ? "AudioStreamPlayer" : "AudioStreamPlayer2D") ||
        !path(x.path) || x.bus.empty() || x.mix_target != 0 || x.pitch <= 0 ||
        x.pitch > 64 || std::abs(x.volume_db) > 1024 ||
        (x.stream && !d.stream(x.stream)) ||
        (x.kind == 2 &&
         (x.max_distance <= 0 || x.attenuation < 0 || x.panning < 0)) ||
        (x.kind == 1 &&
         (x.area_mask || x.max_distance || x.attenuation || x.panning)))
      return fail("Scene audio node/native closure/properties");
    d.nodes_.push_back(std::move(x));
  }
  if (!r.ok || r.at != n || ids != actual)
    return fail("Scene audio complete native coverage/trailing bytes");
  d.valid_ = true;
  *this = std::move(d);
  return true;
}
} // namespace encore::upstream
