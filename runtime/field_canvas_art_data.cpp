#include "encore/field_canvas_art.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <set>
namespace encore::upstream {
namespace {
uint32_t u32(const uint8_t *p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 |
         uint32_t(p[3]) << 24;
}
uint32_t crc(const uint8_t *p, size_t n) {
  uint32_t c = ~0u;
  for (size_t i = 0; i < n; ++i) {
    c ^= p[i];
    for (unsigned j = 0; j < 8; ++j)
      c = (c >> 1) ^ (0xedb88320u & uint32_t(-int(c & 1)));
  }
  return ~c;
}
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
bool path(const std::string &s) {
  return !s.empty() && s.size() < 2048 && s[0] != '/' &&
         s.find(':') == std::string::npos &&
         s.find('\\') == std::string::npos && s.find("..") == std::string::npos;
}
bool nz(const std::array<uint8_t, 32> &h) {
  return std::any_of(h.begin(), h.end(), [](uint8_t b) { return b != 0; });
}
struct R {
  const uint8_t *p;
  size_t n, at = 128;
  bool ok = true;
  uint32_t u() {
    if (at > n || n - at < 4) {
      ok = false;
      return 0;
    }
    auto v = u32(p + at);
    at += 4;
    return v;
  }
  float f() {
    auto bits = u();
    float v;
    std::memcpy(&v, &bits, 4);
    if (!std::isfinite(v) || std::abs(v) > 1000000)
      ok = false;
    return v;
  }
  bool b() {
    auto v = u();
    if (v > 1)
      ok = false;
    return v == 1;
  }
  std::string t() {
    auto k = u();
    if (k > 8192 || at > n || k > n - at) {
      ok = false;
      return {};
    }
    std::string s(reinterpret_cast<const char *>(p + at), k);
    at += k;
    size_t i = 0;
    uint32_t cp;
    while (i < s.size())
      if (!encore::utf8_next(s, i, cp) || !cp || cp < 32) {
        ok = false;
        break;
      }
    return s;
  }
  void hash(std::array<uint8_t, 32> &h) {
    if (at > n || n - at < 32) {
      ok = false;
      return;
    }
    std::copy_n(p + at, 32, h.begin());
    at += 32;
  }
  FieldCanvasAsset asset() {
    FieldCanvasAsset a;
    a.id = u();
    a.width = u();
    a.height = u();
    a.bytes = u();
    a.crc = u();
    a.source = t();
    a.path = t();
    hash(a.source_sha);
    hash(a.output_sha);
    return a;
  }
};
} // namespace
const FieldCanvasRecord *FieldCanvasArtData::record(uint32_t id) const {
  auto it = record_index_.find(id);
  return it == record_index_.end() ? nullptr : &records_[it->second];
}
const FieldCanvasAsset *FieldCanvasArtData::texture(uint32_t id) const {
  auto it = texture_index_.find(id);
  return it == texture_index_.end() ? nullptr : &textures_[it->second];
}
bool FieldCanvasArtData::source_hash(std::string_view p,
                                     std::array<uint8_t, 32> &h) const {
  auto it = sources_.find(std::string(p));
  if (it == sources_.end())
    return false;
  h = it->second;
  return true;
}
bool FieldCanvasArtData::load_file(const char *p, const FieldIdentity &identity,
                                   std::string &e) {
  if (!p)
    return fail(e, "Canvas pack path missing");
  std::ifstream f(p, std::ios::binary);
  if (!f)
    return fail(e, "Canvas pack unavailable");
  f.seekg(0, std::ios::end);
  auto size = f.tellg();
  if (size < 128 || size > 16 * 1024 * 1024)
    return fail(e, "Canvas pack bounded extent rejected");
  std::vector<uint8_t> b(size_t(size), 0);
  f.seekg(0);
  if (!f.read(reinterpret_cast<char *>(b.data()), size))
    return fail(e, "Canvas pack incomplete");
  return load(b.data(), b.size(), identity, e);
}
bool FieldCanvasArtData::load(const uint8_t *p, size_t n,
                              const FieldIdentity &identity, std::string &e) {
  valid_ = false;
  if (!p || n < 128 || n > 16 * 1024 * 1024 || std::memcmp(p, "ENCFCA01", 8) ||
      u32(p + 8) != 1 || u32(p + 12) != 128 || u32(p + 16) != n ||
      u32(p + 20) != crc(p + 128, n - 128) || u32(p + 24) != 0x454e0040 ||
      u32(p + 28) != 1 || u32(p + 32) != 1 || u32(p + 124))
    return fail(e, "Canvas format/family/capability/rules/CRC rejected");
  FieldCanvasArtData d;
  d.identity_.scene_id = u32(p + 36);
  std::copy_n(p + 40, 20, d.identity_.upstream_commit.begin());
  std::copy_n(p + 60, 32, d.identity_.source_sha256.begin());
  std::copy_n(p + 92, 32, d.ir_.begin());
  if (d.identity_.scene_id != identity.scene_id ||
      d.identity_.upstream_commit != identity.upstream_commit ||
      d.identity_.source_sha256 != identity.source_sha256 ||
      !identity.scene_id || !nz(identity.source_sha256))
    return fail(e, "Canvas checked scene identity mismatch");
  R r{p, n};
  d.scene_ = r.t();
  d.y_epsilon_ = r.f();
  d.alpha_prune_ = r.f();
  d.pixel_snap_ = r.b();
  r.hash(d.tree_ir_);
  if (!path(d.scene_) || !nz(d.tree_ir_) || d.y_epsilon_ <= 0 ||
      d.y_epsilon_ >= .01f || d.alpha_prune_ < 0 || d.alpha_prune_ >= 1)
    return fail(e, "Canvas source engine parameters rejected");
  auto count = r.u();
  if (!count || count > 65536)
    return fail(e, "Canvas texture count rejected");
  std::set<std::string> paths, original;
  for (uint32_t i = 0; i < count; ++i) {
    auto a = r.asset();
    if (!r.ok || !a.id || !a.width || !a.height || a.width > 1024 ||
        a.height > 1024 || !a.bytes || a.bytes > 16 * 1024 * 1024 ||
        !path(a.source) || !path(a.path) || a.path.rfind("graphics/", 0) != 0 ||
        !nz(a.source_sha) || !nz(a.output_sha) ||
        !paths.insert(a.path).second || !original.insert(a.source).second ||
        !d.texture_index_.emplace(a.id, d.textures_.size()).second)
      return fail(e, "Canvas texture identity/path/extent rejected");
    d.textures_.push_back(std::move(a));
  }
  d.program_ = r.asset();
  auto &a = d.program_;
  if (a.id || a.width || a.height || !a.bytes || a.bytes > 1024 * 1024 ||
      a.bytes % 4 || !path(a.source) || !path(a.path) ||
      a.path.rfind("shaders/", 0) != 0 || !nz(a.source_sha) ||
      !nz(a.output_sha))
    return fail(e, "Canvas PICA metadata rejected");
  count = r.u();
  if (!count || count > 65536)
    return fail(e, "Canvas command count rejected");
  std::set<std::string> nodes;
  for (uint32_t i = 0; i < count; ++i) {
    FieldCanvasRecord v;
    v.id = r.u();
    v.kind = r.u();
    v.flags = r.u();
    v.texture = r.u();
    v.hframes = r.u();
    v.vframes = r.u();
    v.frame = r.u();
    v.centered = r.b();
    v.flip_h = r.b();
    v.flip_v = r.b();
    v.stretch = r.u();
    v.owner = FieldCanvasOwner(r.u());
    v.owner_id = r.u();
    v.shader = FieldCanvasShader(r.u());
    v.offset = {r.f(), r.f()};
    v.size = {r.f(), r.f()};
    v.node = r.t();
    v.owner_script = r.t();
    v.shader_source = r.t();
    r.hash(v.owner_sha);
    auto tex = d.texture(v.texture);
    if (!r.ok || !v.id || v.kind > 1 || !(v.flags & 1) || v.flags >= 1024 ||
        !v.hframes || !v.vframes || v.hframes > 1024 || v.vframes > 1024 ||
        v.frame >= v.hframes * v.vframes || v.size.x < 0 || v.size.y < 0 ||
        (v.texture && !tex) || !path(v.node) || !nodes.insert(v.node).second ||
        !d.record_index_.emplace(v.id, d.records_.size()).second ||
        uint32_t(v.owner) > uint32_t(FieldCanvasOwner::Landmark) ||
        uint32_t(v.shader) > uint32_t(FieldCanvasShader::Flash) ||
        (v.kind == 0 && v.stretch) ||
        (v.kind == 1 && (v.hframes != 1 || v.vframes != 1 || v.frame ||
                         v.centered || (v.stretch != 2 && v.stretch != 3))))
      return fail(e, "Canvas command schema/texture/frame/stretch rejected");
    if ((v.owner == FieldCanvasOwner::Native &&
         (v.owner_id || !v.owner_script.empty() || nz(v.owner_sha) ||
          v.shader != FieldCanvasShader::Default)) ||
        (v.owner != FieldCanvasOwner::Native &&
         (!v.owner_id || v.owner_script.empty() || !nz(v.owner_sha))) ||
        (v.shader == FieldCanvasShader::Default && !v.shader_source.empty()) ||
        (v.shader != FieldCanvasShader::Default && v.shader_source.empty()) ||
        (v.shader == FieldCanvasShader::Outline &&
         v.owner != FieldCanvasOwner::Jump) ||
        (v.shader == FieldCanvasShader::Distortion &&
         v.owner != FieldCanvasOwner::Melody) ||
        (v.kind == 1 && v.stretch == 2 && v.owner != FieldCanvasOwner::Melody))
      return fail(e, "Canvas shader/typed owner rejected");
    d.records_.push_back(std::move(v));
  }
  count = r.u();
  if (!count || count > 100000)
    return fail(e, "Canvas source inventory count rejected");
  for (uint32_t i = 0; i < count; ++i) {
    auto s = r.t();
    std::array<uint8_t, 32> h{};
    r.hash(h);
    if (!path(s) || !nz(h) || !d.sources_.emplace(s, h).second)
      return fail(e, "Canvas duplicate/source inventory rejected");
  }
  if (!r.ok || r.at != n)
    return fail(e, "Canvas truncated/trailing binary rejected");
  std::array<uint8_t, 32> hash{};
  if (!d.source_hash(d.scene_, hash) || hash != identity.source_sha256)
    return fail(e, "Canvas scene inventory source rejected");
  for (const auto &t : d.textures_)
    if (!d.source_hash(t.source, hash) || hash != t.source_sha ||
        !d.source_hash(t.source + ".import", hash))
      return fail(e, "Canvas texture source/import missing");
  for (const auto &v : d.records_) {
    if (v.owner != FieldCanvasOwner::Native) {
      auto p = v.owner_script.substr(0, v.owner_script.find("::"));
      if (!d.source_hash(p, hash))
        return fail(e, "Canvas owner source missing");
    }
    if (v.shader != FieldCanvasShader::Default) {
      auto p = v.shader_source.substr(0, v.shader_source.find("::"));
      if (!d.source_hash(p, hash))
        return fail(e, "Canvas shader source missing");
    }
  }
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
} // namespace encore::upstream
