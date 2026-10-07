#include "encore/crc32.hpp"
#include "encore/prompt_native.hpp"
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
  bool raw(void *out, size_t z) {
    if (z > n)
      return false;
    std::memcpy(out, p, z);
    p += z;
    n -= z;
    return true;
  }
  bool u(uint32_t &v) {
    if (n < 4)
      return false;
    v = uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) |
        (uint32_t(p[3]) << 24);
    p += 4;
    n -= 4;
    return true;
  }
  bool f(float &v) {
    uint32_t b;
    if (!u(b))
      return false;
    std::memcpy(&v, &b, 4);
    return std::isfinite(v);
  }
  bool s(std::string &v, bool empty = false) {
    uint32_t z;
    if (!u(z) || (!z && !empty) || z > 4096 || z > n)
      return false;
    v.assign(reinterpret_cast<const char *>(p), z);
    p += z;
    n -= z;
    size_t c = 0;
    return v.find('\0') == v.npos && encore::utf8_count(v, c);
  }
};
} // namespace
const PromptNativeRecord *PromptNativeData::record(uint32_t id) const {
  for (const auto &r : records_)
    if (r.id == id)
      return &r;
  return nullptr;
}
const PromptNativeRecord *PromptNativeData::leaf(uint32_t id,
                                                 PromptNativeRole role) const {
  for (const auto &r : records_)
    if (r.prompt == id && r.role == role)
      return &r;
  return nullptr;
}
bool PromptNativeData::load(const uint8_t *p, size_t n,
                            const FieldNodeTreeData &t,
                            const FieldPromptData &d, std::string &e) {
  if (valid_ || !p || n < 88 || n > 2 * 1024 * 1024 ||
      std::memcmp(p, "ENCPRN01", 8) || !t.valid() || !d.valid())
    return fail(e, "Prompt native format/source rejected");
  Reader r{p + 8, n - 8};
  uint32_t format = 0, cap = 0, rules = 0, family = 0, scene = 0, bytes = 0,
           crc = 0;
  PromptNativeData c;
  if (!r.u(format) || !r.u(cap) || !r.u(rules) || !r.u(family) || !r.u(scene) ||
      !r.u(bytes) || !r.u(crc) || format != 1 || cap != 1 || rules != 1 ||
      family != 0x454e006b || bytes != n - 88 ||
      !r.raw(c.identity_.upstream_commit.data(), 20) ||
      !r.raw(c.identity_.source_sha256.data(), 32) ||
      encore::crc32(r.p, r.n) != crc)
    return fail(e, "Prompt native version/capability/rules/CRC rejected");
  c.identity_.scene_id = scene;
  if (c.identity_.upstream_commit != t.identity().upstream_commit ||
      c.identity_.source_sha256 != t.identity().source_sha256 ||
      scene != t.identity().scene_id ||
      d.source_pin() != c.identity_.upstream_commit ||
      d.scene_hash() != c.identity_.source_sha256 || d.scene_id() != scene)
    return fail(e, "Prompt native cross-source identity rejected");
  for (auto &s : c.strings_)
    if (!r.s(s))
      return fail(e, "Prompt native source symbol rejected");
  uint32_t align = 0, label = 0, rotation = 0, stretch = 0;
  std::array<uint8_t, 32> proof{};
  if (!r.f(c.box_.x) || !r.f(c.box_.y) || c.box_.x <= 0 || c.box_.y <= 0 ||
      c.box_.x > 4096 || c.box_.y > 4096 || !r.u(align) || !r.u(label) ||
      !r.u(rotation) || !r.u(stretch) || align != 1 || label != 1 ||
      rotation != 90 || stretch != 3 || !r.raw(proof.data(), 32) ||
      proof == std::array<uint8_t, 32>{} || !r.raw(proof.data(), 32) ||
      proof == std::array<uint8_t, 32>{})
    return fail(e, "Prompt native Control layout/proof rejected");
  uint32_t count = 0;
  std::set<std::string> sources;
  for (uint32_t i = 0; i < 3; ++i) {
    uint32_t role = 0;
    auto &v = c.controls_[i];
    if (!r.u(role) || role != i + 1 || !r.f(v.position.x) ||
        !r.f(v.position.y) || !r.f(v.size.x) || !r.f(v.size.y) ||
        v.size.x <= 0 || v.size.y <= 0 || !r.s(v.text, true) ||
        (i != 1 && !v.text.empty()))
      return fail(e, "Prompt native constructor Control fields rejected");
  }
  if (!r.u(count) || !count || count > 32)
    return fail(e, "Prompt native input bindings missing");
  std::set<std::string> actions;
  for (uint32_t i = 0; i < count; ++i) {
    PromptNativeInput v;
    if (!r.s(v.action) || !r.s(v.label) || !r.u(v.mask) || !v.mask ||
        !r.u(v.parameter) || v.parameter < 1 || v.parameter > 32 ||
        !actions.insert(v.action).second)
      return fail(e, "Prompt native input mapping rejected");
    c.inputs_.push_back(std::move(v));
  }
  if (!r.raw(proof.data(), 32) || proof == std::array<uint8_t, 32>{} ||
      !r.raw(proof.data(), 32) || proof == std::array<uint8_t, 32>{})
    return fail(e, "Prompt native adaptation source proofs missing");
  if (!r.u(count) || !count || count > 256)
    return fail(e, "Prompt native source count rejected");
  for (uint32_t i = 0; i < count; ++i) {
    std::string s;
    std::array<uint8_t, 32> h{};
    if (!r.s(s) || !sources.insert(s).second || !r.raw(h.data(), 32) ||
        h == std::array<uint8_t, 32>{})
      return fail(e, "Prompt native source proof rejected");
    std::array<uint8_t, 32> actual{};
    if (t.source_hash(s, actual) && h != actual)
      return fail(e, "Prompt native source SHA differs from actual Tree");
  }
  if (!sources.count(c.font()))
    return fail(e, "Prompt native font source missing");
  if (!r.u(count) || count != d.records().size() * 4 || count > 32768)
    return fail(e, "Prompt native leaf closure rejected");
  std::set<uint32_t> ids;
  std::set<std::pair<uint32_t, uint32_t>> roles;
  for (uint32_t i = 0; i < count; ++i) {
    PromptNativeRecord v;
    uint32_t role = 0;
    if (!r.u(v.id) || !r.u(v.prompt) || !r.u(v.parent) || !r.u(role) ||
        role < 1 || role > 4 || !ids.insert(v.id).second ||
        !roles.emplace(v.prompt, role).second || !r.s(v.path) ||
        !r.s(v.native_class))
      return fail(e, "Prompt native unknown/duplicate leaf rejected");
    v.role = PromptNativeRole(role);
    const std::array<const char *, 4> classes = {
        "HBoxContainer", "Label", "TextureRect", "AnimationPlayer"};
    if (v.native_class != classes[role - 1])
      return fail(e, "Prompt native role/class schema rejected");
    const auto *a = t.record(v.id);
    if (!a || !d.record(v.prompt) || a->parent != v.parent ||
        a->path != v.path || a->class_index >= t.classes().size() ||
        t.classes()[a->class_index] != v.native_class || !a->script.empty() ||
        a->script_sha != std::array<uint8_t, 32>{})
      return fail(e,
                  "Prompt native actual leaf identity/parent/script differs");
    c.records_.push_back(std::move(v));
  }
  for (const auto &a : d.records()) {
    const auto *box = c.leaf(a.id, PromptNativeRole::Box);
    if (!box)
      return fail(e, "Prompt native actual Box missing");
    for (uint32_t role = 1; role <= 4; ++role) {
      const auto *v = c.leaf(a.id, PromptNativeRole(role));
      if (!v || v->parent != (role == 2 ? box->id : a.id))
        return fail(e, "Prompt native parent topology rejected");
    }
  }
  if (r.n)
    return fail(e, "Prompt native trailing metadata rejected");
  c.valid_ = true;
  *this = std::move(c);
  e.clear();
  return true;
}
bool PromptNativeData::load_file(const char *p, const FieldNodeTreeData &t,
                                 const FieldPromptData &d, std::string &e) {
  FILE *f = p ? std::fopen(p, "rb") : nullptr;
  if (!f)
    return fail(e, "Prompt native resource unavailable");
  bool ok = std::fseek(f, 0, SEEK_END) == 0;
  long n = ok ? std::ftell(f) : -1;
  ok = ok && n > 0 && n <= 2 * 1024 * 1024 && std::fseek(f, 0, SEEK_SET) == 0;
  std::vector<uint8_t> b;
  if (ok) {
    b.resize(size_t(n));
    ok = std::fread(b.data(), 1, b.size(), f) == b.size() && !std::ferror(f);
  }
  ok = std::fclose(f) == 0 && ok;
  return ok ? load(b.data(), b.size(), t, d, e)
            : fail(e, "Prompt native resource read failed");
}
} // namespace encore::upstream
