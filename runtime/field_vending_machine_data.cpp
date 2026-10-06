#include "encore/field_vending_machine.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <map>
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
      c = (c >> 1) ^ (0xedb88320u & uint32_t(-int(c & 1)));
  }
  return ~c;
}
bool path(const std::string &s) {
  return !s.empty() && s.size() < 1024 && s[0] != '/' &&
         s.find(':') == std::string::npos &&
         s.find('\\') == std::string::npos && s.find("..") == std::string::npos;
}
struct R {
  const uint8_t *p;
  size_t n, at = 64;
  bool ok = true;
  uint32_t u() {
    if (at > n || n - at < 4) {
      ok = false;
      return 0;
    }
    auto v = u32(p + at);
    at += 4;
    return v;
  }
  bool b() {
    auto v = u();
    if (v > 1)
      ok = false;
    return v == 1;
  }
  float f() {
    auto v = u();
    float x;
    std::memcpy(&x, &v, 4);
    if (!std::isfinite(x) || std::abs(x) > 1000000)
      ok = false;
    return x;
  }
  std::string s() {
    auto k = u();
    if (k > 8192 || at > n || k > n - at) {
      ok = false;
      return {};
    }
    std::string v(reinterpret_cast<const char *>(p + at), k);
    at += k;
    size_t i = 0;
    uint32_t cp;
    while (i < v.size())
      if (!encore::utf8_next(v, i, cp) || cp == 0 || (cp < 32 && cp != 10)) {
        ok = false;
        break;
      }
    return v;
  }
  void hash(std::array<uint8_t, 32> &v) {
    if (at > n || n - at < 32) {
      ok = false;
      return;
    }
    std::copy_n(p + at, 32, v.begin());
    at += 32;
    if (std::all_of(v.begin(), v.end(), [](auto b) { return b == 0; }))
      ok = false;
  }
};
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
} // namespace
bool FieldVendingData::load_file(const char *p, std::string &e) {
  if (!p)
    return fail(e, "Vending binary path missing");
  std::ifstream f(p, std::ios::binary);
  if (!f)
    return fail(e, "Vending pack unavailable");
  f.seekg(0, std::ios::end);
  auto size = f.tellg();
  if (size < 64 || size > 4 * 1024 * 1024)
    return fail(e, "Vending pack bounded length rejected");
  std::vector<uint8_t> b(size_t(size), 0);
  f.seekg(0);
  if (!f.read(reinterpret_cast<char *>(b.data()), size))
    return fail(e, "Vending pack read incomplete");
  return load(b.data(), b.size(), e);
}
bool FieldVendingData::load(const uint8_t *p, size_t n, std::string &e) {
  valid_ = false;
  if (!p || n < 64 || n > 4 * 1024 * 1024 || std::memcmp(p, "ENCFVM01", 8) ||
      u32(p + 8) != 1 || u32(p + 12) != n || u32(p + 16) != crc(p, n) ||
      u32(p + 20) != 1 || u32(p + 24) != 1 || u32(p + 28) != 1 || u32(p + 60))
    return fail(e, "Vending binary schema/capability/rules/CRC rejected");
  FieldVendingData d;
  std::copy_n(p + 32, 20, d.pin_.begin());
  d.scene_id_ = u32(p + 52);
  R r{p, n};
  r.hash(d.scene_);
  r.hash(d.script_);
  auto &v = d.descriptor_;
  v.id = r.u();
  v.ready = r.u();
  v.sprite = r.u();
  v.area = r.u();
  v.prompt = r.u();
  v.body = r.u();
  v.order = r.u();
  v.centered = r.b();
  v.visible = r.b();
  v.z = int32_t(r.u());
  v.z_relative = r.b();
  v.layer = r.u();
  v.mask = r.u();
  v.shape = r.u();
  v.root_position = {r.f(), r.f()};
  v.sprite_position = {r.f(), r.f()};
  v.offset = {r.f(), r.f()};
  for (auto &x : v.modulate) {
    x = r.f();
    if (x < 0 || x > 1)
      r.ok = false;
  }
  for (auto &x : v.self_modulate) {
    x = r.f();
    if (x < 0 || x > 1)
      r.ok = false;
  }
  auto &t = d.texture_;
  t.width = r.u();
  t.height = r.u();
  t.bytes = r.u();
  r.hash(t.sha);
  v.scene = r.s();
  v.node = r.s();
  v.shop = r.s();
  v.program = r.s();
  v.area_path = r.s();
  v.body_path = r.s();
  v.shape_path = r.s();
  v.script_source = r.s();
  if (!path(v.area_path) || !path(v.body_path) || !path(v.shape_path) ||
      !path(v.script_source))
    r.ok = false;
  t.source = r.s();
  t.path = r.s();
  auto count = r.u();
  if (!count || count != u32(p + 56) || count > 8192)
    r.ok = false;
  std::map<std::string, std::array<uint8_t, 32>> sources;
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    auto s = r.s();
    std::array<uint8_t, 32> h{};
    r.hash(h);
    if (!path(s) || !sources.emplace(s, h).second)
      r.ok = false;
  }
  auto a = sources.find(v.scene), b = sources.find(v.script_source);
  if (a == sources.end() || a->second != d.scene_ || b == sources.end() ||
      b->second != d.script_ || !sources.count(t.source) ||
      !sources.count(t.source + ".import"))
    r.ok = false;
  if (!v.id || !v.sprite || !v.area || !v.prompt || !v.body || !v.shape ||
      !d.scene_id_ || !path(v.scene) || !path(v.node) || !path(v.shop) ||
      !path(v.program) || !v.centered || !v.z_relative || !t.width ||
      !t.height || t.width > 1024 || t.height > 1024 || !t.bytes ||
      t.bytes > 16 * 1024 * 1024 || !path(t.path) || !path(t.source) ||
      std::all_of(d.pin_.begin(), d.pin_.end(), [](auto x) { return x == 0; }))
    r.ok = false;
  if (!r.ok || r.at != n)
    return fail(e, "Vending binary source/Sprite binding rejected");
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
} // namespace encore::upstream
