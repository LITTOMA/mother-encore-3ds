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
      std::memcmp(p, "ENCPMOV1", 8) || (word(p + 8) != 1 && word(p + 8) != 2) ||
      word(p + 12) != 128 || word(p + 16) != n ||
      word(p + 20) != crc32(p + 128, n - 128) || word(p + 24) != 0x454e005c ||
      word(p + 28) != word(p + 8) || word(p + 32) != 1 ||
      word(p + 36) != init.identity().scene_id ||
      !std::equal(p + 40, p + 60, init.identity().upstream_commit.begin()) ||
      !std::equal(p + 60, p + 92, init.identity().source_sha256.begin()) ||
      word(p + 124))
    return fail(e, "Player motion format/capability/rules/source rejected");
  PlayerMotionData d;
  d.identity_ = init.identity();
  d.capability_ = word(p + 28);
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
if (d.capability_ == 2) {
  strings(d.business_);
  std::set<std::string> unique;
  for (const auto &v : d.business_)
    if (!unique.insert(v).second)
      r.ok = false;
  auto signal_count = r.u();
  if (!signal_count || signal_count > 64)
    return fail(e, "Player source signal schema rejected");
  unique.clear();
  for (uint32_t i = 0; i < signal_count; ++i) {
    auto name = r.text();
    auto args = r.u();
    if (args > 1 || !unique.insert(name).second)
      r.ok = false;
    d.signals_.emplace_back(std::move(name), args);
  }
}
if (d.capability_ == 2) {
  auto &p = d.lifecycle_;
  p.paused_animation = r.text();
  p.takeoff_timer = r.text();
  p.collision_path = r.text();
  p.collision_native = r.text();
  p.pause_flash = r.text();
  p.resume_flash = r.text();
  p.pause_timers = r.text();
  p.resume_timers = r.text();
  auto field_exists = [&](const std::string &name, uint32_t kind) {
    for (const auto &f : init.fields())
      if (f.name == name && f.kind == kind)
        return true;
    return false;
  };
  bool shape = false;
  if (p.collision_native != "CollisionShape2D" &&
      p.collision_native != "CollisionPolygon2D")
    return fail(e, "Player collision native class rejected");
  for (const auto &v : init.recipe().records())
    if (v.path == p.collision_path &&
        v.class_index < init.recipe().classes().size() &&
        init.recipe().classes()[v.class_index] == p.collision_native)
      shape = true;
  bool takeoff = false;
  auto onready = init.onready_source();
  if (onready && onready->kind == 5)
    for (const auto &row : onready->array) {
      auto name = row->get("name"), path = row->get("path");
      if (name && path && name->kind == 4 && path->kind == 4 &&
          name->string == p.takeoff_timer &&
          path->string == d.node(PlayerMotionNode::TakeoffTimer))
        takeoff = true;
    }
  if (!field_exists(p.paused_animation, 4))
    return fail(e, "Player pause source String declaration rejected");
  if (!takeoff)
    return fail(e, "Player pause source onready timer rejected");
  if (!shape)
    return fail(e, "Player pause source native collision rejected");
  auto loops = r.u();
  if (!loops || loops > 64)
    return fail(e, "Player pause loop schema rejected");
  std::set<std::string> unique;
  for (uint32_t i = 0; i < loops; ++i) {
    auto name = r.text();
    auto parameter = r.text();
    bool found = false;
    for (const auto &state : ready.states())
      if (state.name == name && state.scale_parameter == parameter)
        found = true;
    if (!found || !unique.insert(name).second)
      r.ok = false;
    p.looped_animations.emplace_back(std::move(name), std::move(parameter));
  }
  auto flashes = r.u();
  if (!flashes || flashes > 64)
    return fail(e, "Player pausable Flash schema rejected");
  unique.clear();
  for (uint32_t i = 0; i < flashes; ++i) {
    auto name = r.text();
    if (!unique.insert(name).second)
      r.ok = false;
    p.pausable_flash.push_back(std::move(name));
  }
  auto masks = r.u();
  if (!masks || masks > 32)
    return fail(e, "Player collision mask schema rejected");
  std::set<uint32_t> bits;
  for (uint32_t i = 0; i < masks; ++i) {
    auto bit = r.u();
    if (bit >= 32 || !bits.insert(bit).second)
      r.ok = false;
    p.collision_masks.push_back(bit);
  }
  p.paused_scale = r.f();
  p.playing_scale = r.f();
  if (p.paused_scale < 0 || p.playing_scale <= p.paused_scale)
    r.ok = false;
}
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
