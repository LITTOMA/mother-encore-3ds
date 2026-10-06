#pragma once
#include "encore/field_global_constructor.hpp"
namespace encore::upstream {
struct GlobalChildReadyNode {
  uint32_t kind = 0, id = 0, class_index = 0, methods = 0;
  std::string path, script;
  std::array<uint8_t, 32> script_sha{};
  std::vector<std::string> fields, methods_source;
};
struct GlobalChildReadyPolicy {
  double slow_end = 0, length_scale = 0, moving_frames = 0, idle_reset = 0,
         shown_hide = 0;
  double engine_initial_scale = 0;
  uint32_t visible = 0, hidden = 0, mouse_enter = 0;
  bool default_pitch = false;
};
class GlobalChildReadyData {
public:
  bool load(const uint8_t *, size_t, const FieldGlobalConstructorData &,
            std::string &);
  bool load_file(const char *, const FieldGlobalConstructorData &,
                 std::string &);
  bool valid() const { return valid_; }
  const auto &identity() const { return identity_; }
  const auto &ir_sha256() const { return ir_; }
  const auto &constructor_ir_sha256() const { return constructor_; }
  const auto &nodes() const { return nodes_; }
  const auto &policy() const { return policy_; }
  const auto &audio_source() const { return audio_; }
  const auto &audio_method() const { return audio_method_; }
  bool source_hash(std::string_view, std::array<uint8_t, 32> &) const;

private:
  bool valid_ = false;
  FieldIdentity identity_{};
  std::array<uint8_t, 32> ir_{}, constructor_{};
  std::vector<GlobalChildReadyNode> nodes_;
  GlobalChildReadyPolicy policy_;
  std::string audio_, audio_method_;
  std::map<std::string, std::array<uint8_t, 32>> sources_;
};
// Real native singleton ports; an absent owner is never a successful callback.
class GlobalChildEngine {
public:
  virtual ~GlobalChildEngine() = default;
  virtual bool ticks_msec(uint64_t &, std::string &) = 0;
  virtual bool set_time_scale(double, std::string &) = 0;
  virtual bool time_scale(double &, std::string &) const = 0;
};
class GlobalChildInput {
public:
  virtual ~GlobalChildInput() = default;
  virtual bool mouse_speed(Vec2 &, std::string &) = 0;
  virtual bool mouse_buttons(uint32_t &, std::string &) const = 0;
  virtual bool mouse_mode(uint32_t &, std::string &) const = 0;
  virtual bool set_mouse_mode(uint32_t, std::string &) = 0;
};
class GlobalChildAudio {
public:
  virtual ~GlobalChildAudio() = default;
  virtual bool source_hash(std::string_view,
                           std::array<uint8_t, 32> &) const = 0;
  virtual bool set_sfx_pitch(double, std::string &) = 0;
};
struct GlobalSlowmoState {
  bool ready = false, started = false, processing = false, with_pitch = false;
  uint64_t start = 0;
  double length = 0, value = 0;
};
struct GlobalMouseHiderState {
  bool ready = false, processing = false;
  Vec2 speed{};
  double shown = 0, hidden = 0, idle = 0;
};
class GlobalChildReadyRuntime {
public:
  bool initialize(const GlobalChildReadyData &,
                  const FieldGlobalConstructorData &, FieldGlobalRegistry &,
                  GlobalChildEngine &, GlobalChildInput &, GlobalChildAudio *,
                  std::string &);
  bool construct(FieldNodeTreeRuntime &, FieldObjectId,
                 const FieldNodeDescriptor &, std::string &);
  bool ready_script(FieldNodeTreeRuntime &, FieldObjectId, std::string &);
  bool idle(FieldObjectId, double, std::string &);
  bool input(FieldObjectId, std::string &);
  bool notification(FieldObjectId, uint32_t, std::string &);
  bool start_slowmo(FieldObjectId, double speed, double length, bool pitch,
                    std::string &);
  bool start_slowmo(FieldObjectId, double speed, double length, std::string &);
  bool set_active(FieldObjectId, bool, std::string &);
  bool release(FieldObjectId, std::string &);
  const auto &slowmo() const { return slow_; }
  const auto &mouse() const { return mouse_; }
  bool poisoned() const { return poisoned_; }

private:
  bool live(FieldObjectId, uint32_t, FieldNodeTreeRuntime *&,
            std::string &) const;
  bool poison(std::string &);
  const GlobalChildReadyData *data_ = nullptr;
  const FieldGlobalConstructorData *constructor_ = nullptr;
  FieldGlobalRegistry *registry_ = nullptr;
  GlobalChildEngine *engine_ = nullptr;
  GlobalChildInput *input_ = nullptr;
  GlobalChildAudio *audio_ = nullptr;
  std::array<uint8_t, 32> admitted_ir_{};
  FieldObjectId slow_id_ = 0, mouse_id_ = 0;
  std::map<FieldObjectId, FieldNodeTreeRuntime *> trees_;
  GlobalSlowmoState slow_;
  GlobalMouseHiderState mouse_;
  bool poisoned_ = false;
};
} // namespace encore::upstream
