#include "encore/crc32.hpp"
#include "encore/player_preload_scenes.hpp"
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
bool reject(std::string &e, const char *s) {
  e = s;
  return false;
}
bool nonzero(const std::array<uint8_t, 32> &h) {
  return std::any_of(h.begin(), h.end(), [](uint8_t b) { return b != 0; });
}
struct Reader {
  const uint8_t *p;
  size_t n, at = 128, nodes = 0;
  bool ok = true;
  uint32_t u() {
    if (!ok || at > n || n - at < 4) {
      ok = false;
      return 0;
    }
    auto v = word(p + at);
    at += 4;
    return v;
  }
  template <size_t N> std::array<uint8_t, N> bytes() {
    std::array<uint8_t, N> x{};
    if (!ok || at > n || n - at < N) {
      ok = false;
      return x;
    }
    std::copy_n(p + at, N, x.begin());
    at += N;
    return x;
  }
  double real() {
    auto b = bytes<8>();
    double v = 0;
    std::memcpy(&v, b.data(), 8);
    if (!std::isfinite(v))
      ok = false;
    return v;
  }
  int64_t integer() {
    auto b = bytes<8>();
    int64_t v = 0;
    std::memcpy(&v, b.data(), 8);
    return v;
  }
  std::string text() {
    auto len = u();
    if (!ok || len > 65536 || at > n || n - at < len) {
      ok = false;
      return {};
    }
    std::string s(reinterpret_cast<const char *>(p + at), len);
    at += len;
    std::u32string native;
    if (s.find('\0') != s.npos || !utf8_decode(s, native))
      ok = false;
    return s;
  }
  std::shared_ptr<GlobalYamlValue> value(uint32_t depth = 0) {
    if (!ok || depth > 32 || ++nodes > 600000) {
      ok = false;
      return {};
    }
    auto v = std::make_shared<GlobalYamlValue>();
    v->kind = u();
    if (v->kind == 1) {
      auto b = u();
      if (b > 1)
        ok = false;
      v->boolean = b != 0;
    } else if (v->kind == 2)
      v->integer = integer();
    else if (v->kind == 3)
      v->real = real();
    else if (v->kind == 4)
      v->string = text();
    else if (v->kind == 5 || v->kind == 6) {
      auto count = u();
      if (count > 65536) {
        ok = false;
        return {};
      }
      std::set<std::string> seen;
      for (uint32_t i = 0; i < count && ok; ++i) {
        if (v->kind == 5)
          v->array.push_back(value(depth + 1));
        else {
          auto key = text();
          if (!seen.insert(key).second) {
            ok = false;
            return {};
          }
          v->dictionary.emplace_back(key, value(depth + 1));
        }
      }
    } else if (v->kind != 0)
      ok = false;
    return ok ? v : nullptr;
  }
};

bool equal(const GlobalYamlValue &a, const GlobalYamlValue &b) {
  if (a.kind != b.kind)
    return false;
  switch (a.kind) {
  case 0:
    return true;
  case 1:
    return a.boolean == b.boolean;
  case 2:
    return a.integer == b.integer;
  case 3:
    return a.real == b.real;
  case 4:
    return a.string == b.string;
  case 5:
    if (a.array.size() != b.array.size())
      return false;
    for (size_t i = 0; i < a.array.size(); ++i)
      if (!a.array[i] || !b.array[i] || !equal(*a.array[i], *b.array[i]))
        return false;
    return true;
  case 6:
    if (a.dictionary.size() != b.dictionary.size())
      return false;
    for (size_t i = 0; i < a.dictionary.size(); ++i)
      if (a.dictionary[i].first != b.dictionary[i].first ||
          !a.dictionary[i].second || !b.dictionary[i].second ||
          !equal(*a.dictionary[i].second, *b.dictionary[i].second))
        return false;
    return true;
  default:
    return false;
  }
}
bool text(const std::shared_ptr<const GlobalYamlValue> &v,
          std::string_view expected) {
  return v && v->kind == 4 && v->string == expected;
}
bool native_complete(const PlayerPreloadScene &s) {
  auto n = s.native_source;
  if (!n || n->kind != 6 ||
      !text(n->get("source"), "res://" + s.recipe.source_scene()))
    return false;
  auto nodes = n->get("nodes"), resources = n->get("resources"),
       states = n->get("scene_states");
  if (!nodes || nodes->kind != 5 ||
      nodes->array.size() != s.recipe.records().size() || !resources ||
      resources->kind != 5 || resources->array.empty() || !states ||
      states->kind != 5 || states->array.empty() || !s.connections ||
      s.connections->kind != 5)
    return false;
  for (size_t i = 0; i < nodes->array.size(); ++i) {
    auto &r = s.recipe.records()[i];
    auto v = nodes->array[i];
    if (!v || v->kind != 6 || !text(v->get("path"), r.path) ||
        !text(v->get("class"), r.native_class) || !text(v->get("name"), r.name))
      return false;
    auto props = v->get("properties");
    if (!props || props->kind != 6)
      return false;
  }
  std::set<int64_t> ids;
  for (auto &v : resources->array) {
    if (!v || v->kind != 6)
      return false;
    auto id = v->get("id"), cls = v->get("class");
    if (!id || id->kind != 2 || id->integer < 0 ||
        !ids.insert(id->integer).second || !cls || cls->kind != 4 ||
        cls->string.empty())
      return false;
  }
  for (auto &v : states->array) {
    if (!v || v->kind != 6)
      return false;
    auto path = v->get("source"), rows = v->get("nodes");
    if (!path || path->kind != 4 || path->string.empty() || !rows ||
        rows->kind != 5 || rows->array.empty())
      return false;
    std::array<uint8_t, 32> sha{};
    auto name = path->string;
    if (name.compare(0, 6, "res://") == 0)
      name.erase(0, 6);
    if (!s.recipe.source_hash(name, sha))
      return false;
  }
  for (auto &v : s.connections->array) {
    if (!v || v->kind != 6)
      return false;
    auto d = v->get("declaration"), src = v->get("source");
    if (!d || d->kind != 4 || d->string.empty() || !src || src->kind != 4)
      return false;
    std::array<uint8_t, 32> sha{};
    if (!s.recipe.source_hash(src->string, sha))
      return false;
  }
  return true;
}
} // namespace
const PlayerPreloadScene *
PlayerPreloadScenesData::entry(const GlobalYamlValue &row) const {
  if (!valid_)
    return nullptr;
  for (auto &v : entries_)
    if (v.row && equal(*v.row, row))
      return &v;
  return nullptr;
}
bool PlayerPreloadScenesData::load(const uint8_t *p, size_t n,
                                   const PlayerInitializationData &player,
                                   std::string &e) {
  if (!p || n < 128 || n > 16 * 1024 * 1024 || !player.valid() ||
      std::memcmp(p, "ENCPPSC1", 8) || word(p + 8) != 1 ||
      word(p + 12) != 128 || word(p + 16) != n || word(p + 24) != 0x454e0072 ||
      word(p + 28) != 1 || word(p + 32) != 1 || word(p + 36) != player.identity().scene_id || word(p + 124) ||
      std::memcmp(p + 40, player.identity().upstream_commit.data(), 20) ||
      std::memcmp(p + 60, player.identity().source_sha256.data(), 32) ||
      word(p + 20) != crc32(p + 128, n - 128))
    return reject(e, "Player preload identity/version/CRC rejected");
  PlayerPreloadScenesData d;
  std::copy_n(p + 92, 32, d.ir_.begin());
  if (!nonzero(d.ir_))
    return reject(e, "Player preload IR missing");
  Reader r{p, n};
  d.player_ir_ = r.bytes<32>();
  auto count = r.u();
  auto source = player.onready_source();
  if (!r.ok || d.player_ir_ != player.ir_sha256() || !count || count > 64 ||
      !source || source->kind != 5)
    return reject(e, "Player preload source declarations rejected");
  std::vector<std::shared_ptr<GlobalYamlValue>> actual;
  for (auto &v : source->array) {
    if (!v || v->kind != 6)
      return reject(e, "Player preload original onready row malformed");
    auto kind = v->get("kind"), native = v->get("native");
    if (kind && kind->kind == 2 && kind->integer == 2 &&
        text(native, "PackedScene"))
      actual.push_back(v);
  }
  if (actual.size() != count)
    return reject(e, "Player preload complete onready scope mismatch");
  for (uint32_t i = 0; i < count; ++i) {
    PlayerPreloadScene s;
    s.row = r.value();
    FieldIdentity id;
    id.scene_id = r.u();
    id.upstream_commit = player.identity().upstream_commit;
    id.source_sha256 = r.bytes<32>();
    auto len = r.u();
    if (!r.ok || !s.row || !equal(*s.row, *actual[i]) || len > n - r.at ||
        !id.scene_id || !nonzero(id.source_sha256))
      return reject(e, "Player preload row/recipe length rejected");
    if (!s.recipe.load(p + r.at, len, id, e))
      return false;
    r.at += len;
    auto resource = s.row->get("resource");
    std::array<uint8_t, 32> h{};
    if (!text(resource, s.recipe.source_scene()) ||
        !player.source_hash(s.recipe.source_scene(), h) ||
        h != id.source_sha256)
      return reject(e, "Player preload source resource cross-binding rejected");
    s.native_source = r.value();
    s.connections = r.value();
    if (!r.ok || !native_complete(s))
      return reject(
          e, "Player preload full native SceneState/resource closure rejected");
    d.entries_.push_back(std::move(s));
  }
  if (!r.ok || r.at != n)
    return reject(e, "Player preload trailing/unknown source rejected");
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
bool PlayerPreloadScenesData::load_file(const char *path,
                                        const PlayerInitializationData &p,
                                        std::string &e) {
  if (!path || !*path)
    return reject(e, "Player preload file path rejected");
  FILE *f = std::fopen(path, "rb");
  if (!f)
    return reject(e, "Cannot open Player preload resource");
  if (std::fseek(f, 0, SEEK_END)) {
    std::fclose(f);
    return reject(e, "Player preload seek rejected");
  }
  long size = std::ftell(f);
  if (size < 128 || size > 16 * 1024 * 1024 || std::fseek(f, 0, SEEK_SET)) {
    std::fclose(f);
    return reject(e, "Player preload size rejected");
  }
  std::vector<uint8_t> bytes(size);
  bool ok = std::fread(bytes.data(), 1, bytes.size(), f) == bytes.size();
  std::fclose(f);
  return ok ? load(bytes.data(), bytes.size(), p, e)
            : reject(e, "Player preload read rejected");
}
} // namespace encore::upstream
