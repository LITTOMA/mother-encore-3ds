#include "encore/field_stepping_sounds.hpp"
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

const FieldSteppingBinding *
FieldSteppingSoundsData::binding(uint32_t id) const {
  for (const auto &v : bindings_)
    if (v.id == id)
      return &v;
  return nullptr;
}
bool FieldSteppingSoundsData::source_hash(std::string_view p,
                                          std::array<uint8_t, 32> &out) const {
  auto i = sources_.find(std::string(p));
  if (i == sources_.end())
    return false;
  out = i->second;
  return true;
}
bool FieldSteppingSoundsData::load_file(const char *p, std::string &e) {
  if (!p) {
    e = "Stepping file path absent";
    return false;
  }
  std::ifstream f(p, std::ios::binary);
  if (!f) {
    e = "Stepping file unavailable";
    return false;
  }
  f.seekg(0, std::ios::end);
  auto n = f.tellg();
  if (n < 64 || n > 4 * 1024 * 1024) {
    e = "Stepping file size";
    return false;
  }
  f.seekg(0);
  std::vector<uint8_t> b(static_cast<size_t>(n));
  if (!f.read(reinterpret_cast<char *>(b.data()), n)) {
    e = "Stepping file read";
    return false;
  }
  return load(b.data(), b.size(), e);
}
bool FieldSteppingSoundsData::load(const uint8_t *p, size_t n, std::string &e) {
  auto fail = [&](const char *m) {
    e = m;
    return false;
  };
  if (!p || n < 64 || n > 4 * 1024 * 1024)
    return fail("Stepping pack size");
  if (std::memcmp(p, "ENCSTP01", 8) || u32(p + 8) != 1 || u32(p + 12) != n ||
      u32(p + 20) != 1 || u32(p + 24) != 1 || u32(p + 28) != 3 ||
      u32(p + 16) != crc(p, n))
    return fail("Stepping header/version/capabilities/rules/CRC");
  for (size_t i = 52; i < 64; ++i)
    if (p[i])
      return fail("Stepping reserved header");
  FieldSteppingSoundsData d;
  std::copy(p + 32, p + 52, d.pin_.begin());
  if (std::all_of(d.pin_.begin(), d.pin_.end(), [](uint8_t b) { return !b; }))
    return fail("Stepping source pin absent");
  Reader r{p, n};
  d.scene_ = r.text();
  d.script_ = r.text();
  if (!path(d.scene_) || !path(d.script_))
    return fail("Stepping source paths");
  auto count = r.integer();
  if (count != 2)
    return fail("Stepping signal count");
  std::set<std::string> names;
  for (uint32_t i = 0; i < count; ++i) {
    FieldSteppingConnection c;
    c.role = r.integer();
    c.signal = r.text();
    c.method = r.text();
    if (!r.ok || c.role != i + 1 || !symbol(c.signal) || !symbol(c.method) ||
        !names.insert(c.signal).second)
      return fail("Stepping signal identity/role");
    d.connections_.push_back(std::move(c));
  }
  count = r.integer();
  if (!count || count > 8)
    return fail("Stepping sound count");
  std::set<std::string> sounds;
  for (uint32_t i = 0; i < count; ++i) {
    FieldSteppingSound s;
    s.name = r.text();
    s.path = r.text();
    if (!r.ok || !symbol(s.name) || !path(s.path) ||
        !sounds.insert(s.name).second)
      return fail("Stepping sound identity");
    d.sounds_.push_back(std::move(s));
  }
  count = r.integer();
  if (!count || count > 3)
    return fail("Stepping effect count");
  std::set<std::string> effects;
  for (uint32_t i = 0; i < count; ++i) {
    FieldSteppingEffect s;
    s.name = r.text();
    s.scene = r.text();
    s.texture = r.text();
    s.frames = r.integer();
    s.fps = r.scalar();
    s.behind = r.boolean();
    if (!r.ok || !symbol(s.name) || !path(s.scene) || !path(s.texture) ||
        !s.frames || s.frames > 64 || s.fps <= 0 || s.fps > 120 ||
        !effects.insert(s.name).second)
      return fail("Stepping effect schema");
    d.effects_.push_back(std::move(s));
  }
  count = r.integer();
  if (count != u32(p + 28))
    return fail("Stepping instance count");
  std::set<uint32_t> ids, ordinals;
  names.clear();
  uint32_t previous = 0;
  for (uint32_t i = 0; i < count; ++i) {
    FieldSteppingBinding b;
    b.id = r.integer();
    b.ready_ordinal = r.integer();
    b.layer = r.integer();
    b.mask = r.integer();
    b.flags = r.integer();
    b.enabled = r.boolean();
    b.node = r.text();
    b.entering_sound = r.text();
    b.exiting_sound = r.text();
    b.enter_shadow_effect = r.text();
    b.exit_shadow_effect = r.text();
    auto owners = r.integer();
    if (!r.ok || !b.id || !ids.insert(b.id).second ||
        !ordinals.insert(b.ready_ordinal).second ||
        (i && b.ready_ordinal <= previous) || b.flags > 3 || !path(b.node) ||
        !names.insert(b.node).second || !owners || owners > 256 ||
        !sounds.count(b.entering_sound) ||
        (!b.exiting_sound.empty() && !sounds.count(b.exiting_sound)) ||
        !effects.count(b.enter_shadow_effect) ||
        (!b.exit_shadow_effect.empty() && !effects.count(b.exit_shadow_effect)))
      return fail("Stepping instance identity/reference");
    for (uint32_t j = 0; j < owners; ++j) {
      FieldSteppingShape s;
      s.id = r.integer();
      s.order = r.integer();
      s.disabled = r.boolean();
      s.node = r.text();
      auto parts = r.integer();
      if (!r.ok || !s.id || !ids.insert(s.id).second || s.order != j ||
          !path(s.node) || !names.insert(s.node).second || !parts ||
          parts > 1024)
        return fail("Stepping shape owner/order");
      for (uint32_t k = 0; k < parts; ++k) {
        auto vertices = r.integer();
        if (vertices < 3 || vertices > 128)
          return fail("Stepping convex count");
        std::vector<Vec2> v;
        for (uint32_t l = 0; l < vertices; ++l)
          v.push_back(r.point());
        if (!r.ok)
          return fail("Stepping finite geometry");
        bool pos = false, neg = false;
        for (uint32_t l = 0; l < vertices; ++l) {
          auto a = v[l], x = v[(l + 1) % vertices], y = v[(l + 2) % vertices];
          double c = (double(x.x) - a.x) * (double(y.y) - x.y) -
                     (double(x.y) - a.y) * (double(y.x) - x.x);
          pos |= c > 0;
          neg |= c < 0;
        }
        if (pos == neg)
          return fail("Stepping official decomposition convexity");
        s.parts.push_back(std::move(v));
      }
      b.shapes.push_back(std::move(s));
    }
    previous = b.ready_ordinal;
    d.bindings_.push_back(std::move(b));
  }
  count = r.integer();
  if (!count || count > 2048)
    return fail("Stepping source closure count");
  for (uint32_t i = 0; i < count; ++i) {
    auto name = r.text();
    auto h = r.hash();
    if (!r.ok || !path(name) || !d.sources_.emplace(std::move(name), h).second)
      return fail("Stepping source closure identity/hash");
  }
  if (!d.sources_.count(d.scene_) || !d.sources_.count(d.script_))
    return fail("Stepping source closure missing");
  for (const auto &s : d.sounds_)
    if (!d.sources_.count(s.path) || !d.sources_.count(s.path + ".import"))
      return fail("Stepping genuine audio source binding");
  for (const auto &s : d.effects_)
    if (!d.sources_.count(s.scene) || !d.sources_.count(s.texture))
      return fail("Stepping genuine shadow source binding");
  if (!r.ok || r.at != n)
    return fail("Stepping trailing bytes");
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
} // namespace encore::upstream
