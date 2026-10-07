#include "encore/audio_data.hpp"
#include "encore/field_npc_world.hpp"
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
struct Reader {
  const uint8_t *p;
  size_t n, at = 128;
  bool ok = true;
  uint32_t integer() {
    if (!ok || at > n || n - at < 4) {
      ok = false;
      return 0;
    }
    auto v = u(p + at);
    at += 4;
    return v;
  }
  float scalar() {
    auto bits = integer();
    float v;
    std::memcpy(&v, &bits, 4);
    if (!std::isfinite(v))
      ok = false;
    return v;
  }
  std::string text() {
    auto len = integer();
    if (!ok || len > 2048 || at > n || len > n - at) {
      ok = false;
      return {};
    }
    std::string s(reinterpret_cast<const char *>(p + at), len);
    at += len;
    size_t i = 0;
    uint32_t cp;
    while (i < s.size())
      if (!utf8_next(s, i, cp) || cp < 32) {
        ok = false;
        break;
      }
    return s;
  }
  std::array<uint8_t, 32> hash() {
    std::array<uint8_t, 32> v{};
    if (!ok || at > n || n - at < 32) {
      ok = false;
      return v;
    }
    std::copy(p + at, p + at + 32, v.begin());
    at += 32;
    if (std::all_of(v.begin(), v.end(), [](auto b) { return !b; }))
      ok = false;
    return v;
  }
  bool boolean() {
    auto v = integer();
    if (v > 1)
      ok = false;
    return v != 0;
  }
};
bool path(std::string_view p) {
  return !p.empty() && p.front() != '/' && p.find("..") == p.npos &&
         p.find(':') == p.npos && p.find('\\') == p.npos;
}
bool same(const FieldIdentity &a, const FieldIdentity &b) {
  return a.scene_id == b.scene_id && a.upstream_commit == b.upstream_commit &&
         a.source_sha256 == b.source_sha256;
}
} // namespace

const FieldNpcWorldBody *FieldNpcWorldData::body(uint32_t id) const {
  for (const auto &v : bodies_)
    if (v.id == id)
      return &v;
  return nullptr;
}
const FieldNpcWorldRay *FieldNpcWorldData::ray(uint32_t id) const {
  for (const auto &v : rays_)
    if (v.id == id)
      return &v;
  return nullptr;
}
const FieldNpcWorldLink *FieldNpcWorldData::npc(uint32_t id) const {
  for (const auto &v : npcs_)
    if (v.id == id)
      return &v;
  return nullptr;
}
bool FieldNpcWorldData::load_file(const char *p, const FieldNodeTreeData &t,
                                  const FieldNpcData &npc,
                                  const FieldGeometryView &g, std::string &e) {
  if (!p) {
    e = "NPC world path absent";
    return false;
  }
  std::ifstream f(p, std::ios::binary);
  if (!f) {
    e = "NPC world file unavailable";
    return false;
  }
  f.seekg(0, std::ios::end);
  auto n = f.tellg();
  if (n < 128 || n > 2 * 1024 * 1024) {
    e = "NPC world file size";
    return false;
  }
  f.seekg(0);
  std::vector<uint8_t> b(static_cast<size_t>(n));
  if (!f.read(reinterpret_cast<char *>(b.data()), n)) {
    e = "NPC world read";
    return false;
  }
  return load(b.data(), b.size(), t, npc, g, e);
}
bool FieldNpcWorldData::load(const uint8_t *p, size_t n,
                             const FieldNodeTreeData &t,
                             const FieldNpcData &npc,
                             const FieldGeometryView &g, std::string &e) {
  auto fail = [&](const char *m) {
    e = m;
    return false;
  };
  if (!p || n < 128 || n > 2 * 1024 * 1024 || std::memcmp(p, "ENCNPCW1", 8) ||
      u(p + 8) != 1 || u(p + 12) != 128 || u(p + 16) != n ||
      u(p + 20) != audio_crc32(p + 128, n - 128) || u(p + 24) != 0x454e006a ||
      u(p + 28) != 1 || u(p + 32) != 1 || !t.valid() || !npc.valid() ||
      !g.valid())
    return fail("NPC world header/CRC/schema/required sources");
  FieldNpcWorldData d;
  d.identity_.scene_id = u(p + 36);
  std::copy(p + 40, p + 60, d.identity_.upstream_commit.begin());
  std::copy(p + 60, p + 92, d.identity_.source_sha256.begin());
  if (!same(d.identity_, t.identity()) || !same(d.identity_, g.identity()) ||
      d.identity_.upstream_commit != npc.source_pin() || u(p + 124) ||
      std::all_of(p + 92, p + 124, [](uint8_t v) { return !v; }))
    return fail("NPC world source identity/IR");
  Reader r{p, n};
  d.zero_cast_ = {r.scalar(), r.scalar()};
  auto nb = r.integer(), nr = r.integer(), nn = r.integer();
  if (nb > 4096 || nr > 4096 || nn != npc.npcs().size() || !nb || !nr ||
      d.zero_cast_.x != 0 || d.zero_cast_.y <= 0)
    return fail("NPC world counts/zero-ray policy");
  std::set<uint32_t> actualBodies, actualRays, bodies, rays;
  for (const auto &v : t.records()) {
    auto c =
        v.native_class.empty() ? t.classes().at(v.class_index) : v.native_class;
    if (c == "KinematicBody2D")
      actualBodies.insert(v.id);
    if (c == "RayCast2D")
      actualRays.insert(v.id);
  }
  for (uint32_t i = 0; i < nb; ++i) {
    FieldNpcWorldBody v;
    v.id = r.integer();
    v.layer = r.integer();
    v.mask = r.integer();
    v.platform_leave = r.integer();
    auto ns = r.integer();
    v.margin = r.scalar();
    v.path = r.text();
    auto q = t.record(v.id);
    if (!r.ok || !q || q->path != v.path || !path(v.path) ||
        !bodies.insert(v.id).second || !actualBodies.count(v.id) ||
        v.margin < 0 || v.margin > 100 || v.platform_leave > 2 || ns != 1)
      return fail("NPC body/native shape closure");
    for (uint32_t j = 0; j < ns; ++j) {
      auto id = r.integer();
      auto disabled = r.boolean();
      auto s = t.record(id);
      if (!s || s->parent != v.id)
        return fail("NPC shape parent");
      bool found = false;
      for (uint32_t k = 0; k < g.owner_count(); ++k) {
        auto o = g.owner(k);
        if (g.node(o.node).stable_id != v.id)
          continue;
        if (o.kind != 2 || o.layer != v.layer || o.mask != v.mask ||
            o.safe_margin != v.margin)
          return fail("NPC geometry masks/margin differ");
        for (uint32_t l = 0; l < o.shape_count; ++l) {
          auto sh = g.shape(o.shape_first + l);
          if (g.node(sh.node).stable_id == id) {
            if (bool(sh.flags & 1) != disabled || sh.flags & 2)
              return fail("NPC shape disabled/oneway differs");
            found = true;
          }
        }
      }
      if (!found)
        return fail("NPC body source geometry absent");
      v.shapes.emplace_back(id, disabled);
    }
    d.bodies_.push_back(std::move(v));
  }
  for (uint32_t i = 0; i < nr; ++i) {
    FieldNpcWorldRay v;
    v.id = r.integer();
    v.parent = r.integer();
    v.mask = r.integer();
    v.enabled = r.boolean();
    v.exclude = r.boolean();
    v.bodies = r.boolean();
    v.areas = r.boolean();
    v.cast = {r.scalar(), r.scalar()};
    v.path = r.text();
    auto q = t.record(v.id);
    if (!r.ok || !q || q->path != v.path || q->parent != v.parent ||
        !q->script.empty() || !actualRays.count(v.id) ||
        !rays.insert(v.id).second || std::abs(v.cast.x) > 1000000 ||
        std::abs(v.cast.y) > 1000000)
      return fail("NPC checked Ray properties/parent");
    d.rays_.push_back(std::move(v));
  }
  std::set<uint32_t> npcs;
  for (uint32_t i = 0; i < nn; ++i) {
    FieldNpcWorldLink v;
    v.id = r.integer();
    v.collider = r.integer();
    v.interact = r.integer();
    v.near = r.integer();
    v.view = r.integer();
    v.wander = r.integer();
    v.ray = r.integer();
    v.timer = r.integer();
    v.shadow = r.integer();
    v.sprite = r.integer();
    auto b = d.body(v.id);
    auto ray = d.ray(v.ray);
    const FieldNpcDescriptor *source = nullptr;
    for (const auto &x : npc.npcs())
      if (x.id == v.id)
        source = &x;
    if (!source || !b || b->shapes.front().first != v.collider || !ray ||
        ray->parent != v.id || b->path != source->node ||
        b->margin != source->safe_margin || source->geometry.size() != 9 ||
        !npcs.insert(v.id).second)
      return fail("NPC world source NPC binding");
    uint32_t ids[] = {v.collider, v.interact, v.near,   v.view,  v.wander,
                      v.ray,      v.timer,    v.shadow, v.sprite};
    for (auto id : ids) {
      auto q = t.record(id);
      if (!q ||
          q->path.compare(0, source->node.size() + 1, source->node + "/") != 0)
        return fail("NPC child source closure");
    }
    if (source->geometry[5].mask != ray->mask ||
        source->geometry[5].value.x != ray->cast.x ||
        source->geometry[5].value.y != ray->cast.y)
      return fail("NPC Ray source tuning differs");
    d.npcs_.push_back(v);
  }
  auto nc = r.integer();
  if (nc != 13)
    return fail("NPC source callback coverage");
  std::set<uint32_t> ops;
  std::set<std::string> names;
  for (uint32_t i = 0; i < nc; ++i) {
    FieldNpcWorldCallback c;
    c.op = r.integer();
    c.arity = r.integer();
    c.method = r.text();
    if (!r.ok || c.op < 1 || c.op > 13 ||
        c.arity != uint32_t(c.op >= 3 && c.op <= 6) || c.method.empty() ||
        !ops.insert(c.op).second || !names.insert(c.method).second)
      return fail("NPC checked callback opcode/method/arity");
    d.callbacks_.push_back(std::move(c));
  }
  d.visibility_signal_ = r.text();
  if (d.visibility_signal_.empty())
    return fail("NPC visibility source signal absent");
  if (!r.ok || r.at != n || bodies != actualBodies || rays != actualRays)
    return fail("NPC native coverage/trailing bytes");
  d.valid_ = true;
  *this = std::move(d);
  return true;
}
} // namespace encore::upstream
