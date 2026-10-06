#include "encore/global_ready.hpp"
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
bool nonzero(const std::array<uint8_t, 32> &h) {
  return std::any_of(h.begin(), h.end(), [](uint8_t x) { return x != 0; });
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
  uint32_t count(uint32_t max) {
    auto v = u();
    if (v > max)
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
    if (!nonzero(h))
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
    size_t chars = 0;
    if (s.find('\0') != s.npos || !encore::utf8_count(s, chars))
      ok = false;
    return s;
  }
};
bool native_path(const std::string &s) {
  return s.rfind("sdmc:/", 0) == 0 && s.find("..") == s.npos &&
         s.find('\\') == s.npos && s.size() > 6;
}
} // namespace
bool GlobalReadyData::load(const uint8_t *b, size_t n,
                           const FieldGlobalConstructorData &ctor,
                           const PlayerInitializationData &player,
                           const GlobalLoadData &cold, std::string &e) {
  if (!ctor.valid() || !player.valid() || !cold.valid() || !b || n < 128 ||
      n > 65536 || std::memcmp(b, "ENCGRED1", 8) || u32(b + 8) != 1 ||
      u32(b + 12) != 128 || u32(b + 16) != n ||
      u32(b + 20) != crc(b + 128, n - 128) || u32(b + 24) != 0x454e005d ||
      u32(b + 28) != 1 || u32(b + 32) != 1 || u32(b + 124))
    return fail(e, "Global Ready format/version/capability/rules/CRC rejected");
  GlobalReadyData d;
  d.identity_.scene_id = u32(b + 36);
  std::copy(b + 40, b + 60, d.identity_.upstream_commit.begin());
  std::copy(b + 60, b + 92, d.identity_.source_sha256.begin());
  std::copy(b + 92, b + 124, d.ir_.begin());
  if (d.identity_.scene_id != ctor.identity().scene_id ||
      d.identity_.upstream_commit != ctor.identity().upstream_commit ||
      d.identity_.source_sha256 != ctor.identity().source_sha256 ||
      d.identity_.upstream_commit != player.identity().upstream_commit ||
      d.identity_.upstream_commit != cold.identity().upstream_commit ||
      !nonzero(d.ir_))
    return fail(e, "Global Ready actual source identity rejected");
  Reader r{b + 128, n - 128};
  d.constructor_ = r.hash();
  d.player_ = r.hash();
  d.load_ = r.hash();
  auto owner = r.text();
  auto &file = d.settings_file_;
  file.identity = d.identity_;
  file.stable_id = r.u();
  file.name = r.text();
  file.native_class = r.text();
  file.source = file.script = owner;
  file.source_sha = file.script_sha = d.identity_.source_sha256;
  file.role = 5;
  if (d.constructor_ != ctor.ir_sha256() || d.player_ != player.ir_sha256() ||
      d.load_ != cold.ir_sha256() || owner != ctor.owner_source() ||
      owner != player.owner_source() || owner != cold.owner_source())
    return fail(e, "Global Ready independent owner dependencies differ");
  auto k = r.count(64);
  std::set<std::string> sources;
  bool saw_owner = false;
  while (k-- && r.ok) {
    auto path = r.text();
    auto h = r.hash();
    std::array<uint8_t, 32> a{}, p{}, l{};
    if (!sources.insert(path).second || !ctor.source_hash(path, a) ||
        !player.source_hash(path, p) || !cold.source_hash(path, l) || h != a ||
        h != p || h != l)
      r.ok = false;
    if (path == owner && h == d.identity_.source_sha256)
      saw_owner = true;
  }
  auto &policy = d.policy_;
  policy.party_space = r.u();
  auto encrypted = r.u();
  policy.settings_source_path = r.text();
  policy.settings_native_path = r.text();
  policy.locale_preference_path = r.text();
  policy.slot_preference_path = r.text();
  policy.source_language_default = r.text();
  policy.source_save_slot_member = r.text();
  policy.input_path = r.text();
  policy.input_sha = r.hash();
  policy.input_bytes = r.u();
  policy.input_crc = r.u();
  k = r.count(64);
  std::set<std::string> languages;
  while (k-- && r.ok) {
    auto s = r.text();
    if (s.empty() || !languages.insert(s).second)
      r.ok = false;
    policy.languages.push_back(std::move(s));
  }
  k = r.count(6);
  while (k-- && r.ok) {
    auto v = r.u();
    auto s = r.text();
    if (v != policy.steps.size() + 1)
      r.ok = false;
    policy.steps.push_back(GlobalReadyStep(v));
    policy.source_steps.push_back(std::move(s));
  }
  const FieldGlobalConstructorField *default_language = nullptr;
  const FieldGlobalConstructorField *source_languages = nullptr;
  // Constant semantics are admitted by the source constructor producer; find
  // the unique matching typed values rather than content names in executor
  // code.
  for (const auto &c : ctor.constants()) {
    if (c.kind == FieldGlobalLiteralKind::String &&
        c.string_value == policy.source_language_default)
      default_language = &c;
    if (c.kind == FieldGlobalLiteralKind::StringArray &&
        c.strings == policy.languages)
      source_languages = &c;
  }
  if (!r.ok || r.n || !saw_owner || !file.stable_id || file.name.empty() ||
      file.native_class != "File" || encrypted || policy.party_space == 0 ||
      policy.party_space > 4096 || !default_language || !source_languages ||
      !languages.count(policy.source_language_default) ||
      policy.source_steps != ctor.ready_steps() || policy.steps.size() != 6 ||
      policy.settings_source_path.rfind("user://", 0) != 0 ||
      !native_path(policy.settings_native_path) ||
      !native_path(policy.locale_preference_path) ||
      !native_path(policy.slot_preference_path) ||
      policy.settings_native_path == policy.locale_preference_path ||
      policy.settings_native_path == policy.slot_preference_path ||
      policy.locale_preference_path == policy.slot_preference_path ||
      policy.input_path.rfind("data/", 0) != 0 ||
      policy.input_path.find("..") != policy.input_path.npos ||
      policy.input_path.find('\\') != policy.input_path.npos ||
      policy.input_bytes < 32 || policy.input_bytes > 65536 ||
      policy.source_save_slot_member.empty())
    return fail(e, "Global Ready source policy/order/metadata rejected");
  // The source text is part of the independently admitted constructor pack.
  auto resize = policy.source_steps[2];
  const auto *member = ctor.member(FieldGlobalMemberRole::PartySpace);
  if (!member || resize != member->name + ".resize(" +
                               std::to_string(policy.party_space) + ")")
    return fail(e, "Global Ready source partySpace count differs");
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
bool GlobalReadyData::load_file(const char *path,
                                const FieldGlobalConstructorData &ctor,
                                const PlayerInitializationData &player,
                                const GlobalLoadData &cold, std::string &e) {
  auto *f = path ? std::fopen(path, "rb") : nullptr;
  if (!f)
    return fail(e, "Cannot open Global Ready resource");
  if (std::fseek(f, 0, SEEK_END)) {
    std::fclose(f);
    return fail(e, "Global Ready seek failed");
  }
  long size = std::ftell(f);
  if (size < 128 || size > 65536 || std::fseek(f, 0, SEEK_SET)) {
    std::fclose(f);
    return fail(e, "Global Ready file size rejected");
  }
  std::vector<uint8_t> b(size_t(size), uint8_t{});
  auto read = std::fread(b.data(), 1, b.size(), f);
  bool bad = std::ferror(f) != 0;
  bool close = std::fclose(f) != 0;
  if (read != b.size() || bad || close)
    return fail(e, "Global Ready resource read failed");
  return load(b.data(), b.size(), ctor, player, cold, e);
}
} // namespace encore::upstream
