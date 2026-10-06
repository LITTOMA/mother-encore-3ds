#include "encore/field_goods.hpp"
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
         s.find(':') == s.npos && s.find('\\') == s.npos &&
         s.find("..") == s.npos;
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
    auto v = u32(p + at);
    at += 4;
    return v;
  }
  int32_t i() {
    auto v = u();
    int32_t r;
    std::memcpy(&r, &v, 4);
    return r;
  }
  int64_t q() {
    if (!ok || at > n || n - at < 8) {
      ok = false;
      return 0;
    }
    uint64_t v = 0;
    for (unsigned k = 0; k < 8; ++k)
      v |= uint64_t(p[at + k]) << (8 * k);
    at += 8;
    int64_t r;
    std::memcpy(&r, &v, 8);
    return r;
  }
  float f() {
    auto v = u();
    float r;
    std::memcpy(&r, &v, 4);
    if (!std::isfinite(r))
      ok = false;
    return r;
  }
  double d() {
    auto v = q();
    double r;
    std::memcpy(&r, &v, 8);
    if (!std::isfinite(r))
      ok = false;
    return r;
  }
  bool boolean() {
    auto v = u();
    if (v > 1)
      ok = false;
    return v == 1;
  }
  std::string s() {
    auto k = u();
    if (!ok || k > 8192 || at > n || k > n - at) {
      ok = false;
      return {};
    }
    std::string r(reinterpret_cast<const char *>(p + at), k);
    at += k;
    size_t i = 0;
    uint32_t cp = 0;
    while (i < r.size())
      if (!encore::utf8_next(r, i, cp) || cp < 32 || cp == 127) {
        ok = false;
        break;
      }
    return r;
  }
  void hash(std::array<uint8_t, 32> &r) {
    if (!ok || at > n || n - at < r.size()) {
      ok = false;
      return;
    }
    std::copy_n(p + at, r.size(), r.begin());
    at += r.size();
    if (std::all_of(r.begin(), r.end(), [](auto v) { return !v; }))
      ok = false;
  }
  uint32_t count(uint32_t max) {
    auto v = u();
    if (v > max)
      ok = false;
    return ok ? v : 0;
  }
};

} // namespace
const std::array<float, 4> *FieldGoodsData::layout(const std::string &k) const {
  auto i = layouts_.find(k);
  return i == layouts_.end() ? nullptr : &i->second;
}
const std::array<float, 4> *
FieldGoodsData::parameter(const std::string &k) const {
  auto i = parameters_.find(k);
  return i == parameters_.end() ? nullptr : &i->second;
}
const FieldGoodsClip *FieldGoodsData::clip(const std::string &k) const {
  for (const auto &v : clips_)
    if (v.name == k)
      return &v;
  return nullptr;
}
const FieldInventoryText *FieldGoodsData::text(const std::string &k) const {
  for (const auto &v : texts_)
    if (v.key == k)
      return &v;
  return nullptr;
}
bool FieldGoodsData::bind_inventory(const FieldInventoryData &d,
                                    std::string &e) const {
  if (!valid_ || !d.valid() || d.source_pin() != pin_ ||
      d.identity() != inventory_identity_) {
    e = "Goods owning source identity mismatch";
    return false;
  }
  e.clear();
  return true;
}
bool FieldGoodsData::load_file(const char *p, std::string &e) {
  std::ifstream f(p ? p : "", std::ios::binary | std::ios::ate);
  if (!f || f.tellg() < 0 || f.tellg() > 4 * 1024 * 1024) {
    e = "Goods resource absent/size rejected";
    return false;
  }
  std::vector<uint8_t> b(size_t(f.tellg()));
  f.seekg(0);
  if (!f.read(reinterpret_cast<char *>(b.data()), std::streamsize(b.size()))) {
    e = "Goods read truncated";
    return false;
  }
  return load(b.data(), b.size(), e);
}
bool FieldGoodsData::load(const uint8_t *p, size_t n, std::string &e) {
  auto reject = [&]() {
    e = "Goods typed binary/source rejected";
    return false;
  };
  if (!p || n < 128 || n > 4 * 1024 * 1024 || std::memcmp(p, "ENCFGDS1", 8) ||
      u32(p + 8) != 1 || u32(p + 12) != n || crc(p, n) != u32(p + 16) ||
      u32(p + 20) != 1 || u32(p + 24) != 1 || u32(p + 28) != 0x454e0048 ||
      u32(p + 52) || u32(p + 56) || u32(p + 60))
    return reject();
  FieldGoodsData d;
  std::copy_n(p + 32, 20, d.pin_.begin());
  std::copy_n(p + 64, 32, d.identity_.begin());
  std::copy_n(p + 96, 32, d.inventory_identity_.begin());
  for (const auto &hash : {d.identity_, d.inventory_identity_})
    if (std::all_of(hash.begin(), hash.end(), [](auto v) { return !v; }))
      return reject();
  Reader r{p, n};
  for (auto *map : {&d.layouts_, &d.parameters_}) {
    auto count = r.count(128);
    for (uint32_t i = 0; i < count && r.ok; ++i) {
      auto key = r.s();
      std::array<float, 4> v{};
      for (auto &x : v)
        x = r.f();
      if (key.empty() || !map->emplace(key, v).second)
        r.ok = false;
    }
  }
  std::set<uint32_t> order;
  for (auto &v : d.order_) {
    v = r.u();
    if (v >= d.order_.size() || !order.insert(v).second)
      r.ok = false;
  }
  for (auto &v : d.stat_labels_)
    v = r.s();
  for (auto &v : d.labels_)
    v = r.s();
  for (auto &v : d.sounds_) {
    v = r.s();
    if (!path(v))
      r.ok = false;
  }
  auto count = r.count(8192);
  std::set<std::string> names;
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    FieldInventoryText v;
    v.key = r.s();
    v.en = r.s();
    v.zh = r.s();
    if (v.key.empty() || !names.insert(v.key).second)
      r.ok = false;
    d.texts_.push_back(std::move(v));
  }
  count = r.count(32);
  names.clear();
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    FieldGoodsClip c;
    c.name = r.s();
    c.length = r.f();
    auto keys = r.count(64);
    if (c.name.empty() || !names.insert(c.name).second || c.length <= 0 ||
        c.length > 60 || !keys)
      r.ok = false;
    float prev = -1;
    for (uint32_t j = 0; j < keys && r.ok; ++j) {
      FieldGoodsClip::Key k;
      k.time = r.f();
      k.value = r.f();
      k.transition = r.f();
      if (k.time < 0 || k.time <= prev || k.time > c.length ||
          std::abs(k.value) > 4096 || std::abs(k.transition) > 1000)
        r.ok = false;
      prev = k.time;
      c.keys.push_back(k);
    }
    d.clips_.push_back(std::move(c));
  }
  count = r.count(256);
  names.clear();
  std::set<std::string> paths;
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    FieldGoodsTexture t;
    t.name = r.s();
    t.path = r.s();
    t.bytes = r.u();
    t.width = r.u();
    t.height = r.u();
    r.hash(t.sha);
    if (t.name.empty() || !names.insert(t.name).second || !path(t.path) ||
        t.path.compare(0, 9, "graphics/") || !paths.insert(t.path).second ||
        !t.bytes || t.bytes > 4 * 1024 * 1024 || !t.width || !t.height ||
        t.width > 1024 || t.height > 1024)
      r.ok = false;
    d.textures_.push_back(std::move(t));
  }
  count = r.count(4096);
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    auto s = r.s();
    std::array<uint8_t, 32> h{};
    r.hash(h);
    if (!path(s) || !d.sources_.emplace(s, h).second)
      r.ok = false;
  }
  for (const auto &v : d.sounds_)
    if (!d.sources_.count(v) || !d.sources_.count(v + ".import"))
      r.ok = false;
  for (const auto &v : d.stat_labels_)
    if (!d.text(v))
      r.ok = false;
  for (const auto &v : d.labels_)
    if (!d.text(v))
      r.ok = false;
  for (const auto *k :
       {"Confirm", "Sort", "Stats", "StatsNumbers", "StatsTitles",
        "StatsPortrait", "Select", "SelectTitle"})
    if (!d.layout(k))
      r.ok = false;
  for (const auto *k : {"ConfirmPatch",
                        "ConfirmInset",
                        "ConfirmCursor",
                        "SortPatch",
                        "SortInset",
                        "SortCursor",
                        "ActionRow",
                        "ActionCursor",
                        "TargetInset",
                        "Input",
                        "Bounce",
                        "SelectPatch",
                        "Portraits",
                        "SelectTitleInset",
                        "SelectTitlePatch",
                        "StatsPatch",
                        "StatsPortraitParent",
                        "StatValue",
                        "StatIcon",
                        "StatDivider",
                        "StatDividerColor",
                        "StatTitleDivider",
                        "StatTitleColor",
                        "StatTitleMin",
                        "ScrollRect",
                        "ScrollBG",
                        "ScrollThumb",
                        "ScrollBGPatch",
                        "ScrollThumbPatch",
                        "ScrollColor",
                        "ScrollArrow",
                        "GridCancelSound",
                        "Highlight",
                        "Blink",
                        "Indicator",
                        "IndicatorPosition",
                        "IndicatorBox",
                        "ActionPatch",
                        "ScrollRotation",
                        "ConfirmTitleColor",
                        "SortTitleColor",
                        "ScopeHint",
                        "ScopeColor",
                        "Portraitsuitable",
                        "Portraitequipped",
                        "Portraitbetter",
                        "Portraitlower"})
    if (!d.parameter(k))
      r.ok = false;
  for (const auto *k : {"Open", "Close", "MessageOpen", "MessageClose",
                        "StatsOpen", "StatsClose"})
    if (!d.clip(k))
      r.ok = false;
  for (const auto *k :
       {"bar", "inside", "ninten", "ninten-hl", "key", "key-hl", "stats", "up",
        "down", "indicator", "scroll-bg", "scroll-thumb", "cursor0", "cursor1",
        "cursor2", "equipped"})
    if (!names.count(k))
      r.ok = false;
  if (!r.ok || r.at != n ||
      (d.layouts_.size() != 8 || d.parameters_.size() != 47 ||
       d.textures_.size() != 16 || d.clips_.size() != 6) ||
      d.parameters_["Input"][0] <= 0 || d.parameters_["Input"][0] > 10 ||
      d.parameters_["Bounce"][0] <= 0 || d.parameters_["Bounce"][0] > 10)
    return reject();
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
namespace {
double ease(double t, double curve) {
  if (curve > 0)
    return curve < 1 ? 1 - std::pow(1 - t, 1 / curve) : std::pow(t, curve);
  if (curve < 0)
    return t < .5 ? std::pow(t * 2, -curve) * .5
                  : (1 - std::pow(1 - (t - .5) * 2, -curve)) * .5 + .5;
  return 0;
}
} // namespace
float FieldGoodsClip::value(double t) const {
  if (keys.empty())
    return 0;
  if (t <= keys.front().time)
    return keys.front().value;
  for (size_t i = 1; i < keys.size(); ++i)
    if (t < keys[i].time) {
      const auto &a = keys[i - 1], &b = keys[i];
      auto fraction = ease((t - a.time) / (b.time - a.time), a.transition);
      return float(a.value + (b.value - a.value) * fraction);
    }
  return keys.back().value;
}
} // namespace encore::upstream
