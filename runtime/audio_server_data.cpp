#include "encore/audio_data.hpp"
#include "encore/audio_server.hpp"
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
  bool boolean() {
    auto v = integer();
    if (v > 1)
      ok = false;
    return v != 0;
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
    auto l = integer();
    if (!ok || l > 2048 || at > n || l > n - at) {
      ok = false;
      return {};
    }
    std::string s(reinterpret_cast<const char *>(p + at), l);
    at += l;
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
    std::array<uint8_t, 32> h{};
    if (!ok || at > n || n - at < 32) {
      ok = false;
      return h;
    }
    std::copy(p + at, p + at + 32, h.begin());
    at += 32;
    if (std::all_of(h.begin(), h.end(), [](auto x) { return !x; }))
      ok = false;
    return h;
  }
};
bool path(const std::string &p) {
  return !p.empty() && p.front() != '/' && p.find("..") == p.npos &&
         p.find(':') == p.npos && p.find('\\') == p.npos;
}
} // namespace
bool AudioServerData::source_hash(std::string_view p,
                                  std::array<uint8_t, 32> &h) const {
  auto i = sources_.find(std::string(p));
  if (!valid_ || i == sources_.end())
    return false;
  h = i->second;
  return true;
}
bool AudioServerData::load_file(const char *p, std::string &e) {
  if (!p) {
    e = "AudioServer source pack path absent";
    return false;
  }
  std::ifstream f(p, std::ios::binary);
  if (!f) {
    e = "AudioServer source pack unavailable";
    return false;
  }
  f.seekg(0, std::ios::end);
  auto n = f.tellg();
  if (n < 128 || n > 1024 * 1024) {
    e = "AudioServer source pack length rejected";
    return false;
  }
  f.seekg(0);
  std::vector<uint8_t> b(static_cast<size_t>(n));
  if (!f.read(reinterpret_cast<char *>(b.data()), n)) {
    e = "AudioServer source pack read failed";
    return false;
  }
  return load(b.data(), b.size(), e);
}
bool AudioServerData::load(const uint8_t *p, size_t n, std::string &e) {
  auto fail = [&](const char *m) {
    e = m;
    return false;
  };
  if (!p || n < 128 || n > 1024 * 1024 || std::memcmp(p, "ENCBUS01", 8) ||
      u(p + 8) != 1 || u(p + 12) != 128 || u(p + 16) != n ||
      u(p + 20) != audio_crc32(p + 128, n - 128) || u(p + 24) != 0x454e006e ||
      u(p + 28) != 1 || u(p + 32) != 1 || !u(p + 36) || u(p + 124))
    return fail("AudioServer header/CRC/version/capability rejected");
  AudioServerData d;
  d.identity_.scene_id = u(p + 36);
  std::copy(p + 40, p + 60, d.identity_.upstream_commit.begin());
  std::copy(p + 60, p + 92, d.identity_.source_sha256.begin());
  std::copy(p + 92, p + 124, d.ir_.begin());
  if (std::all_of(d.ir_.begin(), d.ir_.end(), [](auto x) { return !x; }) ||
      std::all_of(d.identity_.source_sha256.begin(),
                  d.identity_.source_sha256.end(), [](auto x) { return !x; }))
    return fail("AudioServer empty source/IR proof");
  Reader r{p, n};
  d.engine_ = r.text();
  d.signal_ = r.text();
  d.method_ = r.text();
  auto proofs = r.integer();
  if (!path(d.engine_) || d.signal_.empty() || d.method_.empty() || !proofs ||
      proofs > 64)
    return fail("AudioServer source closure bounds");
  for (uint32_t i = 0; i < proofs; ++i) {
    auto name = r.text();
    auto h = r.hash();
    if (!path(name) || !d.sources_.emplace(name, h).second)
      return fail("AudioServer source proof duplicate/path");
  }
  if (!d.sources_.count(d.engine_) ||
      d.sources_.at(d.engine_) != d.identity_.source_sha256)
    return fail("AudioServer engine source identity mismatch");
  auto count = r.integer();
  if (!count || count > 256)
    return fail("AudioServer bus count rejected");
  std::set<std::string> names;
  for (uint32_t i = 0; i < count; ++i) {
    AudioServerBus b;
    b.name = r.text();
    b.send = r.text();
    b.volume_db = r.scalar();
    b.solo = r.boolean();
    b.muted = r.boolean();
    b.bypass = r.boolean();
    auto effects = r.integer();
    if (!r.ok || b.name.empty() || !names.insert(b.name).second ||
        effects > 64 || (i == 0 && !b.send.empty()))
      return fail("AudioServer bus source fields rejected");
    for (uint32_t j = 0; j < effects; ++j) {
      AudioServerEffect fx;
      fx.source_id = r.integer();
      fx.native_class = r.text();
      fx.resource_name = r.text();
      fx.cutoff = r.scalar();
      fx.resonance = r.scalar();
      fx.enabled = r.boolean();
      if (!r.ok || !fx.source_id || fx.native_class != "AudioEffectFilter" ||
          fx.cutoff <= 0 || fx.resonance <= 0)
        return fail("AudioServer effect source capability rejected");
      b.effects.push_back(std::move(fx));
    }
    d.buses_.push_back(std::move(b));
  }
  auto settings = r.integer();
  if (settings > 64)
    return fail("AudioServer settings source count");
  std::set<std::string> methods;
  for (uint32_t i = 0; i < settings; ++i) {
    AudioServerSetting s;
    s.method = r.text();
    s.member = r.text();
    s.bus = r.text();
    s.signal = r.text();
    if (s.method.empty() || s.member.empty() || s.signal.empty() ||
        !names.count(s.bus) || !methods.insert(s.method).second)
      return fail("AudioServer settings source binding");
    d.settings_.push_back(std::move(s));
  }
  if (!r.ok || r.at != n)
    return fail("AudioServer malformed/trailing source fields");
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
} // namespace encore::upstream
