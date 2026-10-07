#include "encore/crc32.hpp"
#include "encore/player_named_sfx.hpp"
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

bool text(const std::shared_ptr<const GlobalYamlValue> &v, std::string_view x) {
  return v && v->kind == 4 && v->string == x;
}
} // namespace
const PlayerNamedSfxStream *PlayerNamedSfxData::stream(uint32_t id) const {
  for (auto &v : streams_)
    if (v.audio.id == id)
      return &v;
  return nullptr;
}
const PlayerNamedSfxStream *
PlayerNamedSfxData::stream(std::string_view path) const {
  for (auto &v : streams_)
    if (v.audio.source == path)
      return &v;
  return nullptr;
}
bool PlayerNamedSfxData::load(const uint8_t *p, size_t n,
                              const FieldGlobalRegistryData &registry,
                              std::string &e) {
  if (!p || n < 128 || n > 4 * 1024 * 1024 || !registry.valid() ||
      std::memcmp(p, "ENCNSFX1", 8) || word(p + 8) != 1 ||
      word(p + 12) != 128 || word(p + 16) != n || word(p + 24) != 0x454e0073 ||
      word(p + 28) != 1 || word(p + 32) != 1 || !word(p + 36) || word(p + 124) ||
      std::memcmp(p + 40, registry.identity().upstream_commit.data(), 20) ||
      word(p + 20) != crc32(p + 128, n - 128))
    return reject(e, "Named SFX identity/version/CRC rejected");
  PlayerNamedSfxData d;
  std::copy_n(p + 92, 32, d.ir_.begin());
  if (!nonzero(d.ir_))
    return reject(e, "Named SFX IR absent");
  Reader r{p, n};
  FieldIdentity id;
  id.scene_id = r.u();
  id.upstream_commit = registry.identity().upstream_commit;
  id.source_sha256 = r.bytes<32>();
  auto length = r.u();
  if (!r.ok || id.scene_id != word(p + 36) || length > n - r.at ||
      std::memcmp(p + 60, id.source_sha256.data(), 32) ||
      !d.recipe_.load(p + r.at, length, id, e))
    return false;
  r.at += length;
  const FieldGlobalAutoload *a = nullptr;
  for (auto &v : registry.autoloads())
    if (v.path == d.recipe_.source_scene())
      a = &v;
  if (!a || a->native_class != "Control" || a->source_sha != id.source_sha256 ||
      d.recipe_.records().size() != 4)
    return reject(e, "Named SFX complete original autoload recipe rejected");
  auto root = d.recipe_.record(id.scene_id);
  if (!root || root->script != a->script || root->script_sha != a->script_sha)
    return reject(e, "Named SFX script source binding rejected");
  d.native_ = r.value();
  d.constructor_ = r.value();
  d.effects_ = r.value();
  d.methods_ = r.value();
  auto defaults = r.value();
  d.bus_ = r.text();
  d.music_bus_ = r.text();
  d.bank_ = r.text();
  d.bank_sha_ = r.bytes<32>();
  auto voiceid = r.u();
  if (!r.ok || !d.native_ || !d.constructor_ || !d.effects_ || !d.methods_ ||
      !defaults || d.native_->kind != 6 || d.constructor_->kind != 6 ||
      d.effects_->kind != 5 || d.methods_->kind != 6 || defaults->kind != 6 ||
      !voiceid || d.bus_.empty() || d.music_bus_.empty() || d.bank_.empty() ||
      d.bank_.find("..") != d.bank_.npos || !nonzero(d.bank_sha_))
    return reject(e, "Named SFX full source fields rejected");
  auto native_nodes = d.native_->get("nodes");
  if (!native_nodes || native_nodes->kind != 5 ||
      native_nodes->array.size() != d.recipe_.records().size())
    return reject(e, "Named SFX native source closure rejected");
  for (size_t i = 0; i < d.recipe_.records().size(); ++i) {
    auto &node = d.recipe_.records()[i];
    auto v = native_nodes->array[i];
    if (!v || v->kind != 6 || !text(v->get("path"), node.path) ||
        !text(v->get("class"), node.native_class) || !v->get("properties"))
      return reject(e, "Named SFX source native property map mismatch");
    if (i ? (!node.script.empty()) : (node.script != a->script))
      return reject(e, "Named SFX unexpected inherited script");
    if (node.native_class != "Control" && node.native_class != "Node" &&
        node.native_class != "Tween")
      return reject(e, "Named SFX unknown native source class");
  }
  for (auto &kv : d.constructor_->dictionary)
    if (!kv.second || kv.second->kind != 4 || kv.second->string.empty())
      return reject(e, "Named SFX source member binding rejected");
  for (auto key : {"sfx_parent", "music_parent", "tween_node"}) {
    auto v = d.constructor_->get(key);
    bool found = false;
    for (auto &node : d.recipe_.records())
      if (v && node.path == v->string)
        found = true;
    if (!found)
      return reject(e, "Named SFX source child binding absent");
  }
  auto num = [&](const char *k, double expected) {
    auto v = defaults->get(k);
    return v && ((v->kind == 2 && double(v->integer) == expected) ||
                 (v->kind == 3 && v->real == expected));
  };
  auto flag = [&](const char *k) {
    auto v = defaults->get(k);
    return v && v->kind == 1 && !v->boolean;
  };
  auto nil = defaults->get("stream");
  if (!nil || nil->kind || !text(defaults->get("bus"), "Master") ||
      !num("volume_db", 0) || !num("pitch_scale", 1) || !num("mix_target", 0) ||
      !flag("autoplay") || !flag("stream_paused"))
    return reject(e, "Named SFX unsupported native .new defaults");
  d.prototype_.id = voiceid;
  d.prototype_.kind = 1;
  d.prototype_.bus = defaults->get("bus")->string;
  d.prototype_.pitch = 1;
  d.voice_.id = voiceid;
  d.voice_.path = ".";
  d.voice_.index = -1;
  d.voice_.native_class = "AudioStreamPlayer";
  d.voice_.class_index = 14;
  d.voice_.local = d.voice_.world = {Vec2{1, 0}, Vec2{0, 1}, Vec2{0, 0}};
  d.voice_.modulate = d.voice_.self_modulate = {1, 1, 1, 1};
  auto count = r.u();
  if (!count || count > 64)
    return reject(e, "Named SFX stream count rejected");
  std::set<uint32_t> ids;
  std::set<std::string> paths;
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    PlayerNamedSfxStream v;
    v.audio.id = r.u();
    v.audio.asset_id = r.u();
    v.audio.source = r.text();
    v.audio.native_class = r.text();
    v.audio.source_sha = r.bytes<32>();
    v.import_sha = r.bytes<32>();
    v.payload_sha = r.bytes<32>();
    v.metadata = r.value();
    auto bytes = r.u();
    if (!r.ok || !v.audio.id || !v.audio.asset_id ||
        !ids.insert(v.audio.id).second ||
        !paths.insert(v.audio.source).second || !bytes || bytes > 1024 * 1024 ||
        bytes > n - r.at || !nonzero(v.payload_sha) || !v.metadata ||
        (v.audio.native_class != "AudioStreamSample" &&
         v.audio.native_class != "AudioStreamMP3"))
      return reject(e, "Named SFX actual audio Resource rejected");
    std::array<uint8_t, 32> hash{};
    if (!d.recipe_.source_hash(v.audio.source, hash) ||
        hash != v.audio.source_sha ||
        !d.recipe_.source_hash(v.audio.source + ".import", hash) ||
        hash != v.import_sha)
      return reject(e, "Named SFX stream source closure rejected");
    v.payload.assign(p + r.at, p + r.at + bytes);
    r.at += bytes;
    d.streams_.push_back(std::move(v));
  }
  std::set<std::string> names;
  for (auto &v : d.effects_->array) {
    auto name = v ? v->get("name") : nullptr;
    auto source = v ? v->get("source") : nullptr;
    if (!name || name->kind != 4 || name->string.empty() ||
        !names.insert(name->string).second || !source || source->kind != 4 ||
        !d.stream(source->string))
      return reject(e, "Named SFX actual constructor dictionary rejected");
  }
  if (!r.ok || r.at != n)
    return reject(e, "Named SFX trailing or unknown source rejected");
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
bool PlayerNamedSfxData::load_file(const char *path,
                                   const FieldGlobalRegistryData &p,
                                   std::string &e) {
  if (!path || !*path)
    return reject(e, "Named SFX file path rejected");
  FILE *f = std::fopen(path, "rb");
  if (!f)
    return reject(e, "Cannot open Named SFX resource");
  if (std::fseek(f, 0, SEEK_END)) {
    std::fclose(f);
    return reject(e, "Named SFX seek rejected");
  }
  long size = std::ftell(f);
  if (size < 128 || size > 4 * 1024 * 1024 || std::fseek(f, 0, SEEK_SET)) {
    std::fclose(f);
    return reject(e, "Named SFX size rejected");
  }
  std::vector<uint8_t> bytes(size);
  bool ok = std::fread(bytes.data(), 1, bytes.size(), f) == bytes.size();
  std::fclose(f);
  return ok ? load(bytes.data(), bytes.size(), p, e)
            : reject(e, "Named SFX read rejected");
}
} // namespace encore::upstream
