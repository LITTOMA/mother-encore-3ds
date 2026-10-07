#pragma once
#include "encore/field_native_root.hpp"
#include "encore/field_object_signals.hpp"
#include <list>

namespace encore::upstream {
// Actual native Reference held by SceneTree's timer list, separate from Node
// Timer and from source coroutine tokens. ObjectDB stores only a weak Ref.
class FieldSceneTreeTimer final : public FieldGlobalNativeReference {
public:
  ~FieldSceneTreeTimer() override;
  FieldGlobalExternalBinding binding() const override { return binding_; }
  const char *native_class() const override { return "SceneTreeTimer"; }
  const FieldGlobalRegistry *registry() const override { return registry_; }
  bool checked_source_hash(std::string_view,
                           std::array<uint8_t, 32> &) const override;
  float time_left() const { return time_left_; }
  bool set_time_left(float, std::string &);
  bool process_pause() const { return process_pause_; }

private:
  friend class FieldSceneTreeTimers;
  FieldGlobalExternalBinding binding_;
  FieldGlobalRegistry *registry_ = nullptr;
  FieldObjectSignals *signals_ = nullptr;
  float time_left_ = 0;
  bool process_pause_ = true;
};

// Fixed Godot 3.6.2 native create_timer/idle tail, using the real shared idle
// cursor. No private ticking thread, scene Ready approval or Node clock.
class FieldSceneTreeTimers {
public:
  bool initialize(const FieldNativeRootData &, FieldGlobalRegistry &,
                  FieldObjectSignals &, std::string &);
  bool create_timer(float seconds, bool process_pause,
                    std::shared_ptr<FieldSceneTreeTimer> &, std::string &);
  bool idle(uint64_t actual_epoch, float actual_scaled_delta, bool paused,
            std::string &);
  bool shutdown(std::string &);
  bool owns(FieldObjectId) const;
  bool signal_declaration(FieldObjectId, std::string_view, uint32_t &,
                          std::string &) const;
  FieldObjectId emitting() const { return emitting_; }
  const FieldGlobalRegistry *registry() const { return registry_; }
  static constexpr const char *timeout_signal() { return "timeout"; }

private:
  const FieldNativeRootData *data_ = nullptr;
  FieldGlobalRegistry *registry_ = nullptr;
  FieldObjectSignals *signals_ = nullptr;
  std::list<std::shared_ptr<FieldSceneTreeTimer>> timers_;
  std::map<FieldObjectId, std::weak_ptr<FieldSceneTreeTimer>> objects_;
  uint64_t epoch_ = 0;
  FieldObjectId emitting_ = 0;
  bool processing_ = false, failed_ = false;
};
} // namespace encore::upstream
