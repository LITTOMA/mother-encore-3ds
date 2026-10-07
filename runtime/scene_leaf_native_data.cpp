#include "encore/crc32.hpp"
#include "encore/scene_leaf_native.hpp"
#include "encore/utf8.hpp"
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
struct Reader {
  const uint8_t *p;
  size_t n;
  bool raw(void *v, size_t z) {
    if (z > n)
      return false;
    std::memcpy(v, p, z);
    p += z;
    n -= z;
    return true;
  }
  bool u(uint32_t &v) {
    if (n < 4)
      return false;
    v = uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 |
        uint32_t(p[3]) << 24;
    p += 4;
    n -= 4;
    return true;
  }
  bool f(float &v) {
    uint32_t x;
    if (!u(x))
      return false;
    std::memcpy(&v, &x, 4);
    return std::isfinite(v);
  }
  bool s(std::string &v, bool empty = false) {
    uint32_t z;
    if (!u(z) || (!z && !empty) || z > 4096 || z > n)
      return false;
    v.assign(reinterpret_cast<const char *>(p), z);
    p += z;
    n -= z;
    size_t c;
    return v.find('\0') == v.npos && encore::utf8_count(v, c);
  }
};
bool same(const FieldIdentity &a, const FieldIdentity &b) {
  return a.scene_id == b.scene_id && a.upstream_commit == b.upstream_commit &&
         a.source_sha256 == b.source_sha256;
}
bool bound(const SceneLeafNativeRecord &v, const SceneLeafNativeSources &s) {
  switch (v.kind) {
  case SceneLeafKind::CameraAnimation:
  case SceneLeafKind::GameCamera: {
    auto *d = s.camera.record(v.owner);
    return d &&
           ((v.kind == SceneLeafKind::GameCamera && v.id == d->id) ||
            (v.kind == SceneLeafKind::CameraAnimation &&
             v.id == d->animation_id)) &&
           !v.target;
  }
  case SceneLeafKind::JumpAnimation: {
    auto *d = s.transitions.record(v.owner);
    return d && d->kind == 1 && v.target == d->sprite_id;
  }
  case SceneLeafKind::BirdAnimation: {
    auto *d = s.birds.record(v.owner);
    return d && v.id == d->children[4] && v.target == d->children[0];
  }
  case SceneLeafKind::DroppedAnimation:
  case SceneLeafKind::DroppedTween: {
    auto *d = s.dropped.binding(v.owner);
    return d &&
           v.id == (v.kind == SceneLeafKind::DroppedTween ? d->tween_id
                                                          : d->animation_id) &&
           v.target;
  }
  case SceneLeafKind::PayphoneAnimation: {
    auto *d = s.payphone.record(v.owner);
    return d && v.target == d->sprite_id;
  }
  case SceneLeafKind::MelodyAnimation:
  case SceneLeafKind::MelodyRect: {
    auto *d = s.melody.binding(v.owner);
    return d && v.target == d->bg_id &&
           v.id == (v.kind == SceneLeafKind::MelodyAnimation ? d->animation_id
                                                             : d->bg_id);
  }
  case SceneLeafKind::EmptyAnimation:
  case SceneLeafKind::IntroCamera: {
    auto *d = s.tree.record(v.owner);
    return d && d->script.empty() && v.target == 0;
  }
  case SceneLeafKind::EditorReferenceRect:
    return v.editor_only && !v.target;
  }
  return false;
}
} // namespace
const SceneLeafClip *SceneLeafNativeRecord::clip(std::string_view name) const {
  for (const auto &v : clips)
    if (v.name == name)
      return &v;
  return nullptr;
}
const SceneLeafNativeRecord *SceneLeafNativeData::record(uint32_t id) const {
  for (const auto &v : records_)
    if (v.id == id)
      return &v;
  return nullptr;
}
const SceneLeafNativeRecord *SceneLeafNativeData::owner(SceneLeafKind k,
                                                        uint32_t id) const {
  for (const auto &v : records_)
    if (v.kind == k && v.owner == id)
      return &v;
  return nullptr;
}
bool SceneLeafNativeData::load(const uint8_t *p, size_t n,
                               const SceneLeafNativeSources &s,
                               std::string &e) {
  if (valid_ || !p || n < 88 || n > 4 * 1024 * 1024 ||
      std::memcmp(p, "ENCSLN01", 8) || !s.tree.valid() || !s.camera.valid() ||
      !s.transitions.valid() || !s.birds.valid() || !s.dropped.valid() ||
      !s.payphone.valid() || !s.melody.valid())
    return fail(e, "Scene leaf format/typed dependencies rejected");
  Reader r{p + 8, n - 8};
  SceneLeafNativeData c;
  uint32_t version, cap, rules, family, scene, bytes, crc;
  if (!r.u(version) || !r.u(cap) || !r.u(rules) || !r.u(family) ||
      !r.u(scene) || !r.u(bytes) || !r.u(crc) || version != 1 || cap != 1 ||
      rules != 1 || family != 0x454e0070 || bytes != n - 88 ||
      !r.raw(c.identity_.upstream_commit.data(), 20) ||
      !r.raw(c.identity_.source_sha256.data(), 32) ||
      encore::crc32(r.p, r.n) != crc)
    return fail(e, "Scene leaf version/capability/rules/CRC rejected");
  c.identity_.scene_id = scene;
  if (!same(c.identity_, s.tree.identity()) ||
      !same(c.identity_, s.camera.identity()) ||
      s.transitions.source_pin() != c.identity_.upstream_commit ||
      s.transitions.source_scene() != s.tree.source_scene() ||
      s.birds.identity().upstream_commit != c.identity_.upstream_commit ||
      s.birds.source_scene() != s.tree.source_scene() ||
      s.dropped.source_pin() != c.identity_.upstream_commit ||
      s.dropped.scene() != s.tree.source_scene() ||
      s.payphone.source_pin() != c.identity_.upstream_commit ||
      s.payphone.scene_hash() != c.identity_.source_sha256 ||
      s.melody.source_pin() != c.identity_.upstream_commit ||
      s.melody.scene() != s.tree.source_scene())
    return fail(e, "Scene leaf cross-source identity rejected");
  std::set<std::string> names;
  for (auto &v : c.symbols_)
    if (!r.s(v) || !names.insert(v).second)
      return fail(e, "Scene leaf source symbols rejected");
  uint32_t count;
  if (!r.u(count) || !count || count > 8192)
    return fail(e, "Scene leaf source closure rejected");
  for (uint32_t i = 0; i < count; ++i) {
    std::string path;
    std::array<uint8_t, 32> h{}, actual{};
    if (!r.s(path) || !r.raw(h.data(), 32) || h == std::array<uint8_t, 32>{})
      return fail(e, "Scene leaf source proof rejected");
    bool known = false;
    auto check = [&](bool found) {
      if (!found)
        return true;
      known = true;
      return actual == h;
    };
    if (!check(s.tree.source_hash(path, actual)) ||
        !check(s.camera.source_hash(path, actual)) ||
        !check(s.transitions.source_hash(path, actual)) ||
        !check(s.birds.source_hash(path, actual)) ||
        !check(s.dropped.source_hash(path, actual)) ||
        !check(s.payphone.source_hash(path, actual)) ||
        !check(s.melody.source_hash(path, actual)) || !known)
      return fail(e, "Scene leaf source SHA outside typed closure");
  }
  if (!r.u(count) || !count || count > 32768)
    return fail(e, "Scene leaf roster count rejected");
  std::set<uint32_t> ids;
  std::array<size_t, 13> roles{};
  for (uint32_t i = 0; i < count; ++i) {
    SceneLeafNativeRecord v;
    uint32_t kind;
    if (!r.u(v.id) || !r.u(v.owner) || !r.u(v.parent) || !r.u(kind) ||
        kind < 1 || kind > 12 || !r.u(v.target) || !r.u(v.method) ||
        !r.f(v.speed) || !r.s(v.path) || !r.s(v.native_class) ||
        !r.s(v.active_clip, true) || !ids.insert(v.id).second)
      return fail(e, "Scene leaf unknown/duplicate descriptor");
    v.kind = SceneLeafKind(kind);
    ++roles[kind];
    const auto *node = s.tree.record(v.id);
    const auto *parent = s.tree.record(v.owner);
    if (!node || !parent || node->parent != v.parent || node->path != v.path ||
        node->class_index >= s.tree.classes().size() ||
        s.tree.classes()[node->class_index] != v.native_class ||
        ((kind != 9) && !node->script.empty()))
      return fail(e, "Scene leaf actual class/parent/script rejected");
    const std::string klass = kind <= 7    ? "AnimationPlayer"
                              : kind == 8  ? "Tween"
                              : kind <= 10 ? "Camera2D"
                              : kind == 11 ? "ReferenceRect"
                                           : "TextureRect";
    if (v.native_class != klass || v.method != (kind == 1 ? 1u : 0u) ||
        v.speed != (kind <= 7 && kind != 4 ? 1.0f : 0.0f))
      return fail(e, "Scene leaf native playback/defaults rejected");
    uint32_t clips;
    if (!r.u(clips) || clips > 256 || bool(clips) != (kind <= 7 && kind != 6))
      return fail(e, "Scene leaf actual clip library rejected");
    std::set<std::string> cn;
    for (uint32_t j = 0; j < clips; ++j) {
      SceneLeafClip a;
      uint32_t loop;
      if (!r.s(a.name) || !r.f(a.length) || a.length <= 0 || a.length > 3600 ||
          !r.u(loop) || loop > 1 || !cn.insert(a.name).second)
        return fail(e, "Scene leaf animation metadata rejected");
      a.loop = loop;
      v.clips.push_back(std::move(a));
    }
    if ((!v.active_clip.empty()) != (kind == 2 || kind == 4 || kind == 5) ||
        (!v.active_clip.empty() && !v.clip(v.active_clip)))
      return fail(e, "Scene leaf source active clip rejected");
    if (kind == 9 || kind == 10) {
      uint32_t rot, current;
      if (!r.f(v.camera.offset.x) || !r.f(v.camera.offset.y) ||
          !r.f(v.camera.zoom.x) || !r.f(v.camera.zoom.y) ||
          v.camera.zoom.x == 0 || v.camera.zoom.y == 0 ||
          !r.u(v.camera.anchor) || v.camera.anchor != 1 || !r.u(rot) ||
          rot > 1 || !r.u(current) || current != 0 || !r.u(v.camera.process) ||
          v.camera.process > 1)
        return fail(e, "Scene leaf camera source properties rejected");
      v.camera.rotating = rot;
      v.camera.current = current;
      for (auto &x : v.camera.limits) {
        uint32_t bits;
        if (!r.u(bits))
          return fail(e, "Scene leaf camera limits truncated");
        std::memcpy(&x, &bits, 4);
      }
    } else if (kind == 11) {
      for (auto &x : v.rect)
        if (!r.f(x))
          return fail(e, "Scene leaf reference geometry rejected");
      uint32_t ed;
      if (!r.u(ed) || ed != 1)
        return fail(e,
                    "Scene leaf runtime ReferenceRect rendering unsupported");
      v.editor_only = true;
    }
    if (!bound(v, s))
      return fail(e, "Scene leaf does not match actual typed source owner");
    c.records_.push_back(std::move(v));
  }
  size_t jumps = 0;
  for (const auto &v : s.transitions.records())
    if (v.kind == 1)
      ++jumps;
  if (r.n || roles[1] != s.camera.records().size() || roles[2] != jumps ||
      roles[3] != s.birds.records().size() ||
      roles[4] != s.dropped.bindings().size() ||
      roles[5] != s.payphone.records().size() || roles[6] != 1 ||
      roles[7] != s.melody.bindings().size() ||
      roles[8] != s.dropped.bindings().size() ||
      roles[9] != s.camera.records().size() || roles[10] != 1 ||
      roles[11] != 1 || roles[12] != s.melody.bindings().size())
    return fail(e, "Scene leaf source roster incomplete/trailing bytes");
  c.valid_ = true;
  *this = std::move(c);
  e.clear();
  return true;
}
bool SceneLeafNativeData::load_file(const char *path,
                                    const SceneLeafNativeSources &s,
                                    std::string &e) {
  auto *f = path ? std::fopen(path, "rb") : nullptr;
  if (!f)
    return fail(e, "Cannot open scene leaf resource");
  if (std::fseek(f, 0, SEEK_END)) {
    std::fclose(f);
    return fail(e, "Cannot seek scene leaf resource");
  }
  auto n = std::ftell(f);
  if (n < 0 || n > 4 * 1024 * 1024 || std::fseek(f, 0, SEEK_SET)) {
    std::fclose(f);
    return fail(e, "Scene leaf file size rejected");
  }
  std::vector<uint8_t> b(static_cast<size_t>(n));
  auto got = std::fread(b.data(), 1, b.size(), f);
  std::fclose(f);
  return got == b.size() ? load(b.data(), b.size(), s, e)
                         : fail(e, "Scene leaf file truncated");
}
} // namespace encore::upstream
