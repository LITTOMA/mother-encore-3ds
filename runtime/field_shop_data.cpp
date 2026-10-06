#include "encore/field_shop.hpp"
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
const FieldShopPolicy *FieldShopData::policy(uint32_t id) const {
  for (const auto &p : policies_)
    if (p.id == id)
      return &p;
  return nullptr;
}
const FieldShopText *FieldShopData::text(const std::string &key) const {
  for (const auto &t : texts_)
    if (t.key == key)
      return &t;
  return nullptr;
}
const std::string &FieldShopData::sound(uint32_t i) const {
  static const std::string empty;
  return i < sounds_.size() ? sounds_[i] : empty;
}
uint32_t FieldShopData::texture_role(const std::string &key) const {
  auto i = roles_.find(key);
  return i == roles_.end() ? UINT32_MAX : i->second;
}
bool FieldShopData::load_file(const char *p, std::string &e) {
  if (!p)
    return fail(e, "Shop pack path missing");
  std::ifstream f(p, std::ios::binary);
  if (!f)
    return fail(e, "Shop pack unavailable");
  f.seekg(0, std::ios::end);
  auto size = f.tellg();
  if (size < 64 || size > 4 * 1024 * 1024)
    return fail(e, "Shop pack bounded length rejected");
  std::vector<uint8_t> b(size_t(size), 0);
  f.seekg(0);
  if (!f.read(reinterpret_cast<char *>(b.data()), size))
    return fail(e, "Shop pack read incomplete");
  return load(b.data(), b.size(), e);
}
bool FieldShopData::load(const uint8_t *p, size_t n, std::string &e) {
  valid_ = false;
  if (!p || n < 64 || n > 4 * 1024 * 1024 || std::memcmp(p, "ENCSHP01", 8) ||
      u32(p + 8) != 1 || u32(p + 12) != n || u32(p + 16) != crc(p, n) ||
      u32(p + 20) != 1 || u32(p + 24) != 1)
    return fail(e, "Shop binary schema/capability/rules/CRC rejected");
  FieldShopData d;
  std::copy_n(p + 32, 20, d.pin_.begin());
  const uint32_t offers = u32(p + 28), policies = u32(p + 52),
                 panels = u32(p + 56), layout = u32(p + 60);
  if (!offers || offers > 256 || !policies || policies > 1024 || panels != 8 ||
      layout != 47 ||
      std::all_of(d.pin_.begin(), d.pin_.end(), [](auto b) { return b == 0; }))
    return fail(e, "Shop bounded coverage/layout rejected");
  R r{p, n};
  d.name_ = r.s();
  d.lines_ = r.u();
  d.digits_ = r.u();
  d.can_sell_ = r.b();
  d.buy_loop_ = r.b();
  d.sell_loop_ = r.b();
  d.warning_ = r.f();
  d.threshold_ = r.f();
  if (d.name_.empty() || !d.lines_ || d.lines_ > 256 || !d.digits_ ||
      d.digits_ > 32 || d.warning_ <= 0 || d.warning_ > 60 ||
      d.threshold_ <= 0 || d.threshold_ > 1)
    r.ok = false;
  std::set<uint32_t> offerids;
  for (uint32_t i = 0; i < offers && r.ok; ++i) {
    auto id = r.u();
    if (!id || !offerids.insert(id).second)
      r.ok = false;
    d.offers_.push_back(id);
  }
  std::set<uint32_t> ids;
  std::set<std::string> names;
  for (uint32_t i = 0; i < policies && r.ok; ++i) {
    FieldShopPolicy x;
    x.id = r.u();
    x.doses = r.u();
    auto cost = r.u(), value = r.u();
    x.cost = int32_t(cost);
    x.value = int32_t(value);
    x.key = r.b();
    x.source = r.s();
    x.name = r.s();
    x.name_key = r.s();
    x.description_key = r.s();
    x.article_key = r.s();
    x.slot = r.s();
    r.hash(x.source_sha);
    if (!x.id || !ids.insert(x.id).second || !names.insert(x.name).second ||
        !path(x.source) || x.source != "Data/Items/" + x.name + ".yaml" ||
        x.name_key.empty() || x.description_key.empty() ||
        x.article_key.empty() || !x.doses || x.doses > 65535 ||
        cost > INT32_MAX || value > INT32_MAX)
      r.ok = false;
    d.policies_.push_back(std::move(x));
  }
  for (auto id : d.offers_) {
    auto *x = d.policy(id);
    if (!x || x->key || !x->slot.empty())
      r.ok = false;
  }
  auto count = r.u();
  if (count > 8192)
    r.ok = false;
  std::set<std::string> keys;
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    FieldShopText t;
    t.key = r.s();
    t.en = r.s();
    t.zh = r.s();
    if (t.key.empty() || !keys.insert(t.key).second)
      r.ok = false;
    d.texts_.push_back(std::move(t));
  }
  for (unsigned i = 0; i < 5 && r.ok; ++i) {
    auto s = r.s();
    if (!path(s))
      r.ok = false;
    d.sounds_.push_back(std::move(s));
  }
  count = r.u();
  if (!count || count > 1024)
    r.ok = false;
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    FieldShopTexture t;
    t.source = r.s();
    t.path = r.s();
    t.width = r.u();
    t.height = r.u();
    t.bytes = r.u();
    t.flavor = r.b();
    r.hash(t.sha);
    if (!path(t.source) || !path(t.path) ||
        t.path.compare(0, 17, "graphics/ui/shop/") || !t.width || !t.height ||
        t.width > 1024 || t.height > 1024 || !t.bytes ||
        t.bytes > 16 * 1024 * 1024)
      r.ok = false;
    d.textures_.push_back(std::move(t));
  }
  for (uint32_t i = 0; i < panels && r.ok; ++i) {
    FieldShopPanel x;
    x.node = r.s();
    for (auto &v : x.rect)
      v = r.f();
    for (auto &v : x.patch) {
      v = r.f();
      if (v < 0)
        r.ok = false;
    }
    x.texture = r.u();
    if (x.node.empty() || x.texture >= d.textures_.size() ||
        x.rect[2] < x.rect[0] || x.rect[3] < x.rect[1])
      r.ok = false;
    d.panels_.push_back(std::move(x));
  }
  for (uint32_t i = 0; i < layout && r.ok; ++i)
    d.layout_.push_back(r.f());
  for (auto &c : d.colors_)
    c = r.u();
  count = r.u();
  if (count > 2048)
    r.ok = false;
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    auto key = r.s();
    auto v = r.u();
    if (key.empty() || !d.roles_.emplace(key, v).second ||
        (v != UINT32_MAX && v >= d.textures_.size()))
      r.ok = false;
  }
  count = r.u();
  if (count > 128)
    r.ok = false;
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    FieldShopPortrait x;
    x.character = r.s();
    x.texture = r.u();
    if (x.character.empty() || x.texture >= d.textures_.size())
      r.ok = false;
    d.portraits_.push_back(std::move(x));
  }
  for (auto &f : d.fonts_) {
    f = r.s();
    if (!path(f))
      r.ok = false;
  }
  d.portrait_limit_ = r.u();
  if (!d.portrait_limit_ || d.portrait_limit_ > 64)
    r.ok = false;
  d.cursor_speed_ = r.f();
  for (auto &v : d.cursor_size_) {
    v = r.u();
    if (!v || v > 1024)
      r.ok = false;
  }
  count = r.u();
  if (!count || count > 256 || d.cursor_speed_ <= 0 || d.cursor_speed_ > 1000)
    r.ok = false;
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    auto v = r.u();
    auto t = d.texture_role("cursor");
    if (t >= d.textures_.size() ||
        uint64_t(v + 1) * d.cursor_size_[0] > d.textures_[t].width ||
        d.cursor_size_[1] > d.textures_[t].height)
      r.ok = false;
    d.cursor_frames_.push_back(v);
  }
  count = r.u();
  if (count != d.aux_.size())
    r.ok = false;
  for (auto &v : d.aux_)
    for (auto &x : v)
      x = r.f();
  for (auto &k : d.text_keys_) {
    k = r.s();
    if (k.empty() || !d.text(k))
      r.ok = false;
  }
  count = r.u();
  if (!count || count > 8192)
    r.ok = false;
  std::map<std::string, std::array<uint8_t, 32>> sources;
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    auto s = r.s();
    std::array<uint8_t, 32> hash{};
    r.hash(hash);
    if (!path(s) || !sources.emplace(s, hash).second)
      r.ok = false;
  }
  for (const auto &x : d.policies_) {
    auto i = sources.find(x.source);
    if (i == sources.end() || i->second != x.source_sha ||
        !d.text(x.name_key) || !d.text(x.description_key) ||
        !d.text(x.article_key))
      r.ok = false;
  }
  for (const auto &t : d.textures_)
    if (!sources.count(t.source) || !sources.count(t.source + ".import"))
      r.ok = false;
  for (const auto &key : {"icon_panel", "cursor", "dollar_left", "dollar_right",
                          "modifiers", "scroll_bg", "scroll_thumb"})
    if (d.texture_role(key) >= d.textures_.size())
      r.ok = false;
  if (d.portraits_.empty() || d.portraits_.size() % 2 ||
      d.portrait_limit_ > d.portraits_.size() / 2)
    r.ok = false;
  for (size_t i = 0; i < d.portraits_.size() / 2; ++i)
    if (d.portraits_[i].character !=
        d.portraits_[i + d.portraits_.size() / 2].character)
      r.ok = false;
  for (size_t row : {size_t(13), size_t(14)})
    for (float x : d.aux_[row])
      if (x < 0 || x > 2 || std::floor(x) != x)
        r.ok = false;
  for (size_t row : {size_t(6), size_t(7)})
    for (float x : d.aux_[row])
      if (x < 0)
        r.ok = false;
  if (!r.ok || r.at != n)
    return fail(e, "Shop binary typed content/source binding rejected");
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
} // namespace encore::upstream
