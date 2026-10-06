#pragma once
#include "encore/field_geometry_space.hpp"
#include <functional>
#include <map>
namespace encore::upstream {
enum class FieldTransitionParameter : uint32_t {
  PromptSeconds = 0,
  ArrowOffsetX,
  ArrowOffsetY,
  JumpSeconds,
  PathSeconds,
  PathDelay,
  PartyStagger,
  CameraSeconds,
  ReturnSeconds,
  CameraOffsetX,
  CameraOffsetY,
  RunInsideScale,
  RunInsideSeconds,
  CrouchSeconds,
  ScaleDelay,
  DescendFactor,
  ScaleFromX,
  ScaleFromY,
  VibrationDevice,
  VibrationWeak,
  VibrationStrong,
  VibrationSeconds
};
struct FieldTransitionArea {
  uint32_t id = 0, shape_id = 0, layer = 0, mask = 0, kind = 0;
  Vec2 position{};
  std::vector<float> values;
};
struct FieldTransitionArrowKey {
  float time = 0;
  Vec2 value{};
};
struct FieldTransitionDescriptor {
  uint32_t id = 0, ready = 0, kind = 0, camera_id = 0, flags = 0, sprite_id = 0,
           ray_id = 0, ray_mask = 0, ray_flags = 0;
  Vec2 position{}, sprite_position{}, sprite_offset{}, sprite_scale{},
      ray_position{}, ray_cast{};
  float height = 0, step_length = 0, arrow_length = 0, sprite_rotation = 0;
  std::string node, texture;
  std::vector<uint32_t> areas;
  std::vector<Vec2> points;
  std::vector<FieldTransitionArrowKey> offset_keys, rotation_keys;
};
class FieldPlayerTransitionsData {
public:
  bool load(const uint8_t *, size_t, std::string &);
  bool load_file(const char *, std::string &);
  bool valid() const { return valid_; }
  uint32_t scene_id() const { return scene_; }
  const auto &source_pin() const { return pin_; }
  const auto &source_scene() const { return source_scene_; }
  bool source_hash(std::string_view p, std::array<uint8_t, 32> &out) const {
    auto i = sources_.find(std::string(p));
    if (i == sources_.end())
      return false;
    out = i->second;
    return true;
  }
  const auto &sources() const { return sources_; }
  const auto &records() const { return records_; }
  const auto &areas() const { return areas_; }
  const auto &skill_flag() const { return skill_; }
  const auto &prompt_modes() const { return prompts_; }
  const auto &animations() const { return animations_; }
  float parameter(FieldTransitionParameter p) const {
    return parameters_[size_t(p)];
  }
  const FieldTransitionDescriptor *record(uint32_t) const;

private:
  bool valid_ = false;
  uint32_t scene_ = 0;
  std::array<uint8_t, 20> pin_{};
  std::array<float, 22> parameters_{};
  std::string source_scene_, skill_;
  std::array<std::string, 2> prompts_;
  std::array<std::string, 3> animations_;
  std::map<std::string, std::array<uint8_t, 32>> sources_;
  std::vector<FieldTransitionDescriptor> records_;
  std::vector<FieldTransitionArea> areas_;
};
// Body geometry is the actual KinematicBody collider; area_geometry is the
// actual child's Area2D shape used by Stairs, not a foot-point proxy.
struct FieldTransitionActor {
  uint32_t id = 0, kind = 0, jumps = 0, camera_id = 0;
  Vec2 position{}, direction{}, shadow_position{};
  bool valid = true, active = true, physics = true;
  std::vector<FieldGeometryActor> body_geometry, area_geometry;
};
struct FieldTransitionContext {
  uint32_t player = 0;
  bool player_move = false, player_paused = false, stack_empty = false,
       running = false, walking = false, substantial_movement = false,
       player_jumping = false;
  Vec2 velocity{};
  std::string prompt_mode;
  std::vector<FieldTransitionActor> party;
};
enum class FieldTransitionCommandKind : uint32_t {
  CameraCurrent = 1,
  PlayerPause,
  PlayerLayer,
  CanInteract,
  ActorDirection,
  ActorIdle,
  ActorPhysics,
  ActorActive,
  ActorJumps,
  PlayerJumpState,
  ActorAnimation,
  ActorShadow,
  ActorPosition,
  ActorVisualOffset,
  ActorVisualScale,
  JumpFinished,
  ActionDone,
  ResetPartyPositions,
  PlayerUnpause,
  CameraMove,
  CameraReturn,
  ConstraintDirection,
  PartySpaceMove,
  MoveAndSlide,
  ArrowPose,
  ArrowAnimation,
  InsideShapeScale,
  Vibration,
  StateChanged,
  ActorFindPath,
  ActorTranslate
};
struct FieldTransitionCommand {
  FieldTransitionCommandKind kind{};
  uint32_t transition = 0, actor = 0, value = 0;
  Vec2 vector{}, second{};
  float scalar = 0;
  double seconds = 0;
  std::string text{};
};
struct FieldPlayerTransitionsHost {
  // Cross-check PIN/scene, all source node/shape IDs and real source pose
  // paths; binds corresponding SceneHost/Geometry adapters. No blanket script
  // admit.
  std::function<bool(const FieldPlayerTransitionsData &, std::string &)> bind;
  std::function<bool(FieldTransitionContext &, std::string &)> context;
  std::function<bool(const std::string &, bool &, std::string &)> flag;
  // Ray look_at target is source player ground position. collider is last
  // physics cache, matching RayCast2D.get_collider immediately after look_at.
  std::function<bool(const FieldTransitionDescriptor &,
                     const FieldTransitionActor &, uint32_t &collider,
                     std::string &)>
      cached_ray;
  // Checked typed mutations/camera calls, source order; failures poison
  // runtime.
  std::function<bool(const FieldTransitionCommand &, std::string &)> apply;
};
struct FieldTransitionInstance {
  uint32_t id = 0;
  bool ready = false, nearby = false, inside = false, jumping = false,
       arrow_playing = false, pending_state = false, silent_state = false;
  float inside_scale = 1, step_distance = 0;
  Vec2 arrow_position{}, arrow_scale{}, arrow_offset{};
  float arrow_rotation = 0;
  double animation_time = 0, prompt_time = 0;
  Vec2 prompt_from_position{}, prompt_from_scale{}, prompt_target_position{},
      prompt_target_scale{};
  std::vector<double> run_inside_waiters;
  std::vector<uint32_t> moving_actors;
};
class FieldPlayerTransitionsRuntime {
public:
  bool initialize(const FieldPlayerTransitionsData &,
                  FieldPlayerTransitionsHost, std::string &);
  bool ready(uint32_t);
  bool update_state(uint32_t, bool silent);
  bool contacts(const FieldGeometrySpace &);
  bool accept();
  bool physics_step(float);
  bool idle_frame(double, bool source_node_processing = true);
  // Actual source node callbacks, preserving source traversal/order. accept is
  // invoked only for a real action-pressed event; no synthetic polling.
  bool accept_source(uint32_t);
  bool physics_source(uint32_t, float);
  bool process_source(uint32_t, double);
  // Sole native timer/tween/animation schedule for this checked owner. Ordinary
  // _process never advances these jobs; legacy idle_frame retains both passes.
  bool idle_native_source(uint32_t, double, bool source_node_processing = true);
  bool actor_action_done(uint32_t);
  const auto &instances() const { return instances_; }
  const std::string &error() const { return error_; }

  const FieldPlayerTransitionsData *content() const { return data_; }

private:
  struct JumpTask {
    uint32_t transition = 0, actor = 0, point = 0;
    bool leader = false, waiting_signal = false, waiting_active = false,
         started = false, done = false, visual_finished = false;
    double time = 0;
    Vec2 from{}, target{};
  };
  struct PartyStart {
    uint32_t transition = 0, next = 0;
    double wait = 0;
    std::vector<uint32_t> actors;
  };
  struct VisualTask {
    uint32_t transition = 0, actor = 0;
    bool leader = false, animated = false, done = false;
    double time = 0;
  };
  const FieldPlayerTransitionsData *data_ = nullptr;
  FieldPlayerTransitionsHost host_;
  std::vector<FieldTransitionInstance> instances_;
  std::vector<JumpTask> tasks_;
  std::vector<PartyStart> starts_;
  std::vector<VisualTask> visuals_;
  std::string error_;
  bool fail(const char *);
  bool source_node(uint32_t, uint32_t kind);
  bool accept_impl(uint32_t);
  bool physics_impl(float, uint32_t);
  bool idle_impl(double, bool, uint32_t, bool source_process);
  bool context(FieldTransitionContext &);
  bool command(FieldTransitionCommand);
  FieldTransitionInstance *instance(uint32_t);
  const FieldTransitionActor *actor(const FieldTransitionContext &,
                                    uint32_t) const;
  bool ray(const FieldTransitionDescriptor &, const FieldTransitionContext &,
           bool &);
  bool update(FieldTransitionInstance &, const FieldTransitionContext &);
  bool signal_jump(uint32_t);
  bool prepare_point(JumpTask &);
  bool launch(JumpTask &);
  bool start_party_actor(PartyStart &);
  bool finish(JumpTask &);
  float p(FieldTransitionParameter k) const { return data_->parameter(k); }
};
} // namespace encore::upstream
