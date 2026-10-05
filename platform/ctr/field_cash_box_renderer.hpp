#pragma once
#include "battle_renderer.hpp"
#include "encore/field_cash_box.hpp"
#include "field_payphone_renderer.hpp"
#include "source_font_renderer.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <new>
#include <vector>
class FieldCashBoxRenderer {
  using Data = encore::upstream::FieldCashBoxData;
  const Data *data_ = nullptr;
  std::vector<encore::ctr::LoadingSpriteSheet> sheets_;
  static bool fail(std::string &e, const char *s) {
    e = s;
    return false;
  }
  bool region(uint32_t texture, float u, float v, float sw, float sh, float x,
              float y, float w, float h, float depth, float screenw,
              float screenh) const {
    if (texture >= sheets_.size() || sw <= 0 || sh <= 0 || w <= 0 || h <= 0)
      return false;
    if (x + w <= 0 || y + h <= 0 || x >= screenw || y >= screenh)
      return true;
    const float l = std::max(0.f, -x), t = std::max(0.f, -y),
                r = std::max(0.f, x + w - screenw),
                b = std::max(0.f, y + h - screenh);
    u += l * sw / w;
    v += t * sh / h;
    sw -= (l + r) * sw / w;
    sh -= (t + b) * sh / h;
    x += l;
    y += t;
    w -= l + r;
    h -= t + b;
    auto image =
        encore::ctr::loading_sprite_sheet_get_image(sheets_[texture], 0);
    if (!image.subtex || !image.tex)
      return false;
    auto sub = *image.subtex;
    const auto &tex = data_->textures()[texture];
    const float du = (sub.right - sub.left) / tex.width,
                dv = (sub.top - sub.bottom) / tex.height;
    sub.left += u * du;
    sub.right = sub.left + sw * du;
    sub.top -= v * dv;
    sub.bottom = sub.top - sh * dv;
    sub.width = uint16_t(std::ceil(sw));
    sub.height = uint16_t(std::ceil(sh));
    if (!sub.width || !sub.height)
      return true;
    image.subtex = &sub;
    C2D_ImageTint tint;
    C2D_PlainImageTint(&tint, 0xffffffff, 0);
    return C2D_DrawImageAt(image, x, y, depth, &tint, w / sub.width,
                           h / sub.height);
  }

public:
  FieldCashBoxRenderer() = default;
  FieldCashBoxRenderer(const FieldCashBoxRenderer &) = delete;
  FieldCashBoxRenderer &operator=(const FieldCashBoxRenderer &) = delete;
  ~FieldCashBoxRenderer() { free(); }
  void free() {
    for (auto *s : sheets_)
      encore::ctr::loading_sprite_sheet_free(s);
    sheets_.clear();
    data_ = nullptr;
  }
  // Call outside a GPU frame. Source colors/threshold come from this checked
  // binary; the complete checked current palettes/base belong to the existing
  // source menu-flavor service. No independent palette is invented here.
  bool load(const Data &d, const std::string &prefix,
            const std::vector<std::array<uint32_t, 8>> &palettes,
            uint32_t base_index, std::string &e) {
    free();
    if (!d.valid() || prefix.empty())
      return fail(e, "Cash box source GPU admission absent");
    std::vector<std::string> paths;
    for (const auto &t : d.textures())
      if (t.flavor)
        paths.push_back(t.path);
    if (!encore::ctr::loading_menu_flavor_register_checked_paths(
            paths, d.source_colors(), palettes, d.threshold(), base_index))
      return fail(e, "Cash box source MenuFlavors binding rejected");
    for (const auto &t : d.textures()) {
      FILE *f = std::fopen((prefix + t.path).c_str(), "rb");
      if (!f) {
        free();
        return fail(e, "Cash box genuine atlas unavailable");
      }
      std::vector<uint8_t> bytes(t.bytes);
      const bool read =
          std::fread(bytes.data(), 1, bytes.size(), f) == bytes.size();
      const bool exact = std::fgetc(f) == EOF && !std::ferror(f);
      const bool closed = std::fclose(f) == 0;
      if (!read || !exact || !closed ||
          field_payphone_renderer_detail::sha256(bytes.data(), bytes.size()) !=
              t.sha) {
        free();
        return fail(e, "Cash box exact atlas bytes/SHA rejected");
      }
      auto *sheet = new (std::nothrow) encore::ctr::LoadingSpriteSheetData;
      if (!sheet) {
        free();
        return fail(e, "Cash box atlas allocation failed");
      }
      sheet->metadata = Tex3DS_TextureImport(bytes.data(), bytes.size(),
                                             &sheet->texture, nullptr, false);
      if (!sheet->metadata) {
        delete sheet;
        free();
        return fail(e, "Cash box actual tex3ds import failed");
      }
      sheet->source_path =
          encore::ctr::loading_texture_key((prefix + t.path).c_str());
      auto image = encore::ctr::loading_sprite_sheet_get_image(sheet, 0);
      bool valid = encore::ctr::loading_sprite_sheet_count(sheet) == 1 &&
                   image.tex && image.subtex;
      if (valid)
        valid = !Tex3DS_SubTextureRotated(image.subtex) &&
                image.subtex->width == t.width &&
                image.subtex->height == t.height &&
                image.subtex->right > image.subtex->left &&
                image.subtex->top > image.subtex->bottom;
      if (valid)
        for (float v : {image.subtex->left, image.subtex->right,
                        image.subtex->top, image.subtex->bottom})
          valid = valid && std::isfinite(v) && v >= 0 && v <= 1;
      if (!valid || !encore::ctr::loading_menu_flavor_detail::prepare(sheet)) {
        delete sheet;
        free();
        return fail(e, "Cash box atlas dimensions/UV/palette pixels rejected");
      }
      encore::ctr::loading_menu_flavor_detail::sheets.push_back(sheet);
      encore::ctr::loading_menu_flavor_detail::apply(sheet);
      sheet->texture.border = 0;
      C3D_TexSetFilter(&sheet->texture, GPU_NEAREST, GPU_NEAREST);
      C3D_TexSetWrap(&sheet->texture, GPU_CLAMP_TO_BORDER, GPU_CLAMP_TO_BORDER);
      sheets_.push_back(sheet);
    }
    data_ = &d;
    e.clear();
    return true;
  }
  static bool measure(const Data &d, const BattleRenderer &font,
                      const encore::ctr::SourceFontRenderer &source,
                      const std::string &text, encore::upstream::Vec2 &size,
                      std::string &e) {
    const auto *face = source.catalog().selected();
    if (!face || face->source != d.font() || !std::isfinite(face->height) ||
        face->height <= 0)
      return fail(e, "Cash box source font binding rejected");
    size_t at = 0;
    uint32_t cp;
    while (at < text.size()) {
      if (!encore::utf8_next(text, at, cp))
        return fail(e, "Cash box font UTF-8 rejected");
      if (source.handles(cp) && !source.glyph(cp))
        return fail(e, "Cash box source font glyph not admitted");
    }
    const float w = font.text_width(text.c_str());
    if (!std::isfinite(w) || w < 0)
      return fail(e, "Cash box measured width rejected");
    size = {w, face->height};
    e.clear();
    return true;
  }
  bool draw(const encore::upstream::FieldCashBoxRuntime &runtime,
            const BattleRenderer &font,
            const encore::ctr::SourceFontRenderer &source, uint32_t width,
            uint32_t height, float depth, uint32_t checked_theme_text_color,
            std::string &e) const {
    if (!data_ || runtime.data() != data_ ||
        sheets_.size() != data_->textures().size() || !std::isfinite(depth) ||
        depth < 0 || depth > 1)
      return fail(e, "Cash box live GPU binding rejected");
    const auto *face = source.catalog().selected();
    if (!face || face->source != data_->font())
      return fail(e, "Cash box selected source font changed");
    std::vector<encore::upstream::FieldCashDraw> commands;
    for (unsigned mode = 0; mode < 2; ++mode) {
      if (!runtime.draw_commands(mode, width, height, commands, e))
        return false;
      for (const auto &c : commands) {
        auto r = c.rect;
        if (data_->pixel_snap())
          for (float &v : r)
            v = std::floor(v + .5f);
        if (c.kind == 1) {
          if (c.index >= data_->box(mode).styles.size())
            return fail(e, "Cash box style view index rejected");
          const auto &s = data_->box(mode).styles[c.index];
          r[0] -= s.expand[0];
          r[1] -= s.expand[1];
          r[2] += s.expand[2];
          r[3] += s.expand[3];
          const float tx[4] = {r[0], r[0] + s.patch[0], r[2] - s.patch[2],
                               r[2]},
                      ty[4] = {r[1], r[1] + s.patch[1], r[3] - s.patch[3],
                               r[3]};
          const float sx[4] = {s.region[0], s.region[0] + s.patch[0],
                               s.region[0] + s.region[2] - s.patch[2],
                               s.region[0] + s.region[2]},
                      sy[4] = {s.region[1], s.region[1] + s.patch[1],
                               s.region[1] + s.region[3] - s.patch[3],
                               s.region[1] + s.region[3]};
          if (tx[2] < tx[1] || ty[2] < ty[1])
            return fail(e, "Cash box source patch destination rejected");
          for (unsigned y = 0; y < 3; ++y)
            for (unsigned x = 0; x < 3; ++x)
              if (tx[x + 1] > tx[x] && ty[y + 1] > ty[y] &&
                  !region(s.texture, sx[x], sy[y], sx[x + 1] - sx[x],
                          sy[y + 1] - sy[y], tx[x], ty[y], tx[x + 1] - tx[x],
                          ty[y + 1] - ty[y], depth, width, height))
                return fail(e, "Cash box GPU patch submission failed");
        } else if (c.kind == 5) {
          const auto &t = data_->textures()[c.index];
          if (!region(c.index, 0, 0, t.width, t.height, r[0], r[1], r[2] - r[0],
                      r[3] - r[1], depth, width, height))
            return fail(e, "Cash box source icon submission failed");
        } else if (c.kind == 3) {
          if (!font.draw_text_clipped(c.text.c_str(), r[0], r[1], 0, 0, width,
                                      height, checked_theme_text_color))
            return fail(e, "Cash box admitted font submission failed");
        } else
          return fail(e, "Cash box unknown draw command");
      }
    }
    e.clear();
    return true;
  }
};
