#include "encore/audio_data.hpp"
#include "encore/grass_native.hpp"
#include "encore/utf8.hpp"
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
struct R {
  const uint8_t *p;
  size_t n, at = 128;
  bool ok = true;
  uint32_t integer() {
    if (!ok || at > n || n - at < 4) {
      ok = false;
      return 0;
    }
    auto x = u(p + at);
    at += 4;
    return x;
  }
  float scalar() {
    auto v = integer();
    float x;
    std::memcpy(&x, &v, 4);
    if (!std::isfinite(x))
      ok = false;
    return x;
  }
  std::string text() {
    auto len = integer();
    if (!ok || len > 2048 || at > n || len > n - at) {
      ok = false;
      return {};
    }
    std::string s(reinterpret_cast<const char *>(p + at), len);
    at += len;
    size_t a = 0;
    uint32_t cp;
    while (a < s.size())
      if (!utf8_next(s, a, cp) || cp < 32) {
        ok = false;
        break;
      }
    return s;
  }
  bool flag() {
    auto x = integer();
    if (x > 1)
      ok = false;
    return x != 0;
  }
};
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
} // namespace
bool GrassNativeData::load_file(const char *path, const FieldData &f,
                                const FieldNodeTreeData &tree, std::string &e) {
  if (!path)
    return fail(e, "Grass native file path missing");
  std::ifstream file(path, std::ios::binary);
  if (!file)
    return fail(e, "Grass native file missing");
  file.seekg(0, std::ios::end);
  auto n = file.tellg();
  if (n < 128 || n > 2 * 1024 * 1024)
    return fail(e, "Grass native file size");
  file.seekg(0);
  std::vector<uint8_t> b(static_cast<size_t>(n));
  if (!file.read(reinterpret_cast<char *>(b.data()), n))
    return fail(e, "Grass native file read");
  return load(b.data(), b.size(), f, tree, e);
}
bool GrassNativeData::load(const uint8_t *p, size_t n, const FieldData &f,
                           const FieldNodeTreeData &tree, std::string &e) {
  if (!f.valid() || !tree.valid() ||
      tree.identity().scene_id != f.identity().scene_id ||
      tree.identity().source_sha256 != f.identity().source_sha256 ||
      tree.identity().upstream_commit != f.identity().upstream_commit || !p ||
      n < 128 || n > 2 * 1024 * 1024 || std::memcmp(p, "ENCGRS01", 8) ||
      u(p + 8) != 1 || u(p + 12) != 128 || u(p + 16) != n ||
      u(p + 20) != audio_crc32(p + 128, n - 128) || u(p + 24) != 0x454e0071 ||
      u(p + 28) != 1 || u(p + 32) != 1 || u(p + 124))
    return fail(e, "Grass native header/source/CRC/capability rejected");
  GrassNativeData d;
  FieldIdentity identity;
  identity.scene_id = u(p + 36);
  std::copy(p + 40, p + 60, identity.upstream_commit.begin());
  std::copy(p + 60, p + 92, identity.source_sha256.begin());
  std::copy(p + 92, p + 124, d.ir_.begin());
  if (identity.upstream_commit != f.identity().upstream_commit ||
      !identity.scene_id ||
      std::all_of(d.ir_.begin(), d.ir_.end(), [](uint8_t b) { return !b; }))
    return fail(e, "Grass native pin/identity/IR rejected");
  R r{p, n};
  auto length = r.integer();
  if (length > n - r.at || !d.recipe_.load(p + r.at, length, identity, e))
    return false;
  r.at += length;
  if (d.recipe_.records().size() != 6 || d.recipe_.ir_sha256() != d.ir_)
    return fail(e, "Grass complete native recipe/IR rejected");
  std::set<uint32_t> ids;
  for (auto &v : d.nodes_) {
    v = r.integer();
    if (!v || !ids.insert(v).second || !d.recipe_.record(v))
      return fail(e, "Grass role source closure rejected");
  }
  d.script_ = r.text();
  d.tween_source_ = r.text();
  d.tween_signal_ = r.text();
  d.monitoring_ = r.flag();
  d.monitorable_ = r.flag();
  d.disabled_ = r.flag();
  d.centered_ = r.flag();
  auto &v = d.profile_;
  v.stable_id = r.integer();
  v.collision_layer = r.integer();
  v.collision_mask = r.integer();
  v.collision_extents = {r.scalar(), r.scalar()};
  v.sprite_offset = {r.scalar(), r.scalar()};
  v.collision_offset = {r.scalar(), r.scalar()};
  v.idle_delay = r.scalar();
  v.squash = r.scalar();
  v.blend_divisor = r.scalar();
  v.enter_tween = r.scalar();
  v.exit_tween = r.scalar();
  for (auto &frame : v.frames) {
    auto x = r.integer();
    if (x > UINT16_MAX)
      return fail(e, "Grass source frame overflow");
    frame = uint16_t(x);
  }
  const FieldGrassProfile *profile = nullptr;
  FieldGrassProfile source;
  for (uint32_t i = 0; i < f.profile_count(); ++i) {
    auto p0 = f.profile(i);
    if (p0.stable_id == v.stable_id) {
      source = p0;
      profile = &source;
      break;
    }
  }
  auto eq = [](Vec2 a, Vec2 b) { return a.x == b.x && a.y == b.y; };
  if (!profile || v.collision_layer != source.collision_layer ||
      v.collision_mask != source.collision_mask ||
      !eq(v.collision_extents, source.collision_extents) ||
      !eq(v.sprite_offset, source.sprite_offset) ||
      !eq(v.collision_offset, source.collision_offset) ||
      v.idle_delay != source.idle_delay || v.squash != source.squash ||
      v.blend_divisor != source.blend_divisor ||
      float(v.enter_tween) != float(source.enter_tween) ||
      float(v.exit_tween) != float(source.exit_tween) ||
      v.frames != source.frames)
    return fail(e, "Grass actual FieldData source profile differs");
  v = source;
  auto nc = r.integer();
  if (nc != 4)
    return fail(e, "Grass complete source signal count rejected");
  std::set<uint32_t> kinds;
  for (uint32_t i = 0; i < nc; ++i) {
    GrassNativeConnection c;
    c.emitter = r.integer();
    c.receiver = r.integer();
    c.kind = r.integer();
    c.signal = r.text();
    c.method = r.text();
    if (!ids.count(c.emitter) || c.receiver != d.nodes_[0] || c.kind < 1 ||
        c.kind > 4 || !kinds.insert(c.kind).second || c.signal.empty() ||
        c.method.empty() ||
        c.emitter != (c.kind == 3 ? d.nodes_[5] : d.nodes_[0]))
      return fail(e, "Grass source signal binding rejected");
    d.connections_.push_back(std::move(c));
  }
  const char *classes[] = {"Area2D",          "CollisionShape2D", "Sprite",
                           "AnimationPlayer", "AnimationTree",    "Timer"};
  for (size_t i = 0; i < 6; ++i) {
    auto *node = d.recipe_.record(d.nodes_[i]);
    if (node->native_class != classes[i] || node->native_generated ||
        (i == 0 ? (node->script != d.script_ || node->parent != 0 ||
                   node->script_methods != 129)
                : (!node->script.empty() || node->parent != d.nodes_[0])))
      return fail(e, "Grass native/source class role mismatch");
  }
  std::array<uint8_t, 32> hash{};
  std::array<uint8_t, 32> actual{};
  if (!tree.source_hash(d.recipe_.source_scene(), actual) ||
      actual != d.identity().source_sha256 ||
      !tree.source_hash(d.script_, actual) ||
      actual != d.recipe_.record(d.nodes_[0])->script_sha)
    return fail(e, "Grass original scene/script source namespace rejected");
  if (!r.ok || r.at != n || d.script_.empty() || d.tween_source_.empty() ||
      d.tween_signal_.empty() || !d.recipe_.source_hash(d.script_, hash) ||
      hash != d.recipe_.record(d.nodes_[0])->script_sha ||
      !d.recipe_.source_hash(d.tween_source_, hash) || !d.monitoring_ ||
      d.monitorable_ || d.disabled_ || !d.centered_)
    return fail(e, "Grass bounded source capability/payload rejected");
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
bool GrassNativeData::source_hash(std::string_view p,
                                  std::array<uint8_t, 32> &h) const {
  return valid_ && recipe_.source_hash(p, h);
}
bool GrassNativeData::native_matches(const FieldNodeDescriptor &n) const {
  auto *r = recipe_.record(n.id);
  return valid_ && r && r->native_class == n.native_class &&
         r->script == n.script && r->script_sha == n.script_sha &&
         r->path == n.path && r->class_index == n.class_index;
}
} // namespace encore::upstream
