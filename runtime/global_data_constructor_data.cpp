#include "encore/crc32.hpp"
#include "encore/global_data_constructor.hpp"
#include "encore/utf8.hpp"
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
bool nonzero(const std::array<uint8_t, 32> &s) {
  return std::any_of(s.begin(), s.end(), [](uint8_t v) { return v != 0; });
}
bool path(std::string_view s) {
  return !s.empty() && s.front() != '/' && s.find("..") == s.npos &&
         s.find_first_of("\\:") == s.npos;
}
bool symbol(std::string_view s) {
  if (s.empty())
    return false;
  return std::all_of(s.begin(), s.end(), [](unsigned char c) {
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
bool equal(const FieldGlobalDataDefault &a, const FieldGlobalDataDefault &b) {
  return a.name == b.name && a.kind == b.kind &&
         a.string_value == b.string_value && a.integer_value == b.integer_value;
}
bool empty_dictionary(const GlobalDataConstructorDeclaration &x) {
  return x.kind == 6 && x.value && x.value->kind == 6 &&
         x.value->dictionary.empty();
}
} // namespace
bool GlobalDataConstructorData::load(const uint8_t *p, size_t n,
                                     const FieldGlobalDataData &legacy,
                                     const GlobalYamlCachesData &cache,
                                     std::string &e) {
  if (!legacy.valid() || !cache.valid() || !p || n < 128 || n > 1048576 ||
      std::memcmp(p, "ENCGDC01", 8) || word(p + 8) != 1 ||
      word(p + 12) != 128 || word(p + 16) != n ||
      word(p + 20) != crc32(p + 128, n - 128) || word(p + 24) != 0x454e0053 ||
      word(p + 28) != 1 || word(p + 32) != 1 || !word(p + 36) || word(p + 124))
    return fail(e, "Global constructor header/format/capability/CRC rejected");
  GlobalDataConstructorData d;
  d.identity_.scene_id = word(p + 36);
  std::copy_n(p + 40, 20, d.identity_.upstream_commit.begin());
  std::copy_n(p + 60, 32, d.identity_.source_sha256.begin());
  std::copy_n(p + 92, 32, d.ir_.begin());
  if (!nonzero(d.ir_) ||
      d.identity_.upstream_commit != legacy.identity().upstream_commit ||
      d.identity_.upstream_commit != cache.identity().upstream_commit ||
      d.identity_.source_sha256 != cache.identity().source_sha256)
    return fail(e, "Global constructor original owner identity rejected");
  Reader r{p, n};
  d.owner_ = r.text();
  d.legacy_ = r.bytes<32>();
  d.cache_ = r.bytes<32>();
  d.engine_ = r.bytes<32>();
  d.menu_flavor_member_ = r.text();
  if (d.owner_ != legacy.owner_source() || d.owner_ != cache.owner_source() ||
      d.legacy_ != legacy.ir_sha256() || d.cache_ != cache.ir_sha256() ||
      !nonzero(d.engine_) || !symbol(d.menu_flavor_member_))
    return fail(e,
                "Global constructor cache/object source dependencies rejected");
  // The older members pack retains its caller's scene identity. Its own
  // checked script proof is the independent constructor-owner identity.
  std::array<uint8_t, 32> legacy_owner{}, cache_owner{};
  if (!legacy.source_hash(d.owner_, legacy_owner) ||
      !cache.source_hash(d.owner_, cache_owner) ||
      legacy_owner != d.identity_.source_sha256 ||
      cache_owner != d.identity_.source_sha256)
    return fail(e,
                "Global constructor independent owner source proof rejected");
  auto count = r.u();
  if (!count || count > 1024)
    return fail(e, "Global constructor source proof count rejected");
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    auto name = r.text();
    auto h = r.bytes<32>();
    std::array<uint8_t, 32> old{};
    if (!path(name) || !nonzero(h) || !d.sources_.emplace(name, h).second ||
        (legacy.source_hash(name, old) && h != old) ||
        (cache.source_hash(name, old) && h != old))
      return fail(e, "Global constructor source proof mismatch");
  }
  std::array<uint8_t, 32> ownerhash{};
  if (!d.source_hash(d.owner_, ownerhash) ||
      ownerhash != d.identity_.source_sha256)
    return fail(e, "Global constructor owner source proof missing");
  count = r.u();
  if (count != legacy.declarations().size())
    return fail(e, "Global constructor complete object roster rejected");
  std::set<uint32_t> ids;
  std::set<std::string> names;
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    GlobalDataConstructorObject o;
    o.id = r.u();
    o.kind = r.u();
    o.role = r.u();
    o.name = r.text();
    o.native = r.text();
    o.script = r.text();
    const auto &old = legacy.declarations()[i];
    if (o.id != old.id || o.kind != old.kind || o.role != old.role ||
        o.name != old.name || o.native != old.native ||
        o.script != old.script || !ids.insert(o.id).second ||
        !names.insert(o.name).second || !d.sources_.count(o.script))
      return fail(e,
                  "Global constructor real Object/Reference identity rejected");
    auto fields = r.u();
    if (fields < old.defaults.size() || fields > 256)
      return fail(e, "Global constructor empty native defaults rejected");
    std::set<std::string> fieldnames;
    for (uint32_t j = 0; j < fields && r.ok; ++j) {
      FieldGlobalDataDefault f;
      f.name = r.text();
      f.kind = r.u();
      f.string_value = r.text();
      f.integer_value = r.integer();
      if (!symbol(f.name) || f.kind < 1 || f.kind > 6 ||
          !fieldnames.insert(f.name).second ||
          (j < old.defaults.size() && !equal(f, old.defaults[j])) ||
          (j >= old.defaults.size() &&
           (o.kind != 1 || o.role != 0 || f.kind != 5 ||
            !f.string_value.empty() || f.integer_value)))
        return fail(
            e, "Global constructor native defaults or extra property rejected");
      o.defaults.push_back(std::move(f));
    }
    auto getters = r.u();
    if (getters != fields - old.defaults.size())
      return fail(e, "Global constructor property/getter closure rejected");
    for (uint32_t j = 0; j < getters && r.ok; ++j) {
      auto name = r.text();
      auto getter = r.text();
      if (name != o.defaults[old.defaults.size() + j].name || !symbol(getter))
        return fail(e, "Global constructor public getter source rejected");
      o.default_getters.emplace_back(name, getter);
    }
    d.objects_.push_back(std::move(o));
  }
  count = r.u();
  if (!count || count > 512)
    return fail(e, "Global constructor declaration count rejected");
  std::set<std::string> members;
  std::set<uint32_t> references, cache_roles, flag_roles;
  uint32_t character_maps = 0, key_maps = 0, god_members = 0;
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    GlobalDataConstructorDeclaration x;
    x.name = r.text();
    x.type_hint = r.text();
    x.setter = r.text();
    x.getter = r.text();
    auto constant = r.u();
    x.constant = constant != 0;
    x.kind = r.u();
    x.adapter = r.u();
    x.owner_role = r.u();
    x.reference_id = r.u();
    if (!symbol(x.name) || !members.insert(x.name).second || constant > 1 ||
        x.kind > 9 || x.adapter > 6 ||
        (!x.type_hint.empty() && !symbol(x.type_hint)) ||
        (!x.setter.empty() && !symbol(x.setter)) ||
        (!x.getter.empty() && !symbol(x.getter)))
      return fail(e, "Global constructor declaration/source property rejected");
    if (x.kind <= 6) {
      x.value = r.value();
      if (!x.value || x.value->kind != x.kind)
        return fail(e, "Global constructor native Variant rejected");
    } else if (x.kind == 7)
      x.vector = {r.real(), r.real()};
    else if (x.kind == 9) {
      auto length = r.u();
      if (length > d.objects_.size())
        return fail(e, "Global constructor character map count rejected");
      std::set<std::string> keys;
      for (uint32_t j = 0; j < length && r.ok; ++j) {
        auto name = r.text();
        auto id = r.u();
        if (!keys.insert(name).second)
          return fail(e, "Global constructor duplicate character map key");
        x.references.emplace_back(name, id);
      }
    }
    if ((x.kind == 8) != (x.adapter == 2) ||
        (x.kind == 9) != (x.adapter == 1) ||
        (x.reference_id != 0) != (x.adapter == 2) ||
        ((x.adapter == 0 || x.adapter == 1 || x.adapter == 2 ||
          x.adapter == 3 || x.adapter == 6) &&
         x.owner_role))
      return fail(
          e, "Global constructor declaration native owner adapter rejected");
    if (x.adapter == 1) {
      ++character_maps;
      size_t at = 0;
      for (const auto &o : d.objects_)
        if (o.kind == 1) {
          if (at >= x.references.size() ||
              x.references[at] != std::make_pair(o.name, o.id) ||
              !references.insert(o.id).second)
            return fail(e, "Global constructor original ordered character "
                           "references rejected");
          ++at;
        }
      if (at != x.references.size() || x.constant)
        return fail(e,
                    "Global constructor character reference closure rejected");
    } else if (x.adapter == 2) {
      auto o =
          std::find_if(d.objects_.begin(), d.objects_.end(),
                       [&](const auto &o) { return o.id == x.reference_id; });
      if (o == d.objects_.end() || o->kind != 2 || o->name != x.name ||
          x.constant || !references.insert(o->id).second)
        return fail(e, "Global constructor Inventory reference rejected");
    } else if (x.adapter == 3) {
      ++god_members;
      if (x.kind != 0 || x.constant || x.type_hint != "Inventory")
        return fail(
            e, "Global constructor deferred Inventory native default rejected");
    } else if (x.adapter == 4) {
      if (x.owner_role >= cache.policies().size() ||
          !cache_roles.insert(x.owner_role).second || !empty_dictionary(x) ||
          !x.constant || x.name != cache.policies()[x.owner_role].member)
        return fail(e, "Global constructor actual cache owner rejected");
    } else if (x.adapter == 5) {
      if (x.owner_role > 2 || !flag_roles.insert(x.owner_role).second ||
          !empty_dictionary(x) || x.constant)
        return fail(e, "Global constructor actual flags Dictionary rejected");
    } else if (x.adapter == 6) {
      ++key_maps;
      if (!empty_dictionary(x) || x.constant)
        return fail(e, "Global constructor owned keys Dictionary rejected");
    }
    d.declarations_.push_back(std::move(x));
  }
  if (character_maps != 1 || key_maps != 1 || god_members != 1 ||
      references != ids || cache_roles.size() != cache.policies().size() ||
      flag_roles.size() != 3)
    return fail(
        e,
        "Global constructor complete declaration ownership closure rejected");
  for (uint32_t stage = 0; stage < 2 && r.ok; ++stage) {
    count = r.u();
    if (count != (stage ? 1 : cache.policies().size() + 1))
      return fail(e, "Global constructor lifecycle operation count rejected");
    for (uint32_t i = 0; i < count && r.ok; ++i) {
      GlobalDataConstructorStep s;
      s.kind = r.u();
      s.role = r.u();
      s.method = r.text();
      s.member = r.text();
      s.argument = r.text();
      if (!symbol(s.method))
        return fail(e, "Global constructor source lifecycle method rejected");
      if (!stage && !i) {
        if (s.kind != 1 || s.role || !s.member.empty() || !s.argument.empty())
          return fail(e,
                      "Global constructor flags-first source cursor rejected");
      } else if (!stage) {
        const auto &policy = cache.policies()[i - 1];
        if (s.kind != 2 || s.role != policy.role || s.member != policy.member ||
            s.argument != "res://" + policy.directory)
          return fail(
              e, "Global constructor actual six-cache source order rejected");
      } else {
        auto member = std::find_if(
            d.declarations_.begin(), d.declarations_.end(), [&](const auto &x) {
              return x.name == s.member && x.adapter == 3;
            });
        if (s.kind != 3 || s.role != 3 || !s.argument.empty() ||
            member == d.declarations_.end())
          return fail(
              e, "Global constructor actual GodStorage Ready cursor rejected");
      }
      (stage ? d.ready_ : d.constructor_).push_back(std::move(s));
    }
  }
  auto &s = d.speed_;
  s.member = r.text();
  s.constant_name = r.text();
  s.setter = r.text();
  s.getter = r.text();
  s.fallback_divisor = r.u();
  s.comparison = r.u();
  s.threshold = r.real();
  s.closest_initial = r.real();
  s.delta_initial = r.real();
  count = r.u();
  if (!count || count > 256 || !s.fallback_divisor || s.comparison != 1 ||
      !(s.delta_initial > 0))
    return fail(e, "Global constructor source text-speed policy rejected");
  for (uint32_t i = 0; i < count && r.ok; ++i)
    s.speeds.push_back(r.real());
  auto member = std::find_if(d.declarations_.begin(), d.declarations_.end(),
                             [&](const auto &x) { return x.name == s.member; });
  auto choices =
      std::find_if(d.declarations_.begin(), d.declarations_.end(),
                   [&](const auto &x) { return x.name == s.constant_name; });
  auto menu = std::find_if(
      d.declarations_.begin(), d.declarations_.end(),
      [&](const auto &x) { return x.name == d.menu_flavor_member_; });
  if (member == d.declarations_.end() || member->kind != 3 || member->adapter ||
      member->constant || member->setter != s.setter ||
      member->getter != s.getter || !symbol(s.setter) || !symbol(s.getter) ||
      choices == d.declarations_.end() || choices->kind != 5 ||
      !choices->constant || !choices->value ||
      choices->value->array.size() != s.speeds.size() ||
      menu == d.declarations_.end() || menu->kind != 4 || menu->constant ||
      menu->adapter)
    return fail(
        e, "Global constructor text-speed/menu real member binding rejected");
  for (size_t i = 0; i < s.speeds.size(); ++i) {
    const auto &v = choices->value->array[i];
    if (!v || v->kind != 3 || v->real != s.speeds[i])
      return fail(e,
                  "Global constructor exact native text-speed data rejected");
  }
  for (const auto &x : d.declarations_)
    if ((!x.setter.empty() || !x.getter.empty()) && x.name != s.member)
      return fail(e, "Global constructor unsupported property method rejected");
  if (!r.ok || r.at != n)
    return fail(
        e, "Global constructor truncated/trailing/unknown values rejected");
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
bool GlobalDataConstructorData::source_hash(
    std::string_view p, std::array<uint8_t, 32> &out) const {
  auto i = sources_.find(std::string(p));
  if (i == sources_.end())
    return false;
  out = i->second;
  return true;
}
bool GlobalDataConstructorData::load_file(const char *path,
                                          const FieldGlobalDataData &legacy,
                                          const GlobalYamlCachesData &cache,
                                          std::string &e) {
  if (!path)
    return fail(e, "Global constructor null resource path");
  FILE *f = std::fopen(path, "rb");
  if (!f)
    return fail(e, "Cannot open global constructor resource");
  if (std::fseek(f, 0, SEEK_END)) {
    std::fclose(f);
    return fail(e, "Cannot seek global constructor resource");
  }
  long size = std::ftell(f);
  if (size < 128 || size > 1048576 || std::fseek(f, 0, SEEK_SET)) {
    std::fclose(f);
    return fail(e, "Global constructor resource size rejected");
  }
  std::vector<uint8_t> b(size);
  bool ok = std::fread(b.data(), 1, b.size(), f) == b.size();
  std::fclose(f);
  return ok ? load(b.data(), b.size(), legacy, cache, e)
            : fail(e, "Cannot read global constructor resource");
}
} // namespace encore::upstream
