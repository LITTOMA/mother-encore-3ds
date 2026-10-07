#pragma once
#include "encore/collision.hpp"
#include "encore/field_geometry_space.hpp"
#include "encore/field_map_space.hpp"
#include "encore/player_motion.hpp"
namespace encore::upstream {
struct PodunkPlayerKinematicHost {
  // Actual native source-node ownership, not a fabricated stable-ID handle.
  std::function<bool(uint32_t, FieldObjectId &, std::string &)> source_object;
  FieldMapGateQuery map_gates;
};
class PodunkPlayerKinematic {
public:
  bool rebind_tree(FieldNodeTreeRuntime &, std::string &);
  bool initialize(const PlayerInitializationData &, const PlayerMotionData &,
                  PlayerInitializationBody &, FieldNodeTreeRuntime &,
                  FieldGlobalRegistry &, FieldMapSpace &, FieldGeometrySpace &,
                  PodunkPlayerKinematicHost, std::string &);
  // Driven by the existing SceneTree physics frame, never an independent clock.
  bool begin_physics(uint64_t actual_epoch, float actual_delta, std::string &);
  bool move_and_slide(FieldObjectId, Vec2 source_velocity, Vec2 &returned,
                      std::string &);
  bool ray_rotation(FieldObjectId, float, std::string &);
  // Only actual RayCast2D physics-internal notification updates this cache.
  bool ray_physics(FieldObjectId, uint64_t actual_epoch, std::string &);
  bool cached_ray(FieldObjectId, FieldObjectId &, std::string &) const;
  bool set_collision_mask(uint32_t, bool, std::string &);
  bool set_collider_disabled(bool, std::string &);
  bool admitted() const { return data_ && !poisoned_; }

private:
  bool live(std::string &) const;
  const PlayerInitializationData *data_ = nullptr;
  PlayerInitializationBody *body_ = nullptr;
  FieldNodeTreeRuntime *tree_ = nullptr;
  FieldGlobalRegistry *registry_ = nullptr;
  FieldMapSpace *map_ = nullptr;
  FieldGeometrySpace *space_ = nullptr;
  PodunkPlayerKinematicHost host_;
  const PlayerMotionData *motion_ = nullptr;
  FieldObjectId collider_ = 0, ray_ = 0;
  std::vector<Vec2> hull_;
  float margin_ = 0, delta_ = 0;
  uint32_t layer_ = 0, mask_ = 0, ray_mask_ = 0;
  Vec2 cast_{};
  bool disabled_ = false, ray_enabled_ = false, ray_bodies_ = false,
       ray_areas_ = false, exclude_parent_ = false;
  uint64_t epoch_ = 0, ray_epoch_ = 0;
  FieldObjectId ray_hit_ = 0;
  bool ray_cached_ = false, poisoned_ = false;
};
// Analytic-circle/convex/segment source motion execution; finite native solver
// iteration limits only. It never substitutes a circle with tessellation.
struct PodunkSolidShape {
  std::vector<Vec2> vertices, normals;
  Vec2 minimum{}, maximum{}, center{};
  float radius = 0;
  bool circle = false, segment = false;
};
bool podunk_prepare_shape(const FieldGeometryActor &, PodunkSolidShape &,
                          std::string &);
bool podunk_solve_motion(const PodunkSolidShape &actor,
                         const std::vector<PodunkSolidShape> &,
                         Vec2 world_position, Vec2 velocity, float delta,
                         float margin, SlideResult &, MotionQueryBounds &,
                         std::string &);
} // namespace encore::upstream
