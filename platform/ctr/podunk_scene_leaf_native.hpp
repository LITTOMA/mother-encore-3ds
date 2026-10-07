#pragma once
#include "encore/scene_leaf_native.hpp"
#include "podunk_player_camera.hpp"
#include "podunk_scene_loop.hpp"
namespace encore::ctr {
// Native leaf dispatch into existing typed clocks; no generic animation VM.
class PodunkSceneLeafNative final : public PodunkSceneNativeMechanism,
                                    public PodunkSceneCanvasLeaf {
public:
  bool prepare(const upstream::SceneLeafNativeData &,
               const upstream::SceneLeafNativeSources &,
               upstream::FieldNodeTreeRuntime &,
               upstream::FieldGlobalRegistry &, upstream::FieldObjectSignals &,
               PodunkSceneNative &, upstream::FieldGameCameraRuntime &,
               upstream::FieldPlayerTransitionsRuntime &,
               upstream::FieldBirdRuntime &, upstream::FieldDroppedRuntime &,
               upstream::FieldPayphoneRuntime &,
               upstream::FieldMelodyBackgroundRuntime &, std::string &);
  bool apply(upstream::FieldGameCameraHost &, upstream::FieldBirdHost &,
             upstream::FieldPayphoneHost &,
             upstream::FieldMelodyBackgroundHost &, std::string &);
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
  bool finish_factory(std::string &);
  const upstream::FieldNodeTreeRuntime *canvas_tree() const override {
    return tree_;
  }
  const upstream::FieldGlobalRegistry *canvas_registry() const override {
    return registry_;
  }
  bool owns_drawable(upstream::FieldObjectId) const override;
  bool appearance(const upstream::FieldCanvasRecord &, upstream::FieldObjectId,
                  upstream::FieldCanvasAppearance &,
                  std::string &) const override;
  bool draw_leaf(const upstream::FieldCanvasOrderSlot &,
                 const upstream::FieldTransform &, bool,
                 std::string &) override;

private:
  struct Leaf {
    const upstream::SceneLeafNativeRecord *source = nullptr;
    upstream::FieldNodeBinding binding{};
    bool entered = false, ready = false, playing = false, assigned = false;
    float elapsed = 0;
  };
  bool actual(upstream::FieldObjectId, std::string &) const;
  bool event(upstream::SceneLeafKind, uint32_t, std::string_view, uint32_t,
             std::string &);
  bool play_phone(uint32_t, uint32_t, std::string &);
  const upstream::SceneLeafNativeData *data_ = nullptr;
  std::unique_ptr<upstream::SceneLeafNativeSources> sources_;
  upstream::FieldNodeTreeRuntime *tree_ = nullptr;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  upstream::FieldObjectSignals *signals_ = nullptr;
  PodunkSceneNative *native_ = nullptr;
  upstream::FieldGameCameraRuntime *camera_ = nullptr;
  upstream::FieldPlayerTransitionsRuntime *transitions_ = nullptr;
  upstream::FieldBirdRuntime *birds_ = nullptr;
  upstream::FieldDroppedRuntime *dropped_ = nullptr;
  upstream::FieldPayphoneRuntime *phone_ = nullptr;
  upstream::FieldMelodyBackgroundRuntime *melody_ = nullptr;
  std::map<upstream::FieldObjectId, Leaf> leaves_;
  bool applied_ = false, finished_ = false;
};
} // namespace encore::ctr
