#include "encore/field_global_flags.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <set>

namespace encore::upstream {
namespace {
uint32_t u(const uint8_t *p) {
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
bool fail(std::string &e) {
  e = "Global flag source format/identity rejected";
  return false;
}
bool text(std::string_view s) {
  size_t n = 0;
  return s.size() <= 4096 && s.find('\0') == s.npos && encore::utf8_count(s, n);
}
struct Reader {
  const uint8_t *p;
  size_t n, at = 0;
  bool ok = true;
  uint32_t word() {
    if (!ok || at > n || n - at < 4) {
      ok = false;
      return 0;
    }
    auto v = u(p + at);
    at += 4;
    return v;
  }
  std::string string() {
    auto length = word();
    if (!ok || length > 4096 || at > n || length > n - at) {
      ok = false;
      return {};
    }
    std::string s(reinterpret_cast<const char *>(p + at), length);
    at += length;
    if (!text(s))
      ok = false;
    return s;
  }
  template <size_t N> void hash(std::array<uint8_t, N> &h) {
    if (!ok || at > n || N > n - at) {
      ok = false;
      return;
    }
    std::copy_n(p + at, N, h.begin());
    at += N;
    if (std::all_of(h.begin(), h.end(), [](auto c) { return c == 0; }))
      ok = false;
  }
  bool boolean() {
    auto v = word();
    if (v > 1)
      ok = false;
    return v != 0;
  }
  FieldFlagDictionary dictionary() {
    auto count = word();
    FieldFlagDictionary result;
    std::set<std::string> keys;
    if (!ok || count > 16384) {
      ok = false;
      return {};
    }
    for (uint32_t i = 0; i < count && ok; ++i) {
      auto s = string();
      auto v = boolean();
      if (!keys.insert(s).second)
        ok = false;
      result.emplace_back(std::move(s), v);
    }
    return result;
  }
};
} // namespace
bool FieldGlobalFlagsData::load(const uint8_t *p, size_t n, std::string &e) {
  if (!p || n < 96 || n > 4 * 1024 * 1024 || std::memcmp(p, "ENCFGS01", 8) ||
      u(p + 8) != 1 || u(p + 12) != n || u(p + 16) != crc(p + 32, n - 32) ||
      u(p + 20) != 0x454e0047 || u(p + 24) != 1 || u(p + 28) != 1)
    return fail(e);
  Reader r{p + 32, n - 32};
  std::array<uint8_t, 20> pin{};
  std::array<uint8_t, 32> content{};
  r.hash(pin);
  r.hash(content);
  auto owner = r.string();
  auto count = r.word(), profile_count = r.word(), source_count = r.word();
  if (!r.ok || !count || count > 4096 || profile_count != 2 || !source_count ||
      source_count > 1024)
    return fail(e);
  FieldFlagDictionary registered;
  std::set<std::string> names;
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    auto s = r.string();
    auto v = r.boolean();
    if (s.empty() ||
        !std::all_of(s.begin(), s.end(),
                     [](char c) {
                       return (c >= 'a' && c <= 'z') ||
                              (c >= '0' && c <= '9') || c == '_';
                     }) ||
        !names.insert(s).second)
      return fail(e);
    registered.emplace_back(std::move(s), v);
  }
  std::vector<FieldFlagSourceProfile> profiles;
  std::set<std::string> sources;
  for (uint32_t i = 0; i < profile_count && r.ok; ++i) {
    FieldFlagSourceProfile profile;
    profile.source = r.string();
    if (profile.source.empty() || !sources.insert(profile.source).second)
      return fail(e);
    for (const auto &row : registered)
      profile.flags.normal.emplace_back(row.first, r.boolean());
    profile.flags.objects = r.dictionary();
    profile.flags.seen = r.dictionary();
    profiles.push_back(std::move(profile));
  }
  std::map<std::string, std::array<uint8_t, 32>> hashes;
  for (uint32_t i = 0; i < source_count && r.ok; ++i) {
    auto s = r.string();
    std::array<uint8_t, 32> h{};
    r.hash(h);
    if (s.empty() || !hashes.emplace(s, h).second)
      return fail(e);
  }
  if (!r.ok || r.at != r.n)
    return fail(e);
  if (owner.empty() || !hashes.count(owner))
    return fail(e);
  for (const auto &profile : profiles)
    if (!hashes.count(profile.source))
      return fail(e);
  pin_ = pin;
  content_ = content;
  constructor_ = std::move(registered);
  owner_source_ = std::move(owner);
  profiles_ = std::move(profiles);
  sources_ = std::move(hashes);
  valid_ = true;
  e.clear();
  return true;
}
bool FieldGlobalFlagsData::load_file(const char *path, std::string &e) {
  if (!path)
    return fail(e);
  std::ifstream f(path, std::ios::binary | std::ios::ate);
  auto n = f.tellg();
  if (!f || n < 96 || n > 4 * 1024 * 1024)
    return fail(e);
  std::vector<uint8_t> b(static_cast<size_t>(n));
  f.seekg(0);
  if (!f.read(reinterpret_cast<char *>(b.data()), n))
    return fail(e);
  return load(b.data(), b.size(), e);
}
bool FieldGlobalFlagsData::source_hash(std::string_view s,
                                       std::array<uint8_t, 32> &h) const {
  auto i = sources_.find(std::string(s));
  if (i == sources_.end())
    return false;
  h = i->second;
  return true;
}
} // namespace encore::upstream
