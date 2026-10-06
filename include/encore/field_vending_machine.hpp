#pragma once
#include "encore/field_geometry.hpp"
#include "encore/field_shop.hpp"
namespace encore::upstream {
struct FieldVendingDescriptor {
  uint32_t id = 0, ready = 0, sprite = 0, area = 0, prompt = 0, body = 0,
           order = 0, layer = 0, mask = 0, shape = 0;
  bool centered = false, visible = false, z_relative = false;
  int32_t z = 0;
  Vec2 root_position{}, sprite_position{}, offset{};
  std::array<float, 4> modulate{}, self_modulate{};
  std::string scene, node, shop, program, area_path, body_path, shape_path,
      script_source;
};
class FieldVendingData {
public:
  bool load(const uint8_t *, size_t, std::string &);
  bool load_file(const char *, std::string &);
  bool valid() const { return valid_; }
  const auto &source_pin() const { return pin_; }
  const auto &scene_hash() const { return scene_; }
  const auto &script_hash() const { return script_; }
  uint32_t scene_id() const { return scene_id_; }
  const auto &descriptor() const { return descriptor_; }
  const auto &texture() const { return texture_; }
  bool bind_geometry(const FieldGeometryView &, std::string &) const;

private:
  bool valid_ = false;
  uint32_t scene_id_ = 0;
  std::array<uint8_t, 20> pin_{};
  std::array<uint8_t, 32> scene_{}, script_{};
  FieldVendingDescriptor descriptor_;
  FieldShopTexture texture_;
};
struct FieldVendingPlayer {
  uint32_t id = 0, kind = 0;
  bool controlled = false;
};
struct FieldVendingHost {
  std::function<bool(const FieldVendingData &, const FieldShopData &,
                     std::string &)>
      bind;
  std::function<bool(FieldVendingPlayer &, std::string &)> player;
  std::function<bool(const std::string &, std::string &)> set_current_shop;
  std::function<bool(uint32_t, const std::string &, std::string &)>
      open_dialogue;
  std::function<bool(uint32_t, std::string &)> unpause;
};
class FieldVendingRuntime {
public:
  bool initialize(const FieldVendingData &, const FieldShopData &,
                  const FieldGeometryView &, FieldVendingHost, std::string &);
  bool interact(uint32_t actual_player, std::string &);
  bool programme_end(uint32_t source_object, const std::string &source_program,
                     std::string &);
  bool active() const { return active_; }
  const FieldVendingData *data() const { return data_; }

private:
  const FieldVendingData *data_ = nullptr;
  FieldVendingHost host_;
  bool active_ = false;
  uint32_t player_ = 0;
};
} // namespace encore::upstream
