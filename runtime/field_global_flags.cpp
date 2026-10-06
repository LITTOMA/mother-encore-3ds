#include "encore/field_global_flags.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <set>
namespace encore::upstream {
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
bool key(std::string_view s) {
  size_t count = 0;
  return s.size() <= 4096 && s.find('\0') == s.npos &&
         encore::utf8_count(s, count);
}
bool valid(const FieldFlagDictionary &d) {
  if (d.size() > 16384)
    return false;
  std::set<std::string> keys;
  for (const auto &row : d)
    if (!key(row.first) || !keys.insert(row.first).second)
      return false;
  return true;
}
auto find(FieldFlagDictionary &d, std::string_view s) {
  return std::find_if(d.begin(), d.end(),
                      [&](const auto &row) { return row.first == s; });
}
auto find(const FieldFlagDictionary &d, std::string_view s) {
  return std::find_if(d.begin(), d.end(),
                      [&](const auto &row) { return row.first == s; });
}
uint32_t u(const uint8_t *p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 |
         uint32_t(p[3]) << 24;
}
uint32_t crc(const uint8_t *p, size_t n) {
  uint32_t c = ~0u;
  for (size_t i = 0; i < n; ++i) {
    c ^= p[i];
    for (unsigned j = 0; j < 8; ++j)
      c = (c >> 1) ^ (0xedb88320u & uint32_t(-int(c & 1)));
  }
  return ~c;
}
void word(std::vector<uint8_t> &out, uint32_t v) {
  for (unsigned i = 0; i < 4; ++i)
    out.push_back(uint8_t(v >> (i * 8)));
}
void dictionary(std::vector<uint8_t> &out, const FieldFlagDictionary &d) {
  word(out, uint32_t(d.size()));
  for (const auto &row : d) {
    word(out, uint32_t(row.first.size()));
    out.insert(out.end(), row.first.begin(), row.first.end());
    word(out, row.second);
  }
}
} // namespace
bool FieldGlobalFlagsRuntime::available(std::string &e) const {
  if (!data_ || poisoned_ || !data_->valid() || data_->pin() != admitted_pin_ ||
      data_->content_hash() != admitted_content_)
    return fail(e, "Global flags source owner unavailable");
  e.clear();
  return true;
}
bool FieldGlobalFlagsRuntime::initialize(const FieldGlobalFlagsData &d,
                                         const FieldGlobalExternalSpec &owner,
                                         Emit emit, std::string &e) {
  std::array<uint8_t, 32> source{};
  if (!owner.stable_id || owner.identity.upstream_commit != d.pin() ||
      owner.source != d.owner_source() || owner.script != owner.source ||
      !d.source_hash(owner.source, source) || owner.source_sha != source ||
      owner.script_sha != source)
    return fail(e, "Global flags actual source autoload binding rejected");
  if (data_ || !d.valid() || !emit)
    return fail(e, "Global flags actual emission owner missing/reinitialized");
  data_ = &d;
  admitted_pin_ = d.pin();
  admitted_content_ = d.content_hash();
  emit_ = std::move(emit);
  state_.normal = d.constructor();
  state_.objects.clear();
  state_.seen.clear();
  revision_ = 0;
  e.clear();
  return true;
}
bool FieldGlobalFlagsRuntime::read(bool object, std::string_view s,
                                   bool &present, bool &value,
                                   std::string &e) const {
  if (!available(e) || !key(s))
    return fail(e, "Global flags read key rejected");
  const auto &d = object ? state_.objects : state_.normal;
  auto i = find(d, s);
  present = i != d.end();
  value = present && i->second;
  return true;
}
bool FieldGlobalFlagsRuntime::seen(std::string_view s, bool &v,
                                   std::string &e) const {
  if (!available(e) || !key(s))
    return fail(e, "Global seen-dialogue key rejected");
  auto i = find(state_.seen, s);
  v = i != state_.seen.end() && i->second;
  return true;
}
bool FieldGlobalFlagsRuntime::assign(FieldFlagDictionary &d, std::string_view s,
                                     bool v, std::string &e) {
  if (!available(e) || !key(s) ||
      revision_ == std::numeric_limits<uint64_t>::max())
    return fail(e, "Global flags mutation key/revision rejected");
  auto i = find(d, s);
  if (i != d.end())
    i->second = v;
  else {
    if (d.size() >= 16384)
      return fail(e, "Global flag dictionary capacity exceeded");
    d.emplace_back(s, v);
  }
  ++revision_;
  return true;
}
bool FieldGlobalFlagsRuntime::mark_seen(std::string_view s, std::string &e) {
  if (s.empty())
    return fail(e, "Actual NPC dialogue hash absent");
  return assign(state_.seen, s, true, e);
}
bool FieldGlobalFlagsRuntime::write(bool object, std::string_view s, bool v,
                                    std::string &e) {
  if (!available(e) || !key(s))
    return fail(e, "Global flags write key rejected");
  if (!object && find(state_.normal, s) == state_.normal.end())
    return true;
  return assign(object ? state_.objects : state_.normal, s, v, e);
}
bool FieldGlobalFlagsRuntime::emit(std::string &e) {
  if (!available(e))
    return false;
  if (!emit_(e)) {
    poisoned_ = true;
    if (e.empty())
      e = "Actual global flags_updated signal rejected";
    return false;
  }
  return true;
}
bool FieldGlobalFlagsRuntime::set_normal(std::string_view s, bool v,
                                         bool notify, std::string &e) {
  if (!available(e) || !key(s))
    return fail(e, "Global normal setter key rejected");
  if (find(state_.normal, s) == state_.normal.end())
    return true;
  return write(false, s, v, e) && (!notify || emit(e));
}
bool FieldGlobalFlagsRuntime::set_object(std::string_view s, bool v,
                                         bool notify, std::string &e) {
  return write(true, s, v, e) && (!notify || emit(e));
}
bool FieldGlobalFlagsRuntime::load_source(const FieldFlagProjection &source,
                                          std::string &e) {
  if (!available(e) || !valid(source.normal) || !valid(source.objects) ||
      !valid(source.seen) || revision_ == std::numeric_limits<uint64_t>::max())
    return fail(e, "Source saved flag dictionaries rejected");
  FieldFlagProjection next;
  next.normal = data_->constructor();
  for (auto &row : next.normal) {
    auto i = find(source.normal, row.first);
    row.second = i != source.normal.end() && i->second;
  }
  next.objects = source.objects;
  next.seen = source.seen;
  state_ = std::move(next);
  ++revision_;
  return true;
}
bool FieldGlobalFlagsRuntime::load_profile(size_t profile, std::string &e) {
  if (!available(e) || profile >= data_->profiles().size())
    return fail(e, "Unknown checked source save profile");
  return load_source(data_->profiles()[profile].flags, e);
}
bool FieldGlobalFlagsRuntime::encode_save(std::vector<uint8_t> &out,
                                          std::string &e) const {
  if (!available(e))
    return false;
  std::vector<uint8_t> body;
  body.insert(body.end(), data_->pin().begin(), data_->pin().end());
  body.insert(body.end(), data_->content_hash().begin(),
              data_->content_hash().end());
  dictionary(body, state_.normal);
  dictionary(body, state_.objects);
  dictionary(body, state_.seen);
  if (body.size() > 16 * 1024 * 1024 - 32)
    return fail(e, "Global flags save projection too large");
  std::vector<uint8_t> candidate{'E', 'N', 'C', 'F', 'F', 'S', 'V', '1'};
  word(candidate, 1);
  word(candidate, uint32_t(32 + body.size()));
  word(candidate, crc(body.data(), body.size()));
  word(candidate, 0x454e0047);
  word(candidate, 1);
  word(candidate, 1);
  candidate.insert(candidate.end(), body.begin(), body.end());
  out = std::move(candidate);
  return true;
}
bool FieldGlobalFlagsRuntime::restore_save(const uint8_t *p, size_t n,
                                           std::string &e) {
  if (!available(e) || !p || n < 96 || n > 16 * 1024 * 1024 ||
      std::memcmp(p, "ENCFFSV1", 8) || u(p + 8) != 1 || u(p + 12) != n ||
      u(p + 16) != crc(p + 32, n - 32) || u(p + 20) != 0x454e0047 ||
      u(p + 24) != 1 || u(p + 28) != 1 ||
      !std::equal(data_->pin().begin(), data_->pin().end(), p + 32) ||
      !std::equal(data_->content_hash().begin(), data_->content_hash().end(),
                  p + 52))
    return fail(e, "Global flags save projection version/identity rejected");
  size_t at = 84;
  bool ok = true;
  auto get = [&]() -> uint32_t {
    if (!ok || at > n || n - at < 4) {
      ok = false;
      return 0;
    }
    auto v = u(p + at);
    at += 4;
    return v;
  };
  auto dict = [&]() {
    FieldFlagDictionary d;
    auto count = get();
    if (count > 16384) {
      ok = false;
      return d;
    }
    for (uint32_t i = 0; i < count && ok; ++i) {
      auto length = get();
      if (!ok || length > 4096 || at > n || length > n - at) {
        ok = false;
        break;
      }
      std::string s(reinterpret_cast<const char *>(p + at), length);
      at += length;
      auto v = get();
      if (v > 1)
        ok = false;
      d.emplace_back(std::move(s), v != 0);
    }
    if (!valid(d))
      ok = false;
    return d;
  };
  FieldFlagProjection source;
  source.normal = dict();
  source.objects = dict();
  source.seen = dict();
  if (!ok || at != n || source.normal.size() != data_->constructor().size())
    return fail(e, "Global flags saved dictionary structure rejected");
  for (size_t i = 0; i < source.normal.size(); ++i)
    if (source.normal[i].first != data_->constructor()[i].first)
      return fail(e, "Global flags saved registered identity/order rejected");
  return load_source(source, e);
}
} // namespace encore::upstream
