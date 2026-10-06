#include "encore/podunk_player_kinematic.hpp"
#include <cassert>
#include <limits>
// Manual native geometry cases. Never registered or executed automatically.
void podunk_player_kinematic_manual() {
  using namespace encore::upstream;
  std::string e;
  PodunkSolidShape shape;
  FieldGeometryActor circle;
  circle.kind = FieldGeometryKind::Circle;
  circle.radius = 4;
  assert(podunk_prepare_shape(circle, shape, e));
  assert(shape.circle && shape.vertices.empty() && shape.radius == 4);
  circle.transform.y = {0, 2};
  assert(!podunk_prepare_shape(circle, shape, e));
  circle.transform.y = {0, 1};
  circle.radius = -1;
  assert(!podunk_prepare_shape(circle, shape, e));
  FieldGeometryActor capsule;
  capsule.kind = FieldGeometryKind::Capsule;
  assert(!podunk_prepare_shape(capsule, shape, e));
  PodunkPlayerKinematic owner;
  FieldObjectId object = 0;
  assert(!owner.admitted());
  assert(!owner.cached_ray(1, object, e));
  assert(!owner.begin_physics(1, 1.f / 60, e));
  assert(!owner.set_collision_mask(32, true, e));
  Vec2 result{};
  assert(!owner.move_and_slide(1, {}, result, e));
  SlideResult slide;
  MotionQueryBounds bounds;
  PodunkSolidShape invalid;
  assert(!podunk_solve_motion(invalid, {}, {}, {}, 1.f / 60, .08f, slide,
                              bounds, e));
}
