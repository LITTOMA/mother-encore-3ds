#include "encore/field_native_timer.hpp"
#include <algorithm>
#include <cmath>
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
  e = "Native Timer format/source identity rejected";
  return false;
}
template <size_t N> bool nonzero(const std::array<uint8_t, N> &a) {
  return std::any_of(a.begin(), a.end(), [](auto x) { return x != 0; });
}
bool same(const FieldIdentity &a, const FieldIdentity &b) {
  return a.scene_id == b.scene_id && a.upstream_commit == b.upstream_commit &&
         a.source_sha256 == b.source_sha256;
}
} // namespace
bool FieldNativeTimerData::load(const uint8_t *p, size_t n, std::string &e) {
  if (!p || n < 56 || n > 2 * 1024 * 1024 || std::memcmp(p, "ENCFNT01", 8) ||
      u(p + 8) != 1 || u(p + 12) != n || u(p + 16) != crc(p + 32, n - 32) ||
      u(p + 20) != 0x454e0044 || u(p + 24) != 1 || u(p + 28) != 1)
    return fail(e);
  uint32_t count = u(p + 52);
  if (!count || count > 8192 || n != 56 + size_t(count) * 84)
    return fail(e);
  std::array<uint8_t, 20> pin{};
  std::copy_n(p + 32, 20, pin.begin());
  if (!nonzero(pin))
    return fail(e);
  std::vector<FieldNativeTimerDescriptor> rows;
  std::set<std::pair<uint32_t, uint32_t>> keys;
  for (uint32_t i = 0; i < count; ++i) {
    const auto *q = p + 56 + i * 84;
    FieldNativeTimerDescriptor d;
    d.id = u(q);
    d.identity.scene_id = u(q + 4);
    d.identity.upstream_commit = pin;
    d.mode = u(q + 8);
    d.flags = u(q + 12);
    auto bits = u(q + 16);
    std::memcpy(&d.wait, &bits, 4);
    std::copy_n(q + 20, 32, d.identity.source_sha256.begin());
    std::copy_n(q + 52, 32, d.script_sha.begin());
    if (!d.id || !d.identity.scene_id || d.mode > 1 || d.flags > 3 ||
        !std::isfinite(d.wait) || d.wait <= 0 ||
        !nonzero(d.identity.source_sha256) ||
        !keys.emplace(d.identity.scene_id, d.id).second)
      return fail(e);
    rows.push_back(d);
  }
  records_ = std::move(rows);
  valid_ = true;
  return true;
}
bool FieldNativeTimerData::load_file(const char *path, std::string &e) {
  if (!path)
    return fail(e);
  std::ifstream f(path, std::ios::binary | std::ios::ate);
  auto n = f.tellg();
  if (!f || n < 56 || n > 2 * 1024 * 1024)
    return fail(e);
  std::vector<uint8_t> b(static_cast<size_t>(n));
  f.seekg(0);
  if (!f.read(reinterpret_cast<char *>(b.data()), n))
    return fail(e);
  return load(b.data(), b.size(), e);
}
const FieldNativeTimerDescriptor *
FieldNativeTimerData::record(const FieldIdentity &i, uint32_t id) const {
  for (const auto &r : records_)
    if (r.id == id && same(r.identity, i))
      return &r;
  return nullptr;
}
} // namespace encore::upstream
