#include "encore/crc32.hpp"
#include "encore/player_motion.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
namespace encore::upstream {
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
uint32_t word(const uint8_t *p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) |
         (uint32_t(p[3]) << 24);
}
struct Reader {
  const uint8_t *p;
  size_t n, at = 128;
  bool ok = true;
  template <size_t N> std::array<uint8_t, N> bytes() {
    std::array<uint8_t, N> v{};
    if (!ok || at > n || n - at < N) {
      ok = false;
      return v;
    }
    std::copy_n(p + at, N, v.begin());
    at += N;
    return v;
  }
  uint32_t u() {
    auto v = bytes<4>();
    return word(v.data());
  }
  int64_t q() {
    auto v = bytes<8>();
    uint64_t bits = 0;
    for (unsigned i = 0; i < 8; ++i)
      bits |= uint64_t(v[i]) << (8 * i);
    int64_t x;
    std::memcpy(&x, &bits, 8);
    return x;
  }
  double f() {
    auto v = bytes<8>();
    double x;
    std::memcpy(&x, v.data(), 8);
    if (!std::isfinite(x))
      ok = false;
    return x;
  }
  std::string text() {
    auto size = u();
    if (!ok || size > 4096 || at > n || n - at < size) {
      ok = false;
      return {};
    }
    std::string s(reinterpret_cast<const char *>(p + at), size);
    at += size;
    std::u32string chars;
    if (s.empty() || s.find('\0') != s.npos || !utf8_decode(s, chars))
      ok = false;
    return s;
  }
};
} // namespace
bool PlayerMotionData::load(const uint8_t *p, size_t n,
                            const PlayerInitializationData &init,
                            const PlayerReadyData &ready, std::string &e) {
  if (!init.valid() || !ready.valid() || !p || n < 128 || n > 1024 * 1024 ||
      std::memcmp(p, "ENCPMOV1", 8) || word(p + 8) != 1 ||
      word(p + 12) != 128 || word(p + 16) != n ||
      word(p + 20) != crc32(p + 128, n - 128) || word(p + 24) != 0x454e005c ||
      word(p + 28) != 1 || word(p + 32) != 1 ||
      word(p + 36) != init.identity().scene_id ||
      !std::equal(p + 40, p + 60, init.identity().upstream_commit.begin()) ||
      !std::equal(p + 60, p + 92, init.identity().source_sha256.begin()) ||
      word(p + 124))
    return fail(e, "Player motion format/capability/rules/source rejected");
  PlayerMotionData d;
  d.identity_ = init.identity();
  std::copy_n(p + 92, 32, d.ir_.begin());
  Reader r{p, n};
  if (r.bytes<32>() != init.ir_sha256() || r.bytes<32>() != ready.ir_sha256())
    return fail(e, "Player motion actual initialization/Ready IR differs");
  auto count = r.u();
  if (count == 0 || count > 64)
    return fail(e, "Player motion source count rejected");
  std::set<std::string> sources;
  for (uint32_t i = 0; i < count; ++i) {
    auto path = r.text();
    auto hash = r.bytes<32>();
    std::array<uint8_t, 32> actual{};
    if (!sources.insert(path).second ||
        std::all_of(hash.begin(), hash.end(), [](auto x) { return x == 0; }) ||
        (init.source_hash(path, actual) && actual != hash))
      r.ok = false;
  }
  auto strings = [&](auto &values) {
    if (r.u() != values.size()) {
      r.ok = false;
      return;
    }
    for (auto &v : values)
      v = r.text();
  };
  strings(d.fields_);
  strings(d.nodes_);
  strings(d.text_);
  std::set<std::string> fields, nodes;
  for (const auto &f : d.fields_) {
    auto match = std::find_if(init.fields().begin(), init.fields().end(),
                              [&](const auto &x) { return x.name == f; });
    if (match == init.fields().end() || !fields.insert(f).second)
      r.ok = false;
  }
  for (const auto &path : d.nodes_) {
    bool found = false;
    for (const auto &record : init.recipe().records())
      if (record.path == path)
        found = true;
    if (!found || !nodes.insert(path).second)
      r.ok = false;
  }
  if (r.u() != d.states_.size())
    return fail(e, "Player motion state schema rejected");
  std::set<int64_t> states;
  for (auto &v : d.states_) {
    v = r.q();
    if (v < 0 || !states.insert(v).second)
      r.ok = false;
  }
  if (r.u() != d.numbers_.size())
    return fail(e, "Player motion rule schema rejected");
  for (auto &v : d.numbers_)
    v = r.f();
  d.manual_ = r.q();
  auto modecount = r.u();
  if (modecount == 0 || modecount > 16)
    return fail(e, "Player motion teleport schema rejected");
  std::set<int64_t> modes;
  bool manual = false;
  for (uint32_t i = 0; i < modecount; ++i) {
    PlayerTeleportMode m;
    m.id = r.q();
    m.acceleration = r.f();
    m.cap = r.f();
    m.takeoff = r.f();
    m.multiplier = r.f();
    if (!modes.insert(m.id).second || m.acceleration <= 0 || m.cap <= 0 ||
        m.multiplier <= 0)
      r.ok = false;
    if (m.id == d.manual_)
      manual = true;
    d.modes_.push_back(m);
  }
  for (auto &name : d.names_)
    name = r.text();
  auto positive = [&](PlayerMotionNumber role) { return d.number(role) > 0; };
  if (!positive(PlayerMotionNumber::MovementDivisor) ||
      !positive(PlayerMotionNumber::StepDistance) ||
      !positive(PlayerMotionNumber::KnockbackDecay) ||
      !positive(PlayerMotionNumber::CrashShakeDivisor) || !manual || !r.ok ||
      r.at != n)
    return fail(e, "Player motion payload/source binding rejected");
  d.valid_ = true;
  *this = std::move(d);
  return true;
}
bool PlayerMotionData::load_file(const char *path,
                                 const PlayerInitializationData &i,
                                 const PlayerReadyData &r, std::string &e) {
  auto f = path ? std::fopen(path, "rb") : nullptr;
  if (!f)
    return fail(e, "Cannot open Player motion resource");
  if (std::fseek(f, 0, SEEK_END)) {
    std::fclose(f);
    return fail(e, "Player motion seek failed");
  }
  auto length = std::ftell(f);
  if (length < 128 || length > 1024 * 1024 || std::fseek(f, 0, SEEK_SET)) {
    std::fclose(f);
    return fail(e, "Player motion size rejected");
  }
  std::vector<uint8_t> bytes(size_t(length), uint8_t{});
  auto read = std::fread(bytes.data(), 1, bytes.size(), f);
  auto closed = std::fclose(f);
  if (read != bytes.size() || closed)
    return fail(e, "Player motion read failed");
  return load(bytes.data(), bytes.size(), i, r, e);
}
} // namespace encore::upstream
