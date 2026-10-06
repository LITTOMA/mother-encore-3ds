#include "encore/crc32.hpp"
#include "encore/house_ui_continuation.hpp"
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
uint32_t word(const uint8_t *p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 |
         uint32_t(p[3]) << 24;
}
bool nonzero(const std::array<uint8_t, 32> &h) {
  return std::any_of(h.begin(), h.end(), [](uint8_t b) { return b != 0; });
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
    std::array<uint8_t, N> b{};
    if (!ok || at > n || n - at < N) {
      ok = false;
      return b;
    }
    std::copy_n(p + at, N, b.begin());
    at += N;
    return b;
  }
  std::string text() {
    auto k = u();
    if (!ok || !k || k > 4096 || at > n || n - at < k) {
      ok = false;
      return {};
    }
    std::string s(reinterpret_cast<const char *>(p + at), k);
    at += k;
    size_t c = 0;
    if (s.find('\0') != s.npos || !utf8_count(s, c))
      ok = false;
    return s;
  }
};
} // namespace
const HouseUiSourceMethod *
HouseUiContinuationData::method(uint32_t role) const {
  auto x = std::find_if(methods_.begin(), methods_.end(),
                        [&](const auto &v) { return v.role == role; });
  return x == methods_.end() ? nullptr : &*x;
}
const HouseUiSourceSignal *
HouseUiContinuationData::signal(uint32_t role) const {
  auto x = std::find_if(signals_.begin(), signals_.end(),
                        [&](const auto &v) { return v.role == role; });
  return x == signals_.end() ? nullptr : &*x;
}
bool HouseUiContinuationData::source_hash(std::string_view p,
                                          std::array<uint8_t, 32> &out) const {
  auto i = sources_.find(std::string(p));
  if (!valid_ || i == sources_.end())
    return false;
  out = i->second;
  return true;
}
bool HouseUiContinuationData::load(const uint8_t *p, size_t n,
                                   const FieldUiManagerData &ui,
                                   std::string &e) {
  if (valid_)
    return fail(e, "House UI policy owner cannot be replaced");
  if (!p || !ui.valid() || n < 128 || n > 65536 ||
      std::memcmp(p, "ENCHUIC1", 8) || word(p + 8) != 1 ||
      word(p + 12) != 128 || word(p + 16) != n ||
      word(p + 20) != crc32(p + 128, n - 128) || word(p + 24) != 0x454e0064 ||
      word(p + 28) != 1 || word(p + 32) != 1 || word(p + 124))
    return fail(e, "House UI header/CRC/format/capability rejected");
  HouseUiContinuationData d;
  d.identity_.scene_id = word(p + 36);
  std::copy_n(p + 40, 20, d.identity_.upstream_commit.begin());
  std::copy_n(p + 60, 32, d.identity_.source_sha256.begin());
  std::copy_n(p + 92, 32, d.ir_.begin());
  const auto expected = ui.identity();
  if (d.identity_.scene_id != expected.scene_id ||
      d.identity_.upstream_commit != expected.upstream_commit ||
      d.identity_.source_sha256 != expected.source_sha256 || !nonzero(d.ir_))
    return fail(e, "House UI actual independent source identity rejected");
  Reader r{p, n};
  d.ui_ir_ = r.bytes<32>();
  d.script_ = r.text();
  if (!nonzero(d.ui_ir_) || d.script_ != ui.source_script())
    return fail(e, "House UI independent source owner rejected");
  auto count = r.u();
  if (count != 2)
    return fail(e, "House UI complete source closure rejected");
  for (uint32_t i = 0; i < count; ++i) {
    auto path = r.text();
    auto h = r.bytes<32>();
    std::array<uint8_t, 32> old{};
    if (path.find("..") != path.npos ||
        path.find_first_of("\\:") != path.npos || !nonzero(h) ||
        !d.sources_.emplace(path, h).second ||
        (ui.source_hash(path, old) && old != h))
      return fail(e, "House UI source path/hash rejected");
  }
  auto source = d.sources_.find(d.script_);
  if (source == d.sources_.end() || source->second != d.identity_.source_sha256)
    return fail(e, "House UI owning script source rejected");
  count = r.u();
  if (count != 3)
    return fail(e, "House UI complete boolean field policy rejected");
  std::set<std::string> names;
  for (uint32_t i = 0; i < count; ++i) {
    HouseUiSourceField f;
    f.role = r.u();
    auto initial = r.u();
    f.initial = initial != 0;
    f.member = r.text();
    if (f.role != i + 1 || initial > 1 || !names.insert(f.member).second)
      return fail(e, "House UI source field schema rejected");
    d.fields_.push_back(std::move(f));
  }
  count = r.u();
  if (count != 10)
    return fail(e, "House UI source method roster rejected");
  names.clear();
  for (uint32_t i = 0; i < count; ++i) {
    HouseUiSourceMethod m;
    m.role = r.u();
    m.name = r.text();
    m.sha = r.bytes<32>();
    if (m.role != i + 1 || !names.insert(m.name).second || !nonzero(m.sha))
      return fail(e, "House UI source method proof rejected");
    d.methods_.push_back(std::move(m));
  }
  count = r.u();
  if (count != 3)
    return fail(e, "House UI actual signal roster rejected");
  names.clear();
  for (uint32_t i = 0; i < count; ++i) {
    HouseUiSourceSignal s;
    s.role = r.u();
    s.arity = r.u();
    s.name = r.text();
    if (s.role != i + 1 || s.arity || !names.insert(s.name).second)
      return fail(e, "House UI native source signal arity rejected");
    d.signals_.push_back(std::move(s));
  }
  if (!r.ok || r.at != n)
    return fail(e, "House UI truncated/trailing resource rejected");
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
bool HouseUiContinuationData::load_file(const char *path,
                                        const FieldUiManagerData &ui,
                                        std::string &e) {
  auto *f = path ? std::fopen(path, "rb") : nullptr;
  if (!f)
    return fail(e, "Cannot open House UI policy");
  bool ok = std::fseek(f, 0, SEEK_END) == 0;
  auto n = ok ? std::ftell(f) : -1;
  ok = ok && n >= 128 && n <= 65536 && std::fseek(f, 0, SEEK_SET) == 0;
  std::vector<uint8_t> b(ok ? size_t(n) : 0);
  ok = ok && std::fread(b.data(), 1, b.size(), f) == b.size();
  std::fclose(f);
  return ok ? load(b.data(), b.size(), ui, e)
            : fail(e, "House UI policy read rejected");
}
} // namespace encore::upstream
