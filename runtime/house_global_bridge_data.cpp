#include "encore/house_global_bridge.hpp"
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
} // namespace
bool HouseGlobalBridgeData::load(const uint8_t *p, size_t n,
                                 const FieldCharacterLoadData &chars,
                                 const GlobalLoadData &cold,
                                 const NativeSessionData &session,
                                 std::string &e) {
  if (!p || n < 128 || n > 1024 * 1024 || !chars.valid() || !cold.valid() ||
      !session.valid() || std::memcmp(p, "ENCHGB01", 8) || u32(p + 8) != 1 ||
      u32(p + 12) != 128 || u32(p + 16) != n ||
      u32(p + 20) != crc(p + 128, n - 128) || u32(p + 24) != 0x454e0060 ||
      u32(p + 28) != 1 || u32(p + 32) != 1 || u32(p + 124))
    return fail(
        e, "House continuation format/version/capability/rules/CRC rejected");
  HouseGlobalBridgeData d;
  d.identity_.scene_id = u32(p + 36);
  std::copy(p + 40, p + 60, d.identity_.upstream_commit.begin());
  std::copy(p + 60, p + 92, d.identity_.source_sha256.begin());
  std::copy(p + 92, p + 124, d.ir_.begin());
  if (!d.identity_.scene_id ||
      d.identity_.upstream_commit != chars.identity().upstream_commit ||
      d.identity_.upstream_commit != cold.identity().upstream_commit ||
      d.identity_.source_sha256 != cold.identity().source_sha256)
    return fail(e, "House continuation source identity rejected");
  Reader r{p + 128, n - 128};
  d.character_ir_ = r.hash();
  d.load_ir_ = r.hash();
  d.definition_ir_ = r.hash();
  d.session_ir_ = r.hash();
  if (d.character_ir_ != chars.ir_sha256() || d.load_ir_ != cold.ir_sha256())
    return fail(e, "House continuation source loader dependency rejected");
  d.leader_ = r.text();
  d.declaration_ = r.u();
  auto c = std::find_if(
      chars.rows().begin(), chars.rows().end(), [&](const auto &v) {
        return v.id == d.declaration_ && v.role == 0 && v.name == d.leader_;
      });
  if (c == chars.rows().end() || session.defaults().party.size() != 1 ||
      session.leader_id() != d.leader_)
    return fail(e, "House continuation actual character declaration rejected");
  auto k = r.count(64);
  for (uint32_t i = 0; i < k; ++i) {
    HouseGlobalAssignment a;
    a.role = r.u();
    a.index = r.u();
    a.kind = r.u();
    a.member_kind = r.u();
    a.adapter = r.u();
    a.member = r.text();
    a.key = r.text();
    if (a.role != i + 1 || a.index >= cold.assignments().size() ||
        a.member_kind > 7 || a.adapter > 6)
      return fail(e, "House continuation unknown/reordered field mapping");
    const auto &b = cold.assignments()[a.index];
    if (a.member != b.member || a.key != b.key || a.kind != b.kind)
      return fail(e, "House continuation source SAVE/LOAD mapping differs");
    d.assignments_.push_back(a);
  }
  if (d.assignments_.size() != 19)
    return fail(e, "House continuation execution field schema incomplete");
  auto &s = d.status_;
  s.id = r.u();
  s.times = r.u();
  s.turns = r.u();
  s.script = r.text();
  s.native = r.text();
  s.getter = r.text();
  s.ailment = r.text();
  s.turns_field = r.text();
  s.times_field = r.text();
  s.probability = r.text();
  s.healing_key = r.text();
  s.passive_key = r.text();
  s.probability_key = r.text();
  if (!s.id || s.native != "Node" || s.script.empty() || s.getter.empty() ||
      !s.times || s.turns || s.ailment.empty() || s.turns_field.empty() ||
      s.times_field.empty() || s.probability.empty() || s.healing_key.empty() ||
      s.passive_key.empty() || s.probability_key.empty())
    return fail(e, "House continuation Status native/default schema rejected");
  k = r.count(64);
  std::set<std::string> names;
  for (uint32_t i = 0; i < k; ++i) {
    HouseGlobalStatusPolicy a;
    a.id = r.text();
    a.source = r.text();
    a.sha = r.hash();
    auto b = r.u();
    a.priority = int32_t(r.u());
    a.probability = int32_t(r.u());
    if (b > 1 || a.id.empty() || !names.insert(a.id).second ||
        a.probability < 0 || a.probability > 100)
      return fail(e, "House continuation Status policy rejected");
    a.passive = b != 0;
    auto policy =
        std::find_if(session.status_policies().begin(),
                     session.status_policies().end(), [&](const auto &x) {
                       return x.id == a.id && x.passive_healing == a.passive;
                     });
    if (policy == session.status_policies().end())
      return fail(e, "House continuation unreviewed native Status");
    s.policies.push_back(a);
  }
  if (s.policies.size() != session.status_policies().size())
    return fail(e, "House continuation Status scope incomplete");
  k = r.count(4096);
  for (uint32_t i = 0; i < k; ++i) {
    auto path = r.text();
    auto h = r.hash();
    if (path.empty() || path.find("..") != path.npos ||
        !d.sources_.emplace(path, h).second)
      return fail(e, "House continuation duplicate/invalid source path");
    std::array<uint8_t, 32> x{};
    if ((chars.source_hash(path, x) || cold.source_hash(path, x)) && x != h)
      return fail(e, "House continuation source proof differs");
  }
  for (const auto &a : s.policies) {
    auto h = d.sources_.find(a.source);
    if (h == d.sources_.end() || h->second != a.sha)
      return fail(e, "House continuation Status YAML proof missing");
  }
  if (!r.ok || r.n || !d.sources_.count(s.script))
    return fail(e, "House continuation malformed/trailing binary");
  d.chars_ = &chars;
  d.cold_ = &cold;
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
bool HouseGlobalBridgeData::source_hash(std::string_view p,
                                        std::array<uint8_t, 32> &h) const {
  auto i = sources_.find(std::string(p));
  if (!valid_ || i == sources_.end())
    return false;
  h = i->second;
  return true;
}
bool HouseGlobalBridgeData::load_file(const char *path,
                                      const FieldCharacterLoadData &c,
                                      const GlobalLoadData &l,
                                      const NativeSessionData &s,
                                      std::string &e) {
  if (!path)
    return fail(e, "House continuation path absent");
  FILE *f = std::fopen(path, "rb");
  if (!f)
    return fail(e, "Cannot open House continuation resource");
  if (std::fseek(f, 0, SEEK_END)) {
    std::fclose(f);
    return fail(e, "House continuation seek failed");
  }
  auto n = std::ftell(f);
  if (n < 128 || n > 1024 * 1024 || std::fseek(f, 0, SEEK_SET)) {
    std::fclose(f);
    return fail(e, "House continuation resource length rejected");
  }
  std::vector<uint8_t> b(size_t(n), 0);
  bool ok = std::fread(b.data(), 1, b.size(), f) == b.size();
  ok = std::fclose(f) == 0 && ok;
  return ok ? load(b.data(), b.size(), c, l, s, e)
            : fail(e, "House continuation resource read failed");
}
} // namespace encore::upstream
