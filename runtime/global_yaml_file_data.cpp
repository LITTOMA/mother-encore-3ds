#include "encore/global_yaml_file.hpp"
#include "encore/utf8.hpp"
#include "global_yaml_file_hash.hpp"
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
  while (n--) {
    c ^= *p++;
    for (unsigned j = 0; j < 8; ++j)
      c = (c >> 1) ^ (0xedb88320u & uint32_t(-int(c & 1)));
  }
  return ~c;
}
struct Reader {
  const uint8_t *p;
  size_t n;
  bool ok = true;
  size_t nodes = 0;
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
  uint32_t count(uint32_t limit = 65536) {
    auto v = u();
    if (v > limit)
      ok = false;
    return ok ? v : 0;
  }
  std::string bytes(uint32_t limit) {
    auto k = u();
    if (k > limit || k > n) {
      ok = false;
      return {};
    }
    std::string s(reinterpret_cast<const char *>(p), k);
    p += k;
    n -= k;
    return s;
  }
  std::string s(uint32_t limit = 65536) {
    auto v = bytes(limit);
    size_t chars = 0;
    if (v.find('\0') != v.npos || !encore::utf8_count(v, chars))
      ok = false;
    return v;
  }
  std::array<uint8_t, 32> hash() {
    std::array<uint8_t, 32> v{};
    if (n < 32) {
      ok = false;
      return v;
    }
    std::memcpy(v.data(), p, 32);
    p += 32;
    n -= 32;
    if (std::all_of(v.begin(), v.end(), [](uint8_t x) { return x == 0; }))
      ok = false;
    return v;
  }
  std::shared_ptr<GlobalYamlValue> value(unsigned depth = 0) {
    if (depth > 64 || ++nodes > 1000000) {
      ok = false;
      return {};
    }
    auto v = std::make_shared<GlobalYamlValue>();
    v->kind = u();
    switch (v->kind) {
    case 0:
      break;
    case 1: {
      auto b = u();
      if (b > 1)
        ok = false;
      v->boolean = b != 0;
      break;
    }
    case 2: {
      uint64_t b = u();
      b |= uint64_t(u()) << 32;
      v->integer = int64_t(b);
      break;
    }
    case 3: {
      uint64_t b = u();
      b |= uint64_t(u()) << 32;
      std::memcpy(&v->real, &b, 8);
      if (!std::isfinite(v->real))
        ok = false;
      break;
    }
    case 4:
      v->string = s();
      break;
    case 5: {
      auto k = count();
      while (k-- && ok)
        v->array.push_back(value(depth + 1));
      break;
    }
    case 6: {
      auto k = count();
      std::set<std::string> keys;
      while (k-- && ok) {
        auto key = s();
        if (!keys.insert(key).second)
          ok = false;
        auto child = value(depth + 1);
        v->dictionary.emplace_back(key, child);
      }
      break;
    }
    default:
      ok = false;
    }
    return ok ? v : nullptr;
  }
};
bool equal(const GlobalYamlValue &a, const GlobalYamlValue &b) {
  if (a.kind != b.kind)
    return false;
  switch (a.kind) {
  case 0:
    return true;
  case 1:
    return a.boolean == b.boolean;
  case 2:
    return a.integer == b.integer;
  case 3:
    return std::memcmp(&a.real, &b.real, sizeof(a.real)) == 0;
  case 4:
    return a.string == b.string;
  case 5:
    if (a.array.size() != b.array.size())
      return false;
    for (size_t i = 0; i < a.array.size(); ++i)
      if (!a.array[i] || !b.array[i] || !equal(*a.array[i], *b.array[i]))
        return false;
    return true;
  case 6:
    if (a.dictionary.size() != b.dictionary.size())
      return false;
    for (size_t i = 0; i < a.dictionary.size(); ++i) {
      const auto &x = a.dictionary[i];
      const auto &y = b.dictionary[i];
      if (x.first != y.first || !x.second || !y.second ||
          !equal(*x.second, *y.second))
        return false;
    }
    return true;
  default:
    return false;
  }
}
} // namespace
bool GlobalYamlFileData::source_hash(std::string_view p,
                                     std::array<uint8_t, 32> &out) const {
  auto i = sources_.find(std::string(p));
  if (i == sources_.end())
    return false;
  out = i->second;
  return true;
}
const GlobalYamlFileRecord *
GlobalYamlFileData::record(std::string_view p) const {
  if (p.substr(0, 6) == "res://")
    p.remove_prefix(6);
  for (const auto &r : records_)
    if (r.source == p)
      return &r;
  return nullptr;
}
bool GlobalYamlFileData::load(const uint8_t *p, size_t n,
                              const GlobalYamlCachesData &c, std::string &e) {
  if (!c.valid() || !p || n < 128 || n > 32 * 1024 * 1024 ||
      std::memcmp(p, "ENCYFIL1", 8) || word(p + 8) != 1 ||
      word(p + 12) != 128 || word(p + 16) != n ||
      word(p + 20) != crc(p + 128, n - 128) || word(p + 24) != 0x454e0052 ||
      word(p + 28) != 1 || word(p + 32) != 1 || !word(p + 36) ||
      std::memcmp(p + 40, c.identity().upstream_commit.data(), 20) ||
      word(p + 124))
    return fail(e, "YAML File format/capability/identity rejected");
  GlobalYamlFileData d;
  d.caches_ir_ = c.ir_sha256();
  d.identity_ = c.identity();
  d.identity_.scene_id = word(p + 36);
  std::memcpy(d.identity_.source_sha256.data(), p + 60, 32);
  std::memcpy(d.ir_.data(), p + 92, 32);
  Reader r{p + 128, n - 128};
  d.parser_ = r.s();
  d.owner_ = r.s();
  auto k = r.count(32);
  while (k-- && r.ok) {
    auto path = r.s();
    auto h = r.hash();
    std::array<uint8_t, 32> expected{};
    if (!c.source_hash(path, expected) || h != expected ||
        !d.sources_.emplace(path, h).second)
      r.ok = false;
  }
  std::array<uint8_t, 32> ph{}, oh{};
  if (d.owner_ != c.owner_source() || !d.source_hash(d.parser_, ph) ||
      !d.source_hash(d.owner_, oh) || ph != d.identity_.source_sha256 ||
      d.sources_.size() != 2)
    r.ok = false;
  k = r.count(3);
  if (k != 3)
    r.ok = false;
  std::set<uint32_t> ids;
  for (uint32_t i = 0; i < k && r.ok; ++i) {
    FieldGlobalExternalSpec s;
    s.stable_id = r.u();
    s.role = r.u();
    s.name = r.s();
    s.native_class = r.s();
    s.source = r.s();
    s.script = s.source;
    s.identity = d.identity_;
    if (!d.source_hash(s.source, s.source_sha))
      r.ok = false;
    s.script_sha = s.source_sha;
    s.identity.source_sha256 = s.source_sha;
    if (!s.stable_id || !ids.insert(s.stable_id).second || s.role != 5 ||
        s.name.empty() || s.native_class != (i == 1 ? "Reference" : "File") ||
        s.source != (i == 0 ? d.owner_ : d.parser_))
      r.ok = false;
    d.bindings_.push_back(s);
  }
  k = r.count(65536);
  if (k != c.records().size())
    r.ok = false;
  for (uint32_t i = 0; i < k && r.ok; ++i) {
    GlobalYamlFileRecord row;
    row.role = r.u();
    row.source = r.s();
    row.sha = r.hash();
    row.bytes = r.s(1024 * 1024);
    row.parsed = r.value();
    const auto &expected = c.records()[i];
    if (row.role != expected.role || row.source != expected.source ||
        row.sha != expected.source_sha ||
        global_yaml_bytes_sha256(row.bytes) != row.sha || !row.parsed ||
        row.parsed->kind != (row.role == 3 ? 5u : 6u) ||
        (row.role != 4 &&
         (!expected.parsed || !equal(*row.parsed, *expected.parsed))))
      r.ok = false;
    d.records_.push_back(std::move(row));
  }
  if (!r.ok || r.n ||
      std::all_of(d.ir_.begin(), d.ir_.end(), [](uint8_t x) { return x == 0; }))
    return fail(e, "YAML File source bytes/result/cache closure rejected");
  d.valid_ = true;
  *this = std::move(d);
  return true;
}
bool GlobalYamlFileData::load_file(const char *path,
                                   const GlobalYamlCachesData &c,
                                   std::string &e) {
  auto *f = path ? std::fopen(path, "rb") : nullptr;
  if (!f)
    return fail(e, "Cannot open YAML File resource");
  bool ok = std::fseek(f, 0, SEEK_END) == 0;
  auto n = std::ftell(f);
  ok = ok && n >= 128 && n <= 32 * 1024 * 1024 &&
       std::fseek(f, 0, SEEK_SET) == 0;
  std::vector<uint8_t> b(ok ? size_t(n) : 0);
  ok = ok && std::fread(b.data(), 1, b.size(), f) == b.size();
  std::fclose(f);
  if (!ok)
    return fail(e, "YAML File resource read rejected");
  return load(b.data(), b.size(), c, e);
}
} // namespace encore::upstream
