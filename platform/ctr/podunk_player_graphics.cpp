#include "podunk_player_graphics.hpp"
#include <cmath>
#include <fstream>
namespace encore::ctr {
namespace {
inline uint32_t rotate(uint32_t v, unsigned n) {
  return (v >> n) | (v << (32 - n));
}
// Integrity machinery only. The digest binds converted tex3ds bytes to the
// checked resource; all sprite pixels, grids and transforms come from data.
inline std::array<uint8_t, 32> sha256(const uint8_t *data, size_t size) {
  static constexpr uint32_t constants[64] = {
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
  uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  const size_t blocks = (size + 9 + 63) / 64;
  for (size_t block = 0; block < blocks; ++block) {
    uint8_t raw[64]{};
    for (size_t j = 0; j < 64; ++j) {
      const size_t at = block * 64 + j;
      if (at < size)
        raw[j] = data[at];
      else if (at == size)
        raw[j] = 0x80;
    }
    if (block + 1 == blocks) {
      const uint64_t bits = uint64_t(size) * 8;
      for (unsigned j = 0; j < 8; ++j)
        raw[63 - j] = uint8_t(bits >> (j * 8));
    }
    uint32_t w[64];
    for (unsigned j = 0; j < 16; ++j)
      w[j] = uint32_t(raw[j * 4]) << 24 | uint32_t(raw[j * 4 + 1]) << 16 |
             uint32_t(raw[j * 4 + 2]) << 8 | raw[j * 4 + 3];
    for (unsigned j = 16; j < 64; ++j) {
      const auto a = w[j - 15], b = w[j - 2];
      w[j] = w[j - 16] + (rotate(a, 7) ^ rotate(a, 18) ^ (a >> 3)) + w[j - 7] +
             (rotate(b, 17) ^ rotate(b, 19) ^ (b >> 10));
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5],
             g = h[6], v = h[7];
    for (unsigned j = 0; j < 64; ++j) {
      const auto t1 = v + (rotate(e, 6) ^ rotate(e, 11) ^ rotate(e, 25)) +
                      ((e & f) ^ (~e & g)) + constants[j] + w[j],
                 t2 = (rotate(a, 2) ^ rotate(a, 13) ^ rotate(a, 22)) +
                      ((a & b) ^ (a & c) ^ (b & c));
      v = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
    h[5] += f;
    h[6] += g;
    h[7] += v;
  }
  std::array<uint8_t, 32> out{};
  for (unsigned i = 0; i < 8; ++i)
    for (unsigned j = 0; j < 4; ++j)
      out[i * 4 + j] = uint8_t(h[i] >> (24 - j * 8));
  return out;
}

bool fail(std::string &e, const std::string &s) {
  e = s;
  return false;
}
bool read_asset(const std::string &path, const upstream::PlayerGraphicsAsset &a,
                std::string &e) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file || file.tellg() != std::streamoff(a.bytes))
    return fail(e, "Player GPU converted file length rejected: " + path);
  std::vector<uint8_t> bytes(a.bytes);
  file.seekg(0);
  size_t done = 0;
  while (done < bytes.size()) {
    size_t amount = std::min<size_t>(8192, bytes.size() - done);
    if (!file.read(reinterpret_cast<char *>(bytes.data() + done),
                   std::streamsize(amount)))
      return fail(e, "Player GPU converted file read failed: " + path);
    done += amount;
    report_load_progress(LoadPhase::Texture, done, bytes.size());
  }
  if (sha256(bytes.data(), bytes.size()) != a.output_sha)
    return fail(e, "Player GPU converted SHA rejected: " + path);
  return true;
}
bool valid_image(const C2D_Image &i, const upstream::PlayerGraphicsAsset &a) {
  if (!i.tex || !i.tex->data || i.tex->fmt != GPU_RGBA8 || !i.subtex ||
      Tex3DS_SubTextureRotated(i.subtex) || i.subtex->width != a.width ||
      i.subtex->height != a.height || i.tex->width < a.width ||
      i.tex->height < a.height)
    return false;
  for (float x :
       {i.subtex->left, i.subtex->right, i.subtex->top, i.subtex->bottom})
    if (!std::isfinite(x) || x < 0 || x > 1)
      return false;
  return i.subtex->right > i.subtex->left && i.subtex->top > i.subtex->bottom;
}
} // namespace
void PodunkPlayerGraphics::free() {
  for (auto &a : assets_)
    loading_sprite_sheet_free(a.sheet);
  assets_.clear();
  data_ = nullptr;
}
bool PodunkPlayerGraphics::load(const upstream::PlayerGraphicsData &d,
                                const char *root, std::string &e) {
  if (!d.valid() || !root || !*root)
    return fail(e, "Player GPU checked data/root required");
  PodunkPlayerGraphics next;
  for (const auto &a : d.assets()) {
    const auto path = std::string(root) + a.path;
    if (!read_asset(path, a, e))
      return false;
    next.assets_.push_back({a, nullptr});
    auto &entry = next.assets_.back();
    entry.sheet = loading_sprite_sheet_acquire(path.c_str(), &e);
    if (!entry.sheet || loading_sprite_sheet_count(entry.sheet) != 1)
      return fail(e, "Player GPU requires one actual atlas: " + path);
    const auto im = loading_sprite_sheet_get_image(entry.sheet, 0);
    if (!valid_image(im, a))
      return fail(e,
                  "Player GPU actual texture dimensions/UV rejected: " + path);
    C3D_TexSetFilter(im.tex, GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetWrap(im.tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
  }
  C3D_FrameSync();
  assets_.swap(next.assets_);
  data_ = &d;
  e.clear();
  return true;
}
bool PodunkPlayerGraphics::image(std::string_view source,
                                 const std::array<uint8_t, 32> &sha,
                                 C2D_Image &out, std::string &e) const {
  if (!data_ || !data_->valid())
    return fail(e, "Player GPU source owner not loaded");
  const auto *a = data_->asset(source, sha);
  if (!a)
    return fail(e, "Player GPU source/SHA not owned");
  for (const auto &entry : assets_)
    if (entry.binding.id == a->id) {
      if (!entry.sheet || loading_sprite_sheet_count(entry.sheet) != 1)
        return fail(e, "Player GPU actual sheet unavailable");
      const auto im = loading_sprite_sheet_get_image(entry.sheet, 0);
      if (!valid_image(im, *a))
        return fail(e, "Player GPU actual texture no longer valid");
      out = im;
      e.clear();
      return true;
    }
  return fail(e, "Player GPU source texture missing");
}
} // namespace encore::ctr
