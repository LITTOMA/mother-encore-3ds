#pragma once
#include "basement_actor_renderer.hpp"
#include "encore/field_prompts.hpp"
#include <algorithm>
#include <cmath>
// Actual source A atlas is split into independently animated Label and Arrow.
// HBox positions come from the official source-font layout receipt; no font or
// raster conversion, file reads or texture uploads occur during drawing.
class FieldPromptRenderer {
  const encore::upstream::FieldPromptData *data_ = nullptr;
  encore::ctr::LoadingSpriteSheet sheet_ = nullptr;
  mutable bool bind_ = true;
  static uint32_t rgba(const std::array<float, 4> &v, float alpha = 1) {
    return C2D_Color32(uint8_t(std::lround(v[0] * 255)),
                       uint8_t(std::lround(v[1] * 255)),
                       uint8_t(std::lround(v[2] * 255)),
                       uint8_t(std::lround(v[3] * alpha * 255)));
  }
  bool piece(const std::array<uint32_t, 4> &crop, encore::upstream::Vec2 local,
             encore::upstream::Vec2 world, encore::upstream::Vec2 scale,
             float dx, float dy, uint32_t color, float mix, bool snap) const {
    auto image = encore::ctr::loading_sprite_sheet_get_image(sheet_, 0);
    auto sub = *image.subtex;
    const auto &a = data_->art();
    float du = (sub.right - sub.left) / a.width,
          dv = (sub.bottom - sub.top) / a.height, u = sub.left, v = sub.top;
    sub.left = u + crop[0] * du;
    sub.right = u + (crop[0] + crop[2]) * du;
    sub.top = v + crop[1] * dv;
    sub.bottom = v + (crop[1] + crop[3]) * dv;
    sub.width = uint16_t(crop[2]);
    sub.height = uint16_t(crop[3]);
    float x = world.x + local.x * scale.x + dx,
          y = world.y + local.y * scale.y + dy;
    if (scale.x < 0) {
      x += crop[2] * scale.x;
      std::swap(sub.left, sub.right);
    }
    if (scale.y < 0) {
      y += crop[3] * scale.y;
      std::swap(sub.top, sub.bottom);
    }
    if (snap) {
      x = std::floor(x + .5f);
      y = std::floor(y + .5f);
    }
    C2D_ImageTint tint;
    C2D_PlainImageTint(&tint, color, mix);
    return C2D_DrawImageAt({image.tex, &sub}, x, y, 0, &tint, std::abs(scale.x),
                           std::abs(scale.y));
  }

public:
  ~FieldPromptRenderer() { free(); }
  FieldPromptRenderer() = default;
  FieldPromptRenderer(const FieldPromptRenderer &) = delete;
  FieldPromptRenderer &operator=(const FieldPromptRenderer &) = delete;
  void free() {
    encore::ctr::loading_sprite_sheet_free(sheet_);
    sheet_ = nullptr;
    data_ = nullptr;
    bind_ = true;
  }
  bool load(const encore::upstream::FieldPromptData &data, const char *prefix,
            std::string &error) {
    free();
    if (!data.valid() || !prefix) {
      error = "Prompt renderer requires admitted source data";
      return false;
    }
    const auto &a = data.art();
    std::string path = std::string(prefix) + a.path;
    FILE *f = std::fopen(path.c_str(), "rb");
    if (!f) {
      error = "Prompt GPU texture unavailable";
      return false;
    }
    std::vector<uint8_t> bytes(a.bytes);
    const bool ok =
        std::fread(bytes.data(), 1, bytes.size(), f) == bytes.size() &&
        std::fgetc(f) == EOF && !std::ferror(f);
    const bool closed = std::fclose(f) == 0;
    if (!ok || !closed ||
        basement_actor_renderer_detail::sha256(bytes.data(), bytes.size()) !=
            a.sha256) {
      error = "Prompt GPU texture bytes rejected";
      return false;
    }
    auto *sheet = new (std::nothrow) encore::ctr::LoadingSpriteSheetData;
    if (!sheet) {
      error = "Prompt GPU owner allocation failed";
      return false;
    }
    sheet->metadata = Tex3DS_TextureImport(bytes.data(), bytes.size(),
                                           &sheet->texture, nullptr, false);
    if (!sheet->metadata) {
      delete sheet;
      error = "Prompt actual tex3ds import rejected";
      return false;
    }
    sheet_ = sheet;
    const auto image = encore::ctr::loading_sprite_sheet_get_image(sheet_, 0);
    if (encore::ctr::loading_sprite_sheet_count(sheet_) != 1 || !image.tex ||
        !image.subtex || Tex3DS_SubTextureRotated(image.subtex) ||
        image.subtex->width != a.width || image.subtex->height != a.height) {
      free();
      error = "Prompt GPU atlas extent rejected";
      return false;
    }
    C3D_TexSetFilter(image.tex, GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetWrap(image.tex, GPU_CLAMP_TO_BORDER, GPU_CLAMP_TO_BORDER);
    sheet_->texture.border = 0;
    data_ = &data;
    error.clear();
    return true;
  }
  bool draw(const encore::upstream::FieldPromptRuntime &runtime, uint32_t id,
            encore::upstream::Vec2 parent_world,
            encore::upstream::Vec2 parent_scale, float dx, float dy,
            bool ancestor_visible, bool source_pixel_snap,
            std::string &error) const {
    auto reject = [&](const char *t) {
      error = t;
      return false;
    };
    if (!data_ || !sheet_)
      return reject("Prompt GPU renderer not admitted");
    const auto *p = runtime.instance(id);
    if (!p || !p->ready)
      return reject("Prompt source Canvas instance absent");
    for (float f : {parent_world.x, parent_world.y, parent_scale.x,
                    parent_scale.y, dx, dy})
      if (!std::isfinite(f))
        return reject("Prompt draw transform rejected");
    if (!ancestor_visible || !p->visible() || parent_scale.x == 0 ||
        parent_scale.y == 0) {
      error.clear();
      return true;
    }
    if (p->label != data_->art().label)
      return reject("Prompt input label has no admitted source glyph");
    if (bind_) {
      C2D_Flush();
      C3D_TexBind(0, &sheet_->texture);
      bind_ = false;
    }
    const auto &a = data_->art();
    const auto &v = p->properties;
    const encore::upstream::Vec2 world = {parent_world.x +
                                              p->position.x * parent_scale.x,
                                          parent_world.y +
                                              p->position.y * parent_scale.y},
                                 scale = {p->scale.x * parent_scale.x,
                                          p->scale.y * parent_scale.y};
    // The source Label does not inherit the Flash material; Arrow explicitly
    // does.
    const encore::upstream::Vec2 label = {v[1][0] + v[2][0] + a.label_origin.x,
                                          v[1][1] + v[2][1] + a.label_origin.y},
                                 arrow = {v[0][0] + a.arrow_origin.x,
                                          v[0][1] + a.arrow_origin.y};
    if (!piece(a.label_crop, label, world, scale, dx, dy, rgba(v[3], v[4][3]),
               0, source_pixel_snap) ||
        !piece(a.arrow_crop, arrow, world, scale, dx, dy,
               rgba(v[8], v[4][3] * v[5][3]), v[9][0], source_pixel_snap))
      return reject("Prompt GPU submission rejected");
    error.clear();
    return true;
  }
};
