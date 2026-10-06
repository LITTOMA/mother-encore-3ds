#pragma once
#include "encore/player_ready.hpp"
#include "encore/source_random.hpp"
#include <set>
namespace encore::upstream {
enum class PlayerMotionField : size_t {
  State,
  Input,
  Direction,
  Velocity,
  Speed,
  Knockback,
  HitDirection,
  ContinuousDamage,
  AttackDamage,
  DamageVariance,
  Paused,
  Climbing,
  Spinning,
  Steps,
  LastStep,
  StepDistance,
  TapRun,
  Crouch,
  Running,
  Walking,
  Substantial,
  Idle,
  EventCollider,
  CurrentSkill,
  TeleportMode,
  CrouchDone,
  MaxSpeed,
  TakeoffDone,
  DebugSpeed,
  PartyMember,
  RunSound,
  CanInteract,
  CrashResource,
  Count
};
enum class PlayerMotionState : size_t {
  Move,
  AttackPrep,
  Attack,
  Camera,
  Jumping,
  Teleporting,
  Soot,
  Bouncing,
  Landing,
  Count
};
enum class PlayerMotionNode : size_t {
  EventRay,
  AnimationPlayer,
  Camera,
  Timer,
  CrouchTimer,
  TakeoffTimer,
  BlinkTimer,
  AimTimer,
  PkTimer,
  OutlineAnimation,
  AudioPlayer,
  BatCollision,
  BulletSpawn,
  MiscTimer,
  FlashAnimation,
  Count
};
enum class PlayerMotionText : size_t {
  RunVoice,
  WalkAnimation,
  RunAnimation,
  IdleAnimation,
  CrouchAnimation,
  BlinkAnimation,
  SootAnimation,
  FaintedWalkScale,
  TeleportSkill,
  RelaySkill,
  CancelAction,
  ScopeAction,
  ToggleAction,
  AcceptAction,
  NextAction,
  PreviousAction,
  MovedSignal,
  EnteredSignal,
  ExitedSignal,
  TreeExitedSignal,
  ColliderMethod,
  InteractNameToken,
  NoProblemDialogue,
  NoThoughtsDialogue,
  NothingDialogue,
  StrayThoughtsDialogue,
  PromptPath,
  TelepathySkill,
  SwingSkill,
  BeamSkill,
  FireSkill,
  FreezeSkill,
  ThunderSkill,
  ShootHoldAnimation,
  CastHoldAnimation,
  ShootAnimation,
  CastAnimation,
  BatAnimation,
  BatScale,
  ShootBlend,
  FlashAnimation,
  NormalAnimation,
  FootstepsFormat,
  BatAudio,
  BeamMember,
  CastMember,
  SceneObjects,
  BeamHead,
  TeleportPulse,
  TeleportFlash,
  ResetFlash,
  CrashVoice,
  PausedSignal,
  UnpausedSignal,
  FireAnimation,
  FreezeAnimation,
  ThunderAnimation,
  DamageEffect,
  InvulnerableAnimation,
  PlayFlashMethod,
  AfterimageStartMethod,
  AfterimageStopMethod,
  CollisionsMethod,
  AttackFinishedMethod,
  BlinkFinishedMethod,
  AimFinishedMethod,
  PkFinishedMethod,
  CrouchFinishedMethod,
  TakeoffFinishedMethod,
  ShootMethod,
  CastMethod,
  Count
};
enum class PlayerMotionNumber : size_t {
  WalkSpeed,
  RunSpeed,
  LandingSpeed,
  DebugSpeed,
  LandingSlowdown,
  MovementDivisor,
  KnockbackThreshold,
  KnockbackScale,
  KnockbackDecay,
  StepDistance,
  BlinkFrom,
  BlinkSpan,
  FaintedRunScale,
  FaintedWalkScale,
  BounceScale,
  CrashDistance,
  CrashShakeDivisor,
  CrashShakeTime,
  CameraOffsetX,
  CameraOffsetY,
  CameraOffsetTime,
  CameraReturnTime,
  PkAimSeconds,
  BatIncapacitatedScale,
  BatNormalScale,
  CastTimerSeconds,
  CastCount,
  CastStrideX,
  CastStrideY,
  CastVerticalStrideY,
  TeleportWait,
  DamageStepsDefault,
  DamageValueDefault,
  DamageVarianceDefault,
  PartyTeleportDistance,
  TimerDefaultTime,
  RayAngleOffset,
  Count
};
struct PlayerTeleportMode {
  int64_t id = 0;
  double acceleration = 0, cap = 0, takeoff = 0, multiplier = 0;
};
class PlayerMotionData {
public:
  bool load(const uint8_t *, size_t, const PlayerInitializationData &,
            const PlayerReadyData &, std::string &);
  bool load_file(const char *, const PlayerInitializationData &,
                 const PlayerReadyData &, std::string &);
  bool valid() const { return valid_; }
  const auto &identity() const { return identity_; }
  const auto &ir_sha256() const { return ir_; }
  const std::string &field(PlayerMotionField v) const {
    return fields_[size_t(v)];
  }
  int64_t state(PlayerMotionState v) const { return states_[size_t(v)]; }
  const std::string &node(PlayerMotionNode v) const {
    return nodes_[size_t(v)];
  }
  const std::string &text(PlayerMotionText v) const { return text_[size_t(v)]; }
  double number(PlayerMotionNumber v) const { return numbers_[size_t(v)]; }
  const auto &teleport_modes() const { return modes_; }
  int64_t manual_teleport() const { return manual_; }
  const auto &attack_character_names() const { return names_; }

private:
  bool valid_ = false;
  FieldIdentity identity_{};
  std::array<uint8_t, 32> ir_{};
  std::array<std::string, size_t(PlayerMotionField::Count)> fields_{};
  std::array<int64_t, size_t(PlayerMotionState::Count)> states_{};
  std::array<std::string, size_t(PlayerMotionNode::Count)> nodes_{};
  std::array<std::string, size_t(PlayerMotionText::Count)> text_{};
  std::array<double, size_t(PlayerMotionNumber::Count)> numbers_{};
  std::array<std::string, 3> names_{};
  std::vector<PlayerTeleportMode> modes_;
  int64_t manual_ = 0;
};
enum class PlayerInputQuery { Held, JustPressed, JustReleased };
struct PlayerInputEvent {
  std::vector<std::string> pressed;
  bool echo = false;
};
struct PlayerColliderInfo {
  std::string name;
  FieldObjectId parent = 0;
  bool area = false, interact = false, has_dialog = false, dialog = true,
       telepathy = false, has_thoughts = false, no_problem_thoughts = false;
};
struct PlayerDamageEffect {
  int64_t steps = 0, value = 0, variation = 0;
};
struct PlayerAudioVoice {
  bool exists = false, playing = false, same_stream = false;
};
// All services belong to actual source/native owners on this same registry.
// Kinematic calls mutate the true Tree body immediately and return the source
// slide velocity. A free-motion backend never admits a complete scene.
struct PlayerMotionHost {
  std::function<bool(Vec2 &, std::string &)> controls;
  std::function<bool(std::string_view, PlayerInputQuery, bool &, std::string &)>
      input;
  std::function<bool(FieldObjectId, Vec2, Vec2 &, std::string &)>
      move_and_slide;
  std::function<bool(FieldObjectId, float, std::string &)> ray_rotation;
  std::function<bool(FieldObjectId, FieldObjectId &, std::string &)> cached_ray;
  std::function<bool(FieldObjectId, std::string_view,
                     const std::vector<FieldDeferredValue> &, std::string &)>
      emit;
  std::function<bool(FieldObjectId, FieldObjectId, std::string_view, bool &,
                     std::string &)>
      collider_connected;
  std::function<bool(FieldObjectId, FieldObjectId, std::string_view, bool,
                     std::string &)>
      collider_connection;
  std::function<bool(FieldObjectId, PlayerColliderInfo &, std::string &)>
      collider_info;
  std::function<bool(FieldObjectId, bool &, std::string &)> is_climbing;
  std::function<bool(FieldObjectId, std::string_view, bool &, std::string &)>
      has_skill;
  std::function<bool(FieldObjectId, std::vector<std::string> &, std::string &)>
      button_skills;
  std::function<bool(FieldObjectId, std::string &, std::string &)>
      character_name;
  std::function<bool(FieldObjectId, bool &, std::string &)> incapacitated;
  std::function<bool(FieldObjectId, std::string_view,
                     std::vector<PlayerDamageEffect> &, std::string &)>
      damage_effects;
  std::function<bool(int64_t, int64_t, Vec2, bool, std::string &)> damage;
  std::function<bool(std::string_view, std::string_view, bool &, std::string &)>
      audio_resource;
  std::function<bool(std::string_view, std::string_view, PlayerAudioVoice &,
                     std::string &)>
      audio_voice;
  std::function<bool(std::string_view, std::string_view, std::string &)>
      audio_play;
  std::function<bool(std::string_view, std::string &)> audio_stop;
  std::function<bool(FieldObjectId, double &, std::string &)> timer_left;
  std::function<bool(FieldObjectId, double, std::string &)> timer_start;
  std::function<bool(FieldObjectId, std::string &)> timer_stop;
  std::function<bool(FieldObjectId, double, std::string &)> timer_wait;
  std::function<bool(FieldObjectId, std::string_view, std::string &)>
      animation_play;
  std::function<bool(FieldObjectId, double, std::string &)> animation_speed;
  std::function<bool(FieldObjectId, std::string &, std::string &)>
      animation_current;
  std::function<bool(std::string_view, const std::vector<FieldDeferredValue> &,
                     std::string &)>
      party_call;
  std::function<bool(std::string &)> dust;
  std::function<bool(int, std::string &)> swap_spin;
  std::function<bool(bool &, std::string &)> ui_stack_empty;
  std::function<bool(std::string_view, std::string &)> dialogue;
  std::function<bool(FieldObjectId, std::string &)> party_turn, interact,
      player_turn, telepathy, press_prompt;
  std::function<bool(FieldObjectId, bool, std::string &)> telepathy_effect;
  std::function<bool(FieldObjectId, Vec2 &, std::string &)> camera_offset;
  std::function<bool(FieldObjectId, Vec2, double, std::string &)> camera_move;
  std::function<bool(FieldObjectId, double, std::string &)> camera_return;
  std::function<bool(FieldObjectId, double, double, Vec2, std::string &)>
      camera_shake;
  std::function<bool(std::string &)> bat_feedback;
  std::function<bool(FieldObjectId, std::string_view, std::string &)>
      audio_play_resource;
  // All callbacks are actual owners. This preflight rejects a missing typed
  // special-state owner before entering any coroutine or attack mutation.
  std::function<bool(std::string_view, std::string &)> admit_special;
  std::function<bool(FieldObjectId, bool, std::string &)> shape_disabled;
  // Source-only projectile factories must instantiate their actual checked
  // PackedScene and return the native/script-admitted body; unknown rejects.
  std::function<bool(FieldObjectId, std::shared_ptr<FieldNodeTreeRuntime> &,
                     FieldObjectId &, std::string &)>
      projectile_instance;
  std::function<bool(FieldObjectId, Vec2, Vec2, float, std::string &)>
      beam_setup;
  std::function<bool(double, bool, FieldObjectId &, std::string &)> new_timer;
  std::function<bool(FieldObjectId, std::string &)> await_timer;
  std::function<bool(double, FieldObjectId &, std::string &)> new_scene_timer;
  std::function<bool(std::string &)> takeoff_fix_camera, pause,
      finish_teleport_camera;
};
class PlayerMotionRuntime {
public:
  bool initialize(const PlayerMotionData &, PlayerInitializationBody &,
                  PlayerReadyRuntime &, PlayerAnimationGraph &,
                  FieldNodeTreeRuntime &, FieldGlobalConstructorRuntime &,
                  FieldGlobalRegistry &, SourceRandom &, PlayerMotionHost,
                  std::string &);
  bool physics(float actual_delta, const FieldNodeBinding &, std::string &);
  bool input(const PlayerInputEvent &, std::string &);
  bool set_event_collider(FieldObjectId, std::string &);
  bool interact_with(std::string &);
  bool use_telepathy(std::string &);
  bool start_teleport(int64_t, std::string &);
  bool native_callback(std::string_view, std::string &);
  bool timer_timeout(FieldObjectId, std::string &);
  bool projectile_shoot(std::string &);
  bool projectile_cast(std::string &);
  bool healthy() const { return data_ && !poisoned_; }

private:
  bool live(std::string &) const;
  bool read(PlayerMotionField, PlayerInitializationMember &,
            std::string &) const;
  bool boolean(PlayerMotionField, bool &, std::string &) const;
  bool number(PlayerMotionField, double &, std::string &) const;
  bool integer(PlayerMotionField, int64_t &, std::string &) const;
  bool vector(PlayerMotionField, Vec2 &, std::string &) const;
  bool object(PlayerMotionField, FieldObjectId &, std::string &) const;
  bool text(PlayerMotionField, std::string &, std::string &) const;
  bool set_boolean(PlayerMotionField, bool, std::string &);
  bool set_number(PlayerMotionField, double, std::string &);
  bool set_integer(PlayerMotionField, int64_t, std::string &);
  bool set_vector(PlayerMotionField, Vec2, std::string &);
  bool set_text(PlayerMotionField, std::string, std::string &);
  bool node(PlayerMotionNode, FieldObjectId &, std::string &) const;
  bool action(PlayerMotionText, PlayerInputQuery, bool &, std::string &) const;
  bool controls(std::string &);
  bool move_state(float, std::string &);
  bool movement(float, std::string &);
  bool set_running(bool, std::string &);
  bool stop_run(std::string &);
  bool update_party_positions(Vec2, double, std::string &);
  bool calculate_steps(std::string &);
  bool physics_tail(std::string &);
  bool teleport_state(float, std::string &);
  bool landing_state(float, std::string &);
  bool soot_state(std::string &);
  bool attack_hold(std::string &);
  bool do_attack(std::string &);
  bool attack_unleash(std::string &);
  bool press_prompt(FieldObjectId, std::string &);
  bool cast_step(FieldObjectId, std::string &);
  struct CastTask {
    FieldObjectId timer = 0;
    Vec2 direction{}, position{};
    uint32_t next = 0;
  };
  const PlayerMotionData *data_ = nullptr;
  PlayerInitializationBody *body_ = nullptr;
  PlayerReadyRuntime *ready_ = nullptr;
  PlayerAnimationGraph *graph_ = nullptr;
  FieldNodeTreeRuntime *tree_ = nullptr;
  FieldGlobalConstructorRuntime *global_ = nullptr;
  FieldGlobalRegistry *registry_ = nullptr;
  SourceRandom *random_ = nullptr;
  PlayerMotionHost host_;
  bool poisoned_ = false;
  std::map<FieldObjectId, CastTask> casts_;
  std::set<FieldObjectId> takeoffs_;
};
} // namespace encore::upstream
