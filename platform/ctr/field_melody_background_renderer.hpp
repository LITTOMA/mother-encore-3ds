#pragma once
#include "encore/field_melody_background.hpp"
#include "loading_texture.hpp"
#include <fstream>
class FieldMelodyBackgroundRenderer {
  const encore::upstream::FieldMelodyBackgroundData *data_ = nullptr;
  encore::ctr::LoadingSpriteSheet sheet_ = nullptr;
  static uint32_t crc(const std::vector<uint8_t> &b) {
    uint32_t c = ~0u;
    for (auto v : b) {
      c ^= v;
      for (unsigned j = 0; j < 8; ++j)
        c = (c >> 1) ^ (0xedb88320u & uint32_t(-int32_t(c & 1)));
    }
    return ~c;
  }

public:
  ~FieldMelodyBackgroundRenderer() { free(); }
  void free() {
    if (sheet_)
      C3D_FrameSync();
    encore::ctr::loading_sprite_sheet_free(sheet_);
    sheet_ = nullptr;
    data_ = nullptr;
  }
  bool load(const encore::upstream::FieldMelodyBackgroundData &d,
            const char *root, std::string &e) {
    if (!d.valid() || !root || !*root) {
      e = "Melody GPU source owner absent";
      return false;
    }
    const auto &a = d.asset();
    auto path = std::string(root) + a.path;
    std::ifstream f(path, std::ios::binary);
    if (!f) {
      e = "Melody GPU source texture unavailable";
      return false;
    }
    f.seekg(0, std::ios::end);
    if (f.tellg() != a.bytes) {
      e = "Melody GPU source texture size";
      return false;
    }
    f.seekg(0);
    std::vector<uint8_t> raw(a.bytes);
    if (!f.read(reinterpret_cast<char *>(raw.data()), a.bytes) ||
        crc(raw) != a.crc) {
      e = "Melody GPU source CRC";
      return false;
    }
    auto next = encore::ctr::loading_sprite_sheet_acquire(path.c_str(), &e);
    if (!next)
      return false;
    auto im = encore::ctr::loading_sprite_sheet_get_image(next, 0);
    if (encore::ctr::loading_sprite_sheet_count(next) != 1 || !im.tex ||
        !im.subtex || Tex3DS_SubTextureRotated(im.subtex) ||
        im.subtex->width != a.width || im.subtex->height != a.height) {
      encore::ctr::loading_sprite_sheet_free(next);
      e = "Melody GPU prepared tile shape";
      return false;
    }
    C3D_TexSetFilter(im.tex, GPU_NEAREST, GPU_NEAREST);
    free();
    sheet_ = next;
    data_ = &d;
    e.clear();
    return true;
  }
  // Actual root world transform/camera, inherited canvas color/YSort depth and
  // global shader TIME come from the real scene. No local cutscene-time reset.
  bool draw(const encore::upstream::FieldMelodyBackgroundRuntime &r,
            uint32_t id, encore::upstream::Vec2 world,
            encore::upstream::Vec2 camera, float shader_time, float width,
            float height, float left, float top, bool ancestor_visible,
            uint32_t inherited, float depth, std::string &e) const {
    const auto *s = r.state(id);
    if (!data_ || !sheet_ || r.content() != data_ || !data_->binding(id) ||
        !s || !s->ready) {
      e = "Melody GPU live source owner";
      return false;
    }
    const auto &p = data_->policy();
    const auto &a = data_->asset();
    if (!std::isfinite(shader_time) || shader_time < 0 || shader_time > 1e6f ||
        !std::isfinite(world.x) || !std::isfinite(world.y) ||
        !std::isfinite(camera.x) || !std::isfinite(camera.y) ||
        !std::isfinite(left) || !std::isfinite(top) || !std::isfinite(depth) ||
        !((width == p.native_viewport.x && height == p.native_viewport.y) ||
          (width == p.source_viewport.x && height == p.source_viewport.y))) {
      e = "Melody GPU actual source time/camera/viewport";
      return false;
    }
    if (!s->alive || !s->visible || !ancestor_visible ||
        s->self_modulate.w == 0) {
      e.clear();
      return true;
    }
    auto im = encore::ctr::loading_sprite_sheet_get_image(sheet_, 0);
    const auto base = *im.subtex;
    const float du = (base.right - base.left) / a.width,
                dv = (base.bottom - base.top) / a.height;
    const auto color = s->modulate, self = s->self_modulate;
    auto channel = [](uint32_t value, unsigned shift, float factor) {
      return uint32_t(std::floor(float((value >> shift) & 255) * factor + .5f));
    };
    const uint32_t tint_color =
        channel(inherited, 0, color.x * self.x) |
        channel(inherited, 8, color.y * self.y) << 8 |
        channel(inherited, 16, color.z * self.z) << 16 |
        channel(inherited, 24, color.w * self.w * p.vertical[0]) << 24;
    C2D_ImageTint tint;
    C2D_PlainImageTint(&tint, tint_color, 0);
    const float x0 = std::floor(left + width / 2 + world.x - camera.x -
                                width / 2 + .5f),
                y0 = std::floor(top + height / 2 + world.y - camera.y -
                                height / 2 + .5f);
    const float ux0 = -width / 2 - p.rect.x, uy0 = -height / 2 - p.rect.y;
    const float osc_time = p.vertical[4] != 0
                               ? std::cos(p.vertical[4] * shader_time)
                               : shader_time;
    const float scroll = shader_time * p.vertical[5] / p.vertical[6];
    const uint32_t columns = uint32_t(width), rows = uint32_t(height);
    for (uint32_t x = 0; x < columns; ++x) {
      const float source_x = ux0 + float(x) + .5f;
      const float u = source_x / a.tile_width;
      float v = (uy0 + .5f) / a.tile_height;
      if (p.vertical[1] != 0 && p.vertical[2] != 0)
        v += p.vertical[1] *
             std::cos(p.vertical[2] * u + osc_time * p.vertical[3]);
      v += scroll;
      const double sample = std::floor(double(v * float(a.tile_height)));
      double fy = std::fmod(sample, double(a.height));
      if (fy < 0)
        fy += a.height;
      double fx = std::fmod(std::floor(double(source_x)), double(a.tile_width));
      if (fx < 0)
        fx += a.tile_width;
      const uint32_t sx = uint32_t(fx), sy = uint32_t(fy);
      uint32_t done = 0;
      while (done < rows) {
        const uint32_t source_y = done ? 0 : sy,
                       amount = std::min(rows - done, a.height - source_y);
        auto sub = base;
        sub.left = base.left + du * sx;
        sub.right = sub.left + du;
        sub.top = base.top + dv * source_y;
        sub.bottom = base.top + dv * (source_y + amount);
        sub.width = 1;
        sub.height = uint16_t(amount);
        im.subtex = &sub;
        if (!C2D_DrawImageAt(im, x0 + x, y0 + done, depth, &tint)) {
          e = "Melody GPU column-strip submission";
          return false;
        }
        done += amount;
      }
    }
    e.clear();
    return true;
  }
};
