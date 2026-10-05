#include "encore/field_interact_dialog.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <set>
namespace encore::upstream {
namespace {
uint32_t integer(const uint8_t *p) {
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
bool path(std::string_view p) {
  return !p.empty() && p.front() != '/' && p.back() != '/' &&
         p.find("..") == p.npos && p.find(':') == p.npos &&
         p.find('\\') == p.npos;
}
struct Reader {
  const uint8_t *p;
  size_t n, at = 64;
  bool ok = true;
  uint32_t value() {
    if (!ok || at > n || n - at < 4) {
      ok = false;
      return 0;
    }
    auto v = integer(p + at);
    at += 4;
    return v;
  }
  float scalar() {
    auto v = value();
    float f;
    std::memcpy(&f, &v, 4);
    if (!std::isfinite(f) || std::abs(f) > 1e6)
      ok = false;
    return f;
  }
  std::string text() {
    auto size = value();
    if (!ok || size > 8192 || at > n || size > n - at) {
      ok = false;
      return {};
    }
    std::string s(reinterpret_cast<const char *>(p + at), size);
    at += size;
    size_t pos = 0;
    uint32_t cp;
    while (pos < s.size())
      if (!encore::utf8_next(s, pos, cp) || cp < 32) {
        ok = false;
        break;
      }
    return s;
  }
  std::array<uint8_t, 32> hash() {
    std::array<uint8_t, 32> h{};
    if (!ok || at > n || n - at < 32) {
      ok = false;
      return h;
    }
    std::copy(p + at, p + at + 32, h.begin());
    at += 32;
    if (std::all_of(h.begin(), h.end(), [](uint8_t v) { return !v; }))
      ok = false;
    return h;
  }
};
} // namespace
const FieldInteractDescriptor *FieldInteractData::record(uint32_t id) const {
  for (const auto &r : records_)
    if (r.id == id)
      return &r;
  return nullptr;
}
bool FieldInteractData::source_hash(std::string_view p,
                                    std::array<uint8_t, 32> &h) const {
  auto it = sources_.find(std::string(p));
  if (it == sources_.end())
    return false;
  h = it->second;
  return true;
}
bool FieldInteractData::load_file(const char *p, std::string &e) {
  if (!p) {
    e = "InteractDialog file path absent";
    return false;
  }
  std::ifstream f(p, std::ios::binary);
  if (!f) {
    e = "InteractDialog pack unavailable";
    return false;
  }
  f.seekg(0, std::ios::end);
  const auto n = f.tellg();
  if (n < 64 || n > 4 * 1024 * 1024) {
    e = "InteractDialog bounded file size";
    return false;
  }
  f.seekg(0);
  std::vector<uint8_t> b(static_cast<size_t>(n));
  if (!f.read(reinterpret_cast<char *>(b.data()), n)) {
    e = "InteractDialog file read rejected";
    return false;
  }
  return load(b.data(), b.size(), e);
}
bool FieldInteractData::load(const uint8_t *p, size_t n, std::string &e) {
  auto fail = [&](const char *m) {
    e = m;
    return false;
  };
  if (!p || n < 64 || n > 4 * 1024 * 1024)
    return fail("InteractDialog bounded pack size");
  if (std::memcmp(p, "ENCFDLG1", 8) || integer(p + 8) != 1 ||
      integer(p + 12) != n || integer(p + 20) != 1 || integer(p + 24) != 1 ||
      !integer(p + 28) || integer(p + 16) != crc(p, n))
    return fail("InteractDialog magic/version/capability/rules/CRC");
  for (size_t i = 52; i < 64; ++i)
    if (p[i])
      return fail("InteractDialog reserved header");
  FieldInteractData d;
  d.scene_id_ = integer(p + 28);
  std::copy(p + 32, p + 52, d.pin_.begin());
  if (std::all_of(d.pin_.begin(), d.pin_.end(), [](uint8_t v) { return !v; }))
    return fail("InteractDialog source pin absent");
  Reader r{p, n};
  d.scene_ = r.text();
  d.script_ = r.text();
  if (!r.ok || !path(d.scene_) || !path(d.script_))
    return fail("InteractDialog source paths");
  auto count = r.value();
  if (!count || count > 16384)
    return fail("InteractDialog source count");
  for (uint32_t i = 0; i < count; ++i) {
    auto name = r.text();
    auto hash = r.hash();
    if (!r.ok || !path(name) || !d.sources_.emplace(name, hash).second)
      return fail("InteractDialog source receipt");
  }
  if (!d.sources_.count(d.scene_) || !d.sources_.count(d.script_))
    return fail("InteractDialog incomplete source closure");
  count = r.value();
  if (!count || count > 4096)
    return fail("InteractDialog instance count");
  std::set<uint32_t> ids, ordinals;
  std::set<std::string> names;
  uint32_t previous = 0;
  for (uint32_t i = 0; i < count; ++i) {
    FieldInteractDescriptor v;
    v.id = r.value();
    v.ready = r.value();
    v.prompt = r.value();
    v.flags = r.value();
    v.button_offset = {r.scalar(), r.scalar()};
    v.node = r.text();
    v.dialogue = r.text();
    v.thoughts = r.text();
    v.key_item = r.text();
    v.appear = r.text();
    v.disappear = r.text();
    auto choices = r.value();
    if (!r.ok || !v.id || !v.prompt || v.flags > 31 ||
        !ids.insert(v.id).second || !ids.insert(v.prompt).second ||
        !ordinals.insert(v.ready).second || (i && v.ready <= previous) ||
        !path(v.node) || !names.insert(v.node).second || choices > 4096)
      return fail("InteractDialog instance/Ready/geometry");
    previous = v.ready;
    auto programme = [&](const std::string &s) {
      return s.empty() ||
             (path(s) && d.sources_.count("Data/Dialogue/" + s + ".yaml"));
    };
    if (!programme(v.dialogue) || !programme(v.thoughts) ||
        (!v.key_item.empty() &&
         (!path(v.key_item) ||
          !d.sources_.count("Data/Items/" + v.key_item + ".yaml"))))
      return fail("InteractDialog programme/item source closure");
    for (uint32_t j = 0; j < choices; ++j) {
      auto flag = r.text();
      auto p = r.text();
      if (!r.ok || !programme(p))
        return fail("InteractDialog source choice programme");
      v.choices.push_back({flag, p});
    }
    d.records_.push_back(std::move(v));
  }
  if (!r.ok || r.at != n)
    return fail("InteractDialog trailing/short payload");
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
} // namespace encore::upstream