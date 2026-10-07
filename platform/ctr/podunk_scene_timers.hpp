#pragma once
#include "encore/field_native_timer.hpp"
#include "encore/field_object_signals.hpp"

namespace encore::ctr {
// Concrete native Timer owner. Attached source scripts retain their own owner;
// this adapter never dispatches script Ready or advances a private frame clock.
class PodunkSceneTimers {
public:
  upstream::FieldNativeTimers &core(){return timers_;}
  bool prepare(const upstream::FieldNativeTimerData &,
               const upstream::FieldNodeTreeData &,
               upstream::FieldNodeTreeRuntime &,
               upstream::FieldGlobalRegistry &, upstream::FieldObjectSignals &,
               std::string &);
  bool construct(upstream::FieldObjectId, const upstream::FieldNodeDescriptor &,
                 const upstream::FieldIdentity &, std::string &);
  bool bind(upstream::FieldObjectId, upstream::FieldNodeBinding &,
            std::string &) const;
  bool finish_factory(std::string &) const;
  bool owns(upstream::FieldObjectId) const;
  bool phase(upstream::FieldObjectId, upstream::FieldTreePhase,
             float actual_delta, bool tree_paused, std::string &);
  bool deferred(const upstream::FieldDeferredMessage &, std::string &);
  bool start(upstream::FieldObjectId, float, std::string &);
  bool stop(upstream::FieldObjectId, std::string &);
  bool set_wait(upstream::FieldObjectId, float, std::string &);
  bool time_left(upstream::FieldObjectId, float &, std::string &) const;
  const upstream::FieldNativeTimerState *state(upstream::FieldObjectId) const;
  const upstream::FieldGlobalRegistry *registry() const { return registry_; }

private:
  struct Instance {
    upstream::FieldNodeBinding binding{};
    bool entered = false, ready = false;
  };
  bool actual(upstream::FieldObjectId, std::string &) const;
  const upstream::FieldNativeTimerData *data_ = nullptr;
  const upstream::FieldNodeTreeData *source_ = nullptr;
  upstream::FieldNodeTreeRuntime *tree_ = nullptr;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  upstream::FieldObjectSignals *signals_ = nullptr;
  upstream::FieldNativeTimers timers_;
  std::map<upstream::FieldObjectId, Instance> instances_;
};
} // namespace encore::ctr
