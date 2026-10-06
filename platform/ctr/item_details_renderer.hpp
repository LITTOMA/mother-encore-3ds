#pragma once
#include "battle_renderer.hpp"
#include "encore/item_details.hpp"
#include "loading_texture.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace item_details_renderer_detail {
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
} // namespace item_details_renderer_detail
// Borrows admitted content. Texture admission and destruction happen outside
// GPU frames; a failed admission preserves the previous live presentation.
class ItemDetailsRenderer {
  using View = encore::upstream::ItemDetailsView;
  using Kind = encore::upstream::ItemDetailsTokenKind;
  struct Asset {
    encore::ctr::LoadingSpriteSheet sheet = nullptr;
    uint32_t width = 0, height = 0;
  };
  View data_;
  std::vector<Asset> assets_;
  std::string locale_, nickname_;
  mutable std::string error_;
  static float pixel(float value) { return std::floor(value + .5f); }
  bool fail(const char *message) const {
    error_ = message;
    return false;
  }
  static uint32_t color(uint32_t rgba) {
    return C2D_Color32(rgba & 255, (rgba >> 8) & 255, (rgba >> 16) & 255,
                       (rgba >> 24) & 255);
  }
  static bool image(const Asset &asset, float x, float y, float left, float top,
                    float right, float bottom, uint32_t tint) {
    if (!asset.sheet)
      return false;
    const float gx = pixel(x), gy = pixel(y);
    const int x0 = std::max(0, int(std::ceil(left - gx))),
              y0 = std::max(0, int(std::ceil(top - gy)));
    const int x1 = std::min(int(asset.width), int(std::floor(right - gx))),
              y1 = std::min(int(asset.height), int(std::floor(bottom - gy)));
    if (x1 <= x0 || y1 <= y0)
      return true;
    const auto source =
        encore::ctr::loading_sprite_sheet_get_image(asset.sheet, 0);
    auto sub = *source.subtex;
    const float du = (sub.right - sub.left) / asset.width,
                dv = (sub.bottom - sub.top) / asset.height;
    const float u = sub.left, v = sub.top;
    sub.left = u + x0 * du;
    sub.right = u + x1 * du;
    sub.top = v + y0 * dv;
    sub.bottom = v + y1 * dv;
    sub.width = x1 - x0;
    sub.height = y1 - y0;
    C2D_ImageTint paint;
    C2D_PlainImageTint(&paint, tint, 0);
    return C2D_DrawImageAt({source.tex, &sub}, gx + x0, gy + y0, 0, &paint, 1,
                           1);
  }

public:
  ItemDetailsRenderer() = default;
  ItemDetailsRenderer(const ItemDetailsRenderer &) = delete;
  ItemDetailsRenderer &operator=(const ItemDetailsRenderer &) = delete;
  ~ItemDetailsRenderer() { free(); }
  bool ready() const { return data_.valid(); }
  const std::string &error() const { return error_; }
  void set_locale(std::string_view locale) { locale_ = std::string(locale); }
  void set_nickname(std::string_view nickname) {
    nickname_ = std::string(nickname);
  }
  bool load(View data, encore::upstream::ItemView items, const char *root,
            std::string &error) {
    error.clear();
    if (!data.valid() || !items.valid() || !root || !*root ||
        !data.bind_items(items, error)) {
      if (error.empty())
        error = "Item details require checked content";
      return false;
    }
    return load_checked(data, root, error);
  }
  bool load_field(View data,
                  const encore::upstream::FieldItemDefinitions &items,
                  const char *root, std::string &error) {
    if (!root || !*root || !data.bind_field_items(items, error))
      return false;
    return load_checked(data, root, error);
  }

private:
  bool load_checked(View data, const char *root, std::string &error) {
    if (!data.verify_resources(root, error))
      return false;
    ItemDetailsRenderer candidate;
    candidate.data_ = data;
    candidate.assets_.resize(
        data.count(encore::upstream::ItemDetailsSection::Resources));
    for (uint32_t i = 0; i < candidate.assets_.size(); ++i) {
      const auto r = data.resource(i);
      auto &a = candidate.assets_[i];
      if (r.kind != 1 || !r.width || !r.height || r.columns != 1 ||
          r.rows != 1) {
        error = "Item details image schema rejected";
        return false;
      }
      a.width = r.width;
      a.height = r.height;
      std::string path(root);
      if (path.back() != '/')
        path += '/';
      path += data.string(r.path);
      FILE *f = std::fopen(path.c_str(), "rb");
      if (!f) {
        error = "Item detail atlas unavailable";
        return false;
      }
      std::vector<uint8_t> raw(r.bytes);
      bool ok = std::fread(raw.data(), 1, raw.size(), f) == raw.size() &&
                std::fgetc(f) == EOF && !std::ferror(f);
      if (std::fclose(f))
        ok = false;
      const auto sha =
          item_details_renderer_detail::sha256(raw.data(), raw.size());
      if (!ok || !std::equal(sha.begin(), sha.end(), r.sha256)) {
        error = "Item detail converted atlas bytes/SHA rejected";
        return false;
      }
      a.sheet = encore::ctr::loading_sprite_sheet_acquire_memory(path.c_str(),
                                                                 raw, error);
      if (!a.sheet)
        return false;
      const auto texture =
          encore::ctr::loading_sprite_sheet_get_image(a.sheet, 0);
      if (encore::ctr::loading_sprite_sheet_count(a.sheet) != 1 ||
          !texture.tex || !texture.subtex ||
          Tex3DS_SubTextureRotated(texture.subtex) ||
          texture.subtex->width != a.width ||
          texture.subtex->height != a.height) {
        error = "Item details image dimensions rejected";
        return false;
      }
      C3D_TexSetFilter(texture.tex, GPU_NEAREST, GPU_NEAREST);
    }
    C3D_FrameSync();
    std::swap(data_, candidate.data_);
    assets_.swap(candidate.assets_);
    error_.clear();
    return true;
  }

public:
  bool draw_field_icon(uint32_t definition, float center_x, float center_y,
                       float left, float top, float right, float bottom) const {
    if (!data_.field_family())
      return fail("Field icon requires checked field family");
    for (uint32_t i = 0;
         i < data_.count(encore::upstream::ItemDetailsSection::Definitions);
         ++i) {
      const auto d = data_.definition(i);
      if (d.definition != definition)
        continue;
      if (d.item_icon == encore::upstream::item_no_index)
        return true;
      if (d.item_icon >= assets_.size())
        return fail("Field item icon rejected");
      const auto &a = assets_[d.item_icon];
      return image(a, center_x - a.width * .5f, center_y - a.height * .5f, left,
                   top, right, bottom, C2D_Color32(255, 255, 255, 255));
    }
    return fail("Field item icon identity unavailable");
  }
  void free() {
    if (!assets_.empty())
      C3D_FrameSync();
    for (auto &asset : assets_)
      if (asset.sheet)
        encore::ctr::loading_sprite_sheet_free(asset.sheet);
    assets_.clear();
    data_ = {};
    error_.clear();
  }
  bool draw(encore::upstream::ItemInstance instance, const BattleRenderer &font,
            float x, float y, float width, float height) const {
    if (data_.field_family())
      return fail("Field item IDs require draw_field");
    return draw_field(instance.definition, instance.doses, font, x, y, width,
                      height);
  }
  bool draw_field(uint32_t definition, uint32_t doses,
                  const BattleRenderer &font, float x, float y, float width,
                  float height) const {
    error_.clear();
    if (!data_.valid())
      return fail("Item details renderer unavailable");
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width) ||
        !std::isfinite(height) || width <= 0 || height <= 0)
      return fail("Item details rectangle rejected");
    encore::upstream::ItemDetailsComposition runs;
    const auto measure = [&font](std::string_view value) {
      const std::string text(value);
      return font.text_width(text.c_str());
    };
    if (!data_.compose(definition, doses, nickname_, locale_, width, measure,
                       runs, error_))
      return false;
    const float left = pixel(x), top = pixel(y), right = left + width,
                bottom = top + height;
    for (const auto &atom : runs.atoms) {
      if (atom.kind == Kind::Text) {
        if (!font.draw_text_clipped(atom.text.c_str(), pixel(left + atom.x),
                                    pixel(top + atom.y), left, top, right,
                                    bottom, color(atom.color)))
          return fail("Item details source glyph unavailable");
      } else if (atom.kind == Kind::InlineImage) {
        if (atom.resource >= assets_.size() ||
            atom.width != assets_[atom.resource].width ||
            atom.height != assets_[atom.resource].height)
          return fail("Item details inline image reference rejected");
        if (!image(assets_[atom.resource], left + atom.x, top + atom.y, left,
                   top, right, bottom, color(atom.color)))
          return fail("Item details inline image draw rejected");
      } else
        return fail("Unknown item details draw atom");
    }
    return true;
  }
};
