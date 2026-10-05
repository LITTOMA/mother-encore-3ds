#pragma once
#include "battle_renderer.hpp"
#include "encore/field_dropped.hpp"
#include "loading_texture.hpp"
#include <fstream>
// Source-sized nearest GPU textures. Root hierarchy modulate is supplied by the
// live scene; the independent prompt is rendered by its checked source owner.
class FieldDroppedRenderer {
  const encore::upstream::FieldDroppedData *data_ = nullptr;
  std::vector<std::pair<uint32_t, encore::ctr::LoadingSpriteSheet>> items_;
  encore::ctr::LoadingSpriteSheet sparkles_ = nullptr;
  static uint32_t crc(const std::vector<uint8_t> &b) {
    uint32_t c = ~0u;
    for (auto v : b) {
      c ^= v;
      for (unsigned j = 0; j < 8; ++j)
        c = (c >> 1) ^ (0xedb88320u & uint32_t(-int32_t(c & 1)));
    }
    return ~c;
  }
  static bool image(encore::ctr::LoadingSpriteSheet sheet, uint32_t u,
                    uint32_t v, uint32_t w, uint32_t h, uint32_t aw,
                    uint32_t ah, float x, float y, float sx, float sy,
                    float angle, uint32_t color, float depth) {
    if (sx == 0 || sy == 0)
      return true;
    auto im = encore::ctr::loading_sprite_sheet_get_image(sheet, 0);
    auto sub = *im.subtex;
    float du = (sub.right - sub.left) / aw, dv = (sub.bottom - sub.top) / ah,
          ox = sub.left, oy = sub.top;
    sub.left = ox + u * du;
    sub.right = ox + (u + w) * du;
    sub.top = oy + v * dv;
    sub.bottom = oy + (v + h) * dv;
    sub.width = uint16_t(w);
    sub.height = uint16_t(h);
    C2D_ImageTint tint;
    C2D_PlainImageTint(&tint, color, 0);
    im.subtex = &sub;
    const float lx = std::floor(-float(w) / 2), ly = std::floor(-float(h) / 2);
    const float left = std::floor(x + lx * sx + .5f),
                top = std::floor(y + ly * sy + .5f);
    const float right = std::floor(x + (lx + w) * sx + .5f),
                bottom = std::floor(y + (ly + h) * sy + .5f);
    if (left == right || top == bottom)
      return true;
    if (angle != 0) {
      const float dx = (lx + float(w) / 2) * sx, dy = (ly + float(h) / 2) * sy;
      const float cx = x + dx * std::cos(angle) - dy * std::sin(angle),
                  cy = y + dx * std::sin(angle) + dy * std::cos(angle);
      return C2D_DrawImageAtRotated(im, std::floor(cx + .5f),
                                    std::floor(cy + .5f), depth, angle, &tint,
                                    sx, sy);
    }
    return C2D_DrawImageAt(im, left, top, depth, &tint, (right - left) / w,
                           (bottom - top) / h);
  }

public:
  ~FieldDroppedRenderer() { free(); }
  void free() {
    if (!items_.empty() || sparkles_)
      C3D_FrameSync();
    for (auto &s : items_)
      encore::ctr::loading_sprite_sheet_free(s.second);
    items_.clear();
    if (sparkles_)
      encore::ctr::loading_sprite_sheet_free(sparkles_);
    sparkles_ = nullptr;
    data_ = nullptr;
  }
  bool load(const encore::upstream::FieldDroppedData &d, const char *root,
            std::string &e) {
    if (!d.valid() || !root || !*root) {
      e = "Dropped GPU source owner absent";
      return false;
    }
    FieldDroppedRenderer next;
    auto read = [&](const std::string &path, uint32_t bytes, uint32_t checksum,
                    uint32_t width, uint32_t height,
                    encore::ctr::LoadingSpriteSheet &sheet) {
      auto file = std::string(root) + path;
      std::ifstream f(file, std::ios::binary);
      if (!f) {
        e = "Dropped GPU asset unavailable";
        return false;
      }
      f.seekg(0, std::ios::end);
      if (f.tellg() != bytes) {
        e = "Dropped GPU byte count";
        return false;
      }
      f.seekg(0);
      std::vector<uint8_t> b(bytes);
      if (!f.read(reinterpret_cast<char *>(b.data()), bytes) ||
          crc(b) != checksum) {
        e = "Dropped GPU source CRC";
        return false;
      }
      sheet = encore::ctr::loading_sprite_sheet_acquire(file.c_str(), &e);
      if (!sheet || encore::ctr::loading_sprite_sheet_count(sheet) != 1) {
        if (e.empty())
          e = "Dropped GPU texture owner";
        return false;
      }
      auto im = encore::ctr::loading_sprite_sheet_get_image(sheet, 0);
      if (!im.tex || !im.subtex || Tex3DS_SubTextureRotated(im.subtex) ||
          im.subtex->width != width || im.subtex->height != height) {
        e = "Dropped GPU texture shape";
        return false;
      }
      C3D_TexSetFilter(im.tex, GPU_NEAREST, GPU_NEAREST);
      return true;
    };
    for (const auto &a : d.assets()) {
      next.items_.emplace_back(a.id, nullptr);
      if (!read(a.path, a.bytes, a.crc, a.width, a.height,
                next.items_.back().second))
        return false;
    }
    if (!read(d.sparkles_path(), d.sparkles_bytes(), d.sparkles_crc(),
              d.sparkles_width(), d.sparkles_height(), next.sparkles_))
      return false;
    C3D_FrameSync();
    items_.swap(next.items_);
    std::swap(sparkles_, next.sparkles_);
    data_ = &d;
    e.clear();
    return true;
  }
  bool draw(const encore::upstream::FieldDroppedRuntime &r, uint32_t id,
            encore::upstream::Vec2 camera, float width, float height,
            float left, float top, bool ancestor_visible, uint32_t tint,
            float depth, std::string &e, bool draw_sparkles = true) const {
    const auto *s = r.state(id);
    auto *b = data_ ? data_->binding(id) : nullptr;
    if (!b || !s || r.content() != data_ || !s->parent_ready) {
      e = "Dropped GPU live source instance";
      return false;
    }
    if (!std::isfinite(camera.x) || !std::isfinite(camera.y) ||
        !std::isfinite(width) || !std::isfinite(height) ||
        !std::isfinite(left) || !std::isfinite(top) || !std::isfinite(depth) ||
        width <= 0 || height <= 0) {
      e = "Dropped GPU camera/viewport";
      return false;
    }
    if (!s->alive || !s->visible || !ancestor_visible || s->scale.x == 0 ||
        s->scale.y == 0) {
      e.clear();
      return true;
    }
    const auto *a = data_->asset(b->asset_id);
    encore::ctr::LoadingSpriteSheet art = nullptr;
    for (const auto &i : items_)
      if (i.first == b->asset_id)
        art = i.second;
    if (!a || !art) {
      e = "Dropped GPU source art binding";
      return false;
    }
    const float dx = left + width / 2 - camera.x,
                dy = top + height / 2 - camera.y;
    float radians = float(double(s->sprite_rotation) * std::acos(-1.0) / 180.0);
    if (s->sprite_visible &&
        !image(art, 0, 0, a->width, a->height, a->width, a->height,
               s->position.x + dx, s->position.y + dy, s->scale.x, s->scale.y,
               radians, tint, depth)) {
      e = "Dropped GPU sprite";
      return false;
    }
    if (!draw_sparkles) {
      e.clear();
      return true;
    }
    if (s->sparkles_frame >= data_->sparkles_frames().size()) {
      e = "Dropped GPU Sparkles source frame";
      return false;
    }
    const auto &f = data_->sparkles_frames()[s->sparkles_frame];
    if (!image(sparkles_, f.x, f.y, f.width, f.height, data_->sparkles_width(),
               data_->sparkles_height(),
               s->position.x + b->sparkles_offset.x * s->scale.x + dx,
               s->position.y + b->sparkles_offset.y * s->scale.y + dy,
               s->scale.x, s->scale.y, 0, tint, depth)) {
      e = "Dropped GPU Sparkles";
      return false;
    }
    e.clear();
    return true;
  }
};
