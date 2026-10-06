#include "encore/house_status_effects.hpp"
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
uint32_t u32(const uint8_t *p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 |
         uint32_t(p[3]) << 24;
}
uint32_t crc(const uint8_t *p, size_t n) {
  uint32_t c = ~0u;
  while (n--) {
    c ^= *p++;
    for (unsigned i = 0; i < 8; ++i)
      c = (c >> 1) ^ (0xedb88320u & uint32_t(-int(c & 1)));
  }
  return ~c;
}
struct Reader {
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
  uint32_t count(uint32_t limit) {
    auto v = u();
    if (v > limit)
      ok = false;
    return ok ? v : 0;
  }
  std::array<uint8_t, 32> hash() {
    std::array<uint8_t, 32> h{};
    if (n < 32) {
      ok = false;
      return h;
    }
    std::copy(p, p + 32, h.begin());
    p += 32;
    n -= 32;
    if (std::all_of(h.begin(), h.end(), [](uint8_t c) { return !c; }))
      ok = false;
    return h;
  }
  std::string text() {
    auto k = u();
    if (k > 4096 || k > n) {
      ok = false;
      return {};
    }
    std::string s(reinterpret_cast<const char *>(p), k);
    p += k;
    n -= k;
    size_t z = 0;
    if (s.find('\0') != s.npos || !encore::utf8_count(s, z))
      ok = false;
    return s;
  }
};

std::shared_ptr<GlobalYamlValue> value(Reader &r, uint32_t depth,
                                       uint32_t &budget) {
  if (depth > 32 || !budget--) {
    r.ok = false;
    return {};
  }
  auto v = std::make_shared<GlobalYamlValue>();
  v->kind = r.u();
  switch (v->kind) {
  case 0:
    break;
  case 1: {
    auto b = r.u();
    if (b > 1)
      r.ok = false;
    v->boolean = b;
    break;
  }
  case 2: {
    if (r.n < 8) {
      r.ok = false;
      return {};
    };
    uint64_t bits = uint64_t(u32(r.p)) | uint64_t(u32(r.p + 4)) << 32;
    std::memcpy(&v->integer, &bits, 8);
    r.p += 8;
    r.n -= 8;
    break;
  }
  case 4:
    v->string = r.text();
    break;
  case 5: {
    auto n = r.count(128);
    for (uint32_t i = 0; i < n && r.ok; ++i)
      v->array.push_back(value(r, depth + 1, budget));
    break;
  }
  case 6: {
    auto n = r.count(128);
    std::set<std::string> keys;
    for (uint32_t i = 0; i < n && r.ok; ++i) {
      auto k = r.text();
      if (k.empty() || !keys.insert(k).second)
        r.ok = false;
      auto x = value(r, depth + 1, budget);
      v->dictionary.emplace_back(k, x);
    }
    break;
  }
  default:
    r.ok = false;
  }
  return r.ok ? v : nullptr;
}

} // namespace
bool HouseStatusEffectsData::load(const uint8_t *p, size_t n,
                                  const HouseGlobalBridgeData &bridge,
                                  const PlayerReadyData &ready,
                                  std::string &e) {
  if (!p || n < 128 || n > 65536 || !bridge.valid() || !ready.valid() ||
      !bridge.characters() || std::memcmp(p, "ENCHSE01", 8) ||
      u32(p + 8) != 1 || u32(p + 12) != 128 || u32(p + 16) != n ||
      u32(p + 20) != crc(p + 128, n - 128) || u32(p + 24) != 0x454e0062 ||
      u32(p + 28) != 1 || u32(p + 32) != 1 || u32(p + 124))
    return fail(e,
                "Status effects format/version/capability/rules/CRC rejected");
  HouseStatusEffectsData d;
  d.identity_.scene_id = u32(p + 36);
  std::copy(p + 40, p + 60, d.identity_.upstream_commit.begin());
  std::copy(p + 60, p + 92, d.identity_.source_sha256.begin());
  std::copy(p + 92, p + 124, d.ir_.begin());
  const auto &chars = *bridge.characters();
  std::array<uint8_t, 32> character{};
  if (!d.identity_.scene_id ||
      d.identity_.upstream_commit != bridge.identity().upstream_commit ||
      d.identity_.upstream_commit != ready.identity().upstream_commit ||
      !chars.source_hash(chars.source_bindings().character_script, character) ||
      character != d.identity_.source_sha256)
    return fail(e,
                "Status effects original Character source identity rejected");
  Reader r{p + 128, n - 128};
  auto bh = r.hash(), rh = r.hash();
  if (bh != bridge.ir_sha256() || rh != ready.ir_sha256())
    return fail(e, "Status effects dependency differs");
  d.effects_ = r.text();
  d.any_ = r.text();
  d.sweat_ = r.text();
  d.incap_ = r.text();
  if (d.effects_.empty() || d.any_.empty() ||
      d.sweat_ != ready.binding(PlayerReadyBinding::SweatEffect) ||
      d.incap_.empty() || d.sweat_ == d.incap_)
    return fail(e, "Status effects source boolean query binding rejected");
  auto count = r.count(32);
  if (count != bridge.status().policies.size() || !count)
    return fail(e, "Status effects status scope differs");
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    HouseStatusEffectsPolicy v;
    v.id = r.text();
    v.source = r.text();
    v.sha = r.hash();
    uint32_t budget = 4096;
    v.expected = value(r, 0, budget);
    const auto &b = bridge.status().policies[i];
    if (v.id != b.id || v.source != b.source || v.sha != b.sha || !v.expected ||
        v.expected->kind != 6)
      return fail(e, "Status effects source policy/value rejected");
    auto cases = v.expected->get(d.effects_);
    if (!cases || cases->kind != 6 || cases->dictionary.size() != 1 ||
        cases->dictionary.front().first != d.any_ ||
        !cases->dictionary.front().second ||
        cases->dictionary.front().second->kind != 6)
      return fail(e, "Status effects unknown source condition");
    for (auto role :
         {HouseStatusBoolean::Sweat, HouseStatusBoolean::Incapacitated}) {
      auto x = cases->dictionary.front().second->get(d.query(role));
      if (x && x->kind != 1)
        return fail(e, "Status effects unsupported source query type");
    }
    d.policies_.push_back(v);
  }
  std::map<std::string, std::array<uint8_t, 32>> sources;
  count = r.count(32);
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    auto path = r.text();
    auto h = r.hash();
    if (path.empty() || !sources.emplace(path, h).second)
      return fail(e, "Status effects duplicate source proof");
  }
  auto proof = [&](const std::string &s, const std::array<uint8_t, 32> &h) {
    auto i = sources.find(s);
    return i != sources.end() && i->second == h;
  };
  std::array<uint8_t, 32> status{};
  if (!r.ok || r.n ||
      !proof(chars.source_bindings().character_script, character) ||
      !bridge.source_hash(bridge.status().script, status) ||
      !proof(bridge.status().script, status))
    return fail(e, "Status effects source proof/trailing values rejected");
  for (const auto &v : d.policies_)
    if (!proof(v.source, v.sha))
      return fail(e, "Status effects status source proof absent");
  d.valid_ = true;
  d.bridge_ = &bridge;
  d.ready_ = &ready;
  *this = std::move(d);
  e.clear();
  return true;
}
bool HouseStatusEffectsData::load_file(const char *path,
                                       const HouseGlobalBridgeData &bridge,
                                       const PlayerReadyData &ready,
                                       std::string &e) {
  if (!path)
    return fail(e, "Status effects path absent");
  FILE *f = std::fopen(path, "rb");
  if (!f)
    return fail(e, "Cannot open Status effects resource");
  if (std::fseek(f, 0, SEEK_END)) {
    std::fclose(f);
    return fail(e, "Status effects seek failed");
  }
  auto n = std::ftell(f);
  if (n < 128 || n > 65536 || std::fseek(f, 0, SEEK_SET)) {
    std::fclose(f);
    return fail(e, "Status effects length rejected");
  }
  std::vector<uint8_t> b(size_t(n), 0);
  bool ok = std::fread(b.data(), 1, b.size(), f) == b.size();
  ok = std::fclose(f) == 0 && ok;
  return ok ? load(b.data(), b.size(), bridge, ready, e)
            : fail(e, "Status effects read failed");
}
} // namespace encore::upstream
