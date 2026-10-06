#include "encore/crc32.hpp"
#include "encore/global_packed_directory.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
namespace encore::upstream {
namespace {
uint32_t word(const uint8_t *p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 |
         uint32_t(p[3]) << 24;
}
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
bool nonzero(const std::array<uint8_t, 32> &a) {
  return std::any_of(a.begin(), a.end(), [](uint8_t x) { return x != 0; });
}
bool source_path(std::string_view s) {
  return !s.empty() && s.front() != '/' && s.back() != '/' &&
         s.find_first_of("\\:") == s.npos && s.find("//") == s.npos &&
         s.find("..") == s.npos;
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
    std::array<uint8_t, N> x{};
    if (!ok || at > n || n - at < N) {
      ok = false;
      return x;
    }
    std::copy_n(p + at, N, x.begin());
    at += N;
    return x;
  }
  std::string text() {
    auto len = u();
    if (!ok || len > 65536 || at > n || n - at < len) {
      ok = false;
      return {};
    }
    std::string s(reinterpret_cast<const char *>(p + at), len);
    at += len;
    std::u32string u;
    if (s.find('\0') != s.npos || !utf8_decode(s, u))
      ok = false;
    return s;
  }
  uint64_t wide() {
    auto x = bytes<8>();
    uint64_t v = 0;
    for (unsigned i = 0; i < 8; ++i)
      v |= uint64_t(x[i]) << (i * 8);
    return v;
  }
};
bool source_less(const std::string &a, const std::string &b) {
  std::u32string x, y;
  return utf8_decode(a, x) && utf8_decode(b, y) && x < y;
}
} // namespace
bool GlobalPackedDirectoryData::source_hash(
    std::string_view p, std::array<uint8_t, 32> &out) const {
  if (!valid_ || p != owner_)
    return false;
  out = identity_.source_sha256;
  return true;
}
int32_t GlobalPackedDirectoryData::find_directory(std::string_view path) const {
  for (size_t i = 0; i < dirs_.size(); ++i)
    if (dirs_[i].path == path)
      return int32_t(i);
  return -1;
}
bool GlobalPackedDirectoryData::directory_in_cache_scope(uint32_t i) const {
  if (i >= dirs_.size())
    return false;
  for (const auto &p : policies_)
    if (dirs_[i].path.compare(0, p.directory.size(), p.directory) == 0)
      return true;
  return false;
}
bool GlobalPackedDirectoryData::load(const uint8_t *p, size_t n,
                                     const GlobalYamlCachesData &c,
                                     const FieldGlobalRegistryData &native,
                                     std::string &e) {
  if (!c.valid() || !native.valid() || !p || n < 128 || n > 2097152 ||
      std::memcmp(p, "ENCPDIR1", 8) || word(p + 8) != 1 ||
      word(p + 12) != 128 || word(p + 16) != n ||
      word(p + 20) != crc32(p + 128, n - 128) || word(p + 24) != 0x454e0051 ||
      word(p + 28) != 1 || word(p + 32) != 1 || !word(p + 36) || word(p + 124))
    return fail(e,
                "Packed Directory header/version/family/capability rejected");
  GlobalPackedDirectoryData d;
  d.identity_.scene_id = word(p + 36);
  std::copy_n(p + 40, 20, d.identity_.upstream_commit.begin());
  std::copy_n(p + 60, 32, d.identity_.source_sha256.begin());
  std::copy_n(p + 92, 32, d.ir_.begin());
  if (d.identity_.upstream_commit != c.identity().upstream_commit ||
      d.identity_.source_sha256 != c.identity().source_sha256 ||
      !nonzero(d.ir_))
    return fail(e, "Packed Directory source/cache owner rejected");
  Reader r{p, n};
  d.owner_ = r.text();
  auto native_class = r.text();
  d.suffix_ = r.text();
  d.class_ = r.u();
  auto mode = r.u();
  auto dirs_first = r.u(), unicode_order = r.u(), synthetic_nav = r.u(),
       hidden = r.u();
  d.cache_ir_ = r.bytes<32>();
  d.engine_commit_ = r.text();
  if (!r.ok || d.owner_ != c.owner_source() || native_class != "Directory" ||
      d.suffix_.empty() || d.suffix_[0] != '.' ||
      d.suffix_.find_first_of("/\\:") != d.suffix_.npos || !d.class_ ||
      mode != 1 || dirs_first != 1 || unicode_order != 1 || synthetic_nav ||
      hidden || d.cache_ir_ != c.ir_sha256() ||
      d.engine_commit_ != native.engine_commit() ||
      d.engine_commit_.size() != 40 ||
      d.engine_commit_.find_first_not_of("0123456789abcdef") !=
          d.engine_commit_.npos)
    return fail(
        e, "Packed Directory mode/native rules/source cross-binding rejected");
  auto count = r.u();
  if (!count || count > 64)
    return fail(e, "Packed Directory engine source schema rejected");
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    auto name = r.text();
    auto h = r.bytes<32>();
    if (!source_path(name) || !nonzero(h) || !d.engine_.emplace(name, h).second)
      return fail(e, "Packed Directory engine proof rejected");
  }
  for (const auto &proof : d.engine_) {
    std::array<uint8_t, 32> expected{};
    if (native.engine_hash(proof.first, expected) && expected != proof.second)
      return fail(
          e, "Packed Directory native engine proof cross-binding rejected");
  }
  count = r.u();
  if (count != c.policies().size())
    return fail(e, "Packed Directory cache directory count rejected");
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    auto role = r.u();
    auto path = r.text();
    auto proof = r.bytes<32>();
    if (role != i || path != c.policies()[i].directory ||
        proof != c.policies()[i].closure)
      return fail(
          e, "Packed Directory original initialization directory rejected");
    d.policies_.push_back(c.policies()[i]);
  }
  count = r.u();
  if (count < d.policies_.size() + 2 || count > 4096)
    return fail(e, "Packed Directory tree count rejected");
  std::set<std::string> paths;
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    GlobalPackedDir dir;
    dir.path = r.text();
    if ((i == 0 && !dir.path.empty()) ||
        (i && (dir.path.empty() || dir.path.back() != '/' ||
               !source_path(std::string_view(dir.path).substr(
                   0, dir.path.size() - 1)))) ||
        !paths.insert(dir.path).second)
      return fail(e, "Packed Directory original path/duplicate rejected");
    d.dirs_.push_back(std::move(dir));
  }
  count = r.u();
  if (count != c.records().size() || count > 16384)
    return fail(e, "Packed Directory full cache closure count rejected");
  std::set<std::string> files;
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    GlobalPackedFile x;
    x.source = r.text();
    x.sha = r.bytes<32>();
    x.size = r.wide();
    const GlobalYamlCacheRecord *expected = nullptr;
    for (const auto &v : c.records())
      if (v.source == x.source)
        expected = &v;
    if (!source_path(x.source) || !files.insert(x.source).second || !expected ||
        expected->source_sha != x.sha || !x.size || x.size > 16777216)
      return fail(e, "Packed Directory source file/SHA/closure rejected");
    if (expected->role >= d.policies_.size() ||
        x.source !=
            d.policies_[expected->role].directory + expected->name + d.suffix_)
      return fail(e,
                  "Packed Directory source filename suffix/cache key rejected");
    auto split = x.source.find_last_of('/');
    if (split == x.source.npos)
      return fail(e, "Packed Directory file parent missing");
    auto parent = x.source.substr(0, split + 1);
    auto index = d.find_directory(parent);
    if (index < 0)
      return fail(e, "Packed Directory file has absent source parent");
    x.name = x.source.substr(split + 1);
    x.role = expected->role;
    d.dirs_[size_t(index)].files.push_back(uint32_t(d.files_.size()));
    d.files_.push_back(std::move(x));
  }
  if (!r.ok || r.at != n)
    return fail(e, "Packed Directory payload/trailing bytes rejected");
  // Reconstruct PackedData's file-created ancestors; no standalone empty dirs.
  std::set<std::string> ancestors{""};
  for (const auto &x : d.files_) {
    auto pos = x.source.find('/');
    while (pos != x.source.npos) {
      ancestors.insert(x.source.substr(0, pos + 1));
      pos = x.source.find('/', pos + 1);
    }
  }
  if (ancestors != paths)
    return fail(e, "Packed Directory fabricated/empty ancestor rejected");
  for (uint32_t i = 1; i < d.dirs_.size(); ++i) {
    auto path = d.dirs_[i].path;
    path.pop_back();
    auto split = path.find_last_of('/');
    auto parent =
        split == path.npos ? std::string() : path.substr(0, split + 1);
    auto idx = d.find_directory(parent);
    if (idx < 0)
      return fail(e, "Packed Directory ancestor parent absent");
    d.dirs_[size_t(idx)].directories.push_back(i);
  }
  for (auto &dir : d.dirs_) {
    auto basename = [&](uint32_t i) {
      auto s = d.dirs_[i].path;
      s.pop_back();
      auto at = s.find_last_of('/');
      return at == s.npos ? s : s.substr(at + 1);
    };
    std::sort(
        dir.directories.begin(), dir.directories.end(),
        [&](auto a, auto b) { return source_less(basename(a), basename(b)); });
    std::sort(dir.files.begin(), dir.files.end(), [&](auto a, auto b) {
      return source_less(d.files_[a].name, d.files_[b].name);
    });
  }
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
bool GlobalPackedDirectoryData::load_file(const char *path,
                                          const GlobalYamlCachesData &c,
                                          const FieldGlobalRegistryData &native,
                                          std::string &e) {
  if (!path)
    return fail(e, "Packed Directory path null");
  auto *f = std::fopen(path, "rb");
  if (!f)
    return fail(e, "Packed Directory file open failed");
  if (std::fseek(f, 0, SEEK_END)) {
    std::fclose(f);
    return fail(e, "Packed Directory file seek failed");
  }
  auto n = std::ftell(f);
  if (n < 0 || n > 2097152 || std::fseek(f, 0, SEEK_SET)) {
    std::fclose(f);
    return fail(e, "Packed Directory file range rejected");
  }
  std::vector<uint8_t> b(size_t(n), 0);
  bool ok = std::fread(b.data(), 1, b.size(), f) == b.size();
  std::fclose(f);
  return ok ? load(b.data(), b.size(), c, native, e)
            : fail(e, "Packed Directory file truncated");
}
} // namespace encore::upstream
