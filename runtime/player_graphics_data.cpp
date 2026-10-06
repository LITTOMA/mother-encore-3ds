#include "encore/player_graphics.hpp"
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
    for (unsigned j = 0; j < 8; ++j)
      c = (c >> 1) ^ (0xedb88320u & uint32_t(-int(c & 1)));
  }
  return ~c;
}
bool hash(const std::array<uint8_t, 32> &h) {
  return std::any_of(h.begin(), h.end(), [](uint8_t x) { return x; });
}
bool path(std::string_view p, std::string_view prefix,
          std::string_view suffix) {
  return p.size() > prefix.size() + suffix.size() &&
         p.substr(0, prefix.size()) == prefix &&
         p.substr(p.size() - suffix.size()) == suffix &&
         p.find("..") == p.npos && p.find('\\') == p.npos;
}
struct R {
  const uint8_t *p;
  size_t n;
  bool ok = true;
  uint32_t u() {
    if (n < 4) {
      ok = false;
      return 0;
    }
    auto x = u32(p);
    p += 4;
    n -= 4;
    return x;
  }
  std::string s() {
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
  std::array<uint8_t, 32> h() {
    std::array<uint8_t, 32> x{};
    if (n < 32) {
      ok = false;
      return x;
    }
    std::copy(p, p + 32, x.begin());
    p += 32;
    n -= 32;
    if (!hash(x))
      ok = false;
    return x;
  }
};
} // namespace
const PlayerGraphicsAsset *
PlayerGraphicsData::asset(std::string_view source,
                          const std::array<uint8_t, 32> &sha) const {
  for (const auto &a : assets_)
    if (a.source == source && a.source_sha == sha)
      return &a;
  return nullptr;
}
bool PlayerGraphicsData::load(const uint8_t *p, size_t n,
                              const PlayerVisualScriptsData &v,
                              std::string &e) {
  if (!v.valid() || !p || n < 128 || n > 1024 * 1024 ||
      std::memcmp(p, "ENCPGFX1", 8) || u32(p + 8) != 1 || u32(p + 12) != 128 ||
      u32(p + 16) != n || u32(p + 20) != crc(p + 128, n - 128) ||
      u32(p + 24) != 0x454e005b || u32(p + 28) != 1 || u32(p + 32) != 1 ||
      u32(p + 124))
    return fail(e, "Player GPU header/version/capability rejected");
  const auto &i = v.identity();
  if (u32(p + 36) != i.scene_id ||
      std::memcmp(p + 40, i.upstream_commit.data(), 20) ||
      std::memcmp(p + 60, i.source_sha256.data(), 32))
    return fail(e, "Player GPU source scene differs");
  PlayerGraphicsData d;
  d.identity_ = i;
  std::copy(p + 92, p + 124, d.ir_.begin());
  if (!hash(d.ir_))
    return fail(e, "Player GPU source IR proof missing");
  R r{p + 128, n - 128};
  d.visual_ir_ = r.h();
  auto count = r.u();
  if (count > 4096 || !count)
    return fail(e, "Player GPU asset count rejected");
  std::set<uint32_t> ids;
  std::set<std::string> sources, paths;
  for (uint32_t j = 0; j < count && r.ok; ++j) {
    PlayerGraphicsAsset a;
    a.id = r.u();
    a.width = r.u();
    a.height = r.u();
    a.bytes = r.u();
    auto nearest = r.u(), repeat = r.u();
    a.nearest = nearest != 0;
    a.repeat = repeat != 0;
    a.source = r.s();
    a.path = r.s();
    a.source_sha = r.h();
    a.import_sha = r.h();
    a.output_sha = r.h();
    std::array<uint8_t, 32> h{}, imp{};
    if (!a.id || !a.width || !a.height || a.width > 1024 || a.height > 1024 ||
        !a.bytes || a.bytes > 64 * 1024 * 1024 || nearest != 1 || repeat != 0 ||
        !path(a.source, "Graphics/", ".png") ||
        !path(a.path, "graphics/", ".t3x") || !ids.insert(a.id).second ||
        !sources.insert(a.source).second || !paths.insert(a.path).second ||
        !v.source_hash(a.source, h) || h != a.source_sha ||
        !v.source_hash(a.source + ".import", imp) || imp != a.import_sha)
      return fail(e, "Player GPU source texture/metadata identity rejected");
    d.assets_.push_back(std::move(a));
  }
  if (!r.ok || r.n || d.visual_ir_ != v.ir_sha256())
    return fail(e, "Player GPU visual resource dependency rejected");
  std::set<uint32_t> required;
  for (const auto &clip : v.shadow().animations)
    for (const auto &f : clip.frames) {
      auto *a = d.asset(f.source, f.source_sha);
      if (!a || a->id != f.texture || f.rect[0] + f.rect[2] > a->width ||
          f.rect[1] + f.rect[3] > a->height)
        return fail(e, "Player GPU Shadow source frame not covered");
      required.insert(a->id);
    }
  const auto &b = v.bat();
  auto *a = d.asset(b.texture_source, b.texture_sha);
  if (!a || a->id != b.texture || a->width % b.columns || a->height % b.rows)
    return fail(e, "Player GPU Bat source frame grid not covered");
  required.insert(a->id);
  if (required.size() != d.assets_.size())
    return fail(e, "Player GPU unknown/unowned texture rejected");
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
bool PlayerGraphicsData::load_file(const char *path,
                                   const PlayerVisualScriptsData &v,
                                   std::string &e) {
  auto *f = std::fopen(path, "rb");
  if (!f)
    return fail(e, "Player GPU resource open failed");
  if (std::fseek(f, 0, SEEK_END)) {
    std::fclose(f);
    return fail(e, "Player GPU resource seek failed");
  }
  long n = std::ftell(f);
  if (n < 128 || n > 1024 * 1024 || std::fseek(f, 0, SEEK_SET)) {
    std::fclose(f);
    return fail(e, "Player GPU resource length rejected");
  }
  std::vector<uint8_t> b(static_cast<size_t>(n));
  bool read = std::fread(b.data(), 1, b.size(), f) == b.size();
  std::fclose(f);
  if (!read)
    return fail(e, "Player GPU resource short read");
  return load(b.data(), b.size(), v, e);
}
} // namespace encore::upstream
