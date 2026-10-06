#include "encore/crc32.hpp"
#include "encore/global_child_ready.hpp"
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
uint32_t word(const uint8_t *p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 |
         uint32_t(p[3]) << 24;
}
bool nonzero(const std::array<uint8_t, 32> &h) {
  return std::any_of(h.begin(), h.end(), [](uint8_t x) { return x != 0; });
}
struct Reader {
  const uint8_t *p;
  size_t n, at = 128;
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
    std::array<uint8_t, N> v{};
    if (!ok || at > n || n - at < N) {
      ok = false;
      return v;
    }
    std::copy_n(p + at, N, v.begin());
    at += N;
    return v;
  }
  double number() {
    auto b = bytes<8>();
    double v = 0;
    std::memcpy(&v, b.data(), 8);
    if (!std::isfinite(v))
      ok = false;
    return v;
  }
  std::string text() {
    auto k = u();
    if (!ok || k > 4096 || at > n || n - at < k) {
      ok = false;
      return {};
    }
    std::string s(reinterpret_cast<const char *>(p + at), k);
    at += k;
    size_t chars = 0;
    if (s.find('\0') != s.npos || !utf8_count(s, chars))
      ok = false;
    return s;
  }
  std::vector<std::string> strings() {
    std::vector<std::string> out;
    auto k = u();
    if (k > 32) {
      ok = false;
      return out;
    }
    std::set<std::string> unique;
    while (k-- && ok) {
      auto s = text();
      if (s.empty() || !unique.insert(s).second) {
        ok = false;
        break;
      }
      out.push_back(std::move(s));
    }
    return out;
  }
};
} // namespace
bool GlobalChildReadyData::source_hash(std::string_view path,
                                       std::array<uint8_t, 32> &out) const {
  auto i = sources_.find(std::string(path));
  if (i == sources_.end())
    return false;
  out = i->second;
  return true;
}
bool GlobalChildReadyData::load(const uint8_t *p, size_t n,
                                const FieldGlobalConstructorData &ctor,
                                std::string &e) {
  if (!ctor.valid() || !p || n < 128 || n > 65536 ||
      std::memcmp(p, "ENCCHD01", 8) || word(p + 8) != 1 ||
      word(p + 12) != 128 || word(p + 16) != n ||
      word(p + 20) != crc32(p + 128, n - 128) || word(p + 24) != 0x454e0057 ||
      word(p + 28) != 1 || word(p + 32) != 1 || word(p + 124))
    return fail(e, "Global child header/CRC/capability rejected");
  GlobalChildReadyData d;
  d.identity_.scene_id = word(p + 36);
  std::copy_n(p + 40, 20, d.identity_.upstream_commit.begin());
  std::copy_n(p + 60, 32, d.identity_.source_sha256.begin());
  std::copy_n(p + 92, 32, d.ir_.begin());
  if (d.identity_.scene_id != ctor.identity().scene_id ||
      d.identity_.upstream_commit != ctor.identity().upstream_commit ||
      d.identity_.source_sha256 != ctor.identity().source_sha256 ||
      !nonzero(d.ir_))
    return fail(e, "Global child actual root source identity rejected");
  Reader r{p, n};
  d.constructor_ = r.bytes<32>();
  auto engine = r.bytes<32>();
  d.audio_ = r.text();
  d.audio_method_ = r.text();
  if (d.constructor_ != ctor.ir_sha256() || !nonzero(engine) ||
      d.audio_.empty() || d.audio_method_.empty())
    return fail(
        e, "Global child independent constructor/native/audio source rejected");
  auto count = r.u();
  if (!count || count > 32)
    return fail(e, "Global child source count rejected");
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    auto source = r.text();
    auto hash = r.bytes<32>();
    std::array<uint8_t, 32> old{};
    if (source.empty() || source.find("..") != source.npos ||
        source.find_first_of("\\:") != source.npos || !nonzero(hash) ||
        !d.sources_.emplace(source, hash).second ||
        (ctor.source_hash(source, old) && old != hash))
      return fail(e, "Global child original source proof rejected");
  }
  if (!d.sources_.count(d.audio_))
    return fail(e, "Global child real audio source proof absent");
  count = r.u();
  if (count != 2)
    return fail(e, "Global child complete two-node source roster rejected");
  std::set<uint32_t> ids;
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    GlobalChildReadyNode row;
    row.kind = r.u();
    row.id = r.u();
    row.class_index = r.u();
    row.methods = r.u();
    row.path = r.text();
    row.script = r.text();
    row.script_sha = r.bytes<32>();
    row.fields = r.strings();
    row.methods_source = r.strings();
    const auto *node = ctor.recipe().record(row.id);
    auto fields =
        std::find_if(ctor.child_fields().begin(), ctor.child_fields().end(),
                     [&](const auto &f) { return f.id == row.id; });
    std::array<uint8_t, 32> hash{};
    if (row.kind != i + 1 || !ids.insert(row.id).second || !node ||
        node->native_class != "Node" || node->class_index != row.class_index ||
        node->path != row.path || node->script != row.script ||
        node->script_sha != row.script_sha ||
        node->script_methods != row.methods ||
        !d.source_hash(row.script, hash) || hash != row.script_sha ||
        fields == ctor.child_fields().end() || fields->script != row.script ||
        row.fields.size() != 4 || fields->fields.size() != row.fields.size() ||
        row.methods_source.size() != (row.kind == 1 ? 4u : 6u))
      return fail(
          e, "Global child actual Node/script/method/declarations rejected");
    for (size_t k = 0; k < row.fields.size(); ++k)
      if (fields->fields[k].name != row.fields[k])
        return fail(e, "Global child initializer member order rejected");
    const auto &f = fields->fields;
    if (row.kind == 1) {
      if (f[0].kind != FieldGlobalLiteralKind::Null ||
          f[1].kind != FieldGlobalLiteralKind::Null ||
          f[2].kind != FieldGlobalLiteralKind::Null ||
          f[3].kind != FieldGlobalLiteralKind::Boolean)
        return fail(e, "Slowmo actual empty constructor rejected");
    } else if (f[0].kind != FieldGlobalLiteralKind::Vector ||
               f[1].kind != FieldGlobalLiteralKind::Number ||
               f[2].kind != FieldGlobalLiteralKind::Number ||
               f[3].kind != FieldGlobalLiteralKind::Number)
      return fail(e, "MouseHider actual typed constructor rejected");
    d.nodes_.push_back(std::move(row));
  }
  auto &v = d.policy_;
  v.slow_end = r.number();
  v.length_scale = r.number();
  v.moving_frames = r.number();
  v.idle_reset = r.number();
  v.shown_hide = r.number();
  v.engine_initial_scale = r.number();
  v.visible = r.u();
  v.hidden = r.u();
  v.mouse_enter = r.u();
  auto pitch = r.u();
  v.default_pitch = pitch != 0;
  if (!r.ok || r.at != n || v.length_scale <= 0 || v.moving_frames <= 0 ||
      v.idle_reset < 0 || v.shown_hide < 0 || v.visible == v.hidden ||
      v.visible > 4 || v.hidden > 4 || v.mouse_enter < 1000 || pitch > 1)
    return fail(
        e, "Global child source timing/native values/trailing data rejected");
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
bool GlobalChildReadyData::load_file(const char *path,
                                     const FieldGlobalConstructorData &ctor,
                                     std::string &e) {
  if (!path)
    return fail(e, "Global child null resource path");
  auto *f = std::fopen(path, "rb");
  if (!f)
    return fail(e, "Cannot open global child resource");
  if (std::fseek(f, 0, SEEK_END)) {
    std::fclose(f);
    return fail(e, "Global child resource seek rejected");
  }
  auto size = std::ftell(f);
  if (size < 128 || size > 65536 || std::fseek(f, 0, SEEK_SET)) {
    std::fclose(f);
    return fail(e, "Global child resource size rejected");
  }
  std::vector<uint8_t> b(size);
  bool ok = std::fread(b.data(), 1, b.size(), f) == b.size();
  std::fclose(f);
  return ok ? load(b.data(), b.size(), ctor, e)
            : fail(e, "Cannot read global child resource");
}
} // namespace encore::upstream
