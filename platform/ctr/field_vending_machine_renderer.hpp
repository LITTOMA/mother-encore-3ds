#pragma once
#include "encore/field_vending_machine.hpp"
#include "field_payphone_renderer.hpp"
class FieldVendingRenderer {
  const encore::upstream::FieldVendingData *data_ = nullptr;
  encore::ctr::LoadingSpriteSheet sheet_ = nullptr;
  static bool fail(std::string &e, const char *s) {
    e = s;
    return false;
  }

public:
  ~FieldVendingRenderer() { free(); }
  FieldVendingRenderer() = default;
  FieldVendingRenderer(const FieldVendingRenderer &) = delete;
  FieldVendingRenderer &operator=(const FieldVendingRenderer &) = delete;
  void free() {
    if (sheet_)
      encore::ctr::loading_sprite_sheet_free(sheet_);
    sheet_ = nullptr;
    data_ = nullptr;
  }
  bool load(const encore::upstream::FieldVendingData &d,
            const std::string &prefix, std::string &e) {
    free();
    if (!d.valid() || prefix.empty())
      return fail(e, "Vending GPU source absent");
    const auto &t = d.texture();
    FILE *f = std::fopen((prefix + t.path).c_str(), "rb");
    if (!f)
      return fail(e, "Vending actual atlas unavailable");
    std::vector<uint8_t> raw(t.bytes);
    bool ok = std::fread(raw.data(), 1, raw.size(), f) == raw.size() &&
              std::fgetc(f) == EOF && !std::ferror(f);
    if (std::fclose(f))
      ok = false;
    if (!ok ||
        field_payphone_renderer_detail::sha256(raw.data(), raw.size()) != t.sha)
      return fail(e, "Vending actual atlas bytes/SHA rejected");
    sheet_ = new (std::nothrow) encore::ctr::LoadingSpriteSheetData;
    if (!sheet_)
      return fail(e, "Vending GPU allocation failed");
    sheet_->metadata = Tex3DS_TextureImport(raw.data(), raw.size(),
                                            &sheet_->texture, nullptr, false);
    if (!sheet_->metadata) {
      delete sheet_;
      sheet_ = nullptr;
      return fail(e, "Vending actual tex3ds import failed");
    }
    sheet_->source_path =
        encore::ctr::loading_texture_key((prefix + t.path).c_str());
    auto image = encore::ctr::loading_sprite_sheet_get_image(sheet_, 0);
    if (encore::ctr::loading_sprite_sheet_count(sheet_) != 1 || !image.tex ||
        !image.subtex || Tex3DS_SubTextureRotated(image.subtex) ||
        image.subtex->width != t.width || image.subtex->height != t.height ||
        image.tex->fmt != GPU_RGBA8) {
      free();
      return fail(e, "Vending source extent/GPU format rejected");
    }
    C3D_TexSetFilter(&sheet_->texture, GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetWrap(&sheet_->texture, GPU_CLAMP_TO_BORDER, GPU_CLAMP_TO_BORDER);
    sheet_->texture.border = 0;
    data_ = &d;
    return true;
  }
  // Host supplies the actual MAIN child's live Canvas transform. This already
  // includes source local(4,-11); source offset is applied exactly once below.
  // color is inherited ancestor modulate; main modulate/self_modulate are data.
  bool draw(const encore::upstream::FieldVendingRuntime &r,
            const encore::upstream::FieldGeometryTransform &main_world,
            encore::upstream::Vec2 camera, float depth, bool ancestors_visible,
            bool pixel_snap, uint32_t color, std::string &e) const {
    if (!sheet_ || !data_ || r.data() != data_ || !std::isfinite(depth) ||
        depth < 0 || depth > 1)
      return fail(e, "Vending GPU typed live binding rejected");
    const auto &d = data_->descriptor();
    if (!ancestors_visible || !d.visible)
      return true;
    for (double v :
         {main_world.x.x, main_world.x.y, main_world.y.x, main_world.y.y,
          main_world.origin.x, main_world.origin.y, camera.x, camera.y})
      if (!std::isfinite(v))
        return fail(e, "Vending Canvas nonfinite transform");
    const float sx = std::hypot(float(main_world.x.x), float(main_world.x.y)),
                sy = std::hypot(float(main_world.y.x), float(main_world.y.y));
    if (!sx || !sy)
      return true;
    if (std::abs(main_world.x.x * main_world.y.x +
                 main_world.x.y * main_world.y.y) > 1e-5 * sx * sy)
      return fail(e, "Vending skew Canvas unsupported");
    float x = main_world.origin.x + main_world.x.x * d.offset.x +
              main_world.y.x * d.offset.y - camera.x,
          y = main_world.origin.y + main_world.x.y * d.offset.x +
              main_world.y.y * d.offset.y - camera.y;
    if (pixel_snap) {
      x = std::floor(x + .5f);
      y = std::floor(y + .5f);
    }
    auto image = encore::ctr::loading_sprite_sheet_get_image(sheet_, 0);
    auto sub = *image.subtex;
    if (main_world.x.x * main_world.y.y - main_world.x.y * main_world.y.x < 0)
      std::swap(sub.top, sub.bottom);
    image.subtex = &sub;
    uint32_t tint = 0;
    for (unsigned i = 0; i < 4; ++i)
      tint |= uint32_t(std::round(float((color >> (8 * i)) & 255) *
                                  d.modulate[i] * d.self_modulate[i]))
              << (8 * i);
    C2D_ImageTint colors;
    C2D_PlainImageTint(&colors, tint, 0);
    if (!C2D_DrawImageAtRotated(
            image, x, y, depth,
            std::atan2(float(main_world.x.y), float(main_world.x.x)), &colors,
            sx, sy))
      return fail(e, "Vending GPU submission rejected");
    e.clear();
    return true;
  }
};
