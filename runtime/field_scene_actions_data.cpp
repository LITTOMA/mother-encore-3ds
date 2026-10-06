#include "encore/field_scene_actions.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
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
// Hash presence only; engine/upstream digests are checked by the offline
// producer.
bool read_hash(Reader &r) {
  r.hash();
  return r.ok;
}

} // namespace
const FieldSceneActionBinding *
FieldSceneActionsData::binding(uint32_t id) const {
  for (const auto &v : bindings_)
    if (v.id == id)
      return &v;
  return nullptr;
}
const FieldSceneActionReference *
FieldSceneActionsData::reference(uint32_t id) const {
  for (const auto &v : references_)
    if (v.id == id)
      return &v;
  return nullptr;
}
bool FieldSceneActionsData::load_file(const char *p, std::string &e) {
  if (!p) {
    e = "Scene actions file path absent";
    return false;
  }
  std::ifstream f(p, std::ios::binary);
  if (!f) {
    e = "Scene actions file unavailable";
    return false;
  }
  f.seekg(0, std::ios::end);
  auto n = f.tellg();
  if (n < 64 || n > 4 * 1024 * 1024) {
    e = "Scene actions file size";
    return false;
  }
  f.seekg(0);
  std::vector<uint8_t> b(static_cast<size_t>(n));
  if (!f.read(reinterpret_cast<char *>(b.data()), n)) {
    e = "Scene actions file read";
    return false;
  }
  return load(b.data(), b.size(), e);
}
bool FieldSceneActionsData::source_hash(std::string_view p,
                                        std::array<uint8_t, 32> &out) const {
  auto i = sources_.find(std::string(p));
  if (i == sources_.end())
    return false;
  out = i->second;
  return true;
}
bool FieldSceneActionsData::load(const uint8_t *p, size_t n, std::string &e) {
  auto reject = [&](const char *m) {
    e = m;
    return false;
  };
  if (!p || n < 64 || n > 4 * 1024 * 1024)
    return reject("Scene actions pack size");
  if (std::memcmp(p, "ENCSAC01", 8) || u32(p + 8) != 1 || u32(p + 12) != n ||
      u32(p + 20) != 1 || u32(p + 24) != 1 || !u32(p + 28) ||
      u32(p + 28) > 4096 || u32(p + 16) != crc(p, n))
    return reject("Scene actions header/version/capability/rules/CRC");
  for (size_t i = 52; i < 64; ++i)
    if (p[i])
      return reject("Scene actions reserved header");
  FieldSceneActionsData d;
  std::copy(p + 32, p + 52, d.pin_.begin());
  if (std::all_of(d.pin_.begin(), d.pin_.end(), [](uint8_t b) { return !b; }))
    return reject("Scene actions source pin absent");
  Reader r{p, n};
  d.scene_ = r.text();
  if (!r.ok || !path(d.scene_))
    return reject("Scene actions source scene");
  auto count = r.integer();
  if (count != 2)
    return reject("Scene actions script capability");
  std::set<std::string> names;
  for (uint32_t i = 0; i < count; ++i) {
    auto s = r.text();
    if (!r.ok || !path(s) || !names.insert(s).second)
      return reject("Scene actions script identity");
    d.scripts_.push_back(s);
  }
  d.method_ = r.text();
  count = r.integer();
  if (!symbol(d.method_) || count != 2)
    return reject("Scene actions deferred method/property capability");
  names.clear();
  for (uint32_t i = 0; i < count; ++i) {
    auto name = r.text();
    if (!r.ok || !symbol(name) || !names.insert(name).second)
      return reject("Scene actions property role identity");
    d.properties_.push_back(name);
  }
  count = r.integer();
  if (count != 2)
    return reject("Scene actions connection count");
  for (uint32_t i = 0; i < count; ++i) {
    FieldSceneActionConnection c;
    c.kind = r.integer();
    c.signal = r.text();
    c.method = r.text();
    if (!r.ok || c.kind != i + 1 || !symbol(c.signal) || !symbol(c.method))
      return reject("Scene actions source signal role");
    d.connections_.push_back(c);
  }
  count = r.integer();
  if (!count || count > 4096)
    return reject("Scene action reference count");
  std::set<uint32_t> ids;
  names.clear();
  for (uint32_t i = 0; i < count; ++i) {
    FieldSceneActionReference v;
    v.id = r.integer();
    v.kind = r.integer();
    v.parent = r.integer();
    v.mask = r.integer();
    v.layer = r.integer();
    v.collision_properties = r.boolean();
    v.descendants = r.integer();
    v.cameras = r.integer();
    v.viewports = r.integer();
    v.node = r.text();
    v.script = r.text();
    v.position = r.point();
    v.scale = r.point();
    v.rotation = r.scalar();
    for (auto &f : v.world)
      f = r.scalar();
    if (!r.ok || !v.id || !v.parent || !ids.insert(v.id).second || v.kind < 1 ||
        v.kind > 4 || v.collision_properties != (v.kind == 1 || v.kind == 3) ||
        !path(v.node) || !names.insert(v.node).second ||
        (!v.script.empty() && !path(v.script)) || v.scale.x <= 0 ||
        v.scale.y <= 0 || v.descendants > 65535 || v.cameras > v.descendants ||
        v.viewports > v.descendants)
      return reject("Scene actions source reference/class/geometry");
    d.references_.push_back(std::move(v));
  }
  count = r.integer();
  if (count != u32(p + 28))
    return reject("Scene actions source object count");
  ids.clear();
  names.clear();
  std::set<uint32_t> ordinals;
  uint32_t previous = 0;
  for (uint32_t i = 0; i < count; ++i) {
    FieldSceneActionBinding b;
    b.id = r.integer();
    b.kind = r.integer();
    b.ready_ordinal = r.integer();
    b.layer = r.integer();
    b.mask = r.integer();
    b.flags = r.integer();
    b.node = r.text();
    auto shapes = r.integer();
    if (!r.ok || !b.id || !ids.insert(b.id).second || b.kind < 1 ||
        b.kind > 2 || !ordinals.insert(b.ready_ordinal).second ||
        (i && b.ready_ordinal <= previous) || b.flags > 3 || !path(b.node) ||
        !names.insert(b.node).second || !shapes || shapes > 16)
      return reject("Scene actions source binding/geometry");
    for (uint32_t j = 0; j < shapes; ++j) {
      FieldSceneActionShape s;
      s.id = r.integer();
      s.disabled = r.boolean();
      s.node = r.text();
      auto parts = r.integer();
      if (!r.ok || !s.id || !ids.insert(s.id).second || !path(s.node) ||
          !names.insert(s.node).second || parts > 128)
        return reject("Scene actions source shape-owner");
      for (uint32_t k = 0; k < parts; ++k) {
        BattleValue v{r.scalar(), r.scalar(), r.scalar(), r.scalar()};
        if (!r.ok || v.z <= 0 || v.w <= 0)
          return reject("Scene actions source rectangle");
        s.parts.push_back(v);
      }
      b.shapes.push_back(std::move(s));
    }
    if (b.kind == 1) {
      b.parent_id = r.integer();
      b.copy = r.boolean();
      b.parent_path = r.text();
      auto objects = r.integer();
      const auto *parent = d.reference(b.parent_id);
      if (!r.ok || !parent || !parent->collision_properties ||
          b.parent_path.empty() || !b.copy || !objects || objects > 128)
        return reject("Scene actions source reparent parent/capability");
      for (uint32_t j = 0; j < objects; ++j) {
        FieldSceneActionObject v;
        v.source_id = r.integer();
        v.path = r.text();
        const auto *ref = d.reference(v.source_id);
        if (!r.ok || !ref || v.path.empty() ||
            (ref->kind != 1 && ref->kind != 2) || ref->descendants ||
            !ref->script.empty())
          return reject("Scene actions source reparented subtree capability");
        b.objects.push_back(std::move(v));
      }
    } else {
      b.object_id = r.integer();
      b.action = r.integer();
      b.check_state = r.boolean();
      b.set_state = r.boolean();
      b.object_path = r.text();
      b.method = r.text();
      b.check_flag = r.text();
      b.set_flag = r.text();
      const auto *ref = d.reference(b.object_id);
      if (!r.ok || (b.object_id && !ref) || b.action > 2 ||
          ((b.action == 0) != b.method.empty()) ||
          (!b.method.empty() && (!symbol(b.method) || !ref || ref->kind != 3 ||
                                 ref->script.empty())) ||
          (b.object_id == 0 && !b.object_path.empty()))
        return reject(
            "Scene actions source event reference/method/flag capability");
    }
    previous = b.ready_ordinal;
    d.bindings_.push_back(std::move(b));
  }
  count = r.integer();
  if (!count || count > 2048)
    return reject("Scene actions source closure count");
  std::set<std::string> sources;
  for (uint32_t i = 0; i < count; ++i) {
    auto name = r.text();
    auto hash = r.hash();
    if (!r.ok || !path(name) || !sources.insert(name).second)
      return reject("Scene actions source closure hash/identity");
    d.sources_.emplace(name, hash);
  }
  if (!sources.count(d.scene_))
    return reject("Scene actions source scene closure");
  for (const auto &s : d.scripts_)
    if (!sources.count(s))
      return reject("Scene action script closure");
  for (const auto &v : d.references_)
    if (!v.script.empty() && !sources.count(v.script))
      return reject("Scene action method source closure");
  count = r.integer();
  if (count != 3)
    return reject("Scene actions reviewed engine source count");
  names.clear();
  for (uint32_t i = 0; i < count; ++i) {
    auto url = r.text();
    if (!r.ok || url.substr(0, 8) != "https://" || !names.insert(url).second ||
        !read_hash(r))
      return reject("Scene actions reviewed engine source hash/URL");
  }
  if (!r.ok || r.at != n)
    return reject("Scene actions trailing bytes");
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
} // namespace encore::upstream
