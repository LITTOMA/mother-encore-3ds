#include "encore/podunk_bundle.hpp"
#include "encore/crc32.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <set>
namespace encore::upstream {
namespace {
uint32_t u32(const uint8_t *p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 |
         uint32_t(p[3]) << 24;
}
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
bool nz(const std::array<uint8_t, 32> &h) {
  return std::any_of(h.begin(), h.end(), [](uint8_t x) { return x != 0; });
}
bool safe(const std::string &s) {
  if (s.empty() || s.size() >= 2048 || s[0] == '/' ||
      s.find(':') != std::string::npos || s.find('\\') != std::string::npos)
    return false;
  size_t a = 0;
  while (a < s.size()) {
    auto b = s.find('/', a);
    if (b == std::string::npos)
      b = s.size();
    auto t = s.substr(a, b - a);
    if (t.empty() || t == "." || t == "..")
      return false;
    a = b + 1;
  }
  return s.back() != '/';
}
uint32_t rr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
// FIPS 180-4 SHA-256 constants are algorithm/schema, never game content.
std::array<uint8_t, 32> digest(const uint8_t *p, size_t n) {
  static constexpr uint32_t k[] = {
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
      0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
      0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
      0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
      0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
      0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
      0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
      0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
      0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
      0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
      0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
  uint32_t h[] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                  0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  const size_t blocks = (n + 9 + 63) / 64;
  const uint64_t bits = uint64_t(n) * 8;
  for (size_t b = 0; b < blocks; ++b) {
    uint8_t part[64]{};
    for (size_t i = 0; i < 64; ++i) {
      size_t pos = b * 64 + i;
      if (pos < n)
        part[i] = p[pos];
      else if (pos == n)
        part[i] = 0x80;
      else if (pos >= blocks * 64 - 8)
        part[i] = uint8_t(bits >> (8 * (blocks * 64 - 1 - pos)));
    }
    uint32_t w[64]{};
    for (size_t i = 0; i < 16; ++i)
      w[i] = uint32_t(part[4 * i]) << 24 | uint32_t(part[4 * i + 1]) << 16 |
             uint32_t(part[4 * i + 2]) << 8 | part[4 * i + 3];
    for (size_t i = 16; i < 64; ++i)
      w[i] =
          (rr(w[i - 2], 17) ^ rr(w[i - 2], 19) ^ (w[i - 2] >> 10)) + w[i - 7] +
          (rr(w[i - 15], 7) ^ rr(w[i - 15], 18) ^ (w[i - 15] >> 3)) + w[i - 16];
    uint32_t a = h[0], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6],
             j = h[7], v = h[1];
    for (size_t i = 0; i < 64; ++i) {
      uint32_t t = j + (rr(e, 6) ^ rr(e, 11) ^ rr(e, 25)) +
                   ((e & f) ^ (~e & g)) + k[i] + w[i];
      uint32_t q =
          (rr(a, 2) ^ rr(a, 13) ^ rr(a, 22)) + ((a & v) ^ (a & c) ^ (v & c));
      j = g;
      g = f;
      f = e;
      e = d + t;
      d = c;
      c = v;
      v = a;
      a = t + q;
    }
    h[0] += a;
    h[1] += v;
    h[2] += c;
    h[3] += d;
    h[4] += e;
    h[5] += f;
    h[6] += g;
    h[7] += j;
  }
  std::array<uint8_t, 32> out{};
  for (size_t i = 0; i < 32; ++i)
    out[i] = uint8_t(h[i / 4] >> (8 * (3 - i % 4)));
  return out;
}
struct Reader {
  const uint8_t *p;
  size_t n, at = 128;
  bool ok = true;
  uint32_t u() {
    if (at > n || n - at < 4) {
      ok = false;
      return 0;
    }
    auto v = u32(p + at);
    at += 4;
    return v;
  }
  std::vector<uint8_t> bytes(size_t count) {
    if (count > n || at > n || count > n - at) {
      ok = false;
      return {};
    }
    std::vector<uint8_t> v(p + at, p + at + count);
    at += count;
    return v;
  }
  std::array<uint8_t, 32> hash() {
    std::array<uint8_t, 32> h{};
    auto v = bytes(32);
    if (ok)
      std::copy(v.begin(), v.end(), h.begin());
    return h;
  }
  std::string text() {
    auto count = u();
    if (count > 2048) {
      ok = false;
      return {};
    }
    auto b = bytes(count);
    std::string s(b.begin(), b.end());
    size_t len = 0;
    if (!utf8_count(s, len) || s.find(char(0)) != std::string::npos)
      ok = false;
    return s;
  }
};
struct Schema {
  const char *magic;
  uint32_t format, family, capability, rules;
};
// Known typed-reader mechanisms. Paths and source identities remain external.
const Schema schemas[] = {
    {"ENCFBGR1", 1, 1162739786, 1, 1}, {"ENCCHAR1", 2, 1162739792, 2, 1},
    {"ENCCHD01", 1, 1162739799, 1, 1}, {"ENCGDC01", 1, 1162739795, 1, 1},
    {"ENCGDAT1", 1, 1162739789, 1, 1}, {"ENCFIT01", 3, 0, 3, 1},
    {"ENCFGS01", 1, 1162739783, 1, 1}, {"ENCGLD01", 1, 1162739796, 1, 1},
    {"ENCFNVP1", 1, 1162739788, 1, 1}, {"ENCGCON1", 1, 1162739797, 1, 1},
    {"ENCPDIR1", 1, 1162739793, 1, 1}, {"ENCGRED1", 1, 1162739805, 1, 1},
    {"ENCFGRG1", 1, 1162739778, 3, 1}, {"ENCFUIM1", 2, 1162739781, 1, 1},
    {"ENCFUPR1", 1, 1162739787, 1, 1}, {"ENCYAML1", 1, 1162739791, 1, 1},
    {"ENCYFIL1", 1, 1162739794, 1, 1}, {"ENCHGB01", 1, 1162739808, 2, 1},
    {"ENCHUIC1", 1, 1162739812, 1, 1}, {"ENCPSCR1", 1, 1162739809, 1, 1},
    {"ENCPFX01", 1, 1162739801, 1, 1}, {"ENCPFET1", 1, 1162739806, 1, 1},
    {"ENCPGFX1", 1, 1162739803, 1, 1}, {"ENCPLYI1", 1, 1162739798, 1, 1},
    {"ENCPMOV1", 1, 1162739804, 1, 1}, {"ENCPRDY1", 1, 1162739802, 1, 1},
    {"ENCPRES1", 2, 1162739807, 2, 1}, {"ENCPVIS1", 1, 1162739800, 1, 1},
    {"ENCFLY01", 1, 0, 1, 1},          {"ENCCSA01", 1, 0, 1, 1},
    {"ENCDBSH1", 1, 0, 1, 1},          {"ENCFDAU1", 1, 1162739785, 1, 1},
    {"ENCDLIF1", 1, 0, 1, 1},          {"ENCDROT1", 1, 1162739790, 1, 1},
    {"ENCDUI01", 1, 1162739777, 1, 1}, {"ENCFDVS1", 1, 1162739782, 1, 1},
    {"ENCDOR01", 1, 0, 1, 1},          {"ENCDRP01", 1, 0, 1, 1},
    {"ENCEMO01", 1, 0, 1, 1},          {"ENCFEN01", 1, 0, 2, 1},
    {"ENCFDLG1", 1, 0, 1, 1},          {"ENCMLB01", 1, 0, 1, 1},
    {"ENCMCA01", 1, 0, 1, 1},          {"ENCNPC01", 1, 0, 1, 1},
    {"ENCFPT01", 1, 0, 1, 1},          {"ENCPRE01", 1, 0, 1, 1},
    {"ENCFPG01", 1, 0, 1, 1},          {"ENCFPR01", 1, 0, 1, 1},
    {"ENCSAC01", 1, 0, 1, 1},          {"ENCSPR01", 1, 0, 1, 1},
    {"ENCSTP01", 1, 0, 1, 1},          {"ENCTINT1", 1, 0, 1, 1},
    {"ENCFARR1", 1, 1162739760, 3, 1}, {"ENCFBRD1", 1, 1162739746, 3, 1},
    {"ENCFCAA1", 1, 1162739747, 3, 1}, {"ENCFCA01", 1, 1162739776, 1, 1},
    {"ENCFDAN1", 1, 1162739741, 3, 1}, {"ENCFDOR1", 1, 1162739743, 3, 1},
    {"ENCFILD1", 1, 1162739735, 1, 1}, {"ENCFID01", 1, 0, 1, 1},
    {"ENCFGEO1", 1, 1162739739, 1, 1}, {"ENCFIT01", 2, 0, 2, 1},
    {"ENCFMAP1", 1, 1162739737, 1, 1}, {"ENCFSCN1", 1, 1162739740, 6, 1},
    {"ENCFGCM1", 1, 1162739761, 3, 1}, {"ENCFGDS1", 1, 1162739784, 1, 1},
    {"ENCFINV1", 1, 1162739779, 1, 1}, {"ENCFNTR1", 1, 1162739772, 3, 1},
    {"ENCFOPN1", 1, 1162739744, 3, 1}, {"ENCFPH01", 1, 0, 1, 1},
    {"ENCFSPL1", 1, 1162739745, 3, 1}, {"ENCFNT01", 1, 1162739780, 1, 1},
    {"ENCFVM01", 1, 0, 1, 1},          {"ENCFNRC1", 1, 1162739773, 3, 1},
    {"ENCSHP01", 1, 0, 1, 1},          {"ENCFDOR1", 1, 1162739743, 3, 1},
    {"ENCHSE01", 1, 0x454e0062, 1, 1},
    {"ENCSAUD1", 1, 0x454e0067, 1, 1},
    {"ENCSIG01", 2, 0x454e0068, 2, 1},
    {"ENCFVS01", 1, 0x454e0069, 1, 1},
    {"ENCNPCW1", 1, 0x454e006a, 1, 1},
    {"ENCPRN01", 1, 0x454e006b, 1, 1}};
const PodunkPackRole script_schemas[] = {PodunkPackRole::Grass,
                                         PodunkPackRole::Npc,
                                         PodunkPackRole::Enemy,
                                         PodunkPackRole::CharacterTint,
                                         PodunkPackRole::SpriteBridge,
                                         PodunkPackRole::SpriteBridge,
                                         PodunkPackRole::SceneLifecycle,
                                         PodunkPackRole::SceneLifecycle,
                                         PodunkPackRole::SceneLifecycle,
                                         PodunkPackRole::SceneLifecycle,
                                         PodunkPackRole::Emotes,
                                         PodunkPackRole::Dandelion,
                                         PodunkPackRole::Door,
                                         PodunkPackRole::Prompt,
                                         PodunkPackRole::DeadBush,
                                         PodunkPackRole::OpenableDoor,
                                         PodunkPackRole::Sparkles,
                                         PodunkPackRole::InteractDialog,
                                         PodunkPackRole::Payphone,
                                         PodunkPackRole::Present,
                                         PodunkPackRole::DroppedItem,
                                         PodunkPackRole::Butterfly,
                                         PodunkPackRole::CutsceneArea,
                                         PodunkPackRole::Birds,
                                         PodunkPackRole::CameraArea,
                                         PodunkPackRole::MusicChanger,
                                         PodunkPackRole::CameraArrows,
                                         PodunkPackRole::GameCamera,
                                         PodunkPackRole::SceneActions,
                                         PodunkPackRole::SceneActions,
                                         PodunkPackRole::SteppingSounds,
                                         PodunkPackRole::PlayerTransitions,
                                         PodunkPackRole::PlayerTransitions,
                                         PodunkPackRole::DoorNpc,
                                         PodunkPackRole::MelodyBackground,
                                         PodunkPackRole::VendingMachine};
bool read_file(const std::string &root, const PodunkBundleFile &f,
               std::vector<uint8_t> &out, std::string &e) {
  if (!safe(f.path) || root.empty())
    return fail(e, "Bundle file path rejected");
  const auto full = root + (root.back() == '/' ? "" : "/") + f.path;
  std::ifstream in(full, std::ios::binary);
  if (!in)
    return fail(e, "Bundle file cannot open");
  in.seekg(0, std::ios::end);
  auto count = in.tellg();
  if (count < 0 || uint64_t(count) != f.size)
    return fail(e, "Bundle file size differs");
  in.seekg(0, std::ios::beg);
  std::vector<uint8_t> b(f.size);
  if (!in.read(reinterpret_cast<char *>(b.data()), b.size()) ||
      in.peek() != std::char_traits<char>::eof())
    return fail(e, "Bundle file read failed");
  if (encore::crc32(b.data(), b.size()) != f.crc32 ||
      digest(b.data(), b.size()) != f.sha256)
    return fail(e, "Bundle file CRC/SHA differs");
  out.swap(b);
  return true;
}
} // namespace
bool PodunkBundleData::load(const uint8_t *p, size_t n,
                            const FieldIdentity &expected, std::string &e) {
  if (!p || n < 128 || n > 4 * 1024 * 1024 || std::memcmp(p, "ENCPBND1", 8) ||
      u32(p + 8) != 1 || u32(p + 12) != 128 || u32(p + 16) != n ||
      u32(p + 24) != 0x454e0066 || u32(p + 28) != 1 || u32(p + 32) != 1 ||
      u32(p + 124) || encore::crc32(p + 128, n - 128) != u32(p + 20))
    return fail(e, "Bundle schema/header rejected");
  if (!expected.scene_id || !nz(expected.source_sha256) ||
      std::all_of(expected.upstream_commit.begin(),
                  expected.upstream_commit.end(),
                  [](uint8_t x) { return !x; }) ||
      u32(p + 36) != expected.scene_id ||
      std::memcmp(p + 40, expected.upstream_commit.data(), 20) ||
      std::memcmp(p + 60, expected.source_sha256.data(), 32) ||
      std::all_of(p + 92, p + 124, [](uint8_t x) { return !x; }))
    return fail(e, "Bundle source identity rejected");
  PodunkBundleData d;
  d.identity_ = expected;
  Reader r{p, n};
  d.scene_ = r.text();
  if (!safe(d.scene_))
    return fail(e, "Bundle source scene rejected");
  auto pc = r.u(), ac = r.u(), sc = r.u();
  if (pc != sizeof(schemas) / sizeof(*schemas) || ac == 0 || ac > 8192 ||
      sc != 36)
    return fail(e, "Bundle required closure count rejected");
  std::set<std::string> paths;
  for (uint32_t i = 0; i < pc; ++i) {
    PodunkBundlePack a;
    auto role = r.u();
    a.role = PodunkPackRole(role);
    a.id = role;
    a.kind = r.u();
    a.size = r.u();
    a.crc32 = r.u();
    a.format = r.u();
    a.family = r.u();
    a.capability = r.u();
    a.rules = r.u();
    a.identity.scene_id = r.u();
    a.identity.upstream_commit = expected.upstream_commit;
    a.path = r.text();
    a.sha256 = r.hash();
    a.identity.source_sha256 = r.hash();
    a.ir_sha256 = r.hash();
    auto hc = r.u();
    if (hc != 128)
      return fail(e, "Bundle original header rejected");
    a.original_header = r.bytes(hc);
    if (!r.ok || role != i + 1 || !safe(a.path) ||
        a.path.substr(0, 5) != "data/" || !paths.insert(a.path).second ||
        a.kind < 1 || a.kind > 2 || !a.size || a.size > 64 * 1024 * 1024 ||
        !a.identity.scene_id || !nz(a.sha256) ||
        !nz(a.identity.source_sha256) || !nz(a.ir_sha256))
      return fail(e, "Bundle typed resource binding rejected");
    const auto &s = schemas[i];
    if (a.format != s.format || a.family != s.family ||
        a.capability != s.capability || a.rules != s.rules ||
        std::memcmp(a.original_header.data(), s.magic, 8) ||
        u32(a.original_header.data() + 8) != a.format)
      return fail(e, "Bundle unknown typed mechanism rejected");
    auto pin = std::search(a.original_header.begin(), a.original_header.end(),
                           expected.upstream_commit.begin(),
                           expected.upstream_commit.end());
    if (pin == a.original_header.end())
      return fail(e, "Bundle original upstream pin differs");
    if (a.kind == 1) {
      auto at = size_t(pin - a.original_header.begin());
      if ((at != 40 && at != 48) || at + 52 > a.original_header.size() ||
          u32(a.original_header.data() + 36) != a.identity.scene_id ||
          !std::equal(a.identity.source_sha256.begin(),
                      a.identity.source_sha256.end(),
                      a.original_header.begin() + at + 20))
        return fail(e, "Bundle actual header identity differs");
    }
    d.packs_.push_back(std::move(a));
  }
  for (uint32_t i = 0; i < ac; ++i) {
    PodunkBundleFile a;
    a.id = r.u();
    a.kind = r.u();
    a.size = r.u();
    a.crc32 = r.u();
    a.path = r.text();
    a.sha256 = r.hash();
    bool type =
        (a.kind == 1 && a.path.substr(0, 9) == "graphics/" &&
         a.path.size() > 4 && a.path.substr(a.path.size() - 4) == ".t3x") ||
        (a.kind == 2 && a.path.substr(0, 6) == "sound/" && a.path.size() > 4 &&
         a.path.substr(a.path.size() - 4) == ".pcm") ||
        (a.kind == 3 &&
         (a.path.substr(0, 9) == "graphics/" ||
          a.path.substr(0, 8) == "shaders/") &&
         a.path.size() > 6 && a.path.substr(a.path.size() - 6) == ".shbin") ||
        (a.kind == 4 && a.path.substr(0, 6) == "fonts/") ||
        (a.kind == 5 && a.path.substr(0, 12) == "sound/banks/" &&
         a.path.size() > 9 &&
         a.path.substr(a.path.size() - 9) == ".encaudio") ||
        (a.kind == 6 && a.path.substr(0, 12) == "sound/banks/" &&
         a.path.size() > 9 && a.path.substr(a.path.size() - 9) == ".encmusic");
    if (!r.ok || a.id != i + 1 || !safe(a.path) || !type ||
        !paths.insert(a.path).second || !a.size || a.size > 64 * 1024 * 1024 ||
        !nz(a.sha256))
      return fail(e, "Bundle asset binding rejected");
    d.assets_.push_back(std::move(a));
  }
  for (uint32_t i = 0; i < sc; ++i) {
    PodunkScriptBinding s{r.u(), r.u()};
    if (!r.ok || s.source_role != i + 1 || !s.pack_role || s.pack_role > pc ||
        s.pack_role != uint32_t(script_schemas[i]))
      return fail(e, "Bundle script resource closure rejected");
    d.scripts_.push_back(s);
  }
  if (!r.ok || r.at != n)
    return fail(e, "Bundle trailing/truncated data rejected");
  d.valid_ = true;
  *this = std::move(d);
  return true;
}
bool PodunkBundleData::load_file(const char *path, const FieldIdentity &id,
                                 std::string &e) {
  if (!path)
    return fail(e, "Bundle path missing");
  std::ifstream f(path, std::ios::binary);
  if (!f)
    return fail(e, "Bundle cannot open");
  f.seekg(0, std::ios::end);
  auto n = f.tellg();
  if (n < 128 || n > 4 * 1024 * 1024)
    return fail(e, "Bundle file size rejected");
  f.seekg(0, std::ios::beg);
  std::vector<uint8_t> b(static_cast<size_t>(n));
  if (!f.read(reinterpret_cast<char *>(b.data()), b.size()))
    return fail(e, "Bundle read failed");
  return load(b.data(), b.size(), id, e);
}
const PodunkBundlePack *PodunkBundleData::entry(PodunkPackRole role) const {
  if (!valid_)
    return nullptr;
  uint32_t i = uint32_t(role);
  return i && i <= packs_.size() ? &packs_[i - 1] : nullptr;
}
bool PodunkBundleData::read(PodunkPackRole role, const std::string &root,
                            std::vector<uint8_t> &out, std::string &e) const {
  auto a = entry(role);
  if (!a)
    return fail(e, "Bundle resource role rejected");
  std::vector<uint8_t> b;
  if (!read_file(root, *a, b, e))
    return false;
  if (b.size() < a->original_header.size() ||
      !std::equal(a->original_header.begin(), a->original_header.end(),
                  b.begin()))
    return fail(e, "Bundle original header differs");
  out.swap(b);
  return true;
}
bool PodunkBundleData::read_asset(uint32_t id, const std::string &root,
                                  std::vector<uint8_t> &out,
                                  std::string &e) const {
  if (!valid_ || !id || id > assets_.size())
    return fail(e, "Bundle asset ID rejected");
  return read_file(root, assets_[id - 1], out, e);
}
bool PodunkBundleData::verify_files(const std::string &root,
                                    std::string &e) const {
  if (!valid_)
    return fail(e, "Bundle uninitialized");
  std::vector<uint8_t> b;
  for (const auto &a : packs_)
    if (!read(a.role, root, b, e))
      return false;
  for (const auto &a : assets_)
    if (!read_asset(a.id, root, b, e))
      return false;
  return true;
}
std::vector<std::string> PodunkBundleData::stage_paths() const {
  std::vector<std::string> v;
  if (valid_) {
    for (const auto &p : packs_)
      v.push_back(p.path);
    for (const auto &a : assets_)
      v.push_back(a.path);
  }
  return v;
}
} // namespace encore::upstream
