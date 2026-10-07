#include "encore/field_scene_materials.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
namespace encore::upstream {
namespace {
uint32_t word(const uint8_t *p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 |
         uint32_t(p[3]) << 24;
}
uint32_t crc(const uint8_t *p, size_t n) {
  uint32_t c = ~0u;
  while (n--) {
    c ^= *p++;
    for (unsigned i = 0; i < 8; ++i)
      c = (c >> 1) ^ ((c & 1) ? 0xedb88320u : 0u);
  }
  return ~c;
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
    auto v = word(p + at);
    at += 4;
    return v;
  }
  int32_t signed_integer() {
    auto v = integer();
    int32_t out;
    std::memcpy(&out, &v, 4);
    return out;
  }
  float scalar() {
    auto v = integer();
    float f;
    std::memcpy(&f, &v, 4);
    if (!std::isfinite(f) || std::abs(f) >= 1000000)
      ok = false;
    return f;
  }
  Vec2 vector() {
    float x = scalar(), y = scalar();
    return {x, y};
  }
  bool boolean() {
    auto v = integer();
    if (v > 1)
      ok = false;
    return v != 0;
  }
  std::string text() {
    auto length = integer();
    if (!ok || length > 65536 || at > n || length > n - at) {
      ok = false;
      return {};
    }
    std::string s(reinterpret_cast<const char *>(p + at), length);
    at += length;
    size_t count = 0;
    if (s.find('\0') != s.npos || !utf8_count(s, count))
      ok = false;
    return s;
  }
  std::array<uint8_t, 32> hash() {
    std::array<uint8_t, 32> h{};
    if (!ok || at > n || n - at < 32) {
      ok = false;
      return h;
    }
    std::copy_n(p + at, 32, h.begin());
    at += 32;
    if (std::all_of(h.begin(), h.end(), [](uint8_t v) { return !v; }))
      ok = false;
    return h;
  }
};
bool path(std::string_view s) {
  if (s.empty() || s.front() == '/' || s.back() == '/' ||
      s.find('\\') != s.npos || s.find(':') != s.npos)
    return false;
  size_t b = 0;
  while (b < s.size()) {
    auto e = s.find('/', b);
    if (e == s.npos)
      e = s.size();
    auto v = s.substr(b, e - b);
    if (v.empty() || v == "." || v == "..")
      return false;
    b = e + 1;
  }
  return true;
}
} // namespace
const FieldMaterialUniform *FieldMaterialRecord::uniform(uint32_t role) const {
  for (const auto &u : uniforms)
    if (u.role == role)
      return &u;
  return nullptr;
}
const FieldMaterialRecord *FieldSceneMaterialsData::record(uint32_t id) const {
  for (const auto &r : records_)
    if (r.id == id)
      return &r;
  return nullptr;
}
const FieldMaterialBinding *
FieldSceneMaterialsData::binding(uint32_t id) const {
  for (const auto &r : bindings_)
    if (r.id == id)
      return &r;
  return nullptr;
}
const FieldMaterialAsset *FieldSceneMaterialsData::asset(uint32_t id) const {
  for (const auto &r : assets_)
    if (r.id == id)
      return &r;
  return nullptr;
}
bool FieldSceneMaterialsData::source_hash(std::string_view p,
                                          std::array<uint8_t, 32> &h) const {
  auto i = sources_.find(std::string(p));
  if (i == sources_.end())
    return false;
  h = i->second;
  return true;
}
bool FieldSceneMaterialsData::load_file(const char *p,
                                        const FieldCanvasArtData &a,
                                        std::string &e) {
  if (!p || !*p) {
    e = "Material path rejected";
    return false;
  }
  FILE *f = std::fopen(p, "rb");
  if (!f) {
    e = "Material file unavailable";
    return false;
  }
  if (std::fseek(f, 0, SEEK_END)) {
    std::fclose(f);
    e = "Material seek rejected";
    return false;
  }
  long n = std::ftell(f);
  if (n < 128 || n > 4 * 1024 * 1024 || std::fseek(f, 0, SEEK_SET)) {
    std::fclose(f);
    e = "Material extent rejected";
    return false;
  }
  std::vector<uint8_t> b(static_cast<size_t>(n));
  bool ok = std::fread(b.data(), 1, b.size(), f) == b.size();
  bool close = std::fclose(f) == 0;
  if (!ok || !close) {
    e = "Material read rejected";
    return false;
  }
  return load(b.data(), b.size(), a, e);
}
bool FieldSceneMaterialsData::load(const uint8_t *p, size_t n,
                                   const FieldCanvasArtData &a,
                                   std::string &e) {
  auto fail = [&](const char *s) {
    e = s;
    return false;
  };
  auto id = a.identity();
  if (!a.valid() || !p || n < 128 || n > 4 * 1024 * 1024 ||
      std::memcmp(p, "ENCMAT01", 8) || word(p + 8) != 1 ||
      word(p + 12) != 128 || word(p + 16) != n || word(p + 24) != 0x454e006f ||
      word(p + 28) != 1 || word(p + 32) != 1 || word(p + 36) != id.scene_id ||
      std::memcmp(p + 40, id.upstream_commit.data(), 20) ||
      std::memcmp(p + 60, id.source_sha256.data(), 32) || word(p + 124) ||
      crc(p + 128, n - 128) != word(p + 20))
    return fail("Material version/capability/identity/CRC rejected");
  FieldSceneMaterialsData d;
  d.identity_ = id;
  std::copy_n(p + 92, 32, d.ir_.begin());
  if (std::all_of(d.ir_.begin(), d.ir_.end(), [](uint8_t v) { return !v; }))
    return fail("Material IR proof rejected");
  Reader r{p, n};
  d.scene_ = r.text();
  auto canvas_ir = r.hash();
  if (canvas_ir != a.ir_sha256())
    return fail("Material Canvas IR differs");
  auto tree_ir = r.hash();
  d.prompt_ir_ = r.hash();
  d.melody_ir_ = r.hash();
  if (d.scene_ != a.source_scene() || tree_ir != a.tree_ir_sha())
    return fail("Material source scene/tree differs");
  auto count = r.integer();
  if (!r.ok || !count || count > 4096)
    return fail("Material roster rejected");
  std::set<uint32_t> ids;
  for (uint32_t i = 0; i < count; ++i) {
    FieldMaterialRecord m;
    m.id = r.integer();
    auto kind = r.integer();
    m.kind = static_cast<FieldCanvasShader>(kind);
    m.local = r.boolean();
    m.source = r.text();
    m.shader = r.text();
    m.name = r.text();
    auto uniforms = r.integer();
    if (!r.ok || !m.id || !ids.insert(m.id).second || kind < 1 || kind > 3 ||
        m.source.empty() || m.shader.empty() || !uniforms || uniforms > 128)
      return fail("Material Resource identity/kind rejected");
    std::set<std::string> names;
    std::set<uint32_t> roles;
    for (uint32_t j = 0; j < uniforms; ++j) {
      FieldMaterialUniform u;
      u.name = r.text();
      u.type = r.integer();
      u.role = r.integer();
      auto values = r.integer();
      auto expected = u.type == 4 ? 2u : u.type == 5 ? 4u : 1u;
      if (!r.ok || u.name.empty() || !names.insert(u.name).second ||
          u.type < 1 || u.type > 6 || values != expected || u.role > 4 ||
          (u.role && !roles.insert(u.role).second))
        return fail("Material uniform schema rejected");
      for (uint32_t k = 0; k < values; ++k) {
        auto v = r.scalar();
        if ((u.type == 2 && std::floor(v) != v) ||
            (u.type == 3 && v != 0 && v != 1) || (u.type == 6 && v != 0))
          return fail("Material typed uniform rejected");
        u.values.push_back(v);
      }
      m.uniforms.push_back(std::move(u));
    }
    if (kind != 2 && m.uniforms.size() != 4)
      return fail("Material bounded shader uniform count rejected");
    if (kind != 2 &&
        (roles != std::set<uint32_t>{1, 2, 3, 4} || m.uniform(1)->type != 5))
      return fail("Material typed shader role closure rejected");
    if (kind == 1 && (m.uniform(2)->type != 1 || m.uniform(3)->type != 2 ||
                      m.uniform(4)->type != 3))
      return fail("Material outline parameters rejected");
    if (kind == 3 && (m.uniform(2)->type != 5 || m.uniform(3)->type != 1 ||
                      m.uniform(4)->type != 1))
      return fail("Material flash parameters rejected");
    d.records_.push_back(std::move(m));
  }
  count = r.integer();
  if (!r.ok || !count || count > a.records().size())
    return fail("Material binding extent rejected");
  ids.clear();
  std::set<uint32_t> used, used_assets;
  for (uint32_t i = 0; i < count; ++i) {
    FieldMaterialBinding b;
    b.id = r.integer();
    b.material = r.integer();
    b.asset = r.integer();
    b.owner = static_cast<FieldCanvasOwner>(r.integer());
    b.owner_id = r.integer();
    b.texture = r.integer();
    b.node = r.text();
    auto *c = a.record(b.id);
    auto *m = d.record(b.material);
    if (!r.ok || !c || !m || !ids.insert(b.id).second || c->shader != m->kind ||
        c->shader_source != m->shader || c->owner != b.owner ||
        c->owner_id != b.owner_id || c->texture != b.texture ||
        c->node != b.node ||
        ((m->kind == FieldCanvasShader::Outline) != (b.asset != 0)))
      return fail("Material actual Canvas binding differs");
    used.insert(b.material);
    if (b.asset)
      used_assets.insert(b.asset);
    d.bindings_.push_back(std::move(b));
  }
  for (const auto &c : a.records())
    if ((c.shader != FieldCanvasShader::Default) !=
        (d.binding(c.id) != nullptr))
      return fail("Material whole Canvas closure differs");
  if (used.size() != d.records_.size())
    return fail("Material unbound resource rejected");
  count = r.integer();
  if (!r.ok || count != used_assets.size())
    return fail("Material outline asset closure rejected");
  ids.clear();
  for (uint32_t i = 0; i < count; ++i) {
    FieldMaterialAsset x;
    x.id = r.integer();
    x.texture = r.integer();
    x.width = r.integer();
    x.height = r.integer();
    x.bytes = r.integer();
    x.crc = r.integer();
    x.source = r.text();
    x.path = r.text();
    x.output_sha = r.hash();
    auto *t = a.texture(x.texture);
    if (!r.ok || !ids.insert(x.id).second || !used_assets.count(x.id) || !t ||
        x.width != t->width || x.height != t->height || x.source != t->source ||
        !x.bytes || x.bytes > 16 * 1024 * 1024 || !path(x.path) ||
        x.path.substr(0, 9) != "graphics/" || x.path.size() < 5 ||
        x.path.substr(x.path.size() - 4) != ".t3x")
      return fail("Material outline output binding rejected");
    d.assets_.push_back(std::move(x));
  }
  count = r.integer();
  if (!r.ok || !count || count > 10000)
    return fail("Material source proof count rejected");
  for (uint32_t i = 0; i < count; ++i) {
    auto pth = r.text();
    auto h = r.hash();
    if (!r.ok || !path(pth) || !d.sources_.emplace(pth, h).second)
      return fail("Material source proof rejected");
    std::array<uint8_t, 32> old{};
    if (a.source_hash(pth, old) && old != h)
      return fail("Material Canvas source SHA differs");
  }
  std::array<uint8_t, 32> h{};
  if (!r.ok || r.at != n || !d.source_hash(d.scene_, h) ||
      h != id.source_sha256)
    return fail("Material source/trailing closure rejected");
  for (const auto &m : d.records_)
    for (const auto *pstr : {&m.source, &m.shader}) {
      auto file = pstr->substr(0, pstr->find("::"));
      if (!path(file) || !d.source_hash(file, h) || !a.source_hash(file, h))
        return fail("Material Resource/Shader original source missing");
    }
  for (const auto &b : d.bindings_)
    if (b.asset && d.asset(b.asset)->texture != b.texture)
      return fail("Material outline texture alias rejected");
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
} // namespace encore::upstream
