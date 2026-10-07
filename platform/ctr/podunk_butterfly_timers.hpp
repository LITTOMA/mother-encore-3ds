#pragma once
#include "encore/field_butterfly.hpp"
#include "podunk_scene_timers.hpp"
namespace encore::ctr {
// Ordered source coroutine continuations borrow each real native Timer.
// This adapter never processes delta, owns a Timer, or manufactures a Node.
class PodunkButterflyTimers {
public:
  bool prepare(const upstream::FieldButterflyData &,
               const upstream::FieldNodeTreeData &,
               const upstream::FieldNativeTimerData &,
               upstream::FieldNodeTreeRuntime &, upstream::FieldGlobalRegistry &,
               PodunkSceneTimers &, upstream::FieldObjectSignals &,
               upstream::FieldButterflyRuntime &, std::string &);
  bool apply(upstream::FieldButterflyHost &, std::string &);
  bool finish_factory(std::string &);
  bool handles_method(const upstream::FieldDeferredMessage &) const;
  bool deferred(const upstream::FieldDeferredMessage &, std::string &);
  // Only actual object destruction cancels source continuations. Ordinary
  // exit/reentry and screen exit retain waits and native Timer time_left.
  bool cancel(uint32_t source_root, std::string &);
private:
  struct Waits {
    const upstream::FieldButterflyBinding *source = nullptr;
    upstream::FieldObjectId root = 0, timer = 0;
    bool slots[2]{false, false};
    bool dispatching = false;
    uint32_t dispatch_slot = 0;
  };
  bool actual(const Waits &, upstream::FieldObjectId &root,
              upstream::FieldObjectId &timer, std::string &) const;
  bool start(uint32_t source_root, uint32_t phase, std::string &);
  bool ready(const upstream::FieldButterflyBinding &, std::string &) const;
  const upstream::FieldButterflyData *data_ = nullptr;
  const upstream::FieldNodeTreeData *nodes_ = nullptr;
  const upstream::FieldNativeTimerData *timer_data_ = nullptr;
  upstream::FieldNodeTreeRuntime *tree_ = nullptr;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  PodunkSceneTimers *timers_ = nullptr;
  upstream::FieldObjectSignals *signals_ = nullptr;
  upstream::FieldButterflyRuntime *runtime_ = nullptr;
  std::map<uint32_t, Waits> waits_;
  bool applied_ = false;
};
} // namespace encore::ctr
