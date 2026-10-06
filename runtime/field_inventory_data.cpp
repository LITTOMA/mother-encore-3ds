#include "encore/field_inventory.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <set>
namespace encore::upstream {
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
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
  size_t n, at = 96;
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
const FieldInventoryOwner *FieldInventoryData::owner(uint32_t id) const {
  for (const auto &r : owners_)
    if (r.id == id)
      return &r;
  return nullptr;
}
const FieldInventoryOwner *FieldInventoryData::role(uint32_t id) const {
  for (const auto &r : owners_)
    if (r.role == id)
      return &r;
  return nullptr;
}
const FieldInventoryPolicy *FieldInventoryData::policy(uint32_t id) const {
  for (const auto &r : policies_)
    if (r.definition == id)
      return &r;
  return nullptr;
}
const FieldInventoryStatusPolicy *
FieldInventoryData::status(const std::string &id) const {
  for (const auto &r : statuses_)
    if (r.id == id)
      return &r;
  return nullptr;
}
const FieldInventoryText *
FieldInventoryData::text(const std::string &key) const {
  for (const auto &r : texts_)
    if (r.key == key)
      return &r;
  return nullptr;
}
const FieldInventoryLayout *
FieldInventoryData::layout(const std::string &key) const {
  auto it = layouts_.find(key);
  return it == layouts_.end() ? nullptr : &it->second;
}
const std::array<float, 4> *
FieldInventoryData::parameter(const std::string &key) const {
  auto it = parameters_.find(key);
  return it == parameters_.end() ? nullptr : &it->second;
}
bool FieldInventoryData::load_file(const char *path, std::string &e) {
  if (!path)
    return fail(e, "Field inventory resource path missing");
  std::ifstream f(path, std::ios::binary | std::ios::ate);
  if (!f)
    return fail(e, "Cannot open field inventory resource");
  auto size = f.tellg();
  if (size < 96 || size > 1024 * 1024)
    return fail(e, "Field inventory resource size rejected");
  std::vector<uint8_t> b(static_cast<size_t>(size));
  f.seekg(0);
  if (!f.read(reinterpret_cast<char *>(b.data()), size))
    return fail(e, "Field inventory resource read failed");
  return load(b.data(), b.size(), e);
}
bool FieldInventoryData::load(const uint8_t *p, size_t n, std::string &e) {
  if (!p || n < 96 || n > 1024 * 1024 || std::memcmp(p, "ENCFINV1", 8) ||
      u32(p + 8) != 1 || u32(p + 12) != n || u32(p + 16) != crc(p, n) ||
      u32(p + 20) != 1 || u32(p + 24) != 1 || u32(p + 28) != 0x454e0043 ||
      u32(p + 60))
    return fail(e, "Unknown field inventory format/capabilities/rules/CRC");
  FieldInventoryData d;
  std::copy_n(p + 32, 20, d.pin_.begin());
  std::copy_n(p + 64, 32, d.identity_.begin());
  if (std::all_of(d.pin_.begin(), d.pin_.end(), [](auto v) { return !v; }) ||
      std::all_of(d.identity_.begin(), d.identity_.end(),
                  [](auto v) { return !v; }))
    return fail(e, "Inventory source identity absent");
  Reader r{p, n};
  auto count = r.count(3);
  if (count != 3)
    return fail(e, "Inventory source owner domain rejected");
  std::set<uint32_t> ids, roles;
  for (uint32_t k = 0; k < count; ++k) {
    FieldInventoryOwner o;
    o.id = r.u();
    o.role = r.u();
    o.source_name = r.s();
    if (!o.id || o.role > 2 || !ids.insert(o.id).second ||
        !roles.insert(o.role).second || o.source_name.empty())
      r.ok = false;
    d.owners_.push_back(std::move(o));
  }
  if (r.u() != d.slots_.size())
    r.ok = false;
  std::set<std::string> names;
  for (auto &v : d.slots_) {
    v = r.s();
    if (v.empty() || !names.insert(v).second)
      r.ok = false;
  }
  if (r.u() != d.stats_.size())
    r.ok = false;
  names.clear();
  for (auto &v : d.stats_) {
    v = r.s();
    if (v.empty() || !names.insert(v).second)
      r.ok = false;
  }
  count = r.count(1000);
  if (!count)
    r.ok = false;
  for (uint32_t k = 0; k < count; ++k) {
    std::array<int32_t, 7> v{};
    for (auto &a : v) {
      a = r.i();
      if (a < 0)
        r.ok = false;
    }
    d.levels_.push_back(v);
  }
  d.refresh_hp_ = r.u();
  d.delay_ = r.d();
  d.initial_level_ = r.u();
  d.initial_hp_ = r.q();
  d.initial_pp_ = r.q();
  d.initial_cash_ = r.q();
  d.description_rows_ = r.u();
  for (auto &v : d.weights_)
    v = r.i();
  d.slot_step_ = r.i();
  for (auto &v : d.scores_)
    v = r.i();
  if (!d.description_rows_ || d.description_rows_ > 1024 || d.slot_step_ <= 0 ||
      std::any_of(d.weights_.begin(), d.weights_.end(),
                  [](auto v) { return v < 0; }) ||
      std::any_of(d.scores_.begin(), d.scores_.end(),
                  [](auto v) { return v < 0; }))
    r.ok = false;
  d.glyph_.path = r.s();
  d.glyph_.bytes = r.u();
  d.glyph_.width = r.u();
  d.glyph_.height = r.u();
  for (auto &v : d.glyph_.position)
    v = r.f();
  r.hash(d.glyph_.sha);
  if (!path(d.glyph_.path) || !d.glyph_.bytes || d.glyph_.bytes > 1024 * 1024 ||
      !d.glyph_.width || !d.glyph_.height || d.glyph_.width > 1024 ||
      d.glyph_.height > 1024)
    r.ok = false;
  if (!d.refresh_hp_ || d.refresh_hp_ > 2147483647 || d.delay_ <= 0 ||
      d.delay_ > 60 || !d.initial_level_ ||
      d.initial_level_ > d.levels_.size() || d.initial_hp_ < 0 ||
      d.initial_pp_ < 0 || d.initial_cash_ < 0)
    r.ok = false;
  count = r.count(4096);
  for (uint32_t k = 0; k < count; ++k) {
    FieldInventoryInitial v;
    v.owner = r.u();
    v.item.definition = r.u();
    v.item.doses = r.u();
    v.item.equipped = r.boolean();
    if (!d.owner(v.owner) || !v.item.definition || !v.item.doses)
      r.ok = false;
    d.initial_.push_back(v);
  }
  count = r.count(4096);
  if (!count || count != u32(p + 52))
    r.ok = false;
  ids.clear();
  for (uint32_t k = 0; k < count; ++k) {
    FieldInventoryPolicy v;
    v.definition = r.u();
    v.source = r.s();
    r.hash(v.source_sha);
    v.consume_allowed = r.boolean();
    v.use_allowed = r.boolean();
    std::set<uint32_t> order;
    for (auto &i : v.boost_order) {
      i = r.u();
      if (i >= 7 || !order.insert(i).second)
        r.ok = false;
    }
    auto ac = r.count(16);
    for (uint32_t j = 0; j < ac; ++j) {
      FieldInventoryAction a;
      a.function = FieldItemFunction(r.u());
      a.name = r.s();
      a.fail = r.s();
      if (uint32_t(a.function) < 1 || uint32_t(a.function) > 3 ||
          (a.function != FieldItemFunction::Equip && a.name.empty()))
        r.ok = false;
      v.actions.push_back(std::move(a));
    }
    if (!v.definition || !ids.insert(v.definition).second || !path(v.source))
      r.ok = false;
    d.policies_.push_back(std::move(v));
  }
  for (const auto &i : d.initial_)
    if (!d.policy(i.item.definition))
      r.ok = false;
  count = r.count(256);
  names.clear();
  unsigned unconscious = 0;
  for (uint32_t k = 0; k < count; ++k) {
    FieldInventoryStatusPolicy v;
    v.id = r.s();
    v.source = r.s();
    v.priority = r.i();
    auto flags = r.u();
    if (flags > 15)
      r.ok = false;
    v.persistent = flags & 1;
    v.passive = flags & 2;
    v.exclusive = flags & 4;
    v.unconscious = flags & 8;
    unconscious += v.unconscious;
    v.heal = r.s();
    v.fail = r.s();
    auto bc = r.count(16);
    for (uint32_t j = 0; j < bc; ++j) {
      FieldInventoryStatusPolicy::Block b;
      b.selector = r.u();
      b.message = r.s();
      if (b.selector < 1 || b.selector > 4 || b.message.empty())
        r.ok = false;
      v.blocks.push_back(std::move(b));
    }
    if (v.id.empty() || !names.insert(v.id).second || !path(v.source))
      r.ok = false;
    d.statuses_.push_back(std::move(v));
  }
  if (unconscious != 1)
    r.ok = false;
  for (auto &v : d.sounds_) {
    v = r.s();
    if (!path(v))
      r.ok = false;
  }
  for (auto &v : d.feedback_)
    v = r.s();
  for (auto &v : d.labels_)
    v = r.s();
  count = r.count(4096);
  names.clear();
  for (uint32_t k = 0; k < count; ++k) {
    FieldInventoryText t;
    t.key = r.s();
    t.en = r.s();
    t.zh = r.s();
    if (t.key.empty() || !names.insert(t.key).second)
      r.ok = false;
    d.texts_.push_back(std::move(t));
  }
  for (const auto &key : d.feedback_)
    if (!d.text(key))
      r.ok = false;
  for (const auto &key : d.labels_)
    if (!d.text(key))
      r.ok = false;
  count = r.count(64);
  for (uint32_t k = 0; k < count; ++k) {
    auto key = r.s();
    FieldInventoryLayout l;
    for (auto &v : l.rect) {
      v = r.f();
      if (std::abs(v) > 8192)
        r.ok = false;
    }
    for (auto &v : l.color) {
      v = r.f();
      if (v < 0 || v > 1)
        r.ok = false;
    }
    if (key.empty() || l.rect[2] <= 0 || l.rect[3] <= 0 ||
        !d.layouts_.emplace(key, l).second)
      r.ok = false;
  }
  count = r.count(64);
  for (uint32_t k = 0; k < count; ++k) {
    auto key = r.s();
    std::array<float, 4> v{};
    for (auto &a : v) {
      a = r.f();
      if (std::abs(a) > 8192)
        r.ok = false;
    }
    if (key.empty() || !d.parameters_.emplace(key, v).second)
      r.ok = false;
  }
  count = r.count(8192);
  if (count != u32(p + 56))
    r.ok = false;
  for (uint32_t k = 0; k < count; ++k) {
    auto key = r.s();
    std::array<uint8_t, 32> hash{};
    r.hash(hash);
    if (!path(key) || !d.sources_.emplace(key, hash).second)
      r.ok = false;
  }
  for (const auto &v : d.policies_) {
    auto it = d.sources_.find(v.source);
    if (it == d.sources_.end() || it->second != v.source_sha)
      r.ok = false;
    for (const auto &a : v.actions)
      if ((!a.name.empty() && !d.text(a.name)) ||
          (!a.fail.empty() && !d.text(a.fail)))
        r.ok = false;
  }
  for (const auto &v : d.statuses_) {
    if (!d.sources_.count(v.source) || (!v.heal.empty() && !d.text(v.heal)) ||
        (!v.fail.empty() && !d.text(v.fail)))
      r.ok = false;
    for (const auto &b : v.blocks)
      if (!d.text(b.message))
        r.ok = false;
  }
  if (!r.ok || r.at != n)
    return fail(e, "Field inventory typed source data rejected");
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
bool FieldInventoryData::bind_definitions(const FieldItemDefinitions &defs,
                                          std::string &e) const {
  if (!valid_ || !defs.valid() || defs.source_pin() != pin_ ||
      defs.definitions().size() != policies_.size())
    return fail(e, "Inventory definitions source domain mismatch");
  for (const auto &p : policies_) {
    auto *d = defs.definition(p.definition);
    std::array<uint8_t, 32> hash{};
    if (!d || d->source != p.source || !defs.source_hash(p.source, hash) ||
        hash != p.source_sha || d->actions.size() != p.actions.size())
      return fail(e, "Inventory definition identity/source hash rejected");
    const auto &name = role(0)->source_name;
    auto allowed = [&](const auto &v) {
      return v.empty() || std::find(v.begin(), v.end(), name) != v.end();
    };
    if (p.consume_allowed != allowed(d->can_consume) ||
        p.use_allowed != allowed(d->can_use) || !d->transform.empty())
      return fail(e, "Inventory permissions/transform source policy rejected");
    for (size_t n = 0; n < p.actions.size(); ++n)
      if (p.actions[n].function != d->actions[n].function ||
          p.actions[n].name != d->actions[n].name ||
          (!d->actions[n].textfail.empty() &&
           p.actions[n].fail != d->actions[n].textfail))
        return fail(e, "Inventory action source binding rejected");
    for (const auto &name : d->status_heals)
      if (!status(name))
        return fail(e, "Inventory status cure source unbound");
  }
  for (const auto &entry : sources_) {
    std::array<uint8_t, 32> hash{};
    if (defs.source_hash(entry.first, hash) && hash != entry.second)
      return fail(e, "Inventory shared source hash mismatch");
  }
  e.clear();
  return true;
}
} // namespace encore::upstream
