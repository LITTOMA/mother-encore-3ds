#include "encore/field_scene_signal_callbacks.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <set>
namespace encore::upstream {
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
uint32_t crc(const uint8_t *p, size_t n) {
  uint32_t v = ~0u;
  while (n--) {
    v ^= *p++;
    for (unsigned i = 0; i < 8; ++i)
      v = (v >> 1) ^ (0xedb88320u & uint32_t(-int32_t(v & 1)));
  }
  return ~v;
}
struct Reader {
  const uint8_t *p;
  size_t n;
  bool u(uint32_t &v) {
    if (n < 4)
      return false;
    v = uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) |
        (uint32_t(p[3]) << 24);
    p += 4;
    n -= 4;
    return true;
  }
  bool raw(void *out, size_t z) {
    if (n < z)
      return false;
    std::memcpy(out, p, z);
    p += z;
    n -= z;
    return true;
  }
  bool text(std::string &s) {
    uint32_t z = 0;
    if (!u(z) || !z || z > 4096 || z > n)
      return false;
    s.assign(reinterpret_cast<const char *>(p), z);
    p += z;
    n -= z;
    size_t count = 0;
    return s.find('\0') == s.npos && encore::utf8_count(s, count);
  }
};
bool nonzero(const std::array<uint8_t, 32> &v) {
  return std::any_of(v.begin(), v.end(), [](uint8_t x) { return x != 0; });
}
} // namespace
const SceneSignalBinding *
FieldSceneSignalCallbacksData::symbol(SceneSignalSymbol role) const {
  for (const auto &v : symbols_)
    if (v.role == role)
      return &v;
  return nullptr;
}
const SceneCallbackBinding *
FieldSceneSignalCallbacksData::callback(uint32_t node,
                                        SceneCallbackRole role) const {
  for (const auto &v : callbacks_)
    if (v.node == node && v.role == role)
      return &v;
  return nullptr;
}
bool FieldSceneSignalCallbacksData::load(const uint8_t *p, size_t size,
                                         const FieldIdentity &expected,
                                         std::string &e) {
  if (valid_ || !p || size < 88 || size > 8 * 1024 * 1024 ||
      std::memcmp(p, "ENCSIG01", 8))
    return fail(e, "Scene signal format/size rejected");
  Reader r{p + 8, size - 8};
  uint32_t format = 0, cap = 0, rules = 0, family = 0, scene = 0, bytes = 0,
           checksum = 0;
  FieldSceneSignalCallbacksData candidate;
  if (!r.u(format) || !r.u(cap) || !r.u(rules) || !r.u(family) || !r.u(scene) ||
      !r.u(bytes) || !r.u(checksum) || format != 2 || cap != 2 || rules != 1 ||
      family != 0x454e0068 || !scene || bytes != size - 88 ||
      !r.raw(candidate.identity_.upstream_commit.data(), 20) ||
      !r.raw(candidate.identity_.source_sha256.data(), 32) ||
      crc(r.p, r.n) != checksum)
    return fail(e, "Scene signal version/capability/rules/CRC rejected");
  candidate.identity_.scene_id = scene;
  if (candidate.identity_.scene_id != expected.scene_id ||
      candidate.identity_.source_sha256 != expected.source_sha256 ||
      candidate.identity_.upstream_commit != expected.upstream_commit ||
      !nonzero(candidate.identity_.source_sha256) ||
      !r.text(candidate.scene_) || !r.text(candidate.wait_source_) ||
      !r.raw(candidate.wait_sha_.data(), 32) || !r.u(candidate.wait_id_) ||
      !candidate.wait_id_ || !r.u(candidate.wait_flags_) ||
      candidate.wait_flags_ != 4 || !r.text(candidate.party_member_name_) ||
      !r.u(candidate.party_member_) || !candidate.party_member_ ||
      !r.raw(candidate.constructor_sha_.data(), 32) ||
      !nonzero(candidate.constructor_sha_))
    return fail(e, "Scene signal source identity/wait policy rejected");
  uint32_t count = 0;
  if (!r.u(count) || !count || count > 4096)
    return fail(e, "Scene signal source count rejected");
  for (uint32_t i = 0; i < count; ++i) {
    std::string name;
    std::array<uint8_t, 32> hash{};
    if (!r.text(name) || !r.raw(hash.data(), 32) || !nonzero(hash) ||
        !candidate.sources_.emplace(name, hash).second)
      return fail(e, "Scene signal duplicate/source proof rejected");
  }
  auto wait = candidate.sources_.find(candidate.wait_source_);
  if (wait == candidate.sources_.end() || wait->second != candidate.wait_sha_)
    return fail(e, "Scene signal FunctionState source proof rejected");
  if (!r.u(count) || count != 15)
    return fail(e, "Scene signal schema symbols rejected");
  std::set<uint32_t> roles;
  for (uint32_t i = 0; i < count; ++i) {
    SceneSignalBinding v;
    uint32_t role = 0;
    if (!r.u(role) || role < 1 || role > 15 || !roles.insert(role).second ||
        !r.text(v.name) || !r.u(v.declaration_arguments) ||
        !r.u(v.emission_arguments) || v.declaration_arguments > 3 ||
        v.emission_arguments > 3 ||
        (role != 2 && v.declaration_arguments != v.emission_arguments) ||
        (role == 2 &&
         (v.declaration_arguments != 0 || v.emission_arguments != 1)))
      return fail(e, "Scene signal unknown symbol/signature policy rejected");
    v.role = static_cast<SceneSignalSymbol>(role);
    candidate.symbols_.push_back(std::move(v));
  }
  if (!r.u(count) || !count || count > 8192)
    return fail(e, "Scene callback count rejected");
  std::set<std::pair<uint32_t, uint32_t>> seen;
  for (uint32_t i = 0; i < count; ++i) {
    SceneCallbackBinding v;
    uint32_t role = 0;
    if (!r.u(v.node) || !v.node || !r.u(role) || role < 1 || role > 10 ||
        !seen.emplace(v.node, role).second || !r.text(v.script) ||
        !r.text(v.method_source) || !r.text(v.method) ||
        !r.raw(v.leaf_sha.data(), 32) || !r.raw(v.method_sha.data(), 32) ||
        !r.u(v.arguments))
      return fail(e, "Scene callback unknown/duplicate binding rejected");
    auto leaf = candidate.sources_.find(v.script),
         source = candidate.sources_.find(v.method_source);
    uint32_t arity = (role == 2                ? 1
                      : role == 3              ? 3
                      : role == 5 || role == 8 ? 2
                                               : 0);
    if (leaf == candidate.sources_.end() ||
        source == candidate.sources_.end() || leaf->second != v.leaf_sha ||
        source->second != v.method_sha || v.arguments != arity)
      return fail(e, "Scene callback source/signature rejected");
    v.role = static_cast<SceneCallbackRole>(role);
    candidate.callbacks_.push_back(std::move(v));
  }
  if (r.n)
    return fail(e, "Scene signal trailing/unknown metadata rejected");
  candidate.valid_ = true;
  *this = std::move(candidate);
  e.clear();
  return true;
}
bool FieldSceneSignalCallbacksData::load_file(const char *path,
                                              const FieldIdentity &id,
                                              std::string &e) {
  if (!path)
    return fail(e, "Scene signal path missing");
  FILE *f = std::fopen(path, "rb");
  if (!f)
    return fail(e, "Cannot open scene signal resource");
  if (std::fseek(f, 0, SEEK_END)) {
    std::fclose(f);
    return fail(e, "Scene signal seek failed");
  }
  long z = std::ftell(f);
  if (z < 0 || z > 8 * 1024 * 1024 || std::fseek(f, 0, SEEK_SET)) {
    std::fclose(f);
    return fail(e, "Scene signal size/seek failed");
  }
  std::vector<uint8_t> b(static_cast<size_t>(z));
  bool ok = std::fread(b.data(), 1, b.size(), f) == b.size();
  std::fclose(f);
  return ok ? load(b.data(), b.size(), id, e)
            : fail(e, "Scene signal read failed");
}
} // namespace encore::upstream
