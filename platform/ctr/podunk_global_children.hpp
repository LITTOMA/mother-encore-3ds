#pragma once
#include "encore/global_child_ready.hpp"
namespace encore::ctr {
// Actual 3DS Engine/Input owner for the full-source scene scheduler. Physical
// HID touch remains the existing gesture device; it is not a PC mouse stream.
class PodunkGlobalClockInput final : public upstream::GlobalChildEngine,
                                     public upstream::GlobalChildInput {
public:
  bool initialize(const upstream::GlobalChildReadyData &, std::string &);
  bool begin_idle_frame(double real_delta, std::string &);
  bool idle_delta(double &, std::string &) const;
  bool ticks_msec(uint64_t &, std::string &) override;
  bool set_time_scale(double, std::string &) override;
  bool time_scale(double &, std::string &) const override;
  bool mouse_speed(upstream::Vec2 &, std::string &) override;
  bool mouse_buttons(uint32_t &, std::string &) const override;
  bool mouse_mode(uint32_t &, std::string &) const override;
  bool set_mouse_mode(uint32_t, std::string &) override;
  bool has_physical_mouse() const { return false; }

private:
  const upstream::GlobalChildReadyData *data_ = nullptr;
  uint64_t origin_ticks_ = 0, last_ticks_ = 0;
  float scale_ = 0;
  double frame_delta_ = 0;
  bool frame_valid_ = false;
  uint32_t mode_ = 0, buttons_ = 0;
  upstream::Vec2 mouse_speed_{};
};
// Root routes only actual source ScriptReady/Idle/Input here. Native Node
// Ready/Enter/Exit and input registration remain the actual kernel owner.
class PodunkGlobalChildren {
public:
  bool initialize(std::shared_ptr<const upstream::GlobalChildReadyData>,
                  std::shared_ptr<const upstream::FieldGlobalConstructorData>,
                  upstream::FieldGlobalRegistry &, upstream::GlobalChildAudio *,
                  std::string &);
  bool construct(upstream::FieldNodeTreeRuntime &, upstream::FieldObjectId,
                 const upstream::FieldNodeDescriptor &, std::string &);
  bool script_phase(upstream::FieldNodeTreeRuntime &, upstream::FieldObjectId,
                    const upstream::FieldNodeBinding &,
                    upstream::FieldTreePhase, std::string &);
  bool notification(upstream::FieldObjectId, uint32_t, std::string &);
  bool release(upstream::FieldObjectId, std::string &);
  bool owns(upstream::FieldObjectId) const;
  PodunkGlobalClockInput &native() { return native_; }
  upstream::GlobalChildReadyRuntime &core() { return core_; }

private:
  std::shared_ptr<const upstream::GlobalChildReadyData> data_;
  std::shared_ptr<const upstream::FieldGlobalConstructorData> constructor_;
  upstream::GlobalChildReadyRuntime core_;
  PodunkGlobalClockInput native_;
  std::map<upstream::FieldObjectId, uint32_t> owners_;
};
} // namespace encore::ctr
