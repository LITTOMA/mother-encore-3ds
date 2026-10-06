#include "encore/crc32.hpp"
#include "encore/player_ready.hpp"
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
struct Reader {
  const uint8_t *p;
  size_t n, at = 128;
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
    std::array<uint8_t, N> b{};
    if (!ok || at > n || n - at < N) {
      ok = false;
      return b;
    }
    std::copy_n(p + at, N, b.begin());
    at += N;
    return b;
  }
  float f() {
    auto b = bytes<4>();
    float v = 0;
    std::memcpy(&v, b.data(), 4);
    if (!std::isfinite(v))
      ok = false;
    return v;
  }
  Vec2 vec() {
    float x = f(), y = f();
    return {x, y};
  }
  bool boolean() {
    auto v = u();
    if (v > 1)
      ok = false;
    return v != 0;
  }
  std::string text() {
    auto size = u();
    if (!ok || size > 65536 || at > n || n - at < size) {
      ok = false;
      return {};
    }
    std::string s(reinterpret_cast<const char *>(p + at), size);
    at += size;
    std::u32string chars;
    if (s.find('\0') != s.npos || !utf8_decode(s, chars))
      ok = false;
    return s;
  }
  uint32_t count(uint32_t max) {
    auto v = u();
    if (v > max)
      ok = false;
    return ok ? v : 0;
  }
};
} // namespace
bool PlayerReadyData::source_hash(std::string_view path,
                                  std::array<uint8_t, 32> &out) const {
  auto it = sources_.find(std::string(path));
  if (it == sources_.end())
    return false;
  out = it->second;
  return true;
}
bool PlayerReadyData::load(const uint8_t *p, size_t n,
                           const PlayerInitializationData &init,
                           std::string &e) {
  if (!init.valid() || !p || n < 128 || n > 1024 * 1024 ||
      std::memcmp(p, "ENCPRDY1", 8) || word(p + 8) != 1 ||
      word(p + 12) != 128 || word(p + 16) != n ||
      word(p + 20) != crc32(p + 128, n - 128) || word(p + 24) != 0x454e005a ||
      word(p + 28) != 1 || word(p + 32) != 1 ||
      word(p + 36) != init.identity().scene_id ||
      !std::equal(p + 40, p + 60, init.identity().upstream_commit.begin()) ||
      !std::equal(p + 60, p + 92, init.identity().source_sha256.begin()) ||
      word(p + 124) != 0)
    return fail(e, "Player Ready format/capability/rules/source rejected");
  PlayerReadyData d;
  d.identity_ = init.identity();
  std::copy_n(p + 92, 32, d.ir_.begin());
  Reader r{p, n};
  d.initialization_ir_ = r.bytes<32>();
  if (d.initialization_ir_ != init.ir_sha256())
    return fail(e, "Player Ready actual initialization IR differs");
  auto count = r.count(64);
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    auto path = r.text();
    auto hash = r.bytes<32>();
    std::array<uint8_t, 32> actual{};
    if (path.empty() || !init.source_hash(path, actual) || actual != hash ||
        !d.sources_.emplace(path, hash).second)
      r.ok = false;
  }
  if (!count)
    r.ok = false;
  for (auto &b : d.bindings_) {
    b = r.text();
    if (b.empty())
      r.ok = false;
  }
  d.party_size_ = r.u();
  d.height_divisor_ = r.u();
  d.height_offset_ = r.u();
  d.process_mode_ = r.u();
  d.playback_ = r.u();
  d.direction_ = r.vec();
  if (!d.height_divisor_ || !d.playback_ || d.process_mode_ != 1 ||
      d.party_size_ > 65536)
    r.ok = false;
  count = r.count(128);
  std::set<std::string> blends;
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    auto name = r.text();
    if (name.empty() || !blends.insert(name).second)
      r.ok = false;
    d.blends_.push_back(std::move(name));
  }
  d.fainted_prefix_ = r.text();
  count = r.count(128);
  std::set<std::string> mapped;
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    auto input = r.text(), target = r.text();
    if (input.empty() || target.empty() || !mapped.insert(input).second)
      r.ok = false;
    d.incap_rules_.emplace_back(std::move(input), std::move(target));
  }
  if (d.fainted_prefix_.empty())
    r.ok = false;
  d.start_ = r.text();
  count = r.count(128);
  std::set<std::string> states, params;
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    PlayerGraphState s;
    s.name = r.text();
    s.position = r.vec();
    s.parameter = r.text();
    s.initial = r.vec();
    s.scale_parameter = r.text();
    s.scale = r.f();
    if (s.name.empty() || !states.insert(s.name).second ||
        (!s.parameter.empty() && !params.insert(s.parameter).second) ||
        (!s.scale_parameter.empty() &&
         !params.insert(s.scale_parameter).second))
      r.ok = false;
    auto points = r.count(64);
    std::set<uint32_t> ids;
    for (uint32_t j = 0; j < points && r.ok; ++j) {
      PlayerGraphPoint point;
      point.position = r.vec();
      point.clip = r.text();
      point.node_id = r.u();
      point.clip_id = r.u();
      point.length = r.f();
      point.loop = r.boolean();
      if (point.clip.empty() || !point.node_id || !point.clip_id ||
          point.length <= 0 || !ids.insert(point.node_id).second)
        r.ok = false;
      s.points.push_back(std::move(point));
    }
    if (!points || (s.parameter.empty() && points != 1))
      r.ok = false;
    d.states_.push_back(std::move(s));
  }
  count = r.count(2048);
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    PlayerGraphEdge edge;
    edge.source = r.text();
    edge.target = r.text();
    edge.mode = r.u();
    edge.priority = r.u();
    edge.auto_advance = r.boolean();
    edge.disabled = r.boolean();
    if (!states.count(edge.source) || !states.count(edge.target) ||
        (edge.mode != 0 && edge.mode != 2) || !edge.priority || edge.disabled)
      r.ok = false;
    d.edges_.push_back(std::move(edge));
  }
  for (const auto &rule : d.incap_rules_)
    if (!states.count(rule.first) ||
        !states.count(d.fainted_prefix_ + rule.second))
      r.ok = false;
  for (const auto &name : d.blends_)
    if (!params.count(name))
      r.ok = false;
  if (!states.count(d.start_) || !r.ok || r.at != n)
    return fail(e, "Player Ready source graph/schema/trailing bytes rejected");
  d.valid_ = true;
  *this = std::move(d);
  return true;
}
bool PlayerReadyData::load_file(const char *path,
                                const PlayerInitializationData &d,
                                std::string &e) {
  auto file = path ? std::fopen(path, "rb") : nullptr;
  if (!file)
    return fail(e, "Cannot open Player Ready resource");
  if (std::fseek(file, 0, SEEK_END)) {
    std::fclose(file);
    return fail(e, "Player Ready seek failed");
  }
  auto length = std::ftell(file);
  if (length < 128 || length > 1024 * 1024 || std::fseek(file, 0, SEEK_SET)) {
    std::fclose(file);
    return fail(e, "Player Ready resource size rejected");
  }
  std::vector<uint8_t> bytes(size_t(length), uint8_t{});
  auto read = std::fread(bytes.data(), 1, bytes.size(), file);
  auto closed = std::fclose(file);
  if (read != bytes.size() || closed)
    return fail(e, "Player Ready resource read failed");
  return load(bytes.data(), bytes.size(), d, e);
}
} // namespace encore::upstream
