#pragma once
#include "battle_renderer.hpp"
#include "encore/field_present.hpp"
#include "loading_texture.hpp"
#include <fstream>
// Independent source Present visual owner, nearest tex3ds pixels at source
// size. Scene visibility, camera and independently checked Tint/self-modulation
// are provided by the real scene host; box tint never leaks into sibling
// Sparkles.
class FieldPresentRenderer {
  const encore::upstream::FieldPresentData *data_ = nullptr;
  encore::ctr::LoadingSpriteSheet box_ = nullptr, sparkles_ = nullptr;
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
                    uint32_t color) {
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
    const float lx = std::floor(-float(w) / 2), ly = std::floor(-float(h) / 2);
    const float left = std::floor(x + lx * sx + .5f),
                top = std::floor(y + ly * sy + .5f);
    const float right = std::floor(x + (lx + w) * sx + .5f),
                bottom = std::floor(y + (ly + h) * sy + .5f);
    if (left == right || top == bottom)
      return true;
    return C2D_DrawImageAt({im.tex, &sub}, left, top, 0, &tint,
                           (right - left) / w, (bottom - top) / h);
  }

public:
  ~FieldPresentRenderer() { free(); }
  void free() {
    if (box_ || sparkles_)
      C3D_FrameSync();
    if (box_)
      encore::ctr::loading_sprite_sheet_free(box_);
    if (sparkles_)
      encore::ctr::loading_sprite_sheet_free(sparkles_);
    box_ = sparkles_ = nullptr;
    data_ = nullptr;
  }
  bool load(const encore::upstream::FieldPresentData &d, const char *root,
            std::string &e) {
    if (!d.valid() || !root || !*root || !d.box_bytes() || !d.box_crc()) {
      e = "Present GPU source owner/receipt absent";
      return false;
    }
    FieldPresentRenderer next;
    auto read = [&](const std::string &path, uint32_t bytes, uint32_t checksum,
                    uint32_t width, uint32_t height,
                    encore::ctr::LoadingSpriteSheet &sheet) {
      const auto file = std::string(root) + path;
      std::ifstream f(file, std::ios::binary);
      if (!f) {
        e = "Present GPU asset unavailable";
        return false;
      }
      f.seekg(0, std::ios::end);
      if (f.tellg() != bytes) {
        e = "Present GPU byte count";
        return false;
      }
      f.seekg(0);
      std::vector<uint8_t> b(bytes);
      if (!f.read(reinterpret_cast<char *>(b.data()), bytes) ||
          crc(b) != checksum) {
        e = "Present GPU source CRC";
        return false;
      }
      sheet = encore::ctr::loading_sprite_sheet_acquire(file.c_str(), &e);
      if (!sheet || encore::ctr::loading_sprite_sheet_count(sheet) != 1)
        return false;
      auto im = encore::ctr::loading_sprite_sheet_get_image(sheet, 0);
      if (!im.tex || !im.subtex || Tex3DS_SubTextureRotated(im.subtex) ||
          im.subtex->width != width || im.subtex->height != height) {
        e = "Present GPU texture shape";
        return false;
      }
      C3D_TexSetFilter(im.tex, GPU_NEAREST, GPU_NEAREST);
      return true;
    };
    if (!read(d.box_path(), d.box_bytes(), d.box_crc(), d.width(), d.height(),
              next.box_) ||
        !read(d.sparkles_path(), d.sparkles_bytes(), d.sparkles_crc(),
              d.sparkles_width(), d.sparkles_height(), next.sparkles_))
      return false;
    C3D_FrameSync();
    std::swap(box_, next.box_);
    std::swap(sparkles_, next.sparkles_);
    data_ = &d;
    e.clear();
    return true;
  }
  bool draw(const encore::upstream::FieldPresentRuntime &r, uint32_t id,
            encore::upstream::Vec2 camera, float width, float height,
            float left, float top, bool ancestor_visible, uint32_t box_tint,
            uint32_t inherited_tint, std::string &e,
            bool draw_sparkles = true) const {
    const auto *s = r.state(id);
    auto *b = data_ ? data_->binding(id) : nullptr;
    if (r.content() != data_ || !s || !b || !s->parent_ready) {
      e = "Present GPU live source instance";
      return false;
    }
    if (!std::isfinite(camera.x) || !std::isfinite(camera.y) ||
        !std::isfinite(width) || !std::isfinite(height) ||
        !std::isfinite(left) || !std::isfinite(top) || width <= 0 ||
        height <= 0) {
      e = "Present GPU camera/viewport";
      return false;
    }
    if (!s->alive || !s->visible || !ancestor_visible) {
      e.clear();
      return true;
    }
    const float dx = left + width / 2 - camera.x,
                dy = top + height / 2 - camera.y;
    auto frame_width = data_->width() / data_->frames();
    if (s->frame >= data_->frames() ||
        !image(box_, s->frame * frame_width, 0, frame_width, data_->height(),
               data_->width(), data_->height(), b->sprite_position.x + dx,
               b->sprite_position.y + dy, b->scale.x, b->scale.y, box_tint)) {
      e = "Present GPU source box frame";
      return false;
    }
    if (draw_sparkles && s->sparkle_visible) {
      if (s->sparkle_frame >= data_->sparkle_frames().size()) {
        e = "Present GPU source sparkle frame";
        return false;
      }
      const auto &f = data_->sparkle_frames()[s->sparkle_frame];
      if (!image(sparkles_, f.x, f.y, f.width, f.height,
                 data_->sparkles_width(), data_->sparkles_height(),
                 b->sparkles_position.x + dx, b->sparkles_position.y + dy,
                 b->scale.x, b->scale.y, inherited_tint)) {
        e = "Present GPU source Sparkles draw";
        return false;
      }
    }
    e.clear();
    return true;
  }
};
