#include "encore/crc32.hpp"
#include "encore/player_effects.hpp"
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
using Value = std::shared_ptr<const GlobalYamlValue>;
Value get(Value v, std::string_view k) { return v ? v->get(k) : nullptr; }
bool txt(Value v, std::string &out) {
  if (!v || v->kind != 4)
    return false;
  out = v->string;
  return !out.empty();
}
bool num(Value v, double &out) {
  if (!v || (v->kind != 2 && v->kind != 3))
    return false;
  out = v->kind == 2 ? double(v->integer) : v->real;
  return std::isfinite(out);
}
bool uint(Value v, uint32_t &out) {
  if (!v || v->kind != 2 || v->integer < 0 || uint64_t(v->integer) > UINT32_MAX)
    return false;
  out = uint32_t(v->integer);
  return true;
}
bool strings(Value v, std::vector<std::string> &out) {
  if (!v || v->kind != 5 || v->array.size() > 32)
    return false;
  for (auto &x : v->array) {
    std::string s;
    if (!txt(x, s))
      return false;
    out.push_back(std::move(s));
  }
  return true;
}
bool hex(Value v, std::array<uint8_t, 32> &out) {
  std::string s;
  if (!txt(v, s) || s.size() != 64)
    return false;
  for (size_t i = 0; i < 32; ++i) {
    auto digit = [](char c) -> int {
      return c >= '0' && c <= '9'   ? c - '0'
             : c >= 'a' && c <= 'f' ? c - 'a' + 10
                                    : -1;
    };
    auto a = digit(s[i * 2]), b = digit(s[i * 2 + 1]);
    if (a < 0 || b < 0)
      return false;
    out[i] = uint8_t(a * 16 + b);
  }
  return nonzero(out);
}
} // namespace
const FieldNodeRecipeData *PlayerEffectsData::recipe(uint32_t i) const {
  return valid_ && i < recipes_.size() ? &recipes_[i] : nullptr;
}
std::shared_ptr<const GlobalYamlValue>
PlayerEffectsData::native(uint32_t i) const {
  return valid_ && i < native_.size() ? native_[i] : nullptr;
}
bool PlayerEffectsData::source_hash(std::string_view p,
                                    std::array<uint8_t, 32> &h) const {
  auto i = sources_.find(std::string(p));
  if (i == sources_.end())
    return false;
  h = i->second;
  return true;
}
bool PlayerEffectsData::load(const uint8_t *p, size_t n,
                             const PlayerInitializationData &player,
                             std::string &e) {
  valid_ = false;
  creators_.clear();
  recipes_.clear();
  native_.clear();
  sources_.clear();
  signals_.reset();
  policy_ = {};
  if (!player.valid() || !p || n < 128 || n > 8 * 1024 * 1024 ||
      std::memcmp(p, "ENCPFX01", 8) || word(p + 8) != 1 ||
      word(p + 12) != 128 || word(p + 16) != n ||
      word(p + 20) != crc32(p + 128, n - 128) || word(p + 24) != 0x454e0059 ||
      word(p + 28) != 1 || word(p + 32) != 1 || word(p + 124) != 0)
    return reject(e, "Player effect format/capability/rules/CRC rejected");
  identity_.scene_id = word(p + 36);
  std::copy_n(p + 40, 20, identity_.upstream_commit.begin());
  std::copy_n(p + 60, 32, identity_.source_sha256.begin());
  std::copy_n(p + 92, 32, ir_.begin());
  auto expected = player.identity();
  if (identity_.scene_id != expected.scene_id ||
      identity_.upstream_commit != expected.upstream_commit ||
      identity_.source_sha256 != expected.source_sha256 || !nonzero(ir_))
    return reject(e, "Player effect source scene/identity rejected");
  Reader r{p, n};
  player_ir_ = r.bytes<32>();
  auto scene = r.text();
  if (player_ir_ != player.ir_sha256() ||
      scene != player.recipe().source_scene())
    return reject(e, "Player effect actual Player dependency rejected");
  auto count = r.u();
  if (!count || count > 4096)
    return reject(e, "Player effect source closure rejected");
  while (count-- && r.ok) {
    auto path = r.text();
    auto h = r.bytes<32>();
    std::array<uint8_t, 32> parent{};
    if (path.empty() || !nonzero(h) || !sources_.emplace(path, h).second ||
        (player.source_hash(path, parent) && parent != h))
      return reject(e, "Player effect original source proof differs");
  }
  auto creators = r.value(), policy = r.value();
  signals_ = r.value();
  auto functions = r.value();
  if (!creators || creators->kind != 5 || creators->array.size() != 2 ||
      !policy || policy->kind != 6 || !signals_ || signals_->kind != 5 ||
      signals_->array.size() != 2 || !functions || functions->kind != 5 ||
      functions->array.empty())
    return reject(e, "Player effect source policy/complete signals missing");
  std::set<uint32_t> ids;
  for (const auto &v : creators->array) {
    PlayerEffectCreator c;
    std::string target_scene;
    if (!uint(get(v, "kind"), c.kind) || c.kind >= 2 ||
        c.kind != creators_.size() || !uint(get(v, "id"), c.id) ||
        !ids.insert(c.id).second || !txt(get(v, "path"), c.path) ||
        !txt(get(v, "native"), c.native) || !txt(get(v, "script"), c.script) ||
        !txt(get(v, "resource_member"), c.resource_member) ||
        !txt(get(v, "scene"), c.scene) ||
        !hex(get(v, "script_sha"), c.script_sha) ||
        !strings(get(v, "methods"), c.methods))
      return reject(e, "Player creator typed source fields rejected");
    auto node = player.recipe().record(c.id);
    std::array<uint8_t, 32> source{};
    if (!node || node->path != c.path || node->native_class != c.native ||
        node->script != c.script || node->script_sha != c.script_sha ||
        !source_hash(c.script, source) || source != c.script_sha)
      return reject(e, "Player creator actual attachment rejected");
    if (c.kind == 0) {
      if (!uint(get(v, "parent_hops"), c.parent_hops) || !c.parent_hops ||
          c.parent_hops > 16 || !txt(get(v, "timer_path"), c.timer_path) ||
          !txt(get(v, "sprite_path"), c.sprite_path) ||
          !txt(get(v, "timeout_method"), c.timeout_method))
        return reject(e, "AfterImage source node paths rejected");
    } else {
      auto bounds = get(v, "rng_bounds");
      std::string flag;
      if (!txt(get(v, "scene_member"), c.scene_member) ||
          !txt(get(v, "objects_path"), c.objects_path) ||
          !txt(get(v, "party_member"), c.party_member) ||
          !txt(get(v, "animation_path"), c.animation_path) ||
          !txt(get(v, "animation"), c.animation) ||
          !txt(get(v, "finished_signal"), c.finished_signal) ||
          !txt(get(v, "finished_method"), c.finished_method) ||
          !txt(get(v, "created_member"), c.created_member) ||
          !txt(get(v, "connect_flag"), flag) || flag != "CONNECT_ONESHOT" ||
          !num(get(v, "party_depth"), c.party_depth) || !bounds ||
          bounds->kind != 5 || bounds->array.size() != 2 ||
          !num(bounds->array[0], c.rng_bounds[0]) ||
          !num(bounds->array[1], c.rng_bounds[1]))
        return reject(e, "Dust source RNG/connection/party policy rejected");
    }
    creators_.push_back(std::move(c));
  }
  if (!txt(get(policy, "after_animation"), policy_.after_animation) ||
      !txt(get(policy, "after_animation_path"), policy_.after_animation_path) ||
      !txt(get(policy, "after_finished_method"),
           policy_.after_finished_method) ||
      !txt(get(policy, "after_finished_signal"),
           policy_.after_finished_signal) ||
      !txt(get(policy, "tint_targets_member"), policy_.tint_targets_member) ||
      !txt(get(policy, "tint_script"), policy_.tint_script) ||
      !strings(get(policy, "tint_paths"), policy_.tint_paths) ||
      !strings(get(policy, "copied_members"), policy_.copied_members))
    return reject(e, "Player effect source method policy rejected");
  auto color = get(policy, "tint_default");
  if (!color || color->kind != 5 || color->array.size() != 4)
    return reject(e, "Player effect tint native default missing");
  for (size_t i = 0; i < 4; ++i) {
    double f = 0;
    if (!num(color->array[i], f) || !std::isfinite(float(f)))
      return reject(e, "Player effect tint default nonfinite");
    policy_.tint_default[i] = float(f);
  }
  count = r.u();
  if (count != 2)
    return reject(e, "Player effect complete PackedScene resources rejected");
  for (uint32_t i = 0; i < count; ++i) {
    auto size = r.u();
    if (!r.ok || size < 128 || r.at > n || n - r.at < size)
      return reject(e, "Player effect nested recipe truncated");
    FieldIdentity id{};
    id.scene_id = word(p + r.at + 36);
    id.upstream_commit = identity_.upstream_commit;
    std::copy_n(p + r.at + 60, 32, id.source_sha256.begin());
    FieldNodeRecipeData recipe;
    if (!recipe.load(p + r.at, size, id, e))
      return false;
    r.at += size;
    auto snapshot = r.value();
    auto nodes = get(snapshot, "nodes"), resources = get(snapshot, "resources");
    std::string source;
    auto admitted = get(snapshot, "native_compatible");
    std::array<uint8_t, 32> h{};
    if (!snapshot || snapshot->kind != 6 ||
        !txt(get(snapshot, "source"), source) ||
        source != "res://" + recipe.source_scene() || !admitted ||
        admitted->kind != 1 || admitted->boolean ||
        recipe.source_scene() != creators_[i].scene ||
        !source_hash(recipe.source_scene(), h) || h != id.source_sha256 ||
        !nodes || nodes->kind != 5 ||
        nodes->array.size() != recipe.records().size() || !resources ||
        resources->kind != 5)
      return reject(
          e,
          "Player effect full original native tree/resource receipt rejected");
    for (size_t j = 0; j < nodes->array.size(); ++j) {
      std::string path, klass;
      auto node = nodes->array[j];
      if (!txt(get(node, "path"), path) || !txt(get(node, "class"), klass) ||
          path != recipe.records()[j].path ||
          klass != recipe.records()[j].native_class)
        return reject(e,
                      "Player effect native structure cross-binding rejected");
    }
    for (auto &node : recipe.records())
      if (!node.script.empty()) {
        std::array<uint8_t, 32> actual{};
        if (!source_hash(node.script, actual) || actual != node.script_sha)
          return reject(e, "Player effect inherited script proof rejected");
      }
    recipes_.push_back(std::move(recipe));
    native_.push_back(std::move(snapshot));
  }
  auto timer_size = r.u();
  if (!r.ok || timer_size < 56 || r.at > n || n - r.at < timer_size ||
      !timer_.load(p + r.at, timer_size, e))
    return reject(e, "Player effect native Timer resource rejected");
  r.at += timer_size;
  auto timer_path = creators_[0].path + "/" + creators_[0].timer_path;
  auto timer_node = std::find_if(
      player.recipe().records().begin(), player.recipe().records().end(),
      [&](const auto &row) { return row.path == timer_path; });
  if (timer_.records().size() != 1 ||
      timer_node == player.recipe().records().end() ||
      timer_node->native_class != "Timer" || !timer_node->script.empty() ||
      !timer_.record(player.identity(), timer_node->id) ||
      timer_.records()[0].script_sha != timer_node->script_sha)
    return reject(e, "Player effect Timer actual source identity differs");
  if (!r.ok || r.at != n)
    return reject(e, "Player effect trailing/unknown Variant rejected");
  valid_ = true;
  e.clear();
  return true;
}
bool PlayerEffectsData::load_file(const char *path,
                                  const PlayerInitializationData &player,
                                  std::string &e) {
  if (!path)
    return reject(e, "Player effect resource path absent");
  auto f = std::fopen(path, "rb");
  if (!f)
    return reject(e, "Player effect resource cannot open");
  if (std::fseek(f, 0, SEEK_END)) {
    std::fclose(f);
    return reject(e, "Player effect resource cannot seek");
  }
  auto n = std::ftell(f);
  if (n < 128 || n > 8 * 1024 * 1024) {
    std::fclose(f);
    return reject(e, "Player effect resource extent rejected");
  }
  std::rewind(f);
  std::vector<uint8_t> b(static_cast<size_t>(n));
  auto got = std::fread(b.data(), 1, b.size(), f);
  auto io = std::ferror(f);
  std::fclose(f);
  if (io || got != b.size())
    return reject(e, "Player effect resource truncated");
  return load(b.data(), b.size(), player, e);
}
} // namespace encore::upstream
