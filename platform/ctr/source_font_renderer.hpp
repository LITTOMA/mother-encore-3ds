#pragma once
#include "encore/crc32.hpp"
#include "encore/source_font.hpp"
#include "encore/utf8.hpp"
#include "loading_texture.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace encore::ctr {
// All C3D_Tex objects live in heap-owned LoadingSpriteSheet objects. Growing
// the page index never moves a texture already referenced by Citro2D commands.
// Load/admit/select/reset/destruction belong OUTSIDE C3D_FrameBegin/FrameEnd.
// Optional begin_frame/end_frame guards make accidental mid-frame mutations
// fail closed. Draw operations do no IO, allocation, deletion or eviction.
class SourceFontRenderer {
public:
  static constexpr size_t maximum_resident_bytes =
      SourceFontCatalog::maximum_resident_bytes;
  SourceFontRenderer() = default;
  SourceFontRenderer(const SourceFontRenderer &) = delete;
  SourceFontRenderer &operator=(const SourceFontRenderer &) = delete;
  ~SourceFontRenderer() {
    if (resident_bytes_) {
      C3D_FrameSync();
      release_pages();
    }
  }
  bool load_catalog_at_safe_boundary(const char *catalog_path,
                                     const char *page_root,
                                     std::string &error) {
    if (!boundary(error))
      return false;
    if (!page_root || !*page_root) {
      error = "Missing source font page root";
      return false;
    }
    SourceFontCatalog candidate;
    if (!candidate.load(catalog_path, error))
      return false;
    C3D_FrameSync();
    release_pages();
    catalog_ = std::move(candidate);
    page_root_ = page_root;
    if (page_root_.back() != '/')
      page_root_ += '/';
    pages_.resize(catalog_.pages().size(), nullptr);
    error.clear();
    return true;
  }
  bool select_font_at_safe_boundary(std::string_view source,
                                    std::string &error) {
    if (!boundary(error))
      return false;
    if (catalog_.selected() && catalog_.selected()->source == source) {
      error.clear();
      return true;
    }
    // Validate selection before releasing the current face or its textures.
    if (!catalog_.select_font(source, error))
      return false;
    C3D_FrameSync();
    release_pages();
    pages_.resize(catalog_.pages().size(), nullptr);
    error.clear();
    return true;
  }
  bool reset_at_safe_boundary(std::string &error) {
    if (!boundary(error))
      return false;
    C3D_FrameSync();
    release_pages();
    pages_.resize(catalog_.pages().size(), nullptr);
    error.clear();
    return true;
  }
  bool begin_frame() {
    if (drawing_)
      return false;
    drawing_ = true;
    return true;
  }
  void end_frame() {
    drawing_ = false;
  } // Call AFTER the application's C3D_FrameEnd.
  bool admit_selected_font(std::string &error) {
    if (!boundary(error))
      return false;
    const auto *face = catalog_.selected();
    if (!face) {
      error = "Source font not selected";
      return false;
    }
    std::vector<uint32_t> ids;
    for (uint32_t i = 0; i < face->page_count; ++i)
      ids.push_back(face->first_page + i);
    return admit(ids, error);
  }
  bool admit_text(std::string_view text, std::string &error) {
    if (!boundary(error))
      return false;
    std::vector<uint32_t> ids;
    size_t cursor = 0;
    uint32_t cp = 0;
    while (cursor < text.size()) {
      if (!utf8_next(text, cursor, cp)) {
        error = "Invalid UTF-8 source font admission";
        return false;
      }
      if (cp == '\n' || !handles(cp))
        continue;
      const auto *g = catalog_.glyph(cp);
      if (!g) {
        error = SourceFontCatalog::missing_glyph(
            cp,
            catalog_.selected() ? catalog_.selected()->source : "<unselected>");
        return false;
      }
      if (std::find(ids.begin(), ids.end(), g->page) == ids.end())
        ids.push_back(g->page);
    }
    return admit(ids, error);
  }
  bool handles(uint32_t cp) const {
    return catalog_.selected() && (!catalog_.legacy_ascii() || cp >= 128);
  }
  const SourceFontCatalog &catalog() const { return catalog_; }
  const std::array<uint8_t,32>&catalog_sha256()const{
    return catalog_.binary_sha256();
  }
  const SourceFontGlyph *glyph(uint32_t cp) const {
    const auto *g = catalog_.glyph(cp);
    if (!g)
      last_error_ = SourceFontCatalog::missing_glyph(
          cp,
          catalog_.selected() ? catalog_.selected()->source : "<unselected>");
    return g;
  }
  bool glyph_advance(uint32_t cp, float &advance) const {
    return catalog_.advance(cp, advance, last_error_);
  }
  float following_spacing(uint32_t cp, bool next) const {
    return catalog_.following_spacing(cp, next);
  }
  const std::string &last_error() const { return last_error_; }
  size_t resident_bytes() const { return resident_bytes_; }
  size_t resident_pages() const {
    return size_t(std::count_if(pages_.begin(), pages_.end(),
                                [](auto *p) { return p != nullptr; }));
  }
  bool draw_glyph(const SourceFontGlyph &g, float x, float y, float sx,
                  float sy, uint32_t color, float depth = 0) const {
    if (!g.width || !g.height)
      return true;
    return region(g, g.u, g.v, g.width, g.height, x + g.offset_x * sx,
                  y + g.offset_y * sy, g.width * sx, g.height * sy, color,
                  depth);
  }
  bool draw_glyph_clipped(const SourceFontGlyph &g, float x, float y,
                          float left, float top, float right, float bottom,
                          uint32_t color) const {
    const float gx = std::floor(x + g.offset_x + .5f),
                gy = std::floor(y + g.offset_y + .5f);
    const int x0 = std::max(0, int(std::ceil(left - gx))),
              y0 = std::max(0, int(std::ceil(top - gy)));
    const int x1 = std::min(int(g.width), int(std::floor(right - gx))),
              y1 = std::min(int(g.height), int(std::floor(bottom - gy)));
    return x1 <= x0 || y1 <= y0 ||
           region(g, g.u + x0, g.v + y0, x1 - x0, y1 - y0, gx + x0, gy + y0,
                  x1 - x0, y1 - y0, color, 0);
  }

private:
  SourceFontCatalog catalog_;
  std::string page_root_;
  std::vector<LoadingSpriteSheet> pages_;
  size_t resident_bytes_ = 0;
  bool drawing_ = false;
  mutable std::string last_error_;
  bool boundary(std::string &error) const {
    if (drawing_) {
      error = "Source font mutation inside active frame rejected";
      return false;
    }
    return true;
  }
  void release_pages() {
    for (auto *page : pages_)
      if (page)
        loading_sprite_sheet_free(page);
    pages_.clear();
    resident_bytes_ = 0;
  }
  bool verify_page(const SourceFontPage &p, std::string &error) const {
    const auto path = page_root_ + p.path;
    FILE *file = std::fopen(path.c_str(), "rb");
    if (!file) {
      error = "Source font page unavailable: " + path;
      return false;
    }
    uint32_t crc = UINT32_MAX;
    size_t count = 0;
    uint8_t buffer[8192];
    bool ok = true;
    while (count < p.file_bytes) {
      const size_t amount =
          std::min(sizeof(buffer), size_t(p.file_bytes) - count);
      if (std::fread(buffer, 1, amount, file) != amount) {
        ok = false;
        break;
      }
      crc = crc32_update(crc, buffer, amount);
      count += amount;
    }
    if (std::fgetc(file) != EOF || std::ferror(file))
      ok = false;
    if (std::fclose(file) != 0)
      ok = false;
    if (!ok || (crc ^ UINT32_MAX) != p.crc32) {
      error = "Source font page length/CRC mismatch: " + path;
      return false;
    }
    return true;
  }
  bool admit(const std::vector<uint32_t> &ids, std::string &error) {
    size_t incoming = 0;
    for (auto id : ids) {
      if (id >= pages_.size()) {
        error = "Source font page index rejected";
        return false;
      }
      if (!pages_[id])
        incoming += catalog_.pages()[id].texture_bytes;
    }
    if (incoming > maximum_resident_bytes - resident_bytes_) {
      error = "Source font resident admission budget exceeded";
      return false;
    }
    // Keep existing pages on failure; newly admitted pages are not yet used
    // by any frame. Texture addresses remain stable for every live page.
    std::vector<uint32_t> added;
    for (auto id : ids) {
      if (pages_[id])
        continue;
      const auto &p = catalog_.pages()[id];
      LoadingSpriteSheet sheet = nullptr;
      if (verify_page(p, error))
        sheet =
            loading_sprite_sheet_load((page_root_ + p.path).c_str(), &error);
      if (sheet) {
        const auto image = loading_sprite_sheet_get_image(sheet, 0);
        if (loading_sprite_sheet_count(sheet) != 1 || !image.tex ||
            !image.subtex || image.tex->fmt != GPU_LA8 ||
            image.tex->size != p.texture_bytes ||
            image.subtex->width != p.width ||
            image.subtex->height != p.height ||
            Tex3DS_SubTextureRotated(image.subtex)) {
          error = "Source font texture dimensions/format mismatch";
          loading_sprite_sheet_free(sheet);
          sheet = nullptr;
        }
      }
      if (!sheet) {
        for (auto rollback : added) {
          resident_bytes_ -= catalog_.pages()[rollback].texture_bytes;
          loading_sprite_sheet_free(pages_[rollback]);
          pages_[rollback] = nullptr;
        }
        return false;
      }
      C3D_TexSetFilter(&sheet->texture, GPU_NEAREST, GPU_NEAREST);
      pages_[id] = sheet;
      resident_bytes_ += p.texture_bytes;
      added.push_back(id);
    }
    error.clear();
    return true;
  }
  bool region(const SourceFontGlyph &g, uint32_t u, uint32_t v, uint32_t w,
              uint32_t h, float x, float y, float width, float height,
              uint32_t color, float depth) const {
    if (g.page >= pages_.size() || !pages_[g.page]) {
      last_error_ = "Source font atlas page not admitted before frame";
      return false;
    }
    const auto &page = catalog_.pages()[g.page];
    if (!w || !h || u > page.width || w > page.width - u || v > page.height ||
        h > page.height - v || width <= 0 || height <= 0)
      return false;
    const auto image = loading_sprite_sheet_get_image(pages_[g.page], 0);
    auto sub = *image.subtex;
    const float du = (sub.right - sub.left) / page.width,
                dv = (sub.bottom - sub.top) / page.height, left = sub.left,
                top = sub.top;
    sub.left = left + u * du;
    sub.right = left + (u + w) * du;
    sub.top = top + v * dv;
    sub.bottom = top + (v + h) * dv;
    sub.width = w;
    sub.height = h;
    C2D_ImageTint tint;
    // White text needs opacity only. Solid white tint would also whiten
    // the atlas's black outline, filling the small source glyphs.
    if ((color & 0x00ffffffu) == 0x00ffffffu)
      C2D_AlphaImageTint(&tint, float(color >> 24) / 255.f);
    else
      C2D_PlainImageTint(&tint, color, 1);
    return C2D_DrawImageAt({image.tex, &sub}, x, y, depth, &tint, width / w,
                           height / h);
  }
};
} // namespace encore::ctr
