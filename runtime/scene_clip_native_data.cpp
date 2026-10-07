#include "encore/crc32.hpp"
#include "encore/scene_clip_native.hpp"
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
    uint32_t a;
    if (!u(a))
      return false;
    std::memcpy(&v, &a, 4);
    return std::isfinite(v);
  }
  bool s(std::string &v, bool empty = false) {
    uint32_t z;
    if (!u(z) || (!empty && !z) || z > 4096 || z > n)
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
} // namespace
const SceneClipNativeRecord *SceneClipNativeData::record(uint32_t id) const {
  for (const auto &v : records_)
    if (v.id == id)
      return &v;
  return nullptr;
}
const SceneClipNativeRecord *SceneClipNativeData::owner(SceneClipOwner k,
                                                        uint32_t id) const {
  for (const auto &v : records_)
    if (v.kind == k && v.owner == id)
      return &v;
  return nullptr;
}
bool SceneClipNativeData::load(const uint8_t *p, size_t n,
                               const FieldNodeTreeData &t,
                               const FieldOpenableDoorData &o,
                               const FieldPresentData &present,
                               const FieldEmoteData &em, const FieldBushData &b,
                               std::string &e) {
  if (valid_ || !p || n < 88 || n > 4 * 1024 * 1024 ||
      std::memcmp(p, "ENCSCL01", 8) || !t.valid() || !o.valid() ||
      !present.valid() || !em.valid() || !b.valid())
    return fail(e, "Scene clips format/typed dependencies rejected");
  Reader r{p + 8, n - 8};
  SceneClipNativeData c;
  uint32_t version, cap, rules, family, scene, bytes, crc;
  if (!r.u(version) || !r.u(cap) || !r.u(rules) || !r.u(family) ||
      !r.u(scene) || !r.u(bytes) || !r.u(crc) || version != 1 || cap != 1 ||
      rules != 1 || family != 0x454e006d || bytes != n - 88 ||
      !r.raw(c.identity_.upstream_commit.data(), 20) ||
      !r.raw(c.identity_.source_sha256.data(), 32) ||
      encore::crc32(r.p, r.n) != crc)
    return fail(e, "Scene clips version/capability/rules/CRC rejected");
  c.identity_.scene_id = scene;
  if (!same(c.identity_, t.identity()) || !same(c.identity_, o.identity()) ||
      present.source_pin() != c.identity_.upstream_commit ||
      present.scene() != t.source_scene() ||
      em.source_pin() != c.identity_.upstream_commit ||
      em.scene_id() != scene || b.source_pin() != c.identity_.upstream_commit ||
      b.scene_id() != scene || b.scene_hash() != c.identity_.source_sha256)
    return fail(e, "Scene clips cross-source identity rejected");
  std::set<std::string> symbols;
  for (auto &v : c.symbols_)
    if (!r.s(v) || !symbols.insert(v).second)
      return fail(e, "Scene clips native symbols rejected");
  std::string root, autoplay;
  uint32_t mode, method;
  float speed, blend;
  if (!r.s(root) || !r.s(autoplay, true) || !r.u(mode) || !r.u(method) ||
      !r.f(speed) || !r.f(blend) || root != ".." || !autoplay.empty() ||
      mode != 1 || method != 0 || speed != 1 || blend != 0)
    return fail(e, "Scene clips unsupported native playback/blending rejected");
  uint32_t count;
  std::set<std::string> sources;
  if (!r.u(count) || !count || count > 8192)
    return fail(e, "Scene clips source closure size rejected");
  for (uint32_t i = 0; i < count; ++i) {
    std::string path;
    std::array<uint8_t, 32> hash{}, actual{};
    if (!r.s(path) || !sources.insert(path).second || !r.raw(hash.data(), 32) ||
        hash == std::array<uint8_t, 32>{})
      return fail(e, "Scene clips source proof rejected");
    bool known = false;
    auto check = [&](bool found) {
      if (found) {
        known = true;
        return actual == hash;
      }
      return true;
    };
    if (!check(t.source_hash(path, actual)) ||
        !check(o.source_hash(path, actual)) ||
        !check(present.source_hash(path, actual)) ||
        !check(em.source_hash(path, actual)) ||
        !check(b.source_hash(path, actual)) || !known)
      return fail(e, "Scene clips source hash outside actual typed closure");
  }
  const size_t expected = o.records().size() + present.bindings().size() +
                          em.records().size() + b.records().size();
  if (!r.u(count) || count != expected || count > 32768)
    return fail(e, "Scene clips actual owner closure rejected");
  std::set<uint32_t> ids;
  std::set<std::pair<uint32_t, uint32_t>> owners;
  for (uint32_t i = 0; i < count; ++i) {
    SceneClipNativeRecord v;
    uint32_t kind;
    if (!r.u(v.id) || !r.u(v.owner) || !r.u(v.parent) || !r.u(kind) ||
        kind < 1 || kind > 4 || !r.u(v.timer) || !r.u(v.frame_target) ||
        !r.s(v.path) || !r.s(v.timer_method, true) ||
        !ids.insert(v.id).second || !owners.emplace(kind, v.owner).second)
      return fail(e, "Scene clips unknown/duplicate role rejected");
    v.kind = SceneClipOwner(kind);
    const auto *node = t.record(v.id);
    const auto *parent = t.record(v.owner);
    if (!node || !parent || node->parent != v.parent || v.parent != v.owner ||
        node->path != v.path || node->class_index >= t.classes().size() ||
        t.classes()[node->class_index] != "AnimationPlayer" ||
        !node->script.empty() || node->script_sha != std::array<uint8_t, 32>{})
      return fail(e, "Scene clips actual AP class/parent/source rejected");
    bool bound = false;
    switch (v.kind) {
    case SceneClipOwner::Openable: {
      const auto *a = o.record(v.owner);
      const auto *timer = t.record(v.timer);
      bound = a && a->children[12] == v.id && a->children[11] == v.timer &&
              timer && timer->parent == v.owner &&
              timer->class_index < t.classes().size() &&
              t.classes()[timer->class_index] == "Timer" &&
              timer->script.empty() && !v.timer_method.empty();
      break;
    }
    case SceneClipOwner::Present: {
      const auto *a = present.binding(v.owner);
      bound = a && parent->path == a->node;
      break;
    }
    case SceneClipOwner::Emote: {
      const auto *a = em.record(v.owner);
      bound = a && parent->path == a->node;
      break;
    }
    case SceneClipOwner::Bush: {
      const auto *a = b.record(v.owner);
      bound = a && parent->path == a->node;
      break;
    }
    }
    const auto *frame = t.record(v.frame_target);
    if ((v.kind == SceneClipOwner::Openable && v.frame_target) ||
        (v.kind != SceneClipOwner::Openable &&
         (!frame || frame->class_index >= t.classes().size() ||
          t.classes()[frame->class_index] != "Sprite" ||
          (v.kind == SceneClipOwner::Emote ? frame->id != v.owner
                                           : frame->parent != v.owner))))
      return fail(e, "Scene clips actual frame target rejected");
    if (v.kind == SceneClipOwner::Bush &&
        (!b.record(v.owner) || b.record(v.owner)->sprite_id != v.frame_target))
      return fail(e, "Scene clips Bush source frame target differs");
    if (!bound || (v.kind != SceneClipOwner::Openable &&
                   (v.timer || !v.timer_method.empty())))
      return fail(e, "Scene clips typed source owner/Timer binding rejected");
    c.records_.push_back(std::move(v));
  }
  if (r.n)
    return fail(e, "Scene clips trailing metadata rejected");
  c.valid_ = true;
  *this = std::move(c);
  e.clear();
  return true;
}
bool SceneClipNativeData::load_file(const char *p, const FieldNodeTreeData &t,
                                    const FieldOpenableDoorData &o,
                                    const FieldPresentData &present,
                                    const FieldEmoteData &em,
                                    const FieldBushData &b, std::string &e) {
  FILE *f = p ? std::fopen(p, "rb") : nullptr;
  if (!f)
    return fail(e, "Scene clips resource unavailable");
  bool ok = std::fseek(f, 0, SEEK_END) == 0;
  long n = ok ? std::ftell(f) : -1;
  ok = ok && n > 0 && n <= 4 * 1024 * 1024 && std::fseek(f, 0, SEEK_SET) == 0;
  std::vector<uint8_t> raw;
  if (ok) {
    raw.resize(size_t(n));
    ok = std::fread(raw.data(), 1, raw.size(), f) == raw.size() &&
         !std::ferror(f);
  }
  ok = std::fclose(f) == 0 && ok;
  return ok ? load(raw.data(), raw.size(), t, o, present, em, b, e)
            : fail(e, "Scene clips resource read failed");
}
} // namespace encore::upstream
