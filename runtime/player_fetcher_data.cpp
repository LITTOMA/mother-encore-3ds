#include "encore/player_fetcher.hpp"
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
using V = std::shared_ptr<const GlobalYamlValue>;
V get(V v, std::string_view key) {
  return v && v->kind == 6 ? v->get(key) : nullptr;
}
std::string string(V v) {
  return v && v->kind == 4 ? v->string : std::string{};
}
bool flag(V v, bool fallback, bool &out) {
  if (!v) {
    out = fallback;
    return true;
  }
  if (v->kind != 1)
    return false;
  out = v->boolean;
  return true;
}
bool real(V v, double fallback, double &out) {
  if (!v) {
    out = fallback;
    return true;
  }
  if (v->kind == 2) {
    out = double(v->integer);
    return true;
  }
  if (v->kind == 3) {
    out = v->real;
    return std::isfinite(out);
  }
  if (string(get(v, "type")) != "real")
    return false;
  auto s = string(get(v, "value"));
  char *end = nullptr;
  out = std::strtod(s.c_str(), &end);
  return !s.empty() && end == s.c_str() + s.size() && std::isfinite(out);
}
bool overrides(const PlayerInitializationData &p, const PlayerFetcherRecord &r,
               const PlayerFetcherExports &exports) {
  auto all = get(p.native_source(), "scene_states");
  if (!all || all->kind != 5)
    return false;
  V props;
  for (const auto &state : all->array) {
    if (string(get(state, "source")) != "res://" + p.recipe().source_scene())
      continue;
    auto nodes = get(state, "nodes");
    if (!nodes || nodes->kind != 5)
      return false;
    for (const auto &node : nodes->array)
      if (string(get(node, "path")) == "./" + r.path) {
        if (props)
          return false;
        props = get(node, "properties");
      }
  }
  if (!props || props->kind != 6)
    return false;
  for (const auto &x : props->dictionary)
    if (x.first != exports.path && x.first != exports.ignore &&
        x.first != exports.flip && x.first != exports.offset)
      return false;
  auto path = get(props, exports.path);
  bool ignore = false, flip = false;
  double offset = 0;
  return string(get(path, "type")) == "NodePath" &&
         string(get(path, "value")) == r.sprite_path &&
         flag(get(props, exports.ignore), exports.default_ignore, ignore) &&
         flag(get(props, exports.flip), exports.default_flip, flip) &&
         real(get(props, exports.offset), exports.default_offset, offset) &&
         ignore == r.ignore && flip == r.toggle_flip &&
         offset == r.reflect_offset;
}
} // namespace
bool PlayerFetcherData::load(const uint8_t *p, size_t n,
                             const PlayerInitializationData &player,
                             const FieldNodeTreeData &tree,
                             const FieldGlobalConstructorData &global,
                             std::string &e) {
  if (!player.valid() || !tree.valid() || !global.valid() || !p || n < 128 ||
      n > 4 * 1024 * 1024 || std::memcmp(p, "ENCPFET1", 8) || u32(p + 8) != 1 ||
      u32(p + 12) != 128 || u32(p + 16) != n ||
      u32(p + 20) != crc(p + 128, n - 128) || u32(p + 24) != 0x454e005e ||
      u32(p + 28) != 1 || u32(p + 32) != 1 || u32(p + 124))
    return fail(e, "Player fetcher header/version/capability rejected");
  const auto &expected = player.identity();
  if (u32(p + 36) != expected.scene_id ||
      std::memcmp(p + 40, expected.upstream_commit.data(), 20) ||
      std::memcmp(p + 60, expected.source_sha256.data(), 32))
    return fail(e, "Player fetcher source identity rejected");
  PlayerFetcherData d;
  d.identity_ = expected;
  d.scene_identity_.upstream_commit = expected.upstream_commit;
  std::copy(p + 92, p + 124, d.ir_.begin());
  R r{p + 128, n - 128};
  d.scene_ = r.s();
  d.player_ir_ = r.h();
  d.global_ir_ = r.h();
  d.tree_ir_ = r.h();
  d.script_ = r.s();
  d.script_sha_ = r.h();
  d.global_script_ = r.s();
  d.global_sha_ = r.h();
  d.current_scene_ = r.s();
  d.reflector_ = r.s();
  d.exports_.path = r.s();
  d.exports_.ignore = r.s();
  d.exports_.flip = r.s();
  d.exports_.offset = r.s();
  d.exports_.default_ignore = r.boolean();
  d.exports_.default_flip = r.boolean();
  d.exports_.initial_has = r.boolean();
  d.exports_.default_offset = r.number();
  auto k = r.count(4096);
  std::set<uint32_t> ids;
  while (k-- && r.ok) {
    PlayerFetcherRecord x;
    x.id = r.u();
    x.target = r.u();
    x.ready = r.u();
    x.ignore = r.boolean();
    x.toggle_flip = r.boolean();
    x.reflect_offset = r.number();
    x.path = r.s();
    x.target_path = r.s();
    x.sprite_path = r.s();
    x.script_sha = r.h();
    if (!x.id || !x.target || !ids.insert(x.id).second || x.path.empty() ||
        x.target_path.empty() || x.sprite_path.empty() ||
        x.script_sha != d.script_sha_)
      r.ok = false;
    d.records_.push_back(std::move(x));
  }
  k = r.count(32);
  std::set<std::string> methods, members;
  while (k-- && r.ok) {
    PlayerFetcherGetter x;
    x.method = r.s();
    x.type = r.s();
    x.member = r.s();
    if (x.method.empty() || !methods.insert(x.method).second ||
        !members.insert(x.member).second ||
        (x.type != "Texture" && x.type != "int" && x.type != "bool"))
      r.ok = false;
    d.getters_.push_back(std::move(x));
  }
  d.sprite_getter_ = r.s();
  d.scene_identity_.scene_id = r.u();
  d.scene_identity_.source_sha256 = r.h();
  d.scene_source_ = r.s();
  k = r.count(100000);
  if (k != tree.records().size() || d.scene_source_ != tree.source_scene() ||
      d.scene_identity_.scene_id != tree.identity().scene_id ||
      d.scene_identity_.source_sha256 != tree.identity().source_sha256 ||
      expected.upstream_commit != tree.identity().upstream_commit)
    r.ok = false;
  size_t at = 0;
  while (k-- && r.ok) {
    auto id = r.u(), parent = r.u();
    auto path = r.s(), name = r.s();
    const auto &actual = tree.records()[at++];
    if (id != actual.id || parent != actual.parent || path != actual.path ||
        name != actual.name ||
        (parent == d.scene_identity_.scene_id && name == d.reflector_))
      r.ok = false;
  }
  k = r.count(10000);
  while (k-- && r.ok) {
    auto path = r.s();
    auto h = r.h();
    if (path.empty() || !d.sources_.emplace(path, h).second)
      r.ok = false;
  }
  const auto *current = global.member(FieldGlobalMemberRole::CurrentScene);
  std::array<uint8_t, 32> h{};
  if (!r.ok || r.n || d.scene_ != player.recipe().source_scene() ||
      d.player_ir_ != player.ir_sha256() ||
      d.global_ir_ != global.ir_sha256() ||
      global.identity().upstream_commit != expected.upstream_commit ||
      !current || current->name != d.current_scene_ || d.reflector_.empty() ||
      d.reflector_.find('/') != d.reflector_.npos || d.reflector_ == "." ||
      d.reflector_ == ".." || d.sprite_getter_.empty() ||
      !methods.insert(d.sprite_getter_).second || d.records_.empty() ||
      !player.source_hash(d.script_, h) || h != d.script_sha_ ||
      !global.source_hash(d.global_script_, h) || h != d.global_sha_ ||
      !d.source_hash(d.script_, h) || h != d.script_sha_ ||
      !d.source_hash(d.global_script_, h) || h != d.global_sha_ ||
      std::all_of(d.ir_.begin(), d.ir_.end(), [](uint8_t x) { return !x; }))
    return fail(e, "Player fetcher layout/source/scene closure rejected");
  // Getter members are native schema; script method spellings come from data.
  if (std::set<std::string>{d.exports_.path, d.exports_.ignore, d.exports_.flip,
                            d.exports_.offset}
              .size() != 4 ||
      d.exports_.path.empty() || d.exports_.ignore.empty() ||
      d.exports_.flip.empty() || d.exports_.offset.empty())
    return fail(e, "Player fetcher export schema rejected");
  if (members != std::set<std::string>{"texture", "hframes", "vframes", "frame",
                                       "visible"})
    return fail(e, "Player fetcher native getter schema rejected");
  for (const auto &x : d.getters_)
    if ((x.member == "texture" && x.type != "Texture") ||
        (x.member == "visible" && x.type != "bool") ||
        (x.member != "texture" && x.member != "visible" && x.type != "int"))
      return fail(e, "Player fetcher native return type rejected");
  size_t actual_count = 0;
  for (const auto &x : player.recipe().records())
    if (x.script == d.script_)
      ++actual_count;
  if (actual_count != d.records_.size())
    return fail(e, "Player fetcher complete actual instance closure rejected");
  for (const auto &x : d.records_) {
    auto *node = player.recipe().record(x.id);
    auto *target = player.recipe().record(x.target);
    if (!node || !target || node->native_class != "Node" ||
        node->script != d.script_ || node->script_sha != x.script_sha ||
        node->path != x.path || node->ready != x.ready ||
        node->script_methods != 64 || target->native_class != "Sprite" ||
        target->path != x.target_path || !overrides(player, x, d.exports_))
      return fail(e, "Player fetcher source instance override/target rejected");
  }
  if (!d.source_hash(d.scene_, h) || h != expected.source_sha256 ||
      !d.source_hash(d.scene_source_, h) || h != tree.identity().source_sha256)
    return fail(e, "Player fetcher source hash closure rejected");
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
bool PlayerFetcherData::load_file(const char *path,
                                  const PlayerInitializationData &p,
                                  const FieldNodeTreeData &t,
                                  const FieldGlobalConstructorData &g,
                                  std::string &e) {
  auto *f = path ? std::fopen(path, "rb") : nullptr;
  if (!f)
    return fail(e, "Cannot open Player fetcher resource");
  if (std::fseek(f, 0, SEEK_END)) {
    std::fclose(f);
    return fail(e, "Cannot size Player fetcher resource");
  }
  auto n = std::ftell(f);
  if (n < 128 || n > 4 * 1024 * 1024) {
    std::fclose(f);
    return fail(e, "Player fetcher resource size rejected");
  }
  std::rewind(f);
  std::vector<uint8_t> b(size_t(n), uint8_t{});
  bool ok = std::fread(b.data(), 1, b.size(), f) == b.size();
  std::fclose(f);
  return ok ? load(b.data(), b.size(), p, t, g, e)
            : fail(e, "Player fetcher short read");
}
bool PlayerFetcherData::source_hash(std::string_view p,
                                    std::array<uint8_t, 32> &h) const {
  auto f = sources_.find(std::string(p));
  if (f == sources_.end())
    return false;
  h = f->second;
  return true;
}
} // namespace encore::upstream
