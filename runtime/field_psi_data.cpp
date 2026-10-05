#include "encore/field_psi.hpp"
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
  float scalar() {
    auto u = integer();
    float f;
    std::memcpy(&f, &u, 4);
    if (!std::isfinite(f) || std::abs(f) > 1000000)
      ok = false;
    return f;
  }
  BattleValue rect() { return {scalar(), scalar(), scalar(), scalar()}; }
  std::string text() {
    auto l = integer();
    if (!ok || l > 8192 || at > n || l > n - at) {
      ok = false;
      return {};
    }
    std::string s(reinterpret_cast<const char *>(p + at), l);
    at += l;
    size_t i = 0;
    uint32_t cp;
    while (i < s.size())
      if (!encore::utf8_next(s, i, cp) || (cp < 32 && cp != '\n')) {
        ok = false;
        break;
      }
    return s;
  }
  bool hash() {
    if (!ok || at > n || n - at < 32) {
      ok = false;
      return false;
    }
    bool any = false;
    for (unsigned i = 0; i < 32; ++i)
      any |= p[at + i] != 0;
    at += 32;
    return ok &= any;
  }
};
bool path(std::string_view p) {
  return !p.empty() && p.front() != '/' && p.back() != '/' &&
         p.find("..") == p.npos && p.find(':') == p.npos &&
         p.find('\\') == p.npos;
}
} // namespace
const FieldPsiSkill *FieldPsiData::skill(uint32_t id) const {
  for (const auto &s : skills_)
    if (s.id == id)
      return &s;
  return nullptr;
}
const FieldPsiSkill *FieldPsiData::skill(std::string_view key) const {
  for (const auto &s : skills_)
    if (s.key == key)
      return &s;
  return nullptr;
}
const FieldPsiItem *FieldPsiData::item(std::string_view key) const {
  for (const auto &s : items_)
    if (s.key == key)
      return &s;
  return nullptr;
}
const FieldPsiStatus *FieldPsiData::status(std::string_view key) const {
  for (const auto &s : statuses_)
    if (s.id == key)
      return &s;
  return nullptr;
}
const FieldPsiLocale *FieldPsiData::locale(std::string_view code) const {
  for (const auto &s : locales_)
    if (s.code == code)
      return &s;
  return nullptr;
}
bool FieldPsiData::load_file(const char *p, std::string &e) {
  if (!p) {
    e = "PSI pack path absent";
    return false;
  }
  std::ifstream f(p, std::ios::binary);
  if (!f) {
    e = "PSI pack unavailable";
    return false;
  }
  f.seekg(0, std::ios::end);
  auto n = f.tellg();
  if (n < 64 || n > 4 * 1024 * 1024) {
    e = "PSI file size";
    return false;
  }
  f.seekg(0);
  std::vector<uint8_t> b(static_cast<size_t>(n));
  if (!f.read(reinterpret_cast<char *>(b.data()), n)) {
    e = "PSI file read";
    return false;
  }
  return load(b.data(), b.size(), e);
}
bool FieldPsiData::load(const uint8_t *p, size_t n, std::string &e) {
  auto reject = [&](const char *m) {
    e = m;
    return false;
  };
  if (!p || n < 64 || n > 4 * 1024 * 1024)
    return reject("PSI pack size");
  if (std::memcmp(p, "ENCPSI01", 8) || u32(p + 8) != 1 || u32(p + 12) != n ||
      u32(p + 20) != 1 || u32(p + 24) != 1 || !u32(p + 28) ||
      u32(p + 28) > 4096 || u32(p + 16) != crc(p, n))
    return reject("PSI header/version/capability/rule/CRC");
  for (size_t i = 52; i < 64; ++i)
    if (p[i])
      return reject("PSI header reserved fields");
  FieldPsiData d;
  std::copy(p + 32, p + 52, d.pin_.begin());
  if (std::all_of(d.pin_.begin(), d.pin_.end(), [](uint8_t b) { return !b; }))
    return reject("PSI source pin absent");
  Reader r{p, n};
  d.message_delay_ = r.scalar();
  d.effect_.color = r.rect();
  d.effect_.cut = r.scalar();
  d.effect_.duration = r.scalar();
  d.effect_.spin = r.scalar();
  d.effect_.restore_cut = r.scalar();
  d.effect_.transition = r.integer();
  d.effect_.ease_in = r.integer();
  d.effect_.ease_out = r.integer();
  d.telepathy_skill_ = r.text();
  d.source_font_ = r.text();
  d.number_font_ = r.text();
  if (!r.ok || d.message_delay_ <= 0 || d.message_delay_ > 60 ||
      d.effect_.duration <= 0 || d.effect_.duration > 60 || d.effect_.cut < 0 ||
      d.effect_.cut > 1 || d.effect_.restore_cut < 0 ||
      d.effect_.restore_cut > 1 || d.effect_.color.w < 0 ||
      d.effect_.color.w > 1 || d.effect_.transition > 10 ||
      d.effect_.ease_in > 3 || d.effect_.ease_out > 3 ||
      d.telepathy_skill_.empty() || !path(d.source_font_) ||
      !path(d.number_font_))
    return reject("PSI message/effect rules");
  for (float c : {d.effect_.color.x, d.effect_.color.y, d.effect_.color.z,
                  d.effect_.color.w})
    if (c < 0 || c > 1)
      return reject("PSI effect source RGBA bounds");
  auto strings = [&](std::vector<std::string> &out, uint32_t min,
                     uint32_t max) {
    auto count = r.integer();
    if (count < min || count > max)
      return false;
    std::set<std::string> ids;
    for (uint32_t i = 0; i < count; ++i) {
      auto s = r.text();
      if (!r.ok || s.empty() || !ids.insert(s).second)
        return false;
      out.push_back(s);
    }
    return true;
  };
  if (!strings(d.order_, 1, 32) || !strings(d.telepathy_users_, 1, 32))
    return reject("PSI canonical order/users");
  for (const auto &s : d.telepathy_users_)
    if (std::find(d.order_.begin(), d.order_.end(), s) == d.order_.end())
      return reject("PSI field user outside party order");
  std::vector<std::string> v;
  if (!strings(v, 4, 4))
    return reject("PSI level labels");
  std::copy(v.begin(), v.end(), d.levels_.begin());
  v.clear();
  if (!strings(v, 7, 7))
    return reject("PSI source sounds");
  std::copy(v.begin(), v.end(), d.sounds_.begin());
  v.clear();
  if (!strings(v, 4, 4))
    return reject("PSI fallback programmes");
  std::copy(v.begin(), v.end(), d.fallbacks_.begin());
  for (const auto &s : d.fallbacks_)
    if (!path(s))
      return reject("PSI fallback source path");
  auto count = r.integer();
  if (!count || count > 32)
    return reject("PSI locale count");
  std::set<std::string> names;
  for (uint32_t i = 0; i < count; ++i) {
    FieldPsiLocale l;
    l.code = r.text();
    l.title = r.text();
    l.whom = r.text();
    l.pp = r.text();
    l.insufficient = r.text();
    l.forgetful = r.text();
    l.hp_max = r.text();
    l.hp_up = r.text();
    if (!r.ok || l.code.empty() || !names.insert(l.code).second ||
        l.title.empty() || l.whom.empty() || l.pp.empty() ||
        l.insufficient.empty() || l.forgetful.empty() ||
        l.hp_max.find("{target}") == l.hp_max.npos ||
        l.hp_up.find("{target}") == l.hp_up.npos)
      return reject("PSI locale/messages");
    d.locales_.push_back(std::move(l));
  }
  count = r.integer();
  if (!count || count > 256)
    return reject("PSI status count");
  names.clear();
  for (uint32_t i = 0; i < count; ++i) {
    FieldPsiStatus s;
    s.id = r.text();
    auto flags = r.integer();
    s.forgetful = flags & 1;
    s.unconscious = flags & 2;
    s.incapacitated = flags & 4;
    if (!r.ok || flags > 7 || s.id.empty() || !names.insert(s.id).second)
      return reject("PSI source status policy");
    d.statuses_.push_back(std::move(s));
  }
  count = r.integer();
  if (count != u32(p + 28))
    return reject("PSI skill count");
  std::set<uint32_t> ids;
  names.clear();
  unsigned tele = 0, heal = 0;
  for (uint32_t i = 0; i < count; ++i) {
    FieldPsiSkill s;
    s.id = r.integer();
    auto op = r.integer();
    s.operation = FieldPsiOperation(op);
    auto level = r.integer();
    std::memcpy(&s.level, &level, 4);
    s.target = r.integer();
    s.pp = r.integer();
    s.heal = r.integer();
    s.variance = r.integer();
    s.iq_divisor = r.integer();
    auto flags = r.integer();
    s.target_unconscious = flags & 1;
    s.target_incapacitated = flags & 2;
    s.source = r.text();
    s.key = r.text();
    s.name_key = r.text();
    s.name_en = r.text();
    s.name_zh = r.text();
    s.desc_en = r.text();
    s.desc_zh = r.text();
    if (!r.ok || !s.id || !ids.insert(s.id).second ||
        !names.insert(s.key).second || s.key.empty() || !path(s.source) ||
        op > 3 || s.level < -1 || s.level > 3 || s.target > 32 ||
        s.pp > 1000000 || s.heal > 1000000 || s.variance > 1000000 ||
        s.iq_divisor > 1000000 || flags > 3 ||
        (op && (s.name_key.empty() || s.name_en.empty() || s.name_zh.empty() ||
                s.desc_en.empty() || s.desc_zh.empty())))
      return reject("PSI checked skill category/rule/locale");
    if (op == 1) {
      ++tele;
      if (s.target != 5 || s.key != d.telepathy_skill_ || s.heal ||
          s.variance || s.iq_divisor)
        return reject("PSI Telepathy source operation");
    }
    if (op == 2) {
      ++heal;
      if (s.target != 1 || !s.iq_divisor)
        return reject("PSI heal source operation");
    }
    d.skills_.push_back(std::move(s));
  }
  if (tele != 1 || heal != 1)
    return reject("PSI capability source operations");
  count = r.integer();
  if (!count || count > 4096)
    return reject("PSI source item grant count");
  names.clear();
  for (uint32_t i = 0; i < count; ++i) {
    FieldPsiItem item;
    item.source = r.text();
    item.key = r.text();
    item.skill = r.text();
    auto equip = r.integer(), users = r.integer();
    if (!r.ok || !path(item.source) || item.key.empty() ||
        !names.insert(item.key).second || equip > 1 || users > 32 ||
        (!item.skill.empty() && !d.skill(item.skill)))
      return reject("PSI source item grant policy");
    item.equippable = equip;
    std::set<std::string> unique;
    for (uint32_t j = 0; j < users; ++j) {
      auto user = r.text();
      if (!r.ok || user.empty() || !unique.insert(user).second)
        return reject("PSI item skill receiver");
      item.users.push_back(std::move(user));
    }
    d.items_.push_back(std::move(item));
  }
  count = r.integer();
  if (count != d.layouts_.size())
    return reject("PSI layout schema");
  for (auto &l : d.layouts_)
    l = r.rect();
  if (!r.ok)
    return reject("PSI layout finite");
  auto integral = [](float x, float lo, float hi) {
    return std::isfinite(x) && x >= lo && x <= hi && std::floor(x) == x;
  };
  auto layout = [&](FieldPsiLayout role) {
    return d.layouts_[size_t(role) - 1];
  };
  auto rows = layout(FieldPsiLayout::DescriptionPolicy);
  if (!integral(rows.x, 1, 32) || !integral(rows.y, 1, rows.x) ||
      !integral(rows.z, 1, 32) || rows.w <= 0)
    return reject("PSI row/font policy bounds");
  for (auto role :
       {FieldPsiLayout::SourceViewport, FieldPsiLayout::PlatformViewport}) {
    auto v = layout(role);
    if (!integral(v.x, 1, 4096) || !integral(v.y, 1, 4096) || v.z || v.w)
      return reject("PSI viewport schema");
  }
  for (auto role : {FieldPsiLayout::PanelPatch, FieldPsiLayout::TargetPatch,
                    FieldPsiLayout::BarPatch, FieldPsiLayout::CostPatch,
                    FieldPsiLayout::TitlePatch}) {
    auto v = layout(role);
    for (float x : {v.x, v.y, v.z, v.w})
      if (!integral(x, 0, 4096))
        return reject("PSI ninepatch margins");
  }
  auto panel = layout(FieldPsiLayout::Panel),
       origin = layout(FieldPsiLayout::RowOrigin),
       pitch = layout(FieldPsiLayout::RowStride),
       levels = layout(FieldPsiLayout::LevelStride),
       targets = layout(FieldPsiLayout::TargetOrigin);
  if (panel.z <= 0 || panel.w <= 0 || origin.z <= 0 || origin.w <= 0 ||
      pitch.x < origin.w || levels.x <= 0 || levels.y <= 0 || levels.z <= 0 ||
      targets.x < 0 || targets.y < 0 || targets.z < origin.w || targets.w <= 0)
    return reject("PSI row/target geometry bounds");
  for (auto role :
       {FieldPsiLayout::DividerColor, FieldPsiLayout::TargetTitleColor,
        FieldPsiLayout::HighlightColor}) {
    auto v = layout(role);
    for (float x : {v.x, v.y, v.z, v.w})
      if (x < 0 || x > 1)
        return reject("PSI source color range");
  }
  for (auto &a : d.animations_) {
    a.length = r.scalar();
    count = r.integer();
    if (!r.ok || a.length <= 0 || a.length > 10 || !count || count > 32)
      return reject("PSI source animation length");
    float last = -1;
    for (uint32_t i = 0; i < count; ++i) {
      FieldPsiAnimationKey k;
      k.time = r.scalar();
      k.value = r.scalar();
      k.ease = r.scalar();
      if (!r.ok || k.time < 0 || k.time < last || k.time > a.length)
        return reject("PSI source animation keys");
      last = k.time;
      a.keys.push_back(k);
    }
  }
  d.cost_animation_.length = r.scalar();
  d.cost_visible_after_ = r.scalar();
  d.cost_base_y_ = r.scalar();
  count = r.integer();
  if (!r.ok || d.cost_animation_.length <= 0 || d.cost_animation_.length > 10 ||
      d.cost_visible_after_ < 0 ||
      d.cost_visible_after_ > d.cost_animation_.length || !count || count > 32)
    return reject("PSI PP animation");
  float cost_last = -1;
  for (uint32_t i = 0; i < count; ++i) {
    FieldPsiAnimationKey k;
    k.time = r.scalar();
    k.value = r.scalar();
    k.ease = r.scalar();
    if (!r.ok || k.time < 0 || k.time < cost_last ||
        k.time > d.cost_animation_.length)
      return reject("PSI PP animation keys");
    cost_last = k.time;
    d.cost_animation_.keys.push_back(k);
  }
  count = r.integer();
  if (!count || count > 64)
    return reject("PSI source assets count");
  ids.clear();
  names.clear();
  for (uint32_t i = 0; i < count; ++i) {
    FieldPsiAsset a;
    a.role = r.integer();
    a.width = r.integer();
    a.height = r.integer();
    a.file_bytes = r.integer();
    a.crc32 = r.integer();
    a.source = r.text();
    a.path = r.text();
    if (!r.ok || !a.role || !ids.insert(a.role).second ||
        !names.insert(a.path).second || !a.width || !a.height ||
        a.width > 4096 || a.height > 4096 || a.file_bytes > 16 * 1024 * 1024 ||
        bool(a.file_bytes) != bool(a.crc32) || !path(a.source) || !path(a.path))
      return reject("PSI asset schema/receipt");
    d.assets_.push_back(std::move(a));
  }
  count = r.integer();
  if (count != d.order_.size())
    return reject("PSI portrait order");
  names.clear();
  for (uint32_t i = 0; i < count; ++i) {
    FieldPsiPortrait q;
    q.character = r.text();
    q.normal = r.integer();
    q.highlight = r.integer();
    if (!r.ok || q.character != d.order_[i] ||
        !names.insert(q.character).second || !ids.count(q.normal) ||
        !ids.count(q.highlight))
      return reject("PSI source portrait binding");
    d.portraits_.push_back(std::move(q));
  }
  count = r.integer();
  if (!count || count > 2048)
    return reject("PSI receipt count");
  names.clear();
  for (uint32_t i = 0; i < count; ++i) {
    auto s = r.text();
    if (!r.ok || !path(s) || !names.insert(s).second || !r.hash())
      return reject("PSI source receipt");
  }
  for (const auto &s : d.skills_)
    if (!names.count(s.source))
      return reject("PSI skill outside source closure");
  for (const auto &a : d.items_)
    if (!names.count(a.source))
      return reject("PSI item outside source closure");
  for (const auto &a : d.assets_)
    if (!names.count(a.source))
      return reject("PSI asset outside source closure");
  if (!r.ok || r.at != n)
    return reject("PSI trailing bytes");
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
} // namespace encore::upstream
