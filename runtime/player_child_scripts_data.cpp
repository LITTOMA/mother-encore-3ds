#include "encore/player_child_scripts.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
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
  uint32_t count(uint32_t max) {
    auto k = u();
    if (k > max)
      ok = false;
    return ok ? k : 0;
  }
  bool boolean() {
    auto v = u();
    if (v > 1)
      ok = false;
    return v != 0;
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
    if (std::all_of(v.begin(), v.end(), [](uint8_t x) { return !x; }))
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
const PlayerChildScriptRecord *
PlayerChildScriptsData::record(uint32_t role) const {
  for (const auto &r : records_)
    if (r.role == role)
      return &r;
  return nullptr;
}
bool PlayerChildScriptsData::source_hash(std::string_view s,
                                         std::array<uint8_t, 32> &h) const {
  auto i = sources_.find(std::string(s));
  if (i == sources_.end())
    return false;
  h = i->second;
  return true;
}
bool PlayerChildScriptsData::load(const uint8_t *p, size_t n,
                                  const PlayerInitializationData &player,
                                  const PlayerReadyData &ready,
                                  std::string &e) {
  *this = PlayerChildScriptsData{};
  if (!player.valid() || !ready.valid() || !p || n < 128 || n > 8000000 ||
      std::memcmp(p, "ENCPSCR1", 8) || u32(p + 8) != 2 || u32(p + 12) != 128 ||
      u32(p + 16) != n || u32(p + 20) != crc(p + 128, n - 128) ||
      u32(p + 24) != 0x454e0061 || u32(p + 28) != 2 || u32(p + 32) != 1 ||
      u32(p + 124) != 0)
    return fail(e, "Player child scripts format/capability/CRC rejected");
  identity_ = player.identity();
  if (u32(p + 36) != identity_.scene_id ||
      !std::equal(p + 40, p + 60, identity_.upstream_commit.begin()) ||
      !std::equal(p + 60, p + 92, identity_.source_sha256.begin()))
    return fail(e, "Player child scripts source identity rejected");
  std::copy(p + 92, p + 124, ir_.begin());
  if (std::all_of(ir_.begin(), ir_.end(), [](uint8_t v) { return !v; }))
    return fail(e, "Player child scripts IR proof missing");
  R r{p + 128, n - 128};
  scene_ = r.s();
  player_ir_ = r.h();
  ready_ir_ = r.h();
  if (scene_ != player.recipe().source_scene() ||
      player_ir_ != player.ir_sha256() || ready_ir_ != ready.ir_sha256() ||
      ready.initialization_ir_sha256() != player_ir_)
    return fail(
        e,
        "Player child scripts actual initialization/Ready dependency rejected");
  auto count = r.count(100);
  if (count != 4)
    return fail(e, "Player child scripts four source owners required");
  std::set<uint32_t> ids;
  for (uint32_t i = 0; i < count; ++i) {
    PlayerChildScriptRecord x;
    x.role = r.u();
    x.id = r.u();
    x.ready = r.u();
    x.path = r.s();
    x.native_class = r.s();
    x.script = r.s();
    x.script_sha = r.h();
    auto *d = player.recipe().record(x.id);
    if (x.role != i + 1 || !ids.insert(x.id).second || !d ||
        x.ready != d->ready || x.path != d->path ||
        x.native_class != d->native_class || x.script != d->script ||
        x.script_sha != d->script_sha)
      return fail(e, "Player child scripts exact recipe identity rejected");
    records_.push_back(std::move(x));
  }
  emote_.object_path = r.s();
  emote_.animation_path = r.s();
  emote_.direction_method = r.s();
  emote_.direction_member = r.s();
  emote_.signal = r.s();
  emote_.method = r.s();
  auto clips = r.count(100);
  std::set<std::string> names;
  for (uint32_t i = 0; i < clips; ++i) {
    auto name = r.s();
    if (name.empty() || !names.insert(name).second)
      return fail(e, "Player emote direction clip rejected");
    emote_.sensitive_clips.push_back(name);
  }
  auto number = [&]() {
    auto bits = r.u();
    float v;
    std::memcpy(&v, &bits, 4);
    if (!std::isfinite(v))
      r.ok = false;
    return v;
  };
  emote_.padding = number();
  emote_.negative_scale = number();
  emote_.other_scale = number();
  for (auto &v : tint_.color)
    v = number();
  tint_.signal = r.s();
  tint_.method = r.s();
  auto paths = r.count(1000);
  for (uint32_t i = 0; i < paths; ++i)
    tint_.paths.push_back(r.s());
  camera_process_mode_ = r.u();
  const auto connection_count=r.count(2);
  if(connection_count!=2)return fail(e,"Player Camera two actual source connections required");
  std::set<std::string> camera_methods;
  for(uint32_t i=0;i<connection_count;++i){
    PlayerCameraSourceConnection c;c.role=r.u();c.signal=r.s();c.method=r.s();
    if(c.role!=i+1||c.signal.empty()||c.method.empty()||
       !camera_methods.insert(c.method).second||
       c.signal.find_first_of("./:\\")!=c.signal.npos||c.method.find_first_of("./:\\")!=c.method.npos)
      return fail(e,"Player Camera source connection role/symbol rejected");
    camera_connections_.push_back(std::move(c));
  }
  if (clips == 0 || emote_.object_path.empty() ||
      emote_.animation_path.empty() || emote_.signal.empty() ||
      emote_.method.empty() || emote_.direction_method.empty() ||
      emote_.direction_member != ready.binding(PlayerReadyBinding::Direction) ||
      emote_.padding < 0 || emote_.padding > 1000000 ||
      emote_.negative_scale >= 0 || emote_.other_scale <= 0 ||
      tint_.signal.empty() || tint_.method.empty() || camera_process_mode_ > 1)
    return fail(e, "Player child source policy rejected");
  for (const auto &s : tint_.paths)
    if (s.empty() || s.front() == '/' || s.find(':') != s.npos)
      return fail(e, "Player tint relative source path rejected");
  auto nested = [&](auto &data) {
    auto len = r.u();
    if (!r.ok || len > r.n || len < 128 ||
        !std::equal(r.p + 92, r.p + 124, ir_.begin()))
      return false;
    bool ok = data.load(r.p, len, identity_, e);
    r.p += len;
    r.n -= len;
    return ok;
  };
  if (!nested(camera_) || !nested(arrows_))
    return fail(e, "Player child checked camera/arrow source pack rejected");
  if (camera_.records().size() != 1 || arrows_.records().size() != 1 ||
      camera_.records()[0].id != record(3)->id ||
      arrows_.records()[0].id != record(4)->id ||
      camera_.script() != record(3)->script ||
      camera_.script_sha() != record(3)->script_sha ||
      arrows_.script() != record(4)->script ||
      arrows_.script_sha() != record(4)->script_sha)
    return fail(e, "Player child camera/arrow record scope rejected");
  auto refs = [&](uint32_t id, std::string_view cls) {
    auto *d = player.recipe().record(id);
    return d && d->native_class == cls;
  };
  const auto &c = camera_.records()[0];
  if (!refs(c.parent_id, player.recipe().records()[0].native_class) ||
      !refs(c.animation_id, "AnimationPlayer") || !refs(c.area_id, "Area2D") ||
      !refs(c.shape_id, "CollisionShape2D") || c.arrows_id != record(4)->id)
    return fail(e, "Player native camera actual children rejected");
  for (const auto &v : arrows_.sprites())
    if (!refs(v.id, "AnimatedSprite") || v.root_id != record(4)->id)
      return fail(e, "Player native arrow Sprite identity rejected");
  for (const auto &v : arrows_.players())
    if (!refs(v.id, "AnimationPlayer"))
      return fail(e, "Player native arrow AnimationPlayer identity rejected");
  auto sources = r.count(10000);
  for (uint32_t i = 0; i < sources; ++i) {
    auto path = r.s();
    auto h = r.h();
    if (!sources_.emplace(path, h).second)
      return fail(e, "Player child duplicate source rejected");
    std::array<uint8_t, 32> actual{};
    if (player.source_hash(path, actual) && actual != h)
      return fail(e, "Player child original source hash mismatch");
  }
  for (const auto &x : records_)
    if (x.script.find("::") == x.script.npos) {
      std::array<uint8_t, 32> h;
      if (!source_hash(x.script, h) || h != x.script_sha)
        return fail(e, "Player child whole script source missing");
    }
  std::array<uint8_t, 32> scene_sha{};
  if (!source_hash(scene_, scene_sha) || scene_sha != identity_.source_sha256 ||
      !r.ok || r.n)
    return fail(e, "Player child trailing/invalid source data rejected");
  valid_ = true;
  e.clear();
  return true;
}
bool PlayerChildScriptsData::load_file(const char *path,
                                       const PlayerInitializationData &p,
                                       const PlayerReadyData &r,
                                       std::string &e) {
  auto *f = std::fopen(path, "rb");
  if (!f)
    return fail(e, "Player child resource cannot open");
  if (std::fseek(f, 0, SEEK_END)) {
    std::fclose(f);
    return fail(e, "Player child seek failed");
  }
  auto n = std::ftell(f);
  if (n < 128 || n > 8000000 || std::fseek(f, 0, SEEK_SET)) {
    std::fclose(f);
    return fail(e, "Player child file bounds rejected");
  }
  std::vector<uint8_t> b(size_t(n), 0);
  auto read = std::fread(b.data(), 1, b.size(), f);
  std::fclose(f);
  return read == b.size() ? load(b.data(), b.size(), p, r, e)
                          : fail(e, "Player child short read rejected");
}
} // namespace encore::upstream
