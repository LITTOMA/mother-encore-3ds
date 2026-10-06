#include "encore/crc32.hpp"
#include "encore/global_load.hpp"
#include "encore/utf8.hpp"
#include "global_yaml_file_hash.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
namespace encore::upstream {
namespace {
uint32_t word(const uint8_t *p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 |
         uint32_t(p[3]) << 24;
}
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
bool nonzero(const std::array<uint8_t, 32> &h) {
  return std::any_of(h.begin(), h.end(), [](uint8_t x) { return x != 0; });
}
bool path(std::string_view s) {
  return !s.empty() && s.front() != '/' && s.find("..") == s.npos &&
         s.find_first_of("\\:") == s.npos;
}
bool symbol(std::string_view s) {
  return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
  });
}
struct Reader {
  const uint8_t *p;
  size_t n, at = 128, nodes = 0;
  bool ok = true;
  uint32_t u() {
    if (!ok || at > n || n - at < 4) {
      ok = false;
      return 0;
    }
    auto v = word(p + at);
    at += 4;
    return v;
  }
  template <size_t N> std::array<uint8_t, N> bytes() {
    std::array<uint8_t, N> x{};
    if (!ok || at > n || n - at < N) {
      ok = false;
      return x;
    }
    std::copy_n(p + at, N, x.begin());
    at += N;
    return x;
  }
  double real() {
    auto b = bytes<8>();
    double v = 0;
    std::memcpy(&v, b.data(), 8);
    if (!std::isfinite(v))
      ok = false;
    return v;
  }
  int64_t integer() {
    auto b = bytes<8>();
    int64_t v = 0;
    std::memcpy(&v, b.data(), 8);
    return v;
  }
  std::string text() {
    auto len = u();
    if (!ok || len > 65536 || at > n || n - at < len) {
      ok = false;
      return {};
    }
    std::string s(reinterpret_cast<const char *>(p + at), len);
    at += len;
    std::u32string native;
    if (s.find('\0') != s.npos || !utf8_decode(s, native))
      ok = false;
    return s;
  }
  std::shared_ptr<GlobalYamlValue> value(uint32_t depth = 0) {
    if (!ok || depth > 32 || ++nodes > 16384) {
      ok = false;
      return {};
    }
    auto v = std::make_shared<GlobalYamlValue>();
    v->kind = u();
    if (v->kind == 1) {
      auto b = u();
      if (b > 1)
        ok = false;
      v->boolean = b != 0;
    } else if (v->kind == 2)
      v->integer = integer();
    else if (v->kind == 3)
      v->real = real();
    else if (v->kind == 4)
      v->string = text();
    else if (v->kind == 5 || v->kind == 6) {
      auto count = u();
      if (count > 4096) {
        ok = false;
        return {};
      }
      std::set<std::string> seen;
      for (uint32_t i = 0; i < count && ok; ++i) {
        if (v->kind == 5)
          v->array.push_back(value(depth + 1));
        else {
          auto key = text();
          if (!seen.insert(key).second) {
            ok = false;
            return {};
          }
          v->dictionary.emplace_back(key, value(depth + 1));
        }
      }
    } else if (v->kind != 0)
      ok = false;
    return ok ? v : nullptr;
  }
};

const GlobalYamlValue *get(const GlobalYamlValue &v, const std::string &key) {
  if (v.kind != 6)
    return nullptr;
  for (const auto &entry : v.dictionary)
    if (entry.first == key)
      return entry.second.get();
  return nullptr;
}
bool equal(const GlobalYamlValue &a, const GlobalYamlValue &b) {
  if (a.kind != b.kind)
    return false;
  if (a.kind == 1)
    return a.boolean == b.boolean;
  if (a.kind == 2)
    return a.integer == b.integer;
  if (a.kind == 3)
    return a.real == b.real;
  if (a.kind == 4)
    return a.string == b.string;
  if (a.kind == 5) {
    if (a.array.size() != b.array.size())
      return false;
    for (size_t i = 0; i < a.array.size(); ++i)
      if (!a.array[i] || !b.array[i] || !equal(*a.array[i], *b.array[i]))
        return false;
  }
  if (a.kind == 6) {
    if (a.dictionary.size() != b.dictionary.size())
      return false;
    for (size_t i = 0; i < a.dictionary.size(); ++i)
      if (a.dictionary[i].first != b.dictionary[i].first ||
          !a.dictionary[i].second || !b.dictionary[i].second ||
          !equal(*a.dictionary[i].second, *b.dictionary[i].second))
        return false;
  }
  return true;
}
std::shared_ptr<GlobalYamlValue> copy(const GlobalYamlValue &v) {
  auto out = std::make_shared<GlobalYamlValue>();
  out->kind = v.kind;
  out->boolean = v.boolean;
  out->integer = v.integer;
  out->real = v.real;
  out->string = v.string;
  for (const auto &x : v.array)
    out->array.push_back(copy(*x));
  for (const auto &x : v.dictionary)
    out->dictionary.emplace_back(x.first, copy(*x.second));
  return out;
}
bool merge(GlobalYamlValue &save, const GlobalYamlValue &override,
           uint32_t depth = 0) {
  if (save.kind != 6 || override.kind != 6 || depth > 32)
    return false;
  for (const auto &entry : override.dictionary) {
    auto index =
        std::find_if(save.dictionary.begin(), save.dictionary.end(),
                     [&](const auto &x) { return x.first == entry.first; });
    const auto &next = *entry.second;
    if (index != save.dictionary.end() && index->second->kind == 6 &&
        next.kind == 6) {
      if (!merge(*index->second, next, depth + 1))
        return false;
    } else if (index != save.dictionary.end() && index->second->kind == 5 &&
               next.kind == 5) {
      auto &old = *index->second;
      if (next.array.empty() ||
          (!old.array.empty() &&
           (old.array.front()->kind == 5 || old.array.front()->kind == 6)))
        index->second = copy(next);
      else {
        for (const auto &x : old.array)
          if (x->kind != 4)
            return false;
        for (const auto &x : next.array) {
          if (x->kind != 4)
            return false;
          if (std::none_of(old.array.begin(), old.array.end(),
                           [&](const auto &old) { return equal(*old, *x); }))
            old.array.push_back(copy(*x));
        }
      }
    } else if (index == save.dictionary.end())
      save.dictionary.emplace_back(entry.first, copy(next));
    else
      index->second = copy(next);
  }
  return true;
}
const GlobalDataConstructorDeclaration *
member(const GlobalDataConstructorData &d, const std::string &name) {
  auto it = std::find_if(d.declarations().begin(), d.declarations().end(),
                         [&](const auto &x) { return x.name == name; });
  return it == d.declarations().end() ? nullptr : &*it;
}
} // namespace
bool GlobalLoadData::load(const uint8_t *p, size_t n,
                          const GlobalDataConstructorData &constructor,
                          const FieldCharacterLoadData &characters,
                          const FieldGlobalFlagsData &flags,
                          const FieldItemDefinitions &definitions,
                          std::string &e) {
  if (!constructor.valid() || !characters.valid() || !flags.valid() ||
      !definitions.valid() || !definitions.global_constructor_scope() || !p ||
      n < 128 || n > 1048576 || std::memcmp(p, "ENCGLD01", 8) ||
      word(p + 8) != 1 || word(p + 12) != 128 || word(p + 16) != n ||
      word(p + 20) != crc32(p + 128, n - 128) || word(p + 24) != 0x454e0054 ||
      word(p + 28) != 1 || word(p + 32) != 1 || !word(p + 36) || word(p + 124))
    return fail(e, "Global LOAD header/source capabilities/CRC rejected");
  GlobalLoadData d;
  d.identity_.scene_id = word(p + 36);
  std::copy_n(p + 40, 20, d.identity_.upstream_commit.begin());
  std::copy_n(p + 60, 32, d.identity_.source_sha256.begin());
  std::copy_n(p + 92, 32, d.ir_.begin());
  if (!nonzero(d.ir_) ||
      d.identity_.upstream_commit != constructor.identity().upstream_commit ||
      d.identity_.upstream_commit != characters.identity().upstream_commit ||
      d.identity_.upstream_commit != flags.pin() ||
      d.identity_.upstream_commit != definitions.source_pin())
    return fail(e, "Global LOAD source pin rejected");
  Reader r{p, n};
  d.owner_ = r.text();
  d.cold_source_ = r.text();
  d.new_source_ = r.text();
  d.override_source_ = r.text();
  d.normal_flags_member_ = r.text();
  d.normal_flags_key_ = r.text();
  d.menu_member_ = r.text();
  d.party_key_ = r.text();
  d.constructor_ = r.bytes<32>();
  d.character_ = r.bytes<32>();
  d.flag_ = r.bytes<32>();
  d.file_ = r.bytes<32>();
  if (!path(d.owner_) || !path(d.cold_source_) || !path(d.new_source_) ||
      !path(d.override_source_) || d.cold_source_ == d.new_source_ ||
      d.cold_source_ == d.override_source_ ||
      d.new_source_ == d.override_source_ || !symbol(d.normal_flags_member_) ||
      !symbol(d.normal_flags_key_) || !symbol(d.menu_member_) ||
      !symbol(d.party_key_) || d.constructor_ != constructor.ir_sha256() ||
      d.character_ != characters.ir_sha256() ||
      d.flag_ != flags.content_hash() || !nonzero(d.file_))
    return fail(e, "Global LOAD independent source dependency rejected");
  auto normal = member(constructor, d.normal_flags_member_);
  if (!normal || normal->adapter != 5 || normal->owner_role ||
      d.menu_member_ != constructor.menu_flavor_member())
    return fail(e, "Global LOAD actual flags/UI member rejected");
  auto count = r.u();
  if (!count || count > 4096)
    return fail(e, "Global LOAD source closure count rejected");
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    auto source = r.text();
    auto h = r.bytes<32>();
    std::array<uint8_t, 32> old{};
    if (!path(source) || !nonzero(h) || !d.sources_.emplace(source, h).second ||
        (constructor.source_hash(source, old) && old != h) ||
        (characters.source_hash(source, old) && old != h) ||
        (flags.source_hash(source, old) && old != h) ||
        (definitions.source_hash(source, old) && old != h))
      return fail(e, "Global LOAD original source proof mismatch");
  }
  std::array<uint8_t, 32> ownerhash{}, charhash{}, flaghash{};
  if (!d.source_hash(d.owner_, ownerhash) ||
      ownerhash != d.identity_.source_sha256 ||
      !characters.source_hash(d.owner_, charhash) || charhash != ownerhash ||
      !flags.source_hash(d.owner_, flaghash) || flaghash != ownerhash ||
      !d.sources_.count(d.cold_source_) || !d.sources_.count(d.new_source_) ||
      !d.sources_.count(d.override_source_))
    return fail(e, "Global LOAD owner/save source proof rejected");
  count = r.u();
  if (count != definitions.definitions().size() || !count)
    return fail(e, "Global LOAD complete Item source roster rejected");
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    auto source = r.text();
    auto h = r.bytes<32>();
    std::array<uint8_t, 32> actual{};
    if (!path(source) || !definitions.source_hash(source, actual) ||
        actual != h || !d.sources_.count(source) ||
        d.sources_.at(source) != h || !d.definitions_.emplace(source, h).second)
      return fail(e, "Global LOAD Item independent source binding rejected");
  }
  for (const auto &definition : definitions.definitions())
    if (!d.definitions_.count(definition.source))
      return fail(e, "Global LOAD truncated full Item domain rejected");
  d.cold_ = r.value();
  d.new_ = r.value();
  d.overrides_ = r.value();
  d.merged_ = r.value();
  if (!d.cold_ || !d.new_ || !d.overrides_ || !d.merged_ ||
      d.cold_->kind != 6 || d.new_->kind != 6 || d.overrides_->kind != 6 ||
      d.merged_->kind != 6)
    return fail(e, "Global LOAD original native Dictionary documents rejected");
  auto merged = copy(*d.cold_);
  if (!merge(*merged, *d.overrides_) || !equal(*merged, *d.merged_))
    return fail(e,
                "Global LOAD exact recursive override source witness rejected");
  count = r.u();
  if (count != 3)
    return fail(e, "Global LOAD complete file document count rejected");
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    GlobalLoadDocument doc;
    doc.kind = r.u();
    doc.caller_source = r.text();
    doc.caller_method = r.text();
    doc.getter_source = r.text();
    doc.getter_method = r.text();
    doc.parser_source = r.text();
    doc.parser_method = r.text();
    doc.file.role = r.u();
    doc.file.source = r.text();
    doc.file.bytes = r.text();
    doc.file.sha = r.bytes<32>();
    doc.file.parsed = r.value();
    const auto &expected_source = i == 0   ? d.cold_source_
                                  : i == 1 ? d.new_source_
                                           : d.override_source_;
    const auto &expected_value = i == 0   ? d.cold_
                                 : i == 1 ? d.new_
                                          : d.overrides_;
    std::array<uint8_t, 32> actual{};
    if (doc.kind != i || doc.file.role != i || doc.caller_source != d.owner_ ||
        !symbol(doc.caller_method) ||
        doc.getter_source != constructor.owner_source() ||
        !symbol(doc.getter_method) || !path(doc.parser_source) ||
        !symbol(doc.parser_method) || !d.sources_.count(doc.parser_source) ||
        doc.file.source != expected_source ||
        !d.source_hash(doc.file.source, actual) || actual != doc.file.sha ||
        global_yaml_bytes_sha256(doc.file.bytes) != doc.file.sha ||
        !doc.file.parsed || !equal(*doc.file.parsed, *expected_value))
      return fail(e, "Global LOAD actual File/SmartReader document rejected");
    d.documents_.push_back(std::move(doc));
  }
  count = r.u();
  if (count != 4)
    return fail(e, "Global LOAD serialized Item argument schema rejected");
  std::set<std::string> itemkeys;
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    auto key = r.text();
    if (!symbol(key) || !itemkeys.insert(key).second)
      return fail(e, "Global LOAD duplicate Item argument source key rejected");
    d.saved_item_keys_.push_back(key);
  }
  d.saved_doses_ = r.u();
  auto equipped = r.u();
  d.saved_equipped_ = equipped != 0;
  if (!d.saved_doses_ || d.saved_doses_ > 65535 || equipped > 1)
    return fail(e, "Global LOAD serialized Item defaults rejected");
  count = r.u();
  if (!count || count > 256)
    return fail(e, "Global LOAD scalar count rejected");
  std::set<std::string> assigned;
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    GlobalLoadAssignment a;
    a.kind = r.u();
    a.coercion = r.u();
    a.fallback_kind = r.u();
    a.constant_index = r.u();
    a.member = r.text();
    a.key = r.text();
    a.key_y = r.text();
    a.fallback_member = r.text();
    a.constant = r.text();
    a.fallback = r.value();
    auto actual = member(constructor, a.member);
    if (!actual || actual->constant || actual->adapter == 1 ||
        actual->adapter == 2 || actual->adapter == 3 || actual->adapter == 4 ||
        !assigned.insert(a.member).second || !symbol(a.key) || !a.fallback ||
        a.kind < 1 || a.kind > 2 || a.coercion > 1 || a.fallback_kind < 1 ||
        a.fallback_kind > 3)
      return fail(e, "Global LOAD scalar/source owner schema rejected");
    if (a.kind == 2) {
      if (actual->kind != 7 || !symbol(a.key_y) || a.coercion ||
          a.fallback_kind != 1 ||
          (a.fallback->kind != 2 && a.fallback->kind != 3))
        return fail(e, "Global LOAD Vector2 source schema rejected");
      for (const auto &key : {a.key, a.key_y}) {
        const auto *v = get(*d.merged_, key);
        if (v && v->kind != 2 && v->kind != 3)
          return fail(e, "Global LOAD Vector2 native argument rejected");
      }
    } else if (!a.key_y.empty())
      return fail(e, "Global LOAD unexpected Vector2 argument rejected");
    if (a.fallback_kind == 1) {
      if (!a.fallback_member.empty() || !a.constant.empty() || a.constant_index)
        return fail(e, "Global LOAD literal fallback shape rejected");
    } else if (a.fallback_kind == 2) {
      if (a.fallback_member != a.member || !a.constant.empty() ||
          a.constant_index || a.fallback->kind)
        return fail(e, "Global LOAD live member fallback rejected");
    } else {
      auto c = member(constructor, a.constant);
      if (!a.fallback_member.empty() || a.fallback->kind || !c ||
          !c->constant || c->kind != 5 || !c->value ||
          a.constant_index >= c->value->array.size() ||
          c->value->array[a.constant_index]->kind != actual->kind)
        return fail(e, "Global LOAD original constant fallback rejected");
    }
    if (a.kind == 1) {
      const auto *v = get(*d.merged_, a.key);
      if (!v && a.fallback_kind == 1)
        v = a.fallback.get();
      if (v) {
        if (a.coercion) {
          if (v->kind != 2 && v->kind != 3)
            return fail(e, "Global LOAD unsupported source int cast type");
        } else if (v->kind != actual->kind &&
                   !(actual->kind == 3 && v->kind == 2))
          return fail(e, "Global LOAD actual saved scalar type rejected");
      }
    }
    d.assignments_.push_back(std::move(a));
  }
  count = r.u();
  size_t expected_inventories =
      std::count_if(constructor.objects().begin(), constructor.objects().end(),
                    [](const auto &o) { return o.kind == 2; });
  if (count != expected_inventories)
    return fail(e, "Global LOAD KEY/STORAGE source roster rejected");
  std::set<uint32_t> inventoryids;
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    GlobalLoadInventory inv;
    inv.id = r.u();
    inv.role = r.u();
    inv.member = r.text();
    inv.source_key = r.text();
    inv.method = r.text();
    auto owner =
        std::find_if(constructor.objects().begin(), constructor.objects().end(),
                     [&](const auto &o) { return o.id == inv.id; });
    if (owner == constructor.objects().end() || owner->kind != 2 ||
        owner->role != inv.role || owner->name != inv.member ||
        !inventoryids.insert(inv.id).second || !symbol(inv.source_key) ||
        !symbol(inv.method))
      return fail(e, "Global LOAD existing Inventory ownership rejected");
    const auto *source = get(*d.merged_, inv.source_key);
    auto items = r.u();
    if (items > 4096 ||
        (source && (source->kind != 5 || source->array.size() != items)) ||
        (!source && items))
      return fail(e, "Global LOAD serialized Inventory count/type rejected");
    for (uint32_t j = 0; j < items && r.ok; ++j) {
      FieldCharacterSavedItem item;
      item.name = r.text();
      auto equ = r.u();
      item.equipped = equ != 0;
      item.doses = r.u();
      auto uid = r.u();
      item.has_uid = uid != 0;
      item.uid = r.u();
      if (equ > 1 || uid > 1 || !item.doses || item.doses > 65535 ||
          (!item.has_uid && item.uid) || !definitions.definition(item.name))
        return fail(
            e, "Global LOAD Item outside full checked definitions rejected");
      const auto &raw = *source->array[j];
      if (raw.kind != 6)
        return fail(e, "Global LOAD original saved Item Dictionary rejected");
      for (const auto &field : raw.dictionary)
        if (!itemkeys.count(field.first))
          return fail(e, "Global LOAD unknown original saved Item field");
      const auto *n = get(raw, d.saved_item_keys_[0]);
      const auto *eq = get(raw, d.saved_item_keys_[1]);
      const auto *dose = get(raw, d.saved_item_keys_[2]);
      const auto *u = get(raw, d.saved_item_keys_[3]);
      if (!n || n->kind != 4 || n->string != item.name ||
          (eq && (eq->kind != 1 || eq->boolean != item.equipped)) ||
          (!eq && item.equipped != d.saved_equipped_) ||
          (dose && (dose->kind != 2 || dose->integer != item.doses)) ||
          (!dose && item.doses != d.saved_doses_) ||
          (u != nullptr) != item.has_uid ||
          (u && (u->kind != 2 || u->integer < 0 ||
                 uint64_t(u->integer) != item.uid)))
        return fail(e,
                    "Global LOAD exact saved Item source arguments rejected");
      inv.items.push_back(std::move(item));
    }
    d.inventories_.push_back(std::move(inv));
  }
  count = r.u();
  if (count != characters.rows().size())
    return fail(e,
                "Global LOAD complete eight-character source roster rejected");
  std::set<uint32_t> characterids;
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    GlobalLoadCharacter c;
    c.id = r.u();
    c.role = r.u();
    c.name = r.text();
    c.saved = r.value();
    const auto &row = characters.rows()[i];
    const auto *saved = get(*d.merged_, c.name);
    auto owner =
        std::find_if(constructor.objects().begin(), constructor.objects().end(),
                     [&](const auto &o) { return o.id == c.id; });
    if (owner == constructor.objects().end() || owner->kind != 1 ||
        owner->role != c.role || owner->name != c.name || c.id != row.id ||
        c.role != row.role || c.name != row.name || !c.saved ||
        c.saved->kind != 6 || !saved || !equal(*saved, *c.saved) ||
        !characterids.insert(c.id).second)
      return fail(e,
                  "Global LOAD existing Character/typed cold cursor rejected");
    d.characters_.push_back(std::move(c));
  }
  if (characterids.size() + inventoryids.size() != constructor.objects().size())
    return fail(e, "Global LOAD actual object ownership closure rejected");
  count = r.u();
  size_t playable =
      std::count_if(constructor.objects().begin(), constructor.objects().end(),
                    [](const auto &o) { return o.kind == 1 && o.role == 0; });
  if (count != playable)
    return fail(e, "Global LOAD original playable constants roster rejected");
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    auto name = r.text();
    auto owner =
        std::find_if(constructor.objects().begin(), constructor.objects().end(),
                     [&](const auto &o) {
                       return o.name == name && o.kind == 1 && o.role == 0;
                     });
    if (owner == constructor.objects().end() ||
        std::find(d.playable_.begin(), d.playable_.end(), name) !=
            d.playable_.end())
      return fail(e, "Global LOAD foreign playable constant rejected");
    d.playable_.push_back(name);
  }
  count = r.u();
  const auto *party = get(*d.merged_, d.party_key_);
  if (!party || party->kind != 5 || party->array.size() != count || count > 256)
    return fail(e, "Global LOAD saved party array rejected");
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    GlobalLoadPartyReference x;
    x.id = r.u();
    auto player = r.u();
    x.playable = player != 0;
    x.name = r.text();
    const auto &raw = *party->array[i];
    auto row = std::find_if(
        d.characters_.begin(), d.characters_.end(),
        [&](const auto &c) { return c.id == x.id && c.name == x.name; });
    if (player > 1 || raw.kind != 4 || raw.string != x.name ||
        row == d.characters_.end() ||
        x.playable != (std::find(d.playable_.begin(), d.playable_.end(),
                                 x.name) != d.playable_.end()))
      return fail(e,
                  "Global LOAD original ordered party classification rejected");
    d.party_.push_back(std::move(x));
  }
  count = r.u();
  if (count != flags.constructor().size())
    return fail(e, "Global LOAD registered normal flag source roster rejected");
  auto profile =
      std::find_if(flags.profiles().begin(), flags.profiles().end(),
                   [&](const auto &x) { return x.source == d.cold_source_; });
  if (profile == flags.profiles().end() ||
      profile->flags.normal.size() != count)
    return fail(e, "Global LOAD cold source flags profile rejected");
  const auto *savedflags = get(*d.merged_, d.normal_flags_key_);
  if (savedflags && savedflags->kind != 6)
    return fail(e, "Global LOAD saved normal flags Dictionary rejected");
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    auto name = r.text();
    auto value = r.u();
    const auto *saved = savedflags ? get(*savedflags, name) : nullptr;
    if (value > 1 || name != flags.constructor()[i].first ||
        name != profile->flags.normal[i].first ||
        (value != 0) != profile->flags.normal[i].second ||
        (saved && (saved->kind != 1 || saved->boolean != (value != 0))) ||
        (!saved && value))
      return fail(
          e, "Global LOAD exact source registered flags order/value rejected");
    d.flags_.emplace_back(name, value != 0);
  }
  count = r.u();
  if (count != 1 + d.assignments_.size() + d.inventories_.size() + 7)
    return fail(e, "Global LOAD complete operation sequence rejected");
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    GlobalLoadStep s;
    s.kind = r.u();
    s.index = r.u();
    s.owner = r.text();
    s.method = r.text();
    s.member = r.text();
    if (s.kind < 1 || s.kind > 10 || !d.sources_.count(s.owner) ||
        (!s.method.empty() && !symbol(s.method)) ||
        (!s.member.empty() && !symbol(s.member)))
      return fail(e, "Global LOAD unknown source operation rejected");
    if (i == 0) {
      if (s.kind != 1 || s.index || s.owner != d.owner_ || s.method.empty() ||
          !s.member.empty())
        return fail(e, "Global LOAD override-first cursor rejected");
    } else if (i <= d.assignments_.size()) {
      const auto &a = d.assignments_[i - 1];
      if (s.kind != 2 || s.index != i - 1 ||
          s.owner != constructor.owner_source() || !s.method.empty() ||
          s.member != a.member)
        return fail(e, "Global LOAD actual scalar cursor order rejected");
    } else if (i <= d.assignments_.size() + d.inventories_.size()) {
      auto at = i - 1 - d.assignments_.size();
      const auto &inv = d.inventories_[at];
      auto owner = std::find_if(constructor.objects().begin(),
                                constructor.objects().end(),
                                [&](const auto &o) { return o.id == inv.id; });
      if (s.kind != 3 || s.index != at || s.owner != owner->script ||
          s.method != inv.method || s.member != inv.member)
        return fail(e, "Global LOAD actual Inventory cursor order rejected");
    } else {
      auto expected = 4 + i - 1 - d.assignments_.size() - d.inventories_.size();
      if (s.kind != expected || s.index)
        return fail(e,
                    "Global LOAD Character/party/UI/flags/goto order rejected");
      if (s.kind == 8 && s.member != d.menu_member_)
        return fail(e, "Global LOAD actual UI flavor bridge rejected");
      if (s.kind == 9 &&
          (s.owner != constructor.owner_source() ||
           s.member != d.normal_flags_member_ || !s.method.empty()))
        return fail(e, "Global LOAD normal flags tail rejected");
    }
    d.steps_.push_back(std::move(s));
  }
  count = r.u();
  if (count != 4)
    return fail(e, "Global LOAD actual source predecessor schema rejected");
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    GlobalLoadPrecondition x;
    x.kind = r.u();
    x.source = r.text();
    x.method = r.text();
    x.member = r.text();
    if (x.kind != i + 1 || !d.sources_.count(x.source) || !symbol(x.method) ||
        !symbol(x.member))
      return fail(e, "Global LOAD actual source predecessor rejected");
    if (i == 0 && x.source != d.owner_)
      return fail(
          e, "Global LOAD actual player initialization predecessor rejected");
    if ((i == 1 || i == 3) && x.source != constructor.owner_source())
      return fail(e, "Global LOAD actual globaldata predecessor rejected");
    if (i == 2 && x.source != d.steps_[d.steps_.size() - 3].owner)
      return fail(
          e, "Global LOAD actual Ui Shader constructor predecessor rejected");
    d.preconditions_.push_back(std::move(x));
  }
  if (!r.ok || r.at != n)
    return fail(e,
                "Global LOAD truncated/trailing/unknown source data rejected");
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
bool GlobalLoadData::source_hash(std::string_view p,
                                 std::array<uint8_t, 32> &out) const {
  auto i = sources_.find(std::string(p));
  if (i == sources_.end())
    return false;
  out = i->second;
  return true;
}
bool GlobalLoadData::load_file(const char *path,
                               const GlobalDataConstructorData &constructor,
                               const FieldCharacterLoadData &characters,
                               const FieldGlobalFlagsData &flags,
                               const FieldItemDefinitions &definitions,
                               std::string &e) {
  if (!path)
    return fail(e, "Global LOAD null resource path");
  FILE *f = std::fopen(path, "rb");
  if (!f)
    return fail(e, "Cannot open global LOAD resource");
  if (std::fseek(f, 0, SEEK_END)) {
    std::fclose(f);
    return fail(e, "Cannot seek global LOAD resource");
  }
  long size = std::ftell(f);
  if (size < 128 || size > 1048576 || std::fseek(f, 0, SEEK_SET)) {
    std::fclose(f);
    return fail(e, "Global LOAD resource size rejected");
  }
  std::vector<uint8_t> b(size);
  bool ok = std::fread(b.data(), 1, b.size(), f) == b.size();
  std::fclose(f);
  return ok ? load(b.data(), b.size(), constructor, characters, flags,
                   definitions, e)
            : fail(e, "Cannot read global LOAD resource");
}
} // namespace encore::upstream
