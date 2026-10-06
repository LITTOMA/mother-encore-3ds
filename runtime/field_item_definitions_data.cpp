#include "encore/field_item_definitions.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cstring>
#include <cmath>
#include <fstream>
#include <set>
#include <tuple>
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
  int32_t signed_number() {
    auto raw = number();
    int32_t value;
    std::memcpy(&value, &raw, 4);
    return value;
  }
  bool boolean() {
    auto v = number();
    if (v > 1)
      ok = false;
    return v == 1;
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
    uint32_t cp;
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
    if (std::all_of(a.begin(), a.end(), [](uint8_t b) { return b == 0; }))
      ok = false;
    return ok;
  }
  bool texts(std::vector<std::string> &out) {
    auto count = number();
    if (count > 256) {
      ok = false;
      return false;
    }
    std::set<std::string> seen;
    for (uint32_t i = 0; i < count; ++i) {
      auto s = text();
      if (s.empty() || !seen.insert(s).second)
        ok = false;
      out.push_back(std::move(s));
    }
    return ok;
  }
};
} // namespace
const FieldItemDefinition *FieldItemDefinitions::definition(uint32_t id) const {
  for (const auto &d : definitions_)
    if (d.id == id)
      return &d;
  return nullptr;
}
const FieldItemDefinition *
FieldItemDefinitions::definition(const std::string &name) const {
  for (const auto &d : definitions_)
    if (d.item_name == name)
      return &d;
  return nullptr;
}
const FieldItemDefinition *FieldItemDefinitions::legacy(uint32_t domain,
                                                        uint32_t id) const {
  if (!domain || !id)
    return nullptr;
  for (const auto &d : definitions_)
    if (d.legacy_domain == domain && d.legacy_id == id)
      return &d;
  return nullptr;
}
const FieldItemBinding *FieldItemDefinitions::binding(FieldItemBindingKind kind,
                                                      const std::string &scene,
                                                      uint32_t object) const {
  for (const auto &b : bindings_)
    if (b.kind == kind && b.scene == scene && b.object_id == object)
      return &b;
  return nullptr;
}
const FieldItemBinding *
FieldItemDefinitions::programme(const std::string &program,
                                const std::string &label) const {
  for (const auto &b : bindings_)
    if (b.kind == FieldItemBindingKind::Programme && b.program == program &&
        b.label == label)
      return &b;
  return nullptr;
}
bool FieldItemDefinitions::source_hash(const std::string &p,
                                       std::array<uint8_t, 32> &h) const {
  auto i = sources_.find(p);
  if (i == sources_.end())
    return false;
  h = i->second;
  return true;
}
bool FieldItemDefinitions::load_file(const char *p, std::string &e) {
  if (!p) {
    e = "Field item file absent";
    return false;
  }
  std::ifstream f(p, std::ios::binary);
  if (!f) {
    e = "Field item resource unavailable";
    return false;
  }
  f.seekg(0, std::ios::end);
  auto n = f.tellg();
  if (n < 64 || n > 4 * 1024 * 1024) {
    e = "Field item size rejected";
    return false;
  }
  f.seekg(0);
  std::vector<uint8_t> b(static_cast<size_t>(n));
  if (!f.read(reinterpret_cast<char *>(b.data()), n)) {
    e = "Field item read failed";
    return false;
  }
  return load(b.data(), b.size(), e);
}
bool FieldItemDefinitions::load(const uint8_t *p, size_t n, std::string &e) {
  auto fail = [&](const char *s) {
    e = s;
    return false;
  };
  if (!p || n < 64 || n > 4 * 1024 * 1024 || std::memcmp(p, "ENCFIT01", 8) ||
      (u32(p + 8) != 2 && u32(p + 8) != 3) || u32(p + 12) != n || u32(p + 16) != crc(p, n) ||
      u32(p + 20) != u32(p + 8) || u32(p + 24) != 1 ||
      (!u32(p + 28) || u32(p + 28) > 4096) ||
      ((u32(p + 8)==2 && !u32(p + 52)) || u32(p + 52) > 4096 || (u32(p + 8)==3 && u32(p + 52))) || !u32(p + 56) ||
      u32(p + 56) > 4096 || u32(p + 60))
    return fail("Field item schema/capabilities/rules/CRC rejected");
  FieldItemDefinitions d;
  d.format_=u32(p+8);
  std::copy_n(p + 32, 20, d.pin_.begin());
  if (std::all_of(d.pin_.begin(), d.pin_.end(), [](uint8_t b) { return !b; }))
    return fail("Field item source pin absent");
  Reader r{p, n};
  for (auto &v : d.capacities_)
    v = r.number();
  d.unbounded_mask_ = r.number();
  d.default_doses_ = r.number();
  d.dose_step_ = r.number();
  d.dose_drop_threshold_ = r.number();
  d.uid_protocol_ = r.number();
  for (auto c : d.capacities_)
    if (c > 100000)
      return fail("Field item inventory capacity rejected");
  if (!r.ok || d.unbounded_mask_ != 10 || !d.capacities_[0] ||
      d.capacities_[1] || !d.capacities_[2] || !d.capacities_[3] ||
      !d.default_doses_ || d.default_doses_ > 65535 || !d.dose_step_ ||
      d.dose_step_ != d.dose_drop_threshold_ || d.dose_step_ > 65535 ||
      d.uid_protocol_ != 7)
    return fail("Field item ownership/dose/UID protocol rejected");
  std::array<uint32_t, 6> expected_coverage{};
  uint64_t total = 0;
  for (auto &value : expected_coverage) {
    value = r.number();
    if ((d.format_==2 && !value) || value > 4096)
      return fail("Field item source coverage capacity rejected");
    total += value;
  }
  if (!r.ok || total != u32(p + 52))
    return fail("Field item source coverage/header mismatch");
  if (d.format_ == 3) {
    d.god_storage_id_ = r.number();
    d.god_storage_role_ = r.number();
    d.god_storage_member_ = r.text();
    d.god_storage_type_field_ = r.text();
    d.god_storage_items_field_ = r.text();
    if (!d.god_storage_id_ || d.god_storage_role_ >= 4 ||
        !d.unbounded(d.god_storage_role_) || d.god_storage_member_.empty() ||
        d.god_storage_type_field_.empty() || d.god_storage_items_field_.empty())
      return fail("Global GodStorage source policy rejected");
    r.texts(d.constructor_sources_);
    if (d.constructor_sources_.size() != 3)
      return fail("Global constructor source count rejected");
  }
  std::set<uint32_t> ids;
  std::set<std::string> names;
  std::set<std::pair<uint32_t, uint32_t>> legacy;
  for (uint32_t i = 0; i < u32(p + 28); ++i) {
    FieldItemDefinition a;
    a.id = r.number();
    a.doses = r.number();
    a.flags = r.number();
    a.legacy_domain = r.number();
    a.legacy_id = r.number();
    a.cost = r.signed_number();
    a.value = r.signed_number();
    a.heal_hp = r.signed_number();
    a.heal_pp = r.signed_number();
    for (auto &v : a.boost)
      v = r.signed_number();
    a.source = r.text();
    a.item_name = r.text();
    a.name_key = r.text();
    a.sorting_key = r.text();
    a.description_key = r.text();
    a.article_key = r.text();
    a.slot = r.text();
    a.transform = r.text();
    r.texts(a.can_use);
    r.texts(a.can_consume);
    r.texts(a.status_heals);
    auto actions = r.number();
    if (!r.ok || !a.id || !ids.insert(a.id).second || !a.doses ||
        a.doses > 65535 || a.flags > 15 || a.legacy_domain > 2 ||
        (a.legacy_domain ? !a.legacy_id : a.legacy_id != 0) ||
        (a.legacy_domain &&
         !legacy.emplace(a.legacy_domain, a.legacy_id).second) ||
        a.cost < 0 || a.value < 0 || a.heal_hp < 0 || a.heal_pp < 0 ||
        !path(a.source) || a.item_name.empty() ||
        !names.insert(a.item_name).second || a.name_key.empty() ||
        a.sorting_key.empty() || a.description_key.empty() ||
        a.article_key.empty() || (d.format_==2 && !a.transform.empty()) || actions > 256 ||
        (a.legacy_domain == 2 && !a.keyitem()) ||
        (a.legacy_domain == 1 && a.keyitem()))
      return fail("Field item complete source definition rejected");
    for (uint32_t j = 0; j < actions; ++j) {
      FieldItemAction action;
      auto fn = r.number();
      action.function = FieldItemFunction(fn);
      action.pending = r.boolean();
      action.name = r.text();
      action.textfail = r.text();
      if (!r.ok || fn < 1 || fn > (d.format_==3 ? 4u : 3u) || !action.pending)
        return fail("Field item unknown executable action rejected");
      a.actions.push_back(std::move(action));
    }
    if (d.format_ == 3) {
      a.unequipped_sort_score = r.signed_number();
      auto locales = r.number();
      if (!locales || locales > 64)
        return fail("Global item sorting locales rejected");
      for (uint32_t j = 0; j < locales; ++j) {
        auto locale = r.text(), text = r.text();
        if (locale.empty() || text.empty() ||
            !a.sorting_translations.emplace(locale, text).second)
          return fail("Global item sorting translation rejected");
      }
      auto fields = r.number();
      if (fields > 64)
        return fail("Global item pending metadata capacity rejected");
      for (uint32_t j = 0; j < fields; ++j) {
        FieldItemPendingValue v;
        v.field = r.number();
        v.kind = r.number();
        v.key = r.text();
        v.text = r.text();
        v.integer = r.signed_number();
        auto lo = r.number(), hi = r.number();
        uint64_t bits = uint64_t(lo) | uint64_t(hi) << 32;
        std::memcpy(&v.real, &bits, 8);
        bool shape = false;
        switch (v.field) {
        case 1:
          shape = (v.key == "skill" || v.key == "dialog") && v.kind == 1;
          shape = shape || (v.key == "target_type" && v.kind == 2) ||
                  (v.key == "reusable" && v.kind == 3);
          break;
        case 2:
        case 7:
        case 8:
        case 9:
        case 11:
          shape = v.key.empty() && v.kind == 1;
          break;
        case 3:
          shape = v.key.empty() && v.kind == 3;
          break;
        case 4:
          shape = !v.key.empty() && v.kind == 2;
          break;
        case 5:
        case 6:
          shape = v.kind == 1;
          break;
        case 10:
          shape = !v.key.empty() && v.kind == 4;
          break;
        default:
          break;
        }
        if (!r.ok || !shape || !std::isfinite(v.real) ||
            (v.kind == 1 && (v.text.empty() || v.integer || v.real)) ||
            (v.kind != 1 && !v.text.empty()) ||
            (v.kind == 3 && (v.integer < 0 || v.integer > 1)) ||
            (v.kind != 4 && v.real) ||
            (v.kind == 4 && (v.integer || v.real < 0)))
          return fail("Global item unknown pending metadata rejected");
        a.pending_metadata.push_back(std::move(v));
      }
    }
    d.definitions_.push_back(std::move(a));
  }
  std::set<std::tuple<uint32_t, uint32_t, std::string>> bindings;
  std::array<uint32_t, 6> coverage{};
  std::set<std::pair<std::string, std::string>> programmes;
  for (uint32_t i = 0; i < u32(p + 52); ++i) {
    FieldItemBinding b;
    auto kind = r.number();
    b.kind = FieldItemBindingKind(kind);
    b.object_id = r.number();
    b.definition = r.number();
    b.operation = r.number();
    b.scene = r.text();
    b.node = r.text();
    b.program = r.text();
    b.label = r.text();
    if (!r.ok || kind < 1 || kind > 6 || !b.object_id ||
        !bindings.emplace(kind, b.object_id, b.scene).second || !path(b.node) ||
        (kind < 6 && !path(b.scene)) || (kind == 6 && !b.scene.empty()) ||
        (b.definition ? !d.definition(b.definition) : kind != 3) ||
        b.operation != (kind < 3 ? 1 : kind - 1))
      return fail("Field item source object/operation binding rejected");
    if (kind == 5) {
      if (!path(b.program) || b.label.empty() ||
          !programmes.emplace(b.program, b.label).second ||
          !d.definition(b.definition)->keyitem())
        return fail("Field item typed programme grant rejected");
    } else if (!b.program.empty() || !b.label.empty())
      return fail("Field item stray programme binding rejected");
    ++coverage[kind - 1];
    d.bindings_.push_back(std::move(b));
  }
  if (coverage != expected_coverage)
    return fail("Field item source mechanism coverage incomplete");
  auto count = r.number();
  if (count != u32(p + 56))
    return fail("Field item source proof count rejected");
  for (uint32_t i = 0; i < count; ++i) {
    auto path_name = r.text();
    std::array<uint8_t, 32> h{};
    if (!path(path_name) || !r.hash(h) ||
        !d.sources_.emplace(path_name, h).second)
      return fail("Field item source proof rejected");
  }
  if (!r.ok || r.at != n)
    return fail("Field item trailing data rejected");
  std::array<uint8_t, 32> h{};
  for (const auto &a : d.definitions_)
    if (!d.source_hash(a.source, h))
      return fail("Field item YAML proof absent");
  for (const auto &b : d.bindings_) {
    if (!b.scene.empty() && !d.source_hash(b.scene, h))
      return fail("Field item source scene proof absent");
    if (b.kind == FieldItemBindingKind::Programme &&
        !d.source_hash("Data/Dialogue/" + b.program + ".yaml", h))
      return fail("Field item programme proof absent");
  }
  if (d.format_ == 3) {
    for (const auto &source : d.constructor_sources_)
      if (!path(source) || !d.source_hash(source, h))
        return fail("Global constructor source proof absent");
    std::map<std::string, std::set<std::pair<int32_t, std::string>>> sort_keys;
    const auto &first = d.definitions_.front().sorting_translations;
    for (const auto &a : d.definitions_) {
      if (a.legacy_domain || a.legacy_id ||
          a.sorting_translations.size() != first.size())
        return fail("Global item constructor domain/locales rejected");
      if (!a.transform.empty() && !d.definition(a.transform))
        return fail("Global item unknown transform target rejected");
      for (const auto &text : a.sorting_translations)
        if (!first.count(text.first) ||
            !sort_keys[text.first]
                 .emplace(a.unequipped_sort_score, text.second)
                 .second)
          return fail("Global item ambiguous source sort comparator rejected");
      std::set<std::pair<uint32_t, std::string>> fields;
      for (const auto &v : a.pending_metadata)
        if (!fields.emplace(v.field, v.key).second)
          return fail("Global item duplicate pending metadata rejected");
    }
  }
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
} // namespace encore::upstream
