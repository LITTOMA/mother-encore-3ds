#include "podunk_global_children.hpp"
#include <3ds.h>
#include <cmath>
#include <limits>
namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
} // namespace
bool PodunkGlobalClockInput::initialize(const GlobalChildReadyData &data,
                                        std::string &e) {
  if (data_ || !data.valid() ||
      !std::isfinite(float(data.policy().engine_initial_scale)))
    return fail(e, "3DS actual Engine/Input source owner rejected");
  data_ = &data;
  origin_ticks_ = last_ticks_ = svcGetSystemTick();
  scale_ = float(data.policy().engine_initial_scale);
  mode_ = data.policy().visible;
  e.clear();
  return true;
}
bool PodunkGlobalClockInput::ticks_msec(uint64_t &out, std::string &e) {
  if (!data_ || !data_->valid())
    return fail(e, "3DS real monotonic clock owner unavailable");
  uint64_t now = svcGetSystemTick();
  if (now < last_ticks_ || now < origin_ticks_)
    return fail(e, "3DS monotonic system tick regression");
  last_ticks_ = now;
  auto elapsed = now - origin_ticks_;
  auto seconds = elapsed / SYSCLOCK_ARM11;
  if (seconds > uint64_t(std::numeric_limits<int64_t>::max()) / 1000)
    return fail(e, "3DS source millisecond clock overflow");
  out = seconds * 1000 + (elapsed % SYSCLOCK_ARM11) * 1000 / SYSCLOCK_ARM11;
  e.clear();
  return true;
}
bool PodunkGlobalClockInput::set_time_scale(double value, std::string &e) {
  if (!data_ || !data_->valid() || !std::isfinite(value) ||
      !std::isfinite(float(value)))
    return fail(e, "3DS actual Engine float scale rejected");
  scale_ = float(value);
  e.clear();
  return true;
}
bool PodunkGlobalClockInput::time_scale(double &out, std::string &e) const {
  if (!data_ || !data_->valid())
    return fail(e, "3DS actual Engine owner unavailable");
  out = scale_;
  e.clear();
  return true;
}
bool PodunkGlobalClockInput::begin_idle_frame(double real_delta,
                                              std::string &e) {
  if (!data_ || !data_->valid() || !std::isfinite(real_delta) ||
      real_delta < 0 || !std::isfinite(real_delta * double(scale_)))
    return fail(e, "3DS actual Engine frame delta rejected");
  frame_delta_ = real_delta * double(scale_);
  frame_valid_ = true;
  e.clear();
  return true;
}
bool PodunkGlobalClockInput::idle_delta(double &out, std::string &e) const {
  if (!data_ || !data_->valid() || !frame_valid_)
    return fail(e, "3DS idle phase before real source frame rejected");
  out = frame_delta_;
  e.clear();
  return true;
}
bool PodunkGlobalClockInput::mouse_speed(Vec2 &out, std::string &e) {
  if (!data_ || !data_->valid())
    return fail(e, "3DS actual Input owner unavailable");
  out = mouse_speed_;
  e.clear();
  return true;
}
bool PodunkGlobalClockInput::mouse_buttons(uint32_t &out,
                                           std::string &e) const {
  if (!data_ || !data_->valid())
    return fail(e, "3DS actual Input owner unavailable");
  out = buttons_;
  e.clear();
  return true;
}
bool PodunkGlobalClockInput::mouse_mode(uint32_t &out, std::string &e) const {
  if (!data_ || !data_->valid())
    return fail(e, "3DS actual Input owner unavailable");
  out = mode_;
  e.clear();
  return true;
}
bool PodunkGlobalClockInput::set_mouse_mode(uint32_t mode, std::string &e) {
  if (!data_ || !data_->valid() ||
      (mode != data_->policy().visible && mode != data_->policy().hidden))
    return fail(e, "3DS source Input mouse mode unsupported");
  mode_ = mode;
  e.clear();
  return true;
}
bool PodunkGlobalChildren::initialize(
    std::shared_ptr<const GlobalChildReadyData> data,
    std::shared_ptr<const FieldGlobalConstructorData> ctor,
    FieldGlobalRegistry &registry, GlobalChildAudio *audio, std::string &e) {
  if (data_ || !data || !ctor || !data->valid() || !ctor->valid() ||
      data->constructor_ir_sha256() != ctor->ir_sha256())
    return fail(e, "3DS global child checked owning resources rejected");
  if (!native_.initialize(*data, e) ||
      !core_.initialize(*data, *ctor, registry, native_, native_, audio, e))
    return false;
  data_ = std::move(data);
  constructor_ = std::move(ctor);
  e.clear();
  return true;
}
bool PodunkGlobalChildren::construct(FieldNodeTreeRuntime &tree,
                                     FieldObjectId id,
                                     const FieldNodeDescriptor &descriptor,
                                     std::string &e) {
  if (!data_ || owners_.count(id))
    return fail(e, "3DS child source constructor duplicate/uninitialized");
  const GlobalChildReadyNode *row = nullptr;
  for (const auto &r : data_->nodes())
    if (r.id == descriptor.id)
      row = &r;
  if (!row || !core_.construct(tree, id, descriptor, e))
    return false;
  owners_.emplace(id, row->kind);
  e.clear();
  return true;
}
bool PodunkGlobalChildren::owns(FieldObjectId id) const {
  return owners_.count(id) != 0;
}
bool PodunkGlobalChildren::script_phase(FieldNodeTreeRuntime &tree,
                                        FieldObjectId id,
                                        const FieldNodeBinding &binding,
                                        FieldTreePhase phase, std::string &e) {
  if (!owns(id))
    return fail(e, "3DS child script phase owner absent");
  const auto *d = tree.descriptor(id);
  if (!d || binding.stable_id != d->id || binding.script_sha != d->script_sha ||
      binding.native_class != d->native_class)
    return fail(e, "3DS child source script phase identity differs");
  if (phase == FieldTreePhase::ReadyScript)
    return core_.ready_script(tree, id, e);
  if (phase == FieldTreePhase::Idle) {
    double delta = 0;
    if (!native_.idle_delta(delta, e))
      return false;
    return core_.idle(id, delta, e);
  }
  if (phase == FieldTreePhase::Input)
    return core_.input(id, e);
  return fail(e, "3DS child unsupported native/script phase");
}
bool PodunkGlobalChildren::notification(FieldObjectId id, uint32_t what,
                                        std::string &e) {
  return core_.notification(id, what, e);
}
bool PodunkGlobalChildren::release(FieldObjectId id, std::string &e) {
  if (!owns(id) || !core_.release(id, e))
    return false;
  owners_.erase(id);
  e.clear();
  return true;
}
} // namespace encore::ctr
