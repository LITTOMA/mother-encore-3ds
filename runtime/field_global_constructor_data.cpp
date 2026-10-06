#include "encore/field_global_constructor.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
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
  uint32_t count(uint32_t max = 4096) {
    auto v = u();
    if (v > max)
      ok = false;
    return ok ? v : 0;
  }
  std::string text() {
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
  double number() {
    uint64_t b = u();
    b |= uint64_t(u()) << 32;
    double v;
    std::memcpy(&v, &b, 8);
    if (!std::isfinite(v))
      ok = false;
    return v;
  }
  float f() {
    auto b = u();
    float v;
    std::memcpy(&v, &b, 4);
    if (!std::isfinite(v))
      ok = false;
    return v;
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
    return h;
  }
  std::vector<std::string> texts() {
    std::vector<std::string> v;
    auto k = count();
    while (k-- && ok)
      v.push_back(text());
    return v;
  }
  bool block(const uint8_t *&out, size_t &len) {
    len = u();
    if (len > n || len < 32) {
      ok = false;
      return false;
    }
    out = p;
    p += len;
    n -= len;
    return true;
  }
  std::vector<FieldGlobalConstructorField> fields(bool roles) {
    std::vector<FieldGlobalConstructorField> v;
    std::set<std::string> names;
    auto k = count();
    while (k-- && ok) {
      FieldGlobalConstructorField f;
      if (roles)
        f.role = u();
      f.name = text();
      f.type_hint = text();
      auto kind = u();
      f.kind = FieldGlobalLiteralKind(kind);
      if (f.name.empty() || !names.insert(f.name).second) {
        ok = false;
        break;
      }
      switch (f.kind) {
      case FieldGlobalLiteralKind::Null:
        break;
      case FieldGlobalLiteralKind::Boolean: {
        auto b = u();
        if (b > 1)
          ok = false;
        f.boolean = b;
        break;
      }
      case FieldGlobalLiteralKind::Integer: {
        uint64_t b = u();
        b |= uint64_t(u()) << 32;
        f.integer = int64_t(b);
        break;
      }
      case FieldGlobalLiteralKind::Number:
        f.number = number();
        break;
      case FieldGlobalLiteralKind::String:
      case FieldGlobalLiteralKind::SourceNode:
        f.string_value = text();
        break;
      case FieldGlobalLiteralKind::StringArray:
        f.strings = texts();
        break;
      case FieldGlobalLiteralKind::Vector:
        f.vector = {number(), number()};
        break;
      default:
        ok = false;
      }
      v.push_back(std::move(f));
    }
    return v;
  }
};
} // namespace
bool FieldGlobalConstructorData::load(const uint8_t *p, size_t n,
                                      const FieldIdentity &expected,
                                      std::string &e) {
  if (!p || n < 128 || n > 4 * 1024 * 1024 || std::memcmp(p, "ENCGCON1", 8) ||
      u32(p + 8) != 1 || u32(p + 12) != 128 || u32(p + 16) != n ||
      u32(p + 20) != crc(p + 128, n - 128) || u32(p + 24) != 0x454e0055 ||
      u32(p + 28) != 1 || u32(p + 32) != 1 ||
      u32(p + 36) != expected.scene_id ||
      std::memcmp(p + 40, expected.upstream_commit.data(), 20) ||
      std::memcmp(p + 60, expected.source_sha256.data(), 32) || u32(p + 124))
    return fail(e, "Global constructor header/identity rejected");
  FieldGlobalConstructorData d;
  d.identity_ = expected;
  std::copy(p + 92, p + 124, d.ir_.begin());
  if (!nonzero(d.ir_))
    return fail(e, "Global constructor source review missing");
  Reader r{p + 128, n - 128};
  d.owner_ = r.text();
  d.scene_ = r.text();
  d.fields_ = r.fields(true);
  d.constants_ = r.fields(false);
  d.signals_ = r.texts();
  auto k = r.count();
  std::set<uint32_t> childids;
  while (k-- && r.ok) {
    FieldGlobalChildFields c;
    c.id = r.u();
    c.script = r.text();
    c.fields = r.fields(false);
    if (!c.id || c.script.empty() || !childids.insert(c.id).second)
      r.ok = false;
    d.children_.push_back(std::move(c));
  }
  auto &t = d.transition_;
  t.script = r.text();
  t.id = r.u();
  t.native_class = r.text();
  t.name = r.text();
  t.script_methods = r.u();
  t.class_index = r.u();
  t.ready = r.u();
  t.pause = r.u();
  t.flags = r.u();
  t.light_mask = r.u();
  t.index = int32_t(r.u());
  t.priority = int32_t(r.u());
  t.z = int32_t(r.u());
  for (auto *a : {&t.local, &t.world})
    for (auto &v : *a)
      v = {r.f(), r.f()};
  for (auto *a : {&t.modulate, &t.self_modulate})
    for (auto &v : *a)
      v = r.f();
  t.path = r.text();
  t.groups = r.texts();
  d.ready_ = r.texts();
  k = r.count();
  while (k-- && r.ok) {
    auto name = r.text();
    auto h = r.hash();
    if (name.empty() || !nonzero(h) || !d.sources_.emplace(name, h).second)
      r.ok = false;
  }
  const uint8_t *raw = nullptr;
  size_t len = 0;
  if (!r.block(raw, len) || len < 128)
    return fail(e, "Global constructor recipe truncated");
  FieldIdentity ri{};
  ri.scene_id = u32(raw + 36);
  std::copy(raw + 40, raw + 60, ri.upstream_commit.begin());
  std::copy(raw + 60, raw + 92, ri.source_sha256.begin());
  if (ri.upstream_commit != expected.upstream_commit ||
      !d.recipe_.load(raw, len, ri, e))
    return false;
  if (!r.block(raw, len) || !d.timers_.load(raw, len, e))
    return false;
  if (!r.ok || r.n || d.owner_.empty() ||
      d.scene_ != d.recipe_.source_scene() || d.fields_.size() != 14 ||
      d.constants_.size() != 8 || d.signals_.size() != 7 ||
      d.children_.size() != 2 || d.ready_.size() != 6 ||
      d.recipe_.records().size() != 4 || d.timers_.records().size() != 1)
    return fail(e, "Global constructor complete source layout rejected");
  std::array<uint8_t, 32> h{};
  if (!d.source_hash(d.owner_, h) || h != expected.source_sha256 ||
      !d.source_hash(d.scene_, h) || h != ri.source_sha256)
    return fail(e, "Global constructor source identities differ");
  for (size_t i = 0; i < d.fields_.size(); ++i) {
    const auto &f = d.fields_[i];
    if (f.role != i + 1)
      return fail(e, "Global constructor role/order rejected");
    auto role = FieldGlobalMemberRole(f.role);
    auto kind = f.kind;
    if (role <= FieldGlobalMemberRole::Persistent) {
      if (kind != FieldGlobalLiteralKind::StringArray || !f.strings.empty() ||
          !f.type_hint.empty())
        return fail(e, "Global constructor Array default rejected");
    } else if (role == FieldGlobalMemberRole::SceneTransition) {
      if (kind != FieldGlobalLiteralKind::SourceNode ||
          f.string_value != t.script)
        return fail(e, "Global constructor Transition owner rejected");
    } else if (role <= FieldGlobalMemberRole::Talker) {
      if (kind != FieldGlobalLiteralKind::Null || f.type_hint.empty())
        return fail(e, "Global constructor Object default rejected");
    } else if (role <= FieldGlobalMemberRole::EnteringDoor) {
      if (kind != FieldGlobalLiteralKind::Boolean)
        return fail(e, "Global constructor boolean default rejected");
    } else if (kind != FieldGlobalLiteralKind::String)
      return fail(e, "Global constructor String default rejected");
  }
  if (!t.id || !t.name.empty() || t.path != "." || t.native_class != "Node" ||
      t.script_methods || t.ready || t.pause || t.flags || t.priority || t.z ||
      t.index != -1 || t.light_mask || !t.groups.empty() ||
      !d.source_hash(t.script, t.script_sha))
    return fail(e, "Global source Node.new descriptor rejected");
  const auto &rr = d.recipe_.records();
  if (rr[0].script != d.owner_ || rr[0].script_sha != expected.source_sha256 ||
      rr[0].native_class != "Node2D" || rr[0].pause != 2 ||
      rr[1].native_class != "Timer" || rr[2].native_class != "Node" ||
      rr[3].native_class != "Node")
    return fail(e, "Global constructor native/script roots rejected");
  for (size_t i = 0; i < d.children_.size(); ++i) {
    const auto &c = d.children_[i];
    const auto *rec = d.recipe_.record(c.id);
    if (!rec || rec->script != c.script || rec->id != rr[i + 2].id ||
        !d.source_hash(c.script, h) || h != rec->script_sha ||
        c.fields.size() != 4)
      return fail(e, "Global constructor child declarations rejected");
  }
  const auto *timer = d.timers_.record(ri, rr[1].id);
  if (!timer || rr[1].script_methods || !rr[1].script.empty())
    return fail(e, "Global constructor Timer binding rejected");
  d.valid_ = true;
  *this = std::move(d);
  return true;
}
bool FieldGlobalConstructorData::load_file(const char *path,
                                           const FieldIdentity &i,
                                           std::string &e) {
  auto *f = std::fopen(path, "rb");
  if (!f)
    return fail(e, "Cannot open global constructor");
  if (std::fseek(f, 0, SEEK_END)) {
    std::fclose(f);
    return fail(e, "Cannot size global constructor");
  }
  auto n = std::ftell(f);
  if (n < 128 || n > 4 * 1024 * 1024) {
    std::fclose(f);
    return fail(e, "Global constructor file size rejected");
  }
  std::rewind(f);
  std::vector<uint8_t> b(size_t(n), uint8_t{});
  bool ok = std::fread(b.data(), 1, b.size(), f) == b.size();
  std::fclose(f);
  return ok ? load(b.data(), b.size(), i, e)
            : fail(e, "Global constructor short read");
}
bool FieldGlobalConstructorData::source_hash(
    std::string_view s, std::array<uint8_t, 32> &out) const {
  auto f = sources_.find(std::string(s));
  if (f == sources_.end())
    return false;
  out = f->second;
  return true;
}
FieldIdentity FieldGlobalConstructorData::transition_identity() const {
  auto i = identity_;
  i.scene_id = transition_.id;
  i.source_sha256 = transition_.script_sha;
  return i;
}
const FieldGlobalConstructorField *
FieldGlobalConstructorData::member(FieldGlobalMemberRole role) const {
  auto n = uint32_t(role);
  return valid_ && n >= 1 && n <= fields_.size() ? &fields_[n - 1] : nullptr;
}
bool FieldGlobalConstructorData::bind_registry(const FieldGlobalExternalSpec &s,
                                               std::string &e) const {
  std::array<uint8_t, 32> h{};
  if (!valid_ || s.role != 3 ||
      s.identity.upstream_commit != identity_.upstream_commit ||
      s.source != scene_ || s.script != owner_ ||
      s.script_sha != identity_.source_sha256 ||
      s.native_class != recipe_.records()[0].native_class ||
      !source_hash(scene_, h) || h != s.source_sha)
    return fail(e, "Global constructor Registry owner differs");
  return true;
}
} // namespace encore::upstream
