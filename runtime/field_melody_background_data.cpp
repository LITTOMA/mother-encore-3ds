#include "encore/field_melody_background.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
namespace encore::upstream {
namespace {
uint32_t u32(const uint8_t *p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 |
         uint32_t(p[3]) << 24;
}
uint32_t crc(const uint8_t *p, size_t n) {
  uint32_t c = ~0u;
  for (size_t i = 0; i < n; ++i) {
    c ^= i >= 16 && i < 20 ? 0 : p[i];
    for (unsigned j = 0; j < 8; ++j)
      c = (c >> 1) ^ (0xedb88320u & uint32_t(-int32_t(c & 1)));
  }
  return ~c;
}
struct Reader {
  const uint8_t *p;
  size_t n, at = 64;
  bool ok = true;
  uint32_t integer() {
    if (!ok || at > n || n - at < 4) {
      ok = false;
      return 0;
    }
    auto v = u32(p + at);
    at += 4;
    return v;
  }
  bool boolean() {
    auto v = integer();
    if (v > 1)
      ok = false;
    return v != 0;
  }
  float scalar() {
    auto u = integer();
    float v;
    std::memcpy(&v, &u, 4);
    if (!std::isfinite(v) || std::abs(v) > 1e6)
      ok = false;
    return v;
  }
  Vec2 point() { return {scalar(), scalar()}; }
  std::string text() {
    auto len = integer();
    if (!ok || len > 8192 || at > n || len > n - at) {
      ok = false;
      return {};
    }
    std::string v(reinterpret_cast<const char *>(p + at), len);
    at += len;
    size_t i = 0;
    uint32_t cp;
    while (i < v.size())
      if (!encore::utf8_next(v, i, cp) || cp < 32) {
        ok = false;
        break;
      }
    return v;
  }
  std::array<uint8_t, 32> hash() {
    std::array<uint8_t, 32> h{};
    if (!ok || at > n || n - at < h.size()) {
      ok = false;
      return h;
    }
    std::copy(p + at, p + at + h.size(), h.begin());
    at += h.size();
    if (std::all_of(h.begin(), h.end(), [](uint8_t b) { return !b; }))
      ok = false;
    return h;
  }
};
bool path(std::string_view p) {
  return !p.empty() && p.front() != '/' && p.back() != '/' &&
         p.find("..") == p.npos && p.find(':') == p.npos &&
         p.find('\\') == p.npos;
}
bool symbol(std::string_view s) {
  if (s.empty())
    return false;
  for (auto c : s)
    if (!(c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9')))
      return false;
  return true;
}
} // namespace

const FieldMelodyBinding *
FieldMelodyBackgroundData::binding(uint32_t id) const {
  for (const auto &v : bindings_)
    if (v.id == id)
      return &v;
  return nullptr;
}
bool FieldMelodyBackgroundData::source_hash(
    std::string_view p, std::array<uint8_t, 32> &out) const {
  auto i = sources_.find(std::string(p));
  if (i == sources_.end())
    return false;
  out = i->second;
  return true;
}
bool FieldMelodyBackgroundData::load_file(const char *p, std::string &e) {
  if (!p) {
    e = "Melody file path absent";
    return false;
  }
  std::ifstream f(p, std::ios::binary);
  if (!f) {
    e = "Melody file unavailable";
    return false;
  }
  f.seekg(0, std::ios::end);
  auto n = f.tellg();
  if (n < 64 || n > 4 * 1024 * 1024) {
    e = "Melody file size";
    return false;
  }
  f.seekg(0);
  std::vector<uint8_t> b(static_cast<size_t>(n));
  if (!f.read(reinterpret_cast<char *>(b.data()), n)) {
    e = "Melody file read";
    return false;
  }
  return load(b.data(), b.size(), e);
}
bool FieldMelodyBackgroundData::load(const uint8_t *p, size_t n,
                                     std::string &e) {
  auto fail = [&](const char *m) {
    e = m;
    return false;
  };
  if (!p || n < 64 || n > 4 * 1024 * 1024)
    return fail("Melody pack size");
  if (std::memcmp(p, "ENCMLB01", 8) || u32(p + 8) != 1 || u32(p + 12) != n ||
      u32(p + 20) != 1 || u32(p + 24) != 1 || u32(p + 28) != 1 ||
      u32(p + 16) != crc(p, n))
    return fail("Melody header/version/capability/rules/CRC");
  for (size_t i = 52; i < 64; ++i)
    if (p[i])
      return fail("Melody reserved header");
  FieldMelodyBackgroundData d;
  std::copy(p + 32, p + 52, d.pin_.begin());
  if (std::all_of(d.pin_.begin(), d.pin_.end(), [](uint8_t b) { return !b; }))
    return fail("Melody source pin absent");
  Reader r{p, n};
  d.scene_ = r.text();
  d.script_ = r.text();
  d.shader_ = r.text();
  if (!path(d.scene_) || !path(d.script_) || !path(d.shader_))
    return fail("Melody source identities");
  auto color = [&]() {
    BattleValue v{r.scalar(), r.scalar(), r.scalar(), r.scalar()};
    if (v.x < 0 || v.x > 1 || v.y < 0 || v.y > 1 || v.z < 0 || v.z > 1 ||
        v.w < 0 || v.w > 1)
      r.ok = false;
    return v;
  };
  auto &c = d.policy_;
  c.source_viewport = r.point();
  c.native_viewport = r.point();
  c.rect = {r.scalar(), r.scalar(), r.scalar(), r.scalar()};
  c.ready_alpha = r.scalar();
  for (auto &v : c.vertical)
    v = r.scalar();
  c.clip = r.text();
  c.length = r.scalar();
  if (!r.ok || !symbol(c.clip) || c.source_viewport.x <= 0 ||
      c.source_viewport.y <= 0 || c.native_viewport.x < c.source_viewport.x ||
      c.native_viewport.y < c.source_viewport.y || c.native_viewport.x > 1024 ||
      c.native_viewport.y > 1024 ||
      c.rect.z - c.rect.x != c.source_viewport.x ||
      c.rect.w - c.rect.y != c.source_viewport.y || c.ready_alpha < 0 ||
      c.ready_alpha > 1 || c.length <= 0 || c.length > 3600 ||
      c.vertical[0] < 0 || c.vertical[0] > 1 || c.vertical[6] <= 0)
    return fail("Melody source viewport/shader/clip policy");
  for (float extent : {c.source_viewport.x, c.source_viewport.y,
                       c.native_viewport.x, c.native_viewport.y})
    if (std::floor(extent) != extent)
      return fail("Melody viewport requires source pixels 1:1");
  for (auto v : c.vertical)
    if (std::abs(v) > 100)
      return fail("Melody shader bounded capability");
  auto count = r.integer();
  if (!count || count > 128)
    return fail("Melody color key count");
  float last = -1;
  for (uint32_t i = 0; i < count; ++i) {
    FieldMelodyKey k;
    k.time = r.scalar();
    k.color = color();
    if (!r.ok || k.time <= last || k.time >= c.length ||
        (i == 0 && k.time != 0))
      return fail("Melody source color key/order");
    last = k.time;
    c.keys.push_back(k);
  }
  c.fade_in.duration = r.scalar();
  c.fade_in.from = color();
  c.fade_in.to = color();
  c.fade_in.explicit_from = true;
  c.fade_out.duration = r.scalar();
  c.fade_out.to = color();
  if (!r.ok || c.fade_in.duration <= 0 || c.fade_in.duration > 60 ||
      c.fade_out.duration <= 0 || c.fade_out.duration > 60)
    return fail("Melody source fade tuning");
  auto &a = d.asset_;
  a.source = r.text();
  a.path = r.text();
  a.tile_width = r.integer();
  a.tile_height = r.integer();
  a.width = r.integer();
  a.height = r.integer();
  a.bytes = r.integer();
  a.crc = r.integer();
  if (!r.ok || !path(a.source) || !path(a.path) ||
      a.path.rfind("graphics/", 0) != 0 || !a.tile_width || !a.tile_height ||
      a.tile_width > 1024 || a.tile_height > 1024 || a.width != a.tile_width ||
      a.height < c.native_viewport.y || a.height > 1024 ||
      a.height % a.tile_height || !a.bytes || a.bytes > 4 * 1024 * 1024)
    return fail("Melody genuine prepared tile asset");
  count = r.integer();
  if (count != u32(p + 28))
    return fail("Melody source instance count");
  std::set<uint32_t> ids;
  for (uint32_t i = 0; i < count; ++i) {
    FieldMelodyBinding b;
    b.id = r.integer();
    b.ready_ordinal = r.integer();
    b.bg_id = r.integer();
    b.bg_ready = r.integer();
    b.animation_id = r.integer();
    b.animation_ready = r.integer();
    b.node = r.text();
    b.position = r.point();
    b.initial_modulate = color();
    b.initial_self = color();
    if (!r.ok || !b.id || !b.bg_id || !b.animation_id ||
        !ids.insert(b.id).second || !ids.insert(b.bg_id).second ||
        !ids.insert(b.animation_id).second ||
        uint64_t(b.bg_ready) + 1 != b.animation_ready ||
        uint64_t(b.animation_ready) + 1 != b.ready_ordinal || !path(b.node))
      return fail("Melody source lifecycle bindings");
    d.bindings_.push_back(std::move(b));
  }
  count = r.integer();
  if (!count || count > 2048)
    return fail("Melody source closure count");
  for (uint32_t i = 0; i < count; ++i) {
    auto name = r.text();
    auto hash = r.hash();
    if (!r.ok || !path(name) ||
        !d.sources_.emplace(std::move(name), hash).second)
      return fail("Melody source closure identity/hash");
  }
  for (const auto *name : {&d.scene_, &d.script_, &d.shader_, &a.source})
    if (!d.sources_.count(*name))
      return fail("Melody source closure missing");
  if (!r.ok || r.at != n)
    return fail("Melody trailing bytes");
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
} // namespace encore::upstream
