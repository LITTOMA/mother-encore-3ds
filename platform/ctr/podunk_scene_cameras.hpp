#pragma once
#include "encore/scene_leaf_native.hpp"
#include "podunk_player_camera.hpp"
#include "podunk_scene_loop.hpp"
namespace encore::ctr {
// One actual native Viewport selection shared by scene and Player cameras.
// global.currentCamera remains the original separately owned script member.
class PodunkSceneCameras final : public PodunkSceneNativeMechanism {
public:
  bool prepare(const upstream::SceneLeafNativeData &,
               const upstream::FieldNodeTreeData &,
               upstream::FieldNodeTreeRuntime &,
               upstream::FieldGlobalRegistry &, PodunkNativeRoot &,
               upstream::FieldGlobalConstructorRuntime &,
               upstream::FieldGameCameraRuntime &,
               upstream::FieldCameraArrowsRuntime &, PodunkCameraViewport,
               std::string &);
  bool register_player(PodunkPlayerCamera &, std::string &);
  bool native_current(upstream::FieldObjectId &, std::string &) const;
  bool make_current(upstream::FieldObjectId, std::string &);
  bool current_snapshot(upstream::FieldObjectId,
                        upstream::FieldGameCameraState &, std::string &) const;
  bool apply(upstream::FieldGameCameraHost &, std::string &);
  bool owns(const upstream::FieldNodeDescriptor &) const override;
  bool owns(upstream::FieldObjectId) const override;
  bool construct(upstream::FieldObjectId, const upstream::FieldNodeDescriptor &,
                 const upstream::FieldIdentity &, std::string &) override;
  bool bind(upstream::FieldObjectId, upstream::FieldNodeBinding &,
            std::string &) override;
  bool phase(upstream::FieldObjectId, upstream::FieldTreePhase, float, bool,
             bool, std::string &) override;
  bool deferred(const upstream::FieldDeferredMessage &, std::string &) override;
  bool method(upstream::FieldObjectId, std::string_view,
              const std::vector<upstream::FieldDeferredValue> &, std::string &);
  bool release(upstream::FieldObjectId, std::string &) override;
  bool finish_factory(std::string &) const;

private:
  struct Camera {
    const upstream::SceneLeafNativeRecord *source = nullptr;
    upstream::FieldNodeBinding binding{};
    upstream::FieldGameCameraState body{};
    bool entered = false, ready = false;
  };
  bool actual(upstream::FieldObjectId, std::string &) const;
  bool update(upstream::FieldObjectId, std::string &);
  bool source(uint32_t, upstream::FieldObjectId &, std::string &) const;
  bool observe(uint32_t, upstream::FieldGameCameraObservation &, std::string &);
  bool publish(uint32_t, const upstream::FieldGameCameraState &, std::string &);
  bool all_ready(upstream::FieldObjectId) const;
  const upstream::SceneLeafNativeData *data_ = nullptr;
  const upstream::FieldNodeTreeData *source_ = nullptr;
  upstream::FieldNodeTreeRuntime *tree_ = nullptr;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  PodunkNativeRoot *root_ = nullptr;
  upstream::FieldGlobalConstructorRuntime *global_ = nullptr;
  upstream::FieldGameCameraRuntime *core_ = nullptr;
  upstream::FieldCameraArrowsRuntime *arrows_ = nullptr;
  PodunkPlayerCamera *player_ = nullptr;
  PodunkCameraViewport viewport_{};
  upstream::FieldObjectId current_ = 0;
  std::map<upstream::FieldObjectId, Camera> cameras_;
  bool applied_ = false, paused_ = false;
};
} // namespace encore::ctr
