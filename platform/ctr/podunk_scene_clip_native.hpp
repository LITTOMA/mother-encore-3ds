#pragma once
#include "encore/scene_clip_native.hpp"
#include "podunk_scene_loop.hpp"
namespace encore::ctr {
class PodunkSceneClipNative final : public PodunkSceneNativeMechanism {
public:
  bool prepare(const upstream::SceneClipNativeData &,
               const upstream::FieldNodeTreeData &,
               upstream::FieldNodeTreeRuntime &,
               upstream::FieldGlobalRegistry &, upstream::FieldObjectSignals &,
               PodunkSceneTimers &, PodunkSceneNative &,
               upstream::FieldOpenableDoorRuntime &,
               upstream::FieldPresentRuntime &, upstream::FieldEmoteRuntime &,
               upstream::FieldBushRuntime &, std::string &);
  // After concrete callbacks are supplied, before the existing core initialize.
  bool apply(upstream::FieldOpenableHost &, upstream::FieldPresentHost &,
             upstream::FieldEmoteHost &, upstream::FieldBushHost &,
             std::string &);
  bool owns(const upstream::FieldNodeDescriptor &) const override;
  bool owns(upstream::FieldObjectId) const override;
  bool construct(upstream::FieldObjectId, const upstream::FieldNodeDescriptor &,
                 const upstream::FieldIdentity &, std::string &) override;
  bool bind(upstream::FieldObjectId, upstream::FieldNodeBinding &,
            std::string &) override;
  bool phase(upstream::FieldObjectId, upstream::FieldTreePhase, float, bool,
             bool, std::string &) override;
  bool deferred(const upstream::FieldDeferredMessage &, std::string &) override;
  bool release(upstream::FieldObjectId, std::string &) override;
  bool declaration(upstream::FieldObjectId, std::string_view, uint32_t &,
                   std::string &) const;
  bool method_owned(const upstream::FieldDeferredMessage &) const;
  bool source_method(const upstream::FieldDeferredMessage &, std::string &);
  bool finish_factory(std::string &);

private:
  struct Leaf {
    const upstream::SceneClipNativeRecord *source = nullptr;
    upstream::FieldNodeBinding binding{};
    bool entered = false, ready = false;
  };
  bool actual(upstream::FieldObjectId, std::string &) const;
  bool find(uint32_t, upstream::FieldObjectId &, std::string &) const;
  bool frame(upstream::SceneClipOwner, uint32_t, uint32_t, std::string &);
  bool event(upstream::SceneClipOwner, uint32_t, uint32_t, uint32_t,
             std::string &);
  const upstream::SceneClipNativeData *data_ = nullptr;
  const upstream::FieldNodeTreeData *source_ = nullptr;
  upstream::FieldNodeTreeRuntime *tree_ = nullptr;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  upstream::FieldObjectSignals *signals_ = nullptr;
  PodunkSceneTimers *timers_ = nullptr;
  PodunkSceneNative *native_ = nullptr;
  upstream::FieldOpenableDoorRuntime *openable_ = nullptr;
  upstream::FieldPresentRuntime *present_ = nullptr;
  upstream::FieldEmoteRuntime *emote_ = nullptr;
  upstream::FieldBushRuntime *bush_ = nullptr;
  std::map<upstream::FieldObjectId, Leaf> leaves_;
  std::map<uint32_t, upstream::FieldObjectId> actuals_;
  bool applied_ = false, finished_ = false;
};
} // namespace encore::ctr
