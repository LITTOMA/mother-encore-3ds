#pragma once
#include "encore/player_effects.hpp"
#include <citro2d.h>
#include <cmath>
namespace encore::ctr {
// Sample from the same actual native Sprite/material owner that receives the
// source AnimationPlayer tracks. This helper keeps no second animation clock.
struct PlayerEffectGpuSample {
  upstream::FieldObjectId object = 0;
  C2D_Image image{};
  std::string source;
  std::array<uint8_t, 32> source_sha{};
  uint32_t columns = 0, rows = 0, frame = 0;
  upstream::Vec2 offset{};
  bool centered = false, flip_h = false, flip_v = false, visible = false;
  bool additive = false, flash_material = false;
  upstream::FieldColor flash_color{};
  float flash_modifier = 0, glow_modifier = 0;
};
class PlayerEffectsRenderer {
public:
  bool draw(const upstream::PlayerEffectsData &data,
            upstream::FieldGlobalRegistry &registry,
            const PlayerEffectGpuSample &s, upstream::Vec2 camera,
            std::string &e) const {
    auto tree = registry.tree_owner(s.object);
    auto node = tree ? tree->state(s.object) : nullptr;
    auto descriptor = tree ? tree->descriptor(s.object) : nullptr;
    upstream::FieldIdentity identity{};
    bool effect_source = false;
    if (data.valid() && tree && descriptor &&
        tree->object_identity(s.object, identity))
      for (uint32_t kind = 0; kind < 2; ++kind) {
        auto recipe = data.recipe(kind);
        auto expected = recipe->record(descriptor->id);
        auto wanted = recipe->identity();
        if (expected && expected->native_class == "Sprite" &&
            descriptor->native_class == expected->native_class &&
            descriptor->script_sha == expected->script_sha &&
            identity.scene_id == wanted.scene_id &&
            identity.upstream_commit == wanted.upstream_commit &&
            identity.source_sha256 == wanted.source_sha256)
          effect_source = true;
      }
    std::array<uint8_t, 32> source{};
    if (!data.valid() || !effect_source || !node || !node->inside ||
        !s.image.tex || !s.image.subtex ||
        !data.source_hash(s.source, source) || source != s.source_sha ||
        !s.columns || !s.rows || uint64_t(s.columns) * s.rows <= s.frame) {
      e = "Player effect actual Sprite/GPU source rejected";
      return false;
    }
    if (!s.visible || !tree->visible_in_tree(s.object)) {
      e.clear();
      return true;
    }
    auto sub = *s.image.subtex;
    if (Tex3DS_SubTextureRotated(&sub) || sub.width % s.columns ||
        sub.height % s.rows) {
      e = "Player effect checked texture frame geometry rejected";
      return false;
    }
    const uint32_t width = sub.width / s.columns, height = sub.height / s.rows;
    float du = (sub.right - sub.left) / sub.width,
          dv = (sub.bottom - sub.top) / sub.height;
    float left = sub.left + float(s.frame % s.columns * width) * du,
          top = sub.top + float(s.frame / s.columns * height) * dv;
    sub.left = left;
    sub.right = left + width * du;
    sub.top = top;
    sub.bottom = top + height * dv;
    sub.width = width;
    sub.height = height;
    if (s.flip_h)
      std::swap(sub.left, sub.right);
    if (s.flip_v)
      std::swap(sub.top, sub.bottom);
    auto image = s.image;
    image.subtex = &sub;
    C3D_TexSetFilter(image.tex, GPU_NEAREST, GPU_NEAREST);
    upstream::FieldTransform world{};
    upstream::FieldColor color{};
    if (!tree->world_transform(s.object, world, e) ||
        !tree->effective_color(s.object, color, e))
      return false;
    if (color[3] == 0) {
      e.clear();
      return true;
    }
    // This current effect source has translation-only parent geometry. A future
    // rotated/skewed parent is an explicit unsupported drawing capability.
    if (world[0].x != 1 || world[0].y != 0 || world[1].x != 0 ||
        world[1].y != 1) {
      e = "Player effect transformed Canvas geometry requires actual affine "
          "GPU owner";
      return false;
    }
    float x = s.offset.x, y = s.offset.y;
    if (s.centered) {
      x = std::floor(x - width * .5f);
      y = std::floor(y - height * .5f);
    }
    x = std::floor(world[2].x + x - camera.x + .5f);
    y = std::floor(world[2].y + y - camera.y + .5f);
    float blend = 0;
    if (s.flash_material) {
      if (s.flash_modifier != 1) {
        e = "Player effect Flash shader branch not admitted by this GPU "
            "primitive";
        return false;
      }
      for (size_t i = 0; i < 3; ++i)
        color[i] *= s.flash_color[i];
      blend = 1;
    }
    auto channel = [](float v) {
      return uint8_t(std::round(std::fmax(0.f, std::fmin(1.f, v)) * 255));
    };
    for (auto v : color)
      if (!std::isfinite(v)) {
        e = "Player effect actual Canvas/material color nonfinite";
        return false;
      }
    C2D_ImageTint tint;
    C2D_PlainImageTint(&tint,
                       C2D_Color32(channel(color[0]), channel(color[1]),
                                   channel(color[2]), channel(color[3])),
                       blend);
    C2D_Flush();
    if (s.additive)
      C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE,
                     GPU_SRC_ALPHA, GPU_ONE);
    bool result = C2D_DrawImageAt(image, x, y, 0, &tint, 1, 1);
    C2D_Flush();
    if (s.additive)
      C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA,
                     GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
    if (!result) {
      e = "Player effect actual GPU submission failed";
      return false;
    }
    e.clear();
    return true;
  }
};
} // namespace encore::ctr
