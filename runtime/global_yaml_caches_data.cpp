#include "encore/crc32.hpp"
#include "encore/global_yaml_caches.hpp"
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
  return std::any_of(s.begin(), s.end(), [](uint8_t x) { return x != 0; });
}
bool path(std::string_view s) {
  return !s.empty() && s.front() != '/' && s.find("..") == s.npos &&
         s.find_first_of("\\:") == s.npos;
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
    uint32_t v = word(p + at);
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
  std::string text() {
    auto len = u();
    if (!ok || len > 65536 || at > n || n - at < len) {
      ok = false;
      return {};
    }
    std::string s(reinterpret_cast<const char *>(p + at), len);
    at += len;
    std::u32string unicode;
    if (s.find('\0') != s.npos || !utf8_decode(s, unicode))
      ok = false;
    return s;
  }
  std::shared_ptr<GlobalYamlValue> value(uint32_t depth = 0) {
    if (!ok || depth > 64 || ++nodes > 65536) {
      ok = false;
      return {};
    }
    auto v = std::make_shared<GlobalYamlValue>();
    v->kind = u();
    if (v->kind == 0)
      return v;
    if (v->kind == 1) {
      auto x = u();
      if (x > 1)
        ok = false;
      v->boolean = x != 0;
    } else if (v->kind == 2) {
      auto x = bytes<8>();
      std::memcpy(&v->integer, x.data(), 8);
    } else if (v->kind == 3) {
      auto x = bytes<8>();
      std::memcpy(&v->real, x.data(), 8);
      if (!std::isfinite(v->real))
        ok = false;
    } else if (v->kind == 4)
      v->string = text();
    else if (v->kind == 5 || v->kind == 6) {
      auto count = u();
      if (!ok || count > 16384) {
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
    } else
      ok = false;
    return ok ? v : nullptr;
  }
};
} // namespace
bool GlobalYamlCachesData::load(const uint8_t *p, size_t n,
                                const FieldGlobalExternalSpec &expected,
                                std::string &e) {
  if (!p || n < 128 || n > 8388608 || std::memcmp(p, "ENCYAML1", 8) ||
      word(p + 8) != 1 || word(p + 12) != 128 || word(p + 16) != n ||
      word(p + 20) != crc32(p + 128, n - 128) || word(p + 24) != 0x454e004f ||
      word(p + 28) != 1 || word(p + 32) != 1 || !word(p + 36) ||
      word(p + 124) || expected.role != 3 || expected.script.empty())
    return fail(e, "Global YAML cache header/format/capability/CRC rejected");
  GlobalYamlCachesData d;
  d.identity_.scene_id = word(p + 36);
  std::copy_n(p + 40, 20, d.identity_.upstream_commit.begin());
  std::copy_n(p + 60, 32, d.identity_.source_sha256.begin());
  std::copy_n(p + 92, 32, d.ir_.begin());
  if (d.identity_.upstream_commit != expected.identity.upstream_commit ||
      d.identity_.source_sha256 != expected.script_sha || !nonzero(d.ir_))
    return fail(e, "Global YAML cache original source owner identity rejected");
  Reader r{p, n};
  d.owner_ = r.text();
  auto count = r.u();
  if (d.owner_ != expected.script || !path(d.owner_) || count != 6)
    return fail(e, "Global YAML cache source initialization schema rejected");
  std::set<std::string> names, members, dirs;
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    GlobalYamlCachePolicy x;
    x.role = r.u();
    x.root_kind = r.u();
    x.name = r.text();
    x.member = r.text();
    x.directory = r.text();
    x.closure = r.bytes<32>();
    if (x.role != i || (x.root_kind != 5 && x.root_kind != 6) ||
        x.name.empty() || x.member.empty() || !path(x.directory) ||
        x.directory.back() != '/' || !nonzero(x.closure) ||
        !names.insert(x.name).second || !members.insert(x.member).second ||
        !dirs.insert(x.directory).second)
      return fail(e, "Global YAML cache Directory policy/duplicate rejected");
    d.policies_.push_back(std::move(x));
  }
  count = r.u();
  if (count != 10)
    return fail(e, "Global YAML cache source getter coverage rejected");
  std::set<std::string> methods;
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    GlobalYamlGetter x;
    x.method = r.text();
    x.role = r.u();
    x.action = r.u();
    auto truth = r.u();
    x.truthiness = truth != 0;
    x.mutation = r.text();
    x.warning = r.text();
    if (x.role >= 6 || x.role == 4 || x.action < 1 || x.action > 3 ||
        truth > 1 || x.method.empty() || !methods.insert(x.method).second ||
        (x.action != 1 && (!x.mutation.empty() || !x.warning.empty())) ||
        (x.action == 1 &&
         ((d.policies_[x.role].root_kind == 6) != !x.mutation.empty())) ||
        (x.action == 1 && x.warning.find("%s") == x.warning.npos))
      return fail(e, "Global YAML cache unknown getter source shape");
    d.getters_.push_back(std::move(x));
  }
  count = r.u();
  if (count < 8 || count > 8192)
    return fail(e, "Global YAML cache source proof count rejected");
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    auto name = r.text();
    auto hash = r.bytes<32>();
    if (!path(name) || !nonzero(hash) || !d.sources_.emplace(name, hash).second)
      return fail(e, "Global YAML cache source proof rejected");
  }
  auto ownerProof = d.sources_.find(d.owner_);
  if (ownerProof == d.sources_.end() ||
      ownerProof->second != expected.script_sha)
    return fail(e, "Global YAML cache owner proof missing");
  count = r.u();
  if (!count || count > 8192 || d.sources_.size() != size_t(count) + 2)
    return fail(e, "Global YAML cache complete source manifest rejected");
  std::set<std::string> paths;
  std::array<std::set<std::string>, 6> keysets;
  std::array<uint32_t, 6> counts{};
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    GlobalYamlCacheRecord x;
    x.role = r.u();
    x.source = r.text();
    x.name = r.text();
    x.source_sha = r.bytes<32>();
    auto has = r.u();
    if (x.role >= 6 || has > 1 || has != (x.role != 4) || !path(x.source) ||
        x.name.empty() || !path(x.name) || !nonzero(x.source_sha) ||
        !paths.insert(x.source).second ||
        !keysets[x.role].insert(x.name).second)
      return fail(e, "Global YAML cache source record/duplicate rejected");
    const auto &policy = d.policies_[x.role];
    if (x.source != policy.directory + x.name + ".yaml" ||
        !d.sources_.count(x.source) || d.sources_.at(x.source) != x.source_sha)
      return fail(e, "Global YAML cache Directory/YAML identity rejected");
    if (has) {
      x.parsed = r.value();
      if (!x.parsed || x.parsed->kind != policy.root_kind)
        return fail(e, "Global YAML cache original native value/type rejected");
    }
    ++counts[x.role];
    d.records_.push_back(std::move(x));
  }
  if (!r.ok || r.at != n ||
      std::any_of(counts.begin(), counts.end(),
                  [](uint32_t x) { return x == 0; }))
    return fail(e, "Global YAML cache truncated/trailing/incomplete source");
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
bool GlobalYamlCachesData::load_file(const char *name,
                                     const FieldGlobalExternalSpec &s,
                                     std::string &e) {
  if (!name)
    return fail(e, "Global YAML cache resource path absent");
  FILE *f = std::fopen(name, "rb");
  if (!f)
    return fail(e, "Global YAML cache resource open failed");
  bool ok = std::fseek(f, 0, SEEK_END) == 0;
  long n = ok ? std::ftell(f) : -1;
  ok = ok && n >= 128 && n <= 8388608 && std::fseek(f, 0, SEEK_SET) == 0;
  std::vector<uint8_t> b(ok ? size_t(n) : 0);
  ok = ok && std::fread(b.data(), 1, b.size(), f) == b.size();
  std::fclose(f);
  if (!ok)
    return fail(e, "Global YAML cache resource size/read rejected");
  return load(b.data(), b.size(), s, e);
}
bool GlobalYamlCachesData::source_hash(std::string_view path,
                                       std::array<uint8_t, 32> &out) const {
  auto i = sources_.find(std::string(path));
  if (!valid_ || i == sources_.end())
    return false;
  out = i->second;
  return true;
}
std::vector<std::pair<std::string, std::array<uint8_t, 32>>>
GlobalYamlCachesData::expected_paths(uint32_t role) const {
  std::vector<std::pair<std::string, std::array<uint8_t, 32>>> out;
  if (valid_ && role < 6)
    for (const auto &x : records_)
      if (x.role == role)
        out.emplace_back(x.source, x.source_sha);
  return out;
}
} // namespace encore::upstream
