#pragma once
#include "encore/field_geometry_space.hpp"
#include "encore/field_global_registry.hpp"
#include "encore/field_map_space.hpp"
#include "encore/field_npc.hpp"
namespace encore::upstream {
struct FieldNpcWorldBody {
  uint32_t id = 0, layer = 0, mask = 0, platform_leave = 0;
  float margin = 0;
  std::string path;
  std::vector<std::pair<uint32_t, bool>> shapes;
};
struct FieldNpcWorldRay {
  uint32_t id = 0, parent = 0, mask = 0;
  bool enabled = false, exclude = false, bodies = false, areas = false;
  Vec2 cast{};
  std::string path;
};
struct FieldNpcWorldLink {
  uint32_t id = 0, collider = 0, interact = 0, near = 0, view = 0, wander = 0,
           ray = 0, timer = 0, shadow = 0, sprite = 0;
};
struct FieldNpcWorldCallback {
  uint32_t op = 0, arity = 0;
  std::string method;
};
class FieldNpcWorldData {
public:
  bool load(const uint8_t *, size_t, const FieldNodeTreeData &,
            const FieldNpcData &, const FieldGeometryView &, std::string &);
  bool load_file(const char *, const FieldNodeTreeData &, const FieldNpcData &,
                 const FieldGeometryView &, std::string &);
  bool valid() const { return valid_; }
  FieldIdentity identity() const { return identity_; }
  const std::vector<FieldNpcWorldBody> &bodies() const { return bodies_; }
  const std::vector<FieldNpcWorldRay> &rays() const { return rays_; }
  const FieldNpcWorldBody *body(uint32_t) const;
  const FieldNpcWorldRay *ray(uint32_t) const;
  const FieldNpcWorldLink *npc(uint32_t) const;
  const auto &callbacks() const { return callbacks_; }
  const std::string &visibility_signal() const { return visibility_signal_; }
  Vec2 zero_cast() const { return zero_cast_; }

private:
  std::vector<FieldNpcWorldCallback> callbacks_;
  std::string visibility_signal_;
  bool valid_ = false;
  FieldIdentity identity_{};
  Vec2 zero_cast_{};
  std::vector<FieldNpcWorldBody> bodies_;
  std::vector<FieldNpcWorldRay> rays_;
  std::vector<FieldNpcWorldLink> npcs_;
};
// Reuse the same analytic-circle/convex native solver as the real Player.
// Map and geometry are the live complete destination, not a cropped fixture.
bool field_npc_world_move(const FieldNpcWorldBody &, FieldObjectId,
                          FieldObjectId, bool disabled, Vec2, float,
                          FieldNodeTreeRuntime &, FieldGlobalRegistry &,
                          FieldMapSpace &, FieldGeometrySpace &,
                          const FieldMapGateQuery &, Vec2 &, Vec2 &,
                          std::string &);
bool field_npc_world_ray(const FieldNpcWorldRay &, FieldObjectId, Vec2,
                         FieldNodeTreeRuntime &, FieldGlobalRegistry &,
                         FieldMapSpace &, FieldGeometrySpace &,
                         const FieldMapGateQuery &, FieldObjectId &,
                         std::string &);
} // namespace encore::upstream
