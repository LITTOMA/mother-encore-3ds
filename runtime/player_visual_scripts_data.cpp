#include "encore/player_visual_scripts.hpp"
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
uint32_t u32(const uint8_t *p) {
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
struct R {
  const uint8_t *p;
  size_t n;
  bool ok = true;
  uint32_t u() {
    if (n < 4) {
      ok = false;
      return 0;
    }
    auto v = u32(p);
    p += 4;
    n -= 4;
    return v;
  }
  uint32_t count(uint32_t max = 4096) {
    auto k = u();
    if (k > max)
      ok = false;
    return ok ? k : 0;
  }
  std::string s() {
    auto k = u();
    if (k > 4096 || k > n) {
      ok = false;
      return {};
    }
    std::string v(reinterpret_cast<const char *>(p), k);
    p += k;
    n -= k;
    size_t chars = 0;
    if (v.find('\0') != v.npos || !encore::utf8_count(v, chars))
      ok = false;
    return v;
  }
  std::array<uint8_t, 32> h() {
    std::array<uint8_t, 32> v{};
    if (n < 32) {
      ok = false;
      return v;
    }
    std::copy(p, p + 32, v.begin());
    p += 32;
    n -= 32;
    if (std::all_of(v.begin(), v.end(), [](uint8_t x) { return x == 0; }))
      ok = false;
    return v;
  }
  float f() {
    auto b = u();
    float v;
    std::memcpy(&v, &b, 4);
    if (!std::isfinite(v))
      ok = false;
    return v;
  }
  double number() {
    uint64_t b = u();
    b |= uint64_t(u()) << 32;
    double v;
    std::memcpy(&v, &b, 8);
    if (!std::isfinite(v))
      ok = false;
    return v;
  }
};
} // namespace
bool PlayerVisualScriptsData::load(const uint8_t *p, size_t n,
                                   const PlayerInitializationData &player,
                                   std::string &e) {
  if (!player.valid() || !p || n < 128 || n > 1024 * 1024 ||
      std::memcmp(p, "ENCPVIS1", 8) || u32(p + 8) != 1 || u32(p + 12) != 128 ||
      u32(p + 16) != n || u32(p + 20) != crc(p + 128, n - 128) ||
      u32(p + 24) != 0x454e0058 || u32(p + 28) != 1 || u32(p + 32) != 1 ||
      u32(p + 124))
    return fail(e, "Player visual script header/version/capability rejected");
  const auto &expected = player.identity();
  if (u32(p + 36) != expected.scene_id ||
      std::memcmp(p + 40, expected.upstream_commit.data(), 20) ||
      std::memcmp(p + 60, expected.source_sha256.data(), 32))
    return fail(e, "Player visual source scene identity differs");
  PlayerVisualScriptsData d;
  d.identity_ = expected;
  std::copy(p + 92, p + 124, d.ir_.begin());
  if (std::all_of(d.ir_.begin(), d.ir_.end(),
                  [](uint8_t x) { return x == 0; }))
    return fail(e, "Player visual source IR proof missing");
  R r{p + 128, n - 128};
  d.scene_ = r.s();
  d.player_ir_ = r.h();
  d.player_ = r.u();
  d.player_script_ = r.s();
  d.player_script_sha_ = r.h();
  auto &s = d.shadow_;
  s.id = r.u();
  s.script = r.s();
  s.script_sha = r.h();
  s.start_member = r.s();
  s.start_default = r.s();
  s.setter = r.s();
  s.front_member = r.s();
  s.start = r.s();
  auto k = r.count();
  while (k-- && r.ok)
    s.front.push_back(r.s());
  s.frames_resource = r.u();
  k = r.count();
  std::set<std::string> names;
  std::set<uint32_t> frameids;
  while (k-- && r.ok) {
    PlayerVisualAnimation a;
    a.name = r.s();
    a.speed = r.number();
    auto loop = r.u();
    a.loop = loop != 0;
    if (loop > 1 || a.name.empty() || a.speed <= 0 || a.speed > 1000 ||
        !names.insert(a.name).second)
      r.ok = false;
    auto frames = r.count();
    while (frames-- && r.ok) {
      PlayerVisualFrame f;
      f.resource = r.u();
      f.texture = r.u();
      f.source = r.s();
      f.source_sha = r.h();
      for (auto &v : f.rect)
        v = r.f();
      if (!f.resource || !f.texture || f.source.empty() ||
          !frameids.insert(f.resource).second || f.rect[0] < 0 ||
          f.rect[1] < 0 || f.rect[2] <= 0 || f.rect[3] <= 0)
        r.ok = false;
      a.frames.push_back(std::move(f));
    }
    if (a.frames.empty())
      r.ok = false;
    s.animations.push_back(std::move(a));
  }
  auto &b = d.bat_;
  b.id = r.u();
  b.script = r.s();
  b.script_sha = r.h();
  b.special_member = r.s();
  b.special_path = r.s();
  b.action_getter = r.s();
  b.action_member = r.s();
  b.visible_action = r.s();
  b.fetcher_getter = r.s();
  b.frame_member = r.s();
  b.fetcher = r.u();
  b.fetcher_script = r.s();
  b.fetcher_sha = r.h();
  b.fetcher_path = r.s();
  b.target = r.u();
  b.columns = r.u();
  b.rows = r.u();
  b.initial_frame = r.u();
  b.texture = r.u();
  b.texture_source = r.s();
  b.texture_sha = r.h();
  k = r.count();
  while (k-- && r.ok) {
    auto path = r.s();
    auto h = r.h();
    if (path.empty() || !d.sources_.emplace(path, h).second)
      r.ok = false;
  }
  if (!r.ok || r.n || d.player_ir_ != player.ir_sha256() ||
      d.scene_ != player.recipe().source_scene() || !s.frames_resource ||
      s.animations.empty() || s.start_member.empty() || s.setter.empty() ||
      s.front_member.empty() || !s.start_default.empty() ||
      !names.count(s.start) || !b.columns || !b.rows || b.columns > 4096 ||
      b.rows > 4096 || uint64_t(b.columns) * b.rows <= b.initial_frame ||
      !b.texture || b.special_member.empty() || b.special_path.empty() ||
      b.action_getter.empty() || b.action_member.empty() ||
      b.visible_action.empty() || b.fetcher_getter.empty() ||
      b.frame_member.empty() || b.fetcher_path.empty())
    return fail(e, "Player visual layout/source dependency rejected");
  for (const auto &v : s.front)
    if (!names.count(v))
      return fail(e, "Player Shadow front animation unknown");
  auto record = [&](uint32_t id, std::string_view script, const auto &sha,
                    std::string_view native) {
    auto *rec = player.recipe().record(id);
    std::array<uint8_t, 32> h{};
    return rec && rec->script == script && rec->script_sha == sha &&
           rec->native_class == native && player.source_hash(script, h) &&
           h == sha;
  };
  if (!record(d.player_, d.player_script_, d.player_script_sha_,
              "KinematicBody2D") ||
      !record(s.id, s.script, s.script_sha, "AnimatedSprite") ||
      !record(b.id, b.script, b.script_sha, "Sprite") ||
      !record(b.fetcher, b.fetcher_script, b.fetcher_sha, "Node"))
    return fail(e, "Player visual actual source recipe binding rejected");
  const auto *target = player.recipe().record(b.target);
  if (!target || target->native_class != "Sprite")
    return fail(e, "Bat actual fetcher Sprite target missing");
  std::array<uint8_t, 32> h{};
  if (!d.source_hash(s.script, h) || h != s.script_sha ||
      !d.source_hash(b.script, h) || h != b.script_sha ||
      !d.source_hash(b.fetcher_script, h) || h != b.fetcher_sha ||
      !d.source_hash(d.player_script_, h) || h != d.player_script_sha_ ||
      !d.source_hash(b.texture_source, h) || h != b.texture_sha)
    return fail(e, "Player visual source hash closure differs");
  for (const auto &a : s.animations)
    for (const auto &f : a.frames)
      if (!d.source_hash(f.source, h) || h != f.source_sha)
        return fail(e, "Shadow source frame texture closure differs");
  auto member = std::find_if(
      player.fields().begin(), player.fields().end(), [&](const auto &f) {
        return f.name == b.action_member && f.value && f.value->kind == 4;
      });
  if (member == player.fields().end())
    return fail(e, "Bat actual Player action source member missing");
  d.valid_ = true;
  *this = std::move(d);
  return true;
}
bool PlayerVisualScriptsData::load_file(const char *path,
                                        const PlayerInitializationData &player,
                                        std::string &e) {
  auto *f = std::fopen(path, "rb");
  if (!f)
    return fail(e, "Cannot open Player visual script resource");
  if (std::fseek(f, 0, SEEK_END)) {
    std::fclose(f);
    return fail(e, "Cannot size Player visual script resource");
  }
  auto n = std::ftell(f);
  if (n < 128 || n > 1024 * 1024) {
    std::fclose(f);
    return fail(e, "Player visual file size rejected");
  }
  std::rewind(f);
  std::vector<uint8_t> b(size_t(n), uint8_t{});
  bool ok = std::fread(b.data(), 1, b.size(), f) == b.size();
  std::fclose(f);
  return ok ? load(b.data(), b.size(), player, e)
            : fail(e, "Player visual short read");
}
bool PlayerVisualScriptsData::source_hash(std::string_view path,
                                          std::array<uint8_t, 32> &h) const {
  auto f = sources_.find(std::string(path));
  if (f == sources_.end())
    return false;
  h = f->second;
  return true;
}
} // namespace encore::upstream
