#include "encore/field_global_data.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
namespace encore::upstream {
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
uint32_t word(const uint8_t *p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 |
         uint32_t(p[3]) << 24;
}
uint32_t crc(const uint8_t *p, size_t n) {
  uint32_t c = ~0u;
  for (size_t i = 0; i < n; ++i) {
    c ^= p[i];
    for (unsigned j = 0; j < 8; ++j)
      c = (c >> 1) ^ (0xedb88320u & uint32_t(-int(c & 1)));
  }
  return ~c;
}
struct Reader {
  const uint8_t *p;
  size_t n;
  bool ok = true;
  uint32_t u() {
    if (n < 4) {
      ok = false;
      return 0;
    }
    auto v = word(p);
    p += 4;
    n -= 4;
    return v;
  }
  int64_t integer() {
    uint64_t lo = u(), hi = u();
    return int64_t(lo | (hi << 32));
  }
  template <size_t N> void bytes(std::array<uint8_t, N> &a) {
    if (n < N) {
      ok = false;
      return;
    }
    std::memcpy(a.data(), p, N);
    p += N;
    n -= N;
  }
  std::string s() {
    auto len = u();
    if (!ok || len > 4096 || len > n) {
      ok = false;
      return {};
    }
    std::string v(reinterpret_cast<const char *>(p), len);
    p += len;
    n -= len;
    size_t chars = 0;
    if (v.find('\0') != v.npos || !encore::utf8_count(v, chars))
      ok = false;
    return v;
  }
};
} // namespace
bool FieldGlobalDataData::source_hash(std::string_view p,
                                      std::array<uint8_t, 32> &out) const {
  auto i = sources_.find(std::string(p));
  if (i == sources_.end())
    return false;
  out = i->second;
  return true;
}
bool FieldGlobalDataData::load(const uint8_t *p, size_t n,
                               const FieldIdentity &expected, std::string &e) {
  *this = {};
  if (!p || n < 84 || n > 4 * 1024 * 1024 || std::memcmp(p, "ENCGDAT1", 8) ||
      word(p + 8) != 1 || word(p + 12) != n ||
      word(p + 16) != crc(p + 32, n - 32) || word(p + 20) != 0x454e004d ||
      word(p + 24) != 1 || word(p + 28) != 1)
    return fail(e, "globalData format/capability/rules rejected");
  FieldGlobalDataData d;
  d.identity_ = expected;
  Reader r{p + 32, n - 32};
  std::array<uint8_t, 20> pin{};
  r.bytes(pin);
  r.bytes(d.ir_);
  if (pin != expected.upstream_commit)
    return fail(e, "globalData source pin differs");
  d.owner_ = r.s();
  r.bytes(d.inventory_);
  auto count = r.u(), pending = r.u(), sources = r.u();
  d.first_ = r.u();
  d.next_ = r.u();
  if (!r.ok || !count || count > 128 || !pending || pending > 128 || !sources ||
      sources > 4096)
    return fail(e, "globalData collection bounds rejected");
  std::set<uint32_t> ids;
  std::set<std::string> names;
  bool inventory = false;
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    FieldGlobalDataDeclaration row;
    row.id = r.u();
    row.kind = r.u();
    row.role = r.u();
    row.name = r.s();
    row.native = r.s();
    row.script = r.s();
    auto fields = r.u();
    if (!row.id || !ids.insert(row.id).second || row.name.empty() ||
        !names.insert(row.name).second || row.script.empty() || !fields ||
        fields > 128 || row.kind < 1 || row.kind > 2 || row.role > 2 ||
        (row.kind == 1 && (inventory || row.native != "Object")) ||
        (row.kind == 2 && (row.native != "Reference" || row.role == 0)))
      return fail(e, "globalData declaration identity/native rejected");
    inventory |= row.kind == 2;
    std::set<std::string> keys;
    for (uint32_t f = 0; f < fields && r.ok; ++f) {
      FieldGlobalDataDefault v;
      v.name = r.s();
      v.kind = r.u();
      v.integer_value = r.integer();
      v.string_value = r.s();
      if (v.name.empty() || v.name.front() != '_' ||
          !keys.insert(v.name).second || v.kind < 1 || v.kind > 6 ||
          (v.kind != 2 && v.integer_value != 0) || !v.string_value.empty())
        return fail(e, "globalData native default rejected");
      row.defaults.push_back(std::move(v));
    }
    d.declarations_.push_back(std::move(row));
  }
  std::set<uint32_t> ownerids;
  for (uint32_t role = 0; role < 3; ++role) {
    FieldInventoryOwner o;
    o.id = r.u();
    o.role = r.u();
    o.source_name = r.s();
    if (!o.id || !ownerids.insert(o.id).second || o.role != role ||
        o.source_name.empty())
      return fail(e, "globalData inventory owner rejected");
    d.owners_.push_back(std::move(o));
  }
  for (auto &role : d.roles_)
    role = r.u();
  if (d.roles_ != std::array<uint32_t, 3>{1, 2, 0})
    return fail(e, "globalData LOAD cursor rejected");
  auto normal_fields = r.u();
  if (!normal_fields || normal_fields > 128)
    return fail(e, "globalData NORMAL defaults bounds rejected");
  std::set<std::string> normal_names;
  for (uint32_t i = 0; i < normal_fields && r.ok; ++i) {
    FieldGlobalDataDefault v;
    v.name = r.s();
    v.kind = r.u();
    v.integer_value = r.integer();
    v.string_value = r.s();
    if (v.name.empty() || v.name.front() != '_' ||
        !normal_names.insert(v.name).second || v.kind < 1 || v.kind > 6 ||
        v.integer_value || !v.string_value.empty())
      return fail(e, "globalData NORMAL declaration default rejected");
    d.normal_.push_back(std::move(v));
  }
  auto &character = d.character_;
  character.name = r.s();
  character.nickname = r.s();
  character.exp = r.integer();
  for (auto *rows : {&character.skills, &character.skills_order,
                     &character.slots, &character.stat_slots}) {
    auto size = r.u();
    if (size > 512)
      return fail(e, "globalData character string bounds rejected");
    std::set<std::string> seen;
    for (uint32_t i = 0; i < size && r.ok; ++i) {
      auto value = r.s();
      if (value.empty() || !seen.insert(value).second)
        return fail(e, "globalData character string duplicates rejected");
      rows->push_back(std::move(value));
    }
  }
  auto levels = r.u();
  if (!levels || levels > 1024)
    return fail(e, "globalData character experience bounds rejected");
  for (uint32_t i = 0; i < levels && r.ok; ++i) {
    auto value = r.integer();
    if (value < 0 || (i ? value <= character.exp_levels.back() : value != 0))
      return fail(e, "globalData character experience ordering rejected");
    character.exp_levels.push_back(value);
  }
  auto affinities = r.u();
  if (affinities > 512)
    return fail(e, "globalData character affinity bounds rejected");
  for (uint32_t i = 0; i < affinities && r.ok; ++i) {
    auto name = r.s();
    uint64_t lo = r.u(), hi = r.u(), raw = lo | (hi << 32);
    double value = 0;
    std::memcpy(&value, &raw, sizeof(value));
    if (name.empty() || !std::isfinite(value) || value < 0 ||
        !character.affinities.emplace(name, value).second)
      return fail(e, "globalData character affinity rejected");
  }
  if (character.name.empty() || character.exp < 0 ||
      character.skills_order.empty() || character.slots.size() != 6 ||
      character.stat_slots.size() != 7)
    return fail(e, "globalData character policy incomplete");
  for (const auto &skill : character.skills)
    if (std::find(character.skills_order.begin(), character.skills_order.end(),
                  skill) == character.skills_order.end())
      return fail(e, "globalData character initial skill unknown");
  for (uint32_t i = 0; i < pending && r.ok; ++i) {
    FieldGlobalDataPending v{r.s(), r.s(), r.s()};
    if (v.cursor.empty() || v.source.empty() || v.argument.empty())
      return fail(e, "globalData pending cursor rejected");
    d.pending_.push_back(std::move(v));
  }
  for (uint32_t i = 0; i < sources && r.ok; ++i) {
    auto path = r.s();
    std::array<uint8_t, 32> sha{};
    r.bytes(sha);
    if (path.empty() || !d.sources_.emplace(path, sha).second)
      return fail(e, "globalData source proof rejected");
  }
  if (!r.ok || r.n || d.owner_.empty() || !d.sources_.count(d.owner_) ||
      d.declarations_.size() < 2 || d.declarations_[0].id != d.first_ ||
      d.declarations_[1].id != d.next_)
    return fail(e, "globalData trailing/continuation proof rejected");
  for (const auto &row : d.declarations_)
    if (!d.sources_.count(row.script))
      return fail(e, "globalData declaration script proof missing");
  for (const auto &v : d.pending_)
    if (!d.sources_.count(v.source))
      return fail(e, "globalData pending script proof missing");
  for (const auto *slots : {&character.slots, &character.stat_slots})
    for (const auto &slot : *slots)
      if (std::none_of(d.declarations_.front().defaults.begin(),
                       d.declarations_.front().defaults.end(),
                       [&](const auto &field) { return field.name == slot; }))
        return fail(e, "globalData character native field binding missing");
  for (const auto &o : d.owners_) {
    auto row =
        std::find_if(d.declarations_.begin(), d.declarations_.end(),
                     [&](const auto &v) { return v.name == o.source_name; });
    if (row == d.declarations_.end() ||
        (o.role ? row->kind != 2 || row->role != o.role
                : row->kind != 1 || row->id != d.first_))
      return fail(e, "globalData source owner role mismatch");
  }
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
bool FieldGlobalDataData::load_file(const char *path, const FieldIdentity &id,
                                    std::string &e) {
  auto *f = std::fopen(path, "rb");
  if (!f)
    return fail(e, "Cannot open globalData owner binary");
  std::vector<uint8_t> raw;
  std::array<uint8_t, 4096> b{};
  size_t n;
  while ((n = std::fread(b.data(), 1, b.size(), f))) {
    if (raw.size() + n > 4 * 1024 * 1024) {
      std::fclose(f);
      return fail(e, "globalData file too large");
    }
    raw.insert(raw.end(), b.begin(), b.begin() + n);
  }
  bool ok = !std::ferror(f);
  std::fclose(f);
  return ok ? load(raw.data(), raw.size(), id, e)
            : fail(e, "globalData file read failed");
}
bool FieldGlobalDataData::bind_inventory(const FieldInventoryData &inventory,
                                         const FieldItemDefinitions &defs,
                                         std::string &e) const {
  if (!valid_ || inventory.source_pin() != identity_.upstream_commit ||
      inventory.identity() != inventory_ ||
      inventory.owners().size() != owners_.size() ||
      !inventory.bind_definitions(defs, e))
    return fail(e, "globalData owning inventory identity differs");
  for (size_t i = 0; i < owners_.size(); ++i) {
    const auto &a = owners_[i];
    const auto &b = inventory.owners()[i];
    if (a.id != b.id || a.role != b.role || a.source_name != b.source_name)
      return fail(e, "globalData inventory source owner differs");
  }
  e.clear();
  return true;
}
} // namespace encore::upstream
