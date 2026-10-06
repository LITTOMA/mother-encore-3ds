#include "encore/field_player_transitions.hpp"
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
      c = (c >> 1) ^ (0xedb88320u & uint32_t(-int(c & 1)));
  }
  return ~c;
}
bool path(const std::string &s) {
  return !s.empty() && s.size() < 1024 && s[0] != '/' &&
         s.find(':') == std::string::npos &&
         s.find('\\') == std::string::npos && s.find("..") == std::string::npos;
}
struct Reader {
  const uint8_t *p;
  size_t n, at = 64;
  bool ok = true;
  uint32_t number() {
    if (!ok || at > n || n - at < 4) {
      ok = false;
      return 0;
    }
    auto v = u32(p + at);
    at += 4;
    return v;
  }
  float real() {
    auto bits = number();
    float v = 0;
    std::memcpy(&v, &bits, 4);
    if (!std::isfinite(v))
      ok = false;
    return v;
  }
  Vec2 vec() {
    float x = real(), y = real();
    return {x, y};
  }
  std::string text() {
    auto k = number();
    if (!ok || k > 8192 || at > n || k > n - at) {
      ok = false;
      return {};
    }
    std::string s(reinterpret_cast<const char *>(p + at), k);
    at += k;
    size_t i = 0;
    uint32_t cp = 0;
    while (i < s.size())
      if (!encore::utf8_next(s, i, cp) || cp < 32) {
        ok = false;
        break;
      }
    return s;
  }
  bool hash(std::array<uint8_t, 32> &a) {
    if (!ok || at > n || n - at < 32) {
      ok = false;
      return false;
    }
    std::copy_n(p + at, 32, a.begin());
    at += 32;
    if (std::all_of(a.begin(), a.end(), [](uint8_t v) { return !v; }))
      ok = false;
    return ok;
  }
};
} // namespace
const FieldTransitionDescriptor *
FieldPlayerTransitionsData::record(uint32_t id) const {
  for (const auto &r : records_)
    if (r.id == id)
      return &r;
  return nullptr;
}
bool FieldPlayerTransitionsData::load_file(const char *p, std::string &e) {
  if (!p) {
    e = "Transition file absent";
    return false;
  }
  std::ifstream f(p, std::ios::binary);
  if (!f) {
    e = "Transition resource unavailable";
    return false;
  }
  f.seekg(0, std::ios::end);
  auto n = f.tellg();
  if (n < 64 || n > 4 * 1024 * 1024) {
    e = "Transition resource size rejected";
    return false;
  }
  f.seekg(0);
  std::vector<uint8_t> b(static_cast<size_t>(n));
  if (!f.read(reinterpret_cast<char *>(b.data()), n)) {
    e = "Transition read failed";
    return false;
  }
  return load(b.data(), b.size(), e);
}
bool FieldPlayerTransitionsData::load(const uint8_t *p, size_t n,
                                      std::string &e) {
  auto fail = [&](const char *s) {
    e = s;
    return false;
  };
  if (!p || n < 64 || n > 4 * 1024 * 1024 || std::memcmp(p, "ENCFPT01", 8) ||
      u32(p + 8) != 1 || u32(p + 12) != n || u32(p + 16) != crc(p, n) ||
      u32(p + 20) != 1 || u32(p + 24) != 1 || !u32(p + 28) ||
      (!u32(p + 52) || u32(p + 52) > 4096) ||
      (!u32(p + 56) || u32(p + 56) > 8192) || u32(p + 60))
    return fail("Transition schema/capabilities/rules/CRC rejected");
  FieldPlayerTransitionsData d;
  d.scene_ = u32(p + 28);
  std::copy_n(p + 32, 20, d.pin_.begin());
  if (std::all_of(d.pin_.begin(), d.pin_.end(), [](uint8_t v) { return !v; }))
    return fail("Transition source pin absent");
  Reader r{p, n};
  for (auto &v : d.parameters_)
    v = r.real();
  for (size_t i : {size_t(0), size_t(3), size_t(4), size_t(6), size_t(7),
                   size_t(8), size_t(11), size_t(12), size_t(13), size_t(15)})
    if (d.parameters_[i] <= 0 || d.parameters_[i] > 60)
      return fail("Transition source timing/scale rejected");
  if (d.parameters_[5] < 0 || d.parameters_[14] < 0 || d.parameters_[16] <= 0 ||
      d.parameters_[17] <= 0)
    return fail("Transition source delay/pose rejected");
  d.source_scene_ = r.text();
  d.skill_ = r.text();
  for (auto &v : d.prompts_)
    v = r.text();
  for (auto &v : d.animations_)
    v = r.text();
  if (!r.ok || !path(d.source_scene_) || d.skill_.empty() ||
      d.prompts_[0].empty() || d.prompts_[1].empty() ||
      d.prompts_[0] == d.prompts_[1])
    return fail("Transition source scene/flag/prompts rejected");
  for (const auto &v : d.animations_)
    if (v.empty())
      return fail("Transition source animation absent");
  std::vector<std::string> required_sources;
  auto required = r.number();
  if (!required || required > 256)
    return fail("Transition required proof count rejected");
  for (uint32_t i = 0; i < required; ++i) {
    auto name = r.text();
    if (!path(name))
      return fail("Transition required proof path rejected");
    required_sources.push_back(name);
  }
  std::set<uint32_t> ids;
  for (unsigned i = 0; i < u32(p + 56); ++i) {
    FieldTransitionArea a;
    a.id = r.number();
    a.shape_id = r.number();
    a.layer = r.number();
    a.mask = r.number();
    a.kind = r.number();
    auto count = r.number();
    a.position = r.vec();
    if (!r.ok || !a.id || !a.shape_id || !ids.insert(a.id).second ||
        !ids.insert(a.shape_id).second || a.kind < 1 || a.kind > 3 ||
        (a.kind == 1 && count != 2) || (a.kind == 2 && count != 1) ||
        (a.kind == 3 && (count < 6 || count > 4096 || count % 2)))
      return fail("Transition actual monitoring shape rejected");
    for (uint32_t j = 0; j < count; ++j)
      a.values.push_back(r.real());
    if (!r.ok || (a.kind < 3 && std::any_of(
                                    a.values.begin(), a.values.end(),
                                    [](float v) { return v <= 0; })))
      return fail("Transition source shape tuning rejected");
    d.areas_.push_back(std::move(a));
  }
  std::set<uint32_t> records, readies, usedareas;
  unsigned jumps = 0, stairs = 0;
  uint32_t lastready = 0;
  for (unsigned i = 0; i < u32(p + 52); ++i) {
    FieldTransitionDescriptor a;
    a.id = r.number();
    a.ready = r.number();
    a.kind = r.number();
    a.camera_id = r.number();
    a.flags = r.number();
    auto areas = r.number(), points = r.number();
    a.position = r.vec();
    a.height = r.real();
    a.step_length = r.real();
    a.arrow_length = r.real();
    a.node = r.text();
    a.texture = r.text();
    if (!r.ok || !a.id || !records.insert(a.id).second ||
        !readies.insert(a.ready).second || (i && a.ready <= lastready) ||
        !path(a.node) || a.kind < 1 || a.kind > 2 ||
        areas != (a.kind == 1 ? 2 : 1) || points > 256 ||
        (a.kind == 1 && (!a.camera_id || a.flags > 7 || !(a.flags & 4) ||
                         points == 0 || a.height <= 0 || a.height > 10000 ||
                         a.arrow_length <= 0 || !path(a.texture))) ||
        (a.kind == 2 &&
         (a.camera_id || a.flags > 3 || points || a.height || a.arrow_length ||
          !a.texture.empty() || a.step_length <= 0)))
      return fail("Transition source actor/point/behavior rejected");
    lastready = a.ready;
    for (uint32_t j = 0; j < areas; ++j) {
      auto index = r.number();
      if (index >= d.areas_.size() || !usedareas.insert(index).second)
        return fail("Transition source area ownership rejected");
      a.areas.push_back(index);
    }
    for (uint32_t j = 0; j < points; ++j)
      a.points.push_back(r.vec());
    if (a.kind == 1) {
      ++jumps;
      a.sprite_position = r.vec();
      a.sprite_offset = r.vec();
      a.sprite_rotation = r.real();
      a.sprite_scale = r.vec();
      a.sprite_id = r.number();
      a.ray_position = r.vec();
      a.ray_cast = r.vec();
      a.ray_mask = r.number();
      auto bodies = r.number(), rayareas = r.number(), exclude = r.number();
      a.ray_flags = bodies | (rayareas << 1) | (exclude << 2);
      a.ray_id = r.number();
      if (bodies > 1 || rayareas > 1 || exclude > 1 || !bodies ||
          !a.sprite_id || !a.ray_id)
        return fail("Transition source Arrow/RayCast binding rejected");
      for (unsigned track = 0; track < 2; ++track) {
        auto count = r.number();
        if (!count || count > 256)
          return fail("Transition Arrow source key count rejected");
        auto &keys = track ? a.rotation_keys : a.offset_keys;
        for (uint32_t j = 0; j < count; ++j) {
          FieldTransitionArrowKey k;
          k.time = r.real();
          k.value = track ? Vec2{r.real(), 0} : r.vec();
          if (!r.ok || k.time < 0 || k.time >= a.arrow_length ||
              (!keys.empty() && k.time <= keys.back().time))
            return fail("Transition source Arrow key order rejected");
          keys.push_back(k);
        }
      }
    } else
      ++stairs;
    d.records_.push_back(std::move(a));
  }
  if (!jumps || !stairs || usedareas.size() != u32(p + 56))
    return fail("Transition scope coverage incomplete");
  auto count = r.number();
  if (!count || count > 4096)
    return fail("Transition source proof count rejected");
  for (uint32_t i = 0; i < count; ++i) {
    auto name = r.text();
    std::array<uint8_t, 32> h{};
    if (!path(name) || !r.hash(h) || !d.sources_.emplace(name, h).second)
      return fail("Transition source proof rejected");
  }
  if (!r.ok || r.at != n)
    return fail("Transition truncated/trailing data rejected");
  if (!d.sources_.count(d.source_scene_))
    return fail("Transition scene proof absent");
  for (const auto &name : required_sources)
    if (!d.sources_.count(name))
      return fail("Transition required source proof absent");
  for (const auto &a : d.records_)
    if (a.kind == 1 && (!d.sources_.count(a.texture) ||
                        !d.sources_.count(a.texture + ".import")))
      return fail("Transition source Arrow proof absent");
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
} // namespace encore::upstream
