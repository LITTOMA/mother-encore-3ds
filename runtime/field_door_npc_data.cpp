#include "encore/field_door_npc.hpp"
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

const FieldDoorProgramme *
FieldDoorBinding::programme(std::string_view id) const {
  for (const auto &v : programmes)
    if (v.dialog == id)
      return &v;
  return nullptr;
}
const FieldDoorBinding *FieldDoorNpcData::binding(uint32_t id) const {
  for (const auto &v : bindings_)
    if (v.id == id)
      return &v;
  return nullptr;
}
bool FieldDoorNpcData::source_hash(std::string_view p,
                                   std::array<uint8_t, 32> &out) const {
  auto i = sources_.find(std::string(p));
  if (i == sources_.end())
    return false;
  out = i->second;
  return true;
}
bool FieldDoorNpcData::load_file(const char *p, std::string &e) {
  if (!p) {
    e = "DoorNPC file path absent";
    return false;
  }
  std::ifstream f(p, std::ios::binary);
  if (!f) {
    e = "DoorNPC file unavailable";
    return false;
  }
  f.seekg(0, std::ios::end);
  auto n = f.tellg();
  if (n < 64 || n > 4 * 1024 * 1024) {
    e = "DoorNPC file size";
    return false;
  }
  f.seekg(0);
  std::vector<uint8_t> b(static_cast<size_t>(n));
  if (!f.read(reinterpret_cast<char *>(b.data()), n)) {
    e = "DoorNPC file read";
    return false;
  }
  return load(b.data(), b.size(), e);
}
bool FieldDoorNpcData::load(const uint8_t *p, size_t n, std::string &e) {
  auto fail = [&](const char *m) {
    e = m;
    return false;
  };
  if (!p || n < 64 || n > 4 * 1024 * 1024)
    return fail("DoorNPC pack size");
  if (std::memcmp(p, "ENCDOR01", 8) || u32(p + 8) != 1 || u32(p + 12) != n ||
      u32(p + 20) != 1 || u32(p + 24) != 1 || u32(p + 28) != 1 ||
      u32(p + 16) != crc(p, n))
    return fail("DoorNPC header/version/capability/rules/CRC");
  for (size_t i = 52; i < 64; ++i)
    if (p[i])
      return fail("DoorNPC reserved header");
  FieldDoorNpcData d;
  std::copy(p + 32, p + 52, d.pin_.begin());
  if (std::all_of(d.pin_.begin(), d.pin_.end(), [](uint8_t b) { return !b; }))
    return fail("DoorNPC source pin absent");
  Reader r{p, n};
  d.scene_ = r.text();
  d.script_ = r.text();
  if (!path(d.scene_) || !path(d.script_))
    return fail("DoorNPC source identity");
  auto &policy = d.policy_;
  for (auto &v : policy.pause)
    v = r.boolean();
  for (auto &v : policy.turn)
    v = r.boolean();
  policy.completion = r.text();
  policy.timer_seconds = r.scalar();
  policy.process_pause = r.boolean();
  policy.ignore_time_scale = r.boolean();
  policy.strict_negative = r.boolean();
  policy.global_fifo = r.boolean();
  if (!r.ok || !symbol(policy.completion) || policy.timer_seconds <= 0 ||
      policy.timer_seconds > 60 || !policy.process_pause ||
      policy.ignore_time_scale || !policy.strict_negative ||
      !policy.global_fifo)
    return fail("DoorNPC source timer/completion policy");
  auto count = r.integer();
  if (count != 1)
    return fail("DoorNPC signal count");
  for (uint32_t i = 0; i < count; ++i) {
    FieldDoorConnection c;
    c.role = r.integer();
    c.signal = r.text();
    c.method = r.text();
    if (!r.ok || c.role != i + 1 || !symbol(c.signal) || !symbol(c.method))
      return fail("DoorNPC signal identity");
    policy.connections.push_back(std::move(c));
  }
  count = r.integer();
  if (count != u32(p + 28))
    return fail("DoorNPC binding count");
  std::set<uint32_t> ids;
  std::set<std::string> names;
  for (uint32_t i = 0; i < count; ++i) {
    FieldDoorBinding b;
    b.id = r.integer();
    b.ready_ordinal = r.integer();
    b.shape_id = r.integer();
    b.audio_id = r.integer();
    b.audio_ready = r.integer();
    b.layer = r.integer();
    b.mask = r.integer();
    b.flags = r.integer();
    b.node = r.text();
    b.dialog = r.text();
    b.appear = r.text();
    b.disappear = r.text();
    b.position = r.point();
    b.centre = r.point();
    b.half = r.point();
    b.audio.path = r.text();
    b.audio.bus = r.text();
    b.audio.volume_db = r.scalar();
    b.audio.pitch = r.scalar();
    if (!r.ok || !b.id || !b.shape_id || !b.audio_id ||
        !ids.insert(b.id).second || !ids.insert(b.shape_id).second ||
        !ids.insert(b.audio_id).second || b.audio_ready >= b.ready_ordinal ||
        b.flags > 3 || !path(b.node) || !names.insert(b.node).second ||
        !path(b.dialog) || (!b.appear.empty() && !symbol(b.appear)) ||
        (!b.disappear.empty() && !symbol(b.disappear)) || b.half.x <= 0 ||
        b.half.y <= 0 || !path(b.audio.path) || !symbol(b.audio.bus) ||
        b.audio.volume_db < -96 || b.audio.volume_db > 24 ||
        b.audio.pitch <= 0 || b.audio.pitch > 16)
      return fail("DoorNPC source instance/audio/geometry");
    auto groups = r.integer();
    if (groups > 256)
      return fail("DoorNPC dialogue groups count");
    for (uint32_t j = 0; j < groups; ++j) {
      auto entries = r.integer();
      if (entries < 2 || entries > 256)
        return fail("DoorNPC dialogue entries count");
      std::vector<std::string> g;
      for (uint32_t k = 0; k < entries; ++k) {
        auto v = r.text();
        if (!r.ok || (k == 0 ? (!v.empty() && !symbol(v)) : !path(v)))
          return fail("DoorNPC dialogue flag/path");
        g.push_back(std::move(v));
      }
      b.groups.push_back(std::move(g));
    }
    auto programmes = r.integer();
    if (!programmes || programmes > 4096)
      return fail("DoorNPC programme count");
    std::set<std::string> dialogs, paths;
    for (uint32_t j = 0; j < programmes; ++j) {
      FieldDoorProgramme v;
      v.dialog = r.text();
      v.path = r.text();
      v.sha = r.hash();
      if (!r.ok || !path(v.dialog) || !path(v.path) ||
          !dialogs.insert(v.dialog).second || !paths.insert(v.path).second)
        return fail("DoorNPC programme source identity/hash");
      b.programmes.push_back(std::move(v));
    }
    if (!b.programme(b.dialog))
      return fail("DoorNPC default programme absent");
    for (const auto &g : b.groups)
      for (size_t j = 1; j < g.size(); ++j)
        if (!b.programme(g[j]))
          return fail("DoorNPC dialogue programme reference");
    d.bindings_.push_back(std::move(b));
  }
  count = r.integer();
  if (!count || count > 2048)
    return fail("DoorNPC source closure count");
  for (uint32_t i = 0; i < count; ++i) {
    auto name = r.text();
    auto hash = r.hash();
    if (!r.ok || !path(name) ||
        !d.sources_.emplace(std::move(name), hash).second)
      return fail("DoorNPC source closure identity/hash");
  }
  if (!d.sources_.count(d.scene_) || !d.sources_.count(d.script_))
    return fail("DoorNPC source closure missing");
  for (const auto &b : d.bindings_) {
    if (!d.sources_.count(b.audio.path) ||
        !d.sources_.count(b.audio.path + ".import"))
      return fail("DoorNPC genuine audio source missing");
    for (const auto &v : b.programmes) {
      auto f = d.sources_.find(v.path);
      if (f == d.sources_.end() || f->second != v.sha)
        return fail("DoorNPC programme closure mismatch");
    }
  }
  if (!r.ok || r.at != n)
    return fail("DoorNPC trailing bytes");
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
} // namespace encore::upstream
