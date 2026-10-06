#include "encore/crc32.hpp"
#include "encore/player_initialization.hpp"
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
Value get(const Value &v, std::string_view k) {
  return v ? v->get(k) : nullptr;
}
bool text(const Value &v, std::string &s) {
  if (!v || v->kind != 4)
    return false;
  s = v->string;
  return true;
}
bool uint(const Value &v, uint32_t &n) {
  if (!v || v->kind != 2 || v->integer < 0 || uint64_t(v->integer) > UINT32_MAX)
    return false;
  n = uint32_t(v->integer);
  return true;
}
bool list(const Value &v, std::vector<std::string> &out) {
  if (!v || v->kind != 5)
    return false;
  for (const auto &x : v->array) {
    std::string s;
    if (!text(x, s) || s.empty())
      return false;
    out.push_back(s);
  }
  return true;
}
bool boolean(const Value &v, bool &b) {
  if (!v || v->kind != 1)
    return false;
  b = v->boolean;
  return true;
}
bool real(const Value &v, double &n) {
  if (!v || (v->kind != 2 && v->kind != 3))
    return false;
  n = v->kind == 2 ? double(v->integer) : v->real;
  return std::isfinite(n);
}
bool default_value(const Value &v, PlayerInitializationField &out) {
  if (!v || v->kind != 6 || !uint(get(v, "kind"), out.kind) || out.kind > 7)
    return false;
  auto x = get(v, "value");
  if (out.kind == 0) {
    out.value = std::make_shared<GlobalYamlValue>();
    return true;
  }
  auto result = std::make_shared<GlobalYamlValue>();
  result->kind = out.kind;
  if (out.kind == 1) {
    if (!boolean(x, result->boolean))
      return false;
  } else if (out.kind == 2) {
    std::string s;
    if (!text(x, s) || s.empty())
      return false;
    bool negative = s[0] == '-';
    size_t at = negative ? 1 : 0;
    uint64_t n = 0,
             limit = negative ? uint64_t(INT64_MAX) + 1 : uint64_t(INT64_MAX);
    if (at == s.size())
      return false;
    for (; at < s.size(); ++at) {
      if (s[at] < '0' || s[at] > '9' ||
          n > (limit - uint64_t(s[at] - '0')) / 10)
        return false;
      n = n * 10 + uint64_t(s[at] - '0');
    }
    result->integer =
        negative ? (n == uint64_t(INT64_MAX) + 1 ? INT64_MIN : -int64_t(n))
                 : int64_t(n);
  } else if (out.kind == 4) {
    if (!text(x, result->string))
      return false;
  } else if (out.kind == 3 || out.kind == 7) {
    std::vector<Value> coordinates;
    if (out.kind == 3)
      coordinates.push_back(get(v, "f64_le"));
    else {
      auto a = get(v, "coordinates");
      if (!a || a->kind != 5 || a->array.size() != 2)
        return false;
      for (auto &z : a->array)
        coordinates.push_back(z);
    }
    for (size_t i = 0; i < coordinates.size(); ++i) {
      std::string s;
      if (!text(coordinates[i], s) || s.size() != 16)
        return false;
      std::array<uint8_t, 8> b{};
      for (size_t j = 0; j < 16; ++j) {
        char c = s[j];
        unsigned k = c >= '0' && c <= '9'   ? unsigned(c - '0')
                     : c >= 'a' && c <= 'f' ? unsigned(c - 'a' + 10)
                                            : 16;
        if (k > 15)
          return false;
        b[j / 2] |= uint8_t(k << (j % 2 ? 0 : 4));
      }
      double n;
      std::memcpy(&n, b.data(), 8);
      if (!std::isfinite(n))
        return false;
      if (out.kind == 3)
        result->real = n;
      else
        out.vector[i] = n;
    }
  } else
    return false; // No original Player declaration uses Array/Dictionary.
  out.value = out.kind == 7 ? nullptr : result;
  return true;
}
} // namespace
bool PlayerInitializationData::source_hash(std::string_view p,
                                           std::array<uint8_t, 32> &h) const {
  auto i = sources_.find(std::string(p));
  if (i == sources_.end())
    return false;
  h = i->second;
  return true;
}
bool PlayerInitializationData::load(const uint8_t *p, size_t n,
                                    const GlobalDataConstructorData &ctor,
                                    std::string &e) {
  if (!ctor.valid() || !p || n < 128 || n > 32 * 1024 * 1024 ||
      std::memcmp(p, "ENCPLYI1", 8) || word(p + 8) != 1 ||
      word(p + 12) != 128 || word(p + 16) != n ||
      word(p + 20) != crc32(p + 128, n - 128) || word(p + 24) != 0x454e0056 ||
      word(p + 28) != 1 || word(p + 32) != 1 || !word(p + 36) || word(p + 124))
    return reject(e, "Player initialization format/capability/CRC rejected");
  PlayerInitializationData d;
  d.identity_.scene_id = word(p + 36);
  std::copy_n(p + 40, 20, d.identity_.upstream_commit.begin());
  std::copy_n(p + 60, 32, d.identity_.source_sha256.begin());
  std::copy_n(p + 92, 32, d.ir_.begin());
  if (d.identity_.upstream_commit != ctor.identity().upstream_commit ||
      !nonzero(d.identity_.source_sha256) || !nonzero(d.ir_))
    return reject(e, "Player initialization source identity rejected");
  Reader r{p, n};
  auto scene = r.text();
  d.owner_ = r.text();
  d.player_ = r.text();
  d.base_ = r.text();
  if (r.bytes<32>() != ctor.ir_sha256())
    return reject(e,
                  "Player constructor actual globaldata dependency rejected");
  auto sources = r.u();
  if (sources < 4 || sources > 4096)
    return reject(e, "Player source closure rejected");
  for (uint32_t i = 0; i < sources && r.ok; ++i) {
    auto path = r.text();
    auto h = r.bytes<32>();
    if (path.empty() || !nonzero(h) || !d.sources_.emplace(path, h).second)
      return reject(e, "Player duplicate/invalid source path");
  }
  std::array<uint8_t, 32> h{};
  if (!d.source_hash(scene, h) || h != d.identity_.source_sha256 ||
      !d.source_hash(d.owner_, h) || !d.source_hash(d.player_, h) ||
      !d.source_hash(d.base_, h))
    return reject(e, "Player source ownership rejected");
  auto bytes = r.u();
  if (!r.ok || bytes < 128 || r.at > n || n - r.at < bytes ||
      !d.recipe_.load(p + r.at, bytes, d.identity_, e))
    return false;
  r.at += bytes;
  if (d.recipe_.source_scene() != scene || d.recipe_.ir_sha256() != d.ir_ ||
      d.recipe_.records().empty() ||
      d.recipe_.records().front().script != d.player_)
    return reject(e, "Player native recipe/source cross-binding rejected");
  auto fields = r.value();
  d.onready_ = r.value();
  auto policy = r.value();
  d.native_ = r.value();
  if (!r.ok || r.at != n || !fields || fields->kind != 5 ||
      fields->array.empty() || !d.onready_ || d.onready_->kind != 5 ||
      !d.native_ || d.native_->kind != 6 || !policy || policy->kind != 6)
    return reject(e, "Player source body/native properties rejected");
  std::set<std::string> names;
  bool leader = false;
  for (const auto &v : fields->array) {
    PlayerInitializationField f;
    if (!text(get(v, "name"), f.name) || f.name.empty() ||
        !names.insert(f.name).second || !text(get(v, "source"), f.source) ||
        !text(get(v, "hint"), f.hint) || !uint(get(v, "adapter"), f.adapter) ||
        f.adapter > 2 || !d.source_hash(f.source, h) ||
        (f.source != d.base_ && f.source != d.player_))
      return reject(e, "Player declaration adapter/source rejected");
    if (f.adapter == 0) {
      if (!default_value(get(v, "value"), f))
        return reject(e, "Player declaration native value rejected");
    } else if (f.adapter == 1) {
      if (leader || !text(get(v, "member"), f.member) ||
          !text(get(v, "key"), f.key) ||
          !uint(get(v, "reference_id"), f.declaration))
        return reject(e, "Player actual Character reference rejected");
      auto it = std::find_if(
          ctor.declarations().begin(), ctor.declarations().end(),
          [&](const auto &x) { return x.name == f.member && x.adapter == 1; });
      if (it == ctor.declarations().end() ||
          std::find(it->references.begin(), it->references.end(),
                    std::make_pair(f.key, f.declaration)) ==
              it->references.end())
        return reject(e, "Player Character source declaration mismatch");
      leader = true;
      f.kind = 8;
    } else {
      if (!text(get(v, "resource"), f.resource) ||
          !text(get(v, "native"), f.native) ||
          f.native != "AudioStreamSample" || !d.source_hash(f.resource, h))
        return reject(e, "Player source ResourceLoader rejected");
      f.kind = 8;
    }
    d.fields_.push_back(std::move(f));
  }
  auto &a = d.policy_;
  if (!leader || !text(get(policy, "current_scene"), a.current_scene) ||
      !text(get(policy, "party"), a.party) ||
      !text(get(policy, "party_objects"), a.party_objects) ||
      !text(get(policy, "player_name"), a.player_name) ||
      !text(get(policy, "leader_member"), a.leader_member) ||
      !text(get(policy, "leader_key"), a.leader_key) ||
      !uint(get(policy, "leader_id"), a.leader_declaration) ||
      !text(get(policy, "ready_signal"), a.ready_signal) ||
      !text(get(policy, "pause_method"), a.pause_method) ||
      !text(get(policy, "followers_method"), a.followers_method) ||
      !text(get(policy, "respawn_method"), a.respawn_method) ||
      !uint(get(policy, "connect_flags"), a.connect_flags) ||
      a.connect_flags != 4 ||
      !boolean(get(policy, "followers_emit"), a.followers_emit) ||
      a.followers_emit ||
      !list(get(policy, "parent_candidates"), a.parent_candidates) ||
      a.parent_candidates.size() != 2 ||
      !list(get(policy, "respawn_fields"), a.respawn_fields) ||
      a.respawn_fields.size() != 4)
    return reject(e, "Player initialization source order/policy rejected");
  auto pos = get(policy, "position");
  double x = 0, y = 0;
  if (!pos || pos->kind != 5 || pos->array.size() != 2 ||
      !real(pos->array[0], x) || !real(pos->array[1], y) || x != 0 || y != 0)
    return reject(e, "Player initial source position rejected");
  a.position = {float(x), float(y)};
  auto f = std::find_if(d.fields_.begin(), d.fields_.end(),
                        [](const auto &v) { return v.adapter == 1; });
  if (f->member != a.leader_member || f->key != a.leader_key ||
      f->declaration != a.leader_declaration)
    return reject(e, "Player leader/body binding mismatch");
  // Onready is a checked source operation list, never executed by this owner.
  for (const auto &v : d.onready_->array) {
    uint32_t kind = 0, id = 0;
    std::string name;
    if (!uint(get(v, "kind"), kind) || kind < 1 || kind > 3 ||
        !text(get(v, "name"), name) || name.empty() ||
        !names.insert(name).second)
      return reject(e, "Player unknown/duplicate onready operation");
    if (kind == 1 || kind == 3) {
      if (!uint(get(v, "node_id"), id) || !d.recipe_.record(id))
        return reject(e, "Player onready actual node rejected");
    } else {
      std::string resource;
      if (!text(get(v, "resource"), resource) || !d.source_hash(resource, h))
        return reject(e, "Player onready preload source rejected");
    }
  }
  d.valid_ = true;
  *this = std::move(d);
  return true;
}
bool PlayerInitializationData::load_file(const char *path,
                                         const GlobalDataConstructorData &ctor,
                                         std::string &e) {
  if (!path || !*path)
    return reject(e, "Player initialization path rejected");
  FILE *f = std::fopen(path, "rb");
  if (!f)
    return reject(e, "Cannot open Player initialization");
  if (std::fseek(f, 0, SEEK_END)) {
    std::fclose(f);
    return reject(e, "Player initialization seek rejected");
  }
  auto n = std::ftell(f);
  if (n < 128 || n > 32 * 1024 * 1024 || std::fseek(f, 0, SEEK_SET)) {
    std::fclose(f);
    return reject(e, "Player initialization size rejected");
  }
  std::vector<uint8_t> b(size_t(n), 0);
  auto got = std::fread(b.data(), 1, b.size(), f);
  auto close = std::fclose(f);
  if (got != b.size() || close)
    return reject(e, "Player initialization read rejected");
  return load(b.data(), b.size(), ctor, e);
}
} // namespace encore::upstream
