#pragma once
#include "encore/field_global_constructor.hpp"
#include "podunk_native_root.hpp"
#include "podunk_global_children.hpp"
namespace encore::ctr {
class PodunkGlobalReady;
// Source script fields are owned here; actual native class notification
// behavior is supplied by the same source-native owner used by other trees.
class PodunkGlobalNativeOwner {
public:
  virtual ~PodunkGlobalNativeOwner() = default;
  virtual bool construct(upstream::FieldNodeTreeRuntime &,
                         upstream::FieldObjectId,
                         const upstream::FieldNodeDescriptor &,
                         std::string &) = 0;
  virtual bool phase(upstream::FieldNodeTreeRuntime &, upstream::FieldObjectId,
                     const upstream::FieldNodeBinding &,
                     upstream::FieldTreePhase, std::string &) = 0;
  virtual bool input_registration(upstream::FieldObjectId, uint32_t, bool,
                                  std::string &) = 0;
  virtual bool release(upstream::FieldObjectId,
                       const upstream::FieldNodeBinding &, std::string &) = 0;
};
class PodunkGlobalHost {
public:
  PodunkGlobalHost() = default;
  PodunkGlobalHost(const PodunkGlobalHost &) = delete;
  PodunkGlobalHost &operator=(const PodunkGlobalHost &) = delete;
  bool initialize(const upstream::FieldGlobalRegistryData &,
                  upstream::FieldGlobalRegistry &,
                  std::shared_ptr<const upstream::FieldGlobalConstructorData>,
                  std::shared_ptr<const upstream::GlobalChildReadyData>,
                  PodunkNativeRoot &, PodunkGlobalNativeOwner &,
                  upstream::GlobalChildAudio *, std::string &);
  bool construct(upstream::FieldObjectId,
                 const upstream::FieldGlobalExternalSpec &,
                 std::unique_ptr<upstream::FieldGlobalExternalObject> &,
                 std::string &);
  upstream::GlobalLoadGlobalOwner *owner() const;
  upstream::FieldGlobalConstructorRuntime &core() { return core_; }
  const upstream::FieldGlobalConstructorRuntime &core() const { return core_; }
  std::shared_ptr<upstream::FieldNodeTreeRuntime> tree() const { return tree_; }
  std::shared_ptr<upstream::FieldNodeTreeRuntime> transition_tree() const {
    return transition_;
  }
  // Uses the same real Engine/Input clock as the complete scene scheduler.
  PodunkGlobalChildren &children() { return children_; }
  bool bind_characters(const upstream::FieldGlobalDataRuntime &, std::string &);
  // Attach the actual source method cursor before the native Tree enters.
  bool bind_ready(PodunkGlobalReady &, std::string &);

private:
  class GlobalObject;
  friend class GlobalObject;
  upstream::FieldNodeTreeHost
  tree_host(std::shared_ptr<upstream::FieldNodeTreeRuntime>,
            bool use_preallocated);
  bool construct_source(upstream::FieldNodeTreeRuntime &,
                        upstream::FieldObjectId,
                        const upstream::FieldNodeDescriptor &,
                        const upstream::FieldIdentity &, std::string &);
  bool bind(upstream::FieldNodeTreeRuntime &, upstream::FieldObjectId,
            const upstream::FieldNodeDescriptor &, upstream::FieldNodeBinding &,
            std::string &);
  bool phase(upstream::FieldNodeTreeRuntime &, upstream::FieldObjectId,
             const upstream::FieldNodeBinding &, upstream::FieldTreePhase,
             std::string &);
  bool state(upstream::FieldGlobalExternalState &, std::string &) const;
  bool deferred(const upstream::FieldDeferredMessage &, std::string &);
  const upstream::FieldGlobalRegistryData *registry_data_ = nullptr;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  PodunkNativeRoot *root_ = nullptr;
  PodunkGlobalNativeOwner *native_ = nullptr;
  PodunkGlobalReady *ready_ = nullptr;
  std::shared_ptr<const upstream::FieldGlobalConstructorData> data_;
  std::shared_ptr<upstream::FieldNodeTreeRuntime> tree_, transition_;
  upstream::FieldGlobalConstructorRuntime core_;
  upstream::FieldGlobalExternalBinding binding_;
  upstream::FieldNativeTimers timers_;
  PodunkGlobalChildren children_;
  std::map<upstream::FieldObjectId, bool> constructed_;
  GlobalObject *object_ = nullptr;
  upstream::FieldObjectId parent_ = 0;
  bool first_allocation_ = false, construction_failed_ = false;
};
} // namespace encore::ctr
