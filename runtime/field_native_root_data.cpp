#include "encore/crc32.hpp"
#include "encore/field_native_root.hpp"
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
struct Reader {
  const uint8_t *p;
  size_t n, at = 128;
  bool ok = true;
  uint32_t u() {
    if (!ok || at > n || n - at < 4) {
      ok = false;
      return 0;
    }
    auto x = word(p + at);
    at += 4;
    return x;
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
  float f() {
    uint32_t w = u();
    float x;
    std::memcpy(&x, &w, 4);
    if (!std::isfinite(x))
      ok = false;
    return x;
  }
  std::string text() {
    auto length = u();
    if (!ok || !length || length > 4096 || at > n || n - at < length) {
      ok = false;
      return {};
    }
    std::string s(reinterpret_cast<const char *>(p + at), length);
    at += length;
    std::u32string decoded;
    if (s.find('\0') != s.npos || !utf8_decode(s, decoded))
      ok = false;
    return s;
  }
};
template <size_t N> std::string hex(const std::array<uint8_t, N> &x) {
  static const char digits[] = "0123456789abcdef";
  std::string s;
  for (auto v : x) {
    s += digits[v >> 4];
    s += digits[v & 15];
  }
  return s;
}
} // namespace
bool FieldNativeRootData::load(const uint8_t *p, size_t n,
                               const FieldGlobalRegistryData &registry,
                               std::string &e) {
  if (!registry.valid() || !p || n < 128 || n > 65536 ||
      std::memcmp(p, "ENCFNVP1", 8) || word(p + 8) != 1 ||
      word(p + 12) != 128 || word(p + 16) != n || word(p + 24) != 0x454e004c ||
      word(p + 28) != 1 || word(p + 32) != 1 ||
      word(p + 36) != registry.identity().scene_id || word(p + 124) ||
      word(p + 20) != crc32(p + 128, n - 128))
    return fail(e, "Native root header/format/capability/CRC rejected");
  FieldNativeRootData d;
  std::copy_n(p + 40, 20, d.identity_.upstream_commit.begin());
  std::copy_n(p + 60, 32, d.identity_.source_sha256.begin());
  d.identity_.scene_id = word(p + 36);
  if (d.identity_.upstream_commit != registry.identity().upstream_commit ||
      d.identity_.source_sha256 != registry.identity().source_sha256 ||
      std::all_of(p + 92, p + 124, [](uint8_t v) { return v == 0; }))
    return fail(e, "Native root original project identity rejected");
  Reader reader{p, n};
  auto scene = reader.text();
  d.root_ = reader.text();
  d.viewport_ = reader.text();
  d.kernel_ = reader.text();
  auto commit = reader.bytes<20>();
  auto ir = reader.bytes<32>();
  d.flags_ = reader.u();
  for (auto &v : d.clear_)
    v = reader.f();
  auto count = reader.u();
  std::array<uint8_t, 32> project_proof;
  if (!reader.ok || !registry.source_hash(scene, project_proof) ||
      project_proof != d.identity_.source_sha256 || scene.empty() ||
      d.root_ != registry.root_name() ||
      d.viewport_ != registry.root_native() ||
      d.kernel_ != registry.kernel_native() ||
      hex(commit) != registry.engine_commit() || d.flags_ != 14 || count != 6 ||
      std::all_of(ir.begin(), ir.end(), [](uint8_t v) { return !v; }) ||
      std::any_of(d.clear_.begin(), d.clear_.end(),
                  [](float v) { return v < 0 || v > 1; }))
    return fail(e, "Native root source policy/class/default rejected");
  for (uint32_t i = 0; i < count; ++i) {
    auto key = reader.text();
    auto sha = reader.bytes<32>();
    if (!reader.ok || key.empty() || key.front() == '/' ||
        key.find("..") != key.npos || key.find_first_of("\\:") != key.npos ||
        std::all_of(sha.begin(), sha.end(), [](uint8_t v) { return !v; }) ||
        !d.engine_.emplace(key, sha).second)
      return fail(e, "Native root engine source proof rejected");
  }
  for (const auto &key : {"scene/main/scene_tree.cpp", "scene/main/node.cpp",
                          "scene/main/viewport.cpp", "scene/2d/canvas_item.cpp",
                          "scene/main/canvas_layer.cpp", "main/main.cpp"})
    if (!d.engine_.count(key))
      return fail(e, "Native root unknown or missing engine source kind");
  for (const auto &key : {"scene/main/scene_tree.cpp", "scene/main/node.cpp"}) {
    std::array<uint8_t, 32> hash;
    auto found = d.engine_.find(key);
    if (found == d.engine_.end() || !registry.engine_hash(key, hash) ||
        hash != found->second)
      return fail(e, "Native root source differs from actual registry engine");
  }
  if (!reader.ok || reader.at != n)
    return fail(e, "Native root trailing or truncated source resource");
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
bool FieldNativeRootData::load_file(const char *name,
                                    const FieldGlobalRegistryData &registry,
                                    std::string &e) {
  if (!name)
    return fail(e, "Native root path absent");
  FILE *f = std::fopen(name, "rb");
  if (!f)
    return fail(e, "Native root resource open failed");
  bool ok = std::fseek(f, 0, SEEK_END) == 0;
  long n = ok ? std::ftell(f) : -1;
  ok = ok && n >= 128 && n <= 65536 && std::fseek(f, 0, SEEK_SET) == 0;
  std::vector<uint8_t> b(ok ? size_t(n) : 0);
  ok = ok && std::fread(b.data(), 1, b.size(), f) == b.size();
  std::fclose(f);
  if (!ok)
    return fail(e, "Native root resource file size/read rejected");
  return load(b.data(), b.size(), registry, e);
}
} // namespace encore::upstream
