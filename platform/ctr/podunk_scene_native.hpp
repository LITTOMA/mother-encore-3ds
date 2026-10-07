#pragma once
#include "encore/field_canvas_art.hpp"
#include "encore/field_geometry_space.hpp"
#include "encore/field_map_space.hpp"
#include "encore/field_object_signals.hpp"
#include "encore/field_scene_host.hpp"
#include "podunk_native_root.hpp"
#include "podunk_player_physics_world.hpp"
#include <memory>

class FieldMapRenderer;
class FieldCanvasArtRenderer;
namespace encore::ctr {
class PodunkPlayerHost;
class PodunkConcretePlayerEffectOwners;
class PodunkPlayerCanvasForeign;
class PodunkSceneAnimatedLeaves;
// Actual native Control leaves join the existing Canvas order; their source
// owner executes construction and draws its own real GPU assets. No separate
// overlay pass or frame clock is introduced.
class PodunkSceneCanvasLeaf {
public:
  virtual ~PodunkSceneCanvasLeaf() = default;
  virtual const upstream::FieldNodeTreeRuntime *canvas_tree() const = 0;
  virtual const upstream::FieldGlobalRegistry *canvas_registry() const = 0;
  virtual bool owns_drawable(upstream::FieldObjectId) const = 0;
  virtual bool appearance(const upstream::FieldCanvasRecord &,
                          upstream::FieldObjectId,
                          upstream::FieldCanvasAppearance &, std::string &e) const {
    e = "Canvas leaf typed appearance is not implemented by its actual owner";
    return false;
  }
  virtual bool draw_leaf(const upstream::FieldCanvasOrderSlot &,
                        const upstream::FieldTransform &, bool pixel_snap,
                        std::string &) = 0;
};
struct PodunkSceneMaterialState {
  upstream::FieldObjectId material = 0;
  std::string shader_source;
  std::array<uint8_t, 32> shader_source_sha{};
};
// Non-default materials must be actual source Resource owners. A callback's
// existence alone is not a material construction/Ready receipt.
class PodunkSceneMaterialOwner {
public:
  virtual ~PodunkSceneMaterialOwner() = default;
  virtual const upstream::FieldGlobalRegistry *registry() const = 0;
  virtual bool material(upstream::FieldObjectId,
                        const upstream::FieldCanvasRecord &,
                        PodunkSceneMaterialState &, std::string &) const = 0;
  virtual bool bind_images(const FieldCanvasArtRenderer &, std::string &) = 0;
  virtual bool begin_frame(uint64_t, float global_shader_time, std::string &) = 0;
  virtual bool draw(const upstream::FieldCanvasDraw &, upstream::Vec2, float,
                    float, std::string &) = 0;
};
// Owns native bodies, shapes and GPU pages for the full checked scene. Script
// construction/Ready, timers, audio and animated sprites have other typed
// owners; this class never grants those classes an empty native admission.
class PodunkSceneNative final : public upstream::FieldCanvasNativeOwner {
public:
  PodunkSceneNative();
  ~PodunkSceneNative();
  PodunkSceneNative(const PodunkSceneNative &) = delete;
  PodunkSceneNative &operator=(const PodunkSceneNative &) = delete;
  bool prepare(const upstream::FieldNodeTreeData &,
               upstream::FieldNodeTreeRuntime &,
               upstream::FieldGlobalRegistry &, PodunkNativeRoot &,
               upstream::FieldMapSpace &, upstream::FieldGeometrySpace &,
               const upstream::FieldCanvasArtData &, const char *asset_root,
               upstream::FieldCanvasArtHost, PodunkSceneMaterialOwner *,
               std::string &);
  bool owns(const upstream::FieldNodeDescriptor &) const;
  bool owns(upstream::FieldObjectId) const;
  // Called after the actual Registry native publication, before script init.
  bool construct(upstream::FieldObjectId, const upstream::FieldNodeDescriptor &,
                 const upstream::FieldIdentity &, std::string &);
  // Validate the actual combined native/script binding; never fabricate one.
  bool bind(upstream::FieldObjectId, const upstream::FieldNodeBinding &,
            std::string &);
  bool finish_factory(std::string &);
  bool bind_external_material(upstream::FieldObjectId,
                              const upstream::FieldNodeDescriptor &, std::string &);
  bool canvas_appearance(const upstream::FieldCanvasRecord &,
                         upstream::FieldObjectId, upstream::FieldCanvasAppearance &,
                         std::string &) const;
  bool draw_default(const upstream::FieldCanvasDraw &, upstream::Vec2, float,
                    float, std::string &);
  bool bind_animated_leaves(PodunkSceneAnimatedLeaves &, std::string &);
  bool bind_canvas_leaf(PodunkSceneCanvasLeaf &, std::string &);
  bool bind_foreign(PodunkPlayerHost &,
                    const upstream::PlayerInitializationData &,
                    PodunkConcretePlayerEffectOwners &,
                    const upstream::PlayerEffectsData &, std::string &);
  bool phase(upstream::FieldObjectId, upstream::FieldTreePhase, std::string &);
  const upstream::FieldCanvasArtData *canvas_data() const override {
    return art_;
  }
  const upstream::FieldNodeTreeRuntime *canvas_tree() const override {
    return tree_;
  }
  bool sprite_snapshot(upstream::FieldObjectId,
                       upstream::FieldCanvasAppearance &,
                       std::string &) const override;
  // Complete native property pose. Source setter signals are emitted only by
  // the explicit setter endpoints, never guessed from a pose difference.
  bool sprite_publish(upstream::FieldObjectId,
                      const upstream::FieldCanvasAppearance &, std::string &);
  bool bind_sprite_signals(upstream::FieldObjectSignals &, std::string &);
  bool sprite_set_frame(upstream::FieldObjectId, uint32_t, std::string &);
  bool sprite_set_texture(upstream::FieldObjectId, uint32_t, std::string &);
  bool set_disabled(upstream::FieldObjectId, bool, std::string &);
  bool set_collision(upstream::FieldObjectId, uint32_t layer, uint32_t mask,
                     std::string &);
  // This source signal boundary requires actual script ancestor admission.
  bool activate_monitors(upstream::FieldSceneHost &, PodunkPlayerPhysicsWorld &,
                         std::string &);
  bool physics_admitted(std::string &) const;
  // Called once by the same real GPU frame owner, after its fence/begin.
  bool begin_draw(uint64_t epoch, float actual_draw_delta,
                  float global_shader_time, std::string &);
  bool draw(const upstream::FieldMapGateQuery &, std::string &);
  bool source_object(uint32_t, upstream::FieldObjectId &, std::string &) const;
  // Caller has waited for the actual GPU fence and exited the real tree.
  bool shutdown(std::string &);

private:
  enum class Kind { Node, Canvas, Map, Body, Area, Shape, Sprite };
  struct Instance {
    Kind kind = Kind::Node;
    uint32_t source = 0, geometry_node = UINT32_MAX, owner = UINT32_MAX,
             shape = UINT32_MAX, map = UINT32_MAX;
    bool entered = false, ready = false, bound = false, disabled = false,
         monitored = false, synchronized = false, space_disabled = false;
    upstream::FieldTransform space_local{};
    upstream::FieldCanvasAppearance sprite{};
  };
  bool actual(upstream::FieldObjectId, const upstream::FieldNodeDescriptor *&,
              const upstream::FieldNodeState *&, std::string &) const;
  bool synchronize(upstream::FieldObjectId, Instance &, std::string &);
  bool material(upstream::FieldObjectId, const upstream::FieldCanvasRecord &,
                std::string &) const;
  const upstream::FieldNodeTreeData *data_ = nullptr;
  upstream::FieldNodeTreeRuntime *tree_ = nullptr;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  PodunkNativeRoot *root_ = nullptr;
  upstream::FieldMapSpace *map_ = nullptr;
  upstream::FieldGeometrySpace *geometry_ = nullptr;
  const upstream::FieldCanvasArtData *art_ = nullptr;
  upstream::FieldCanvasArtHost art_host_;
  PodunkSceneMaterialOwner *materials_ = nullptr;
  upstream::FieldObjectSignals *sprite_signals_ = nullptr;
  PodunkPlayerPhysicsWorld *world_ = nullptr;
  std::unique_ptr<FieldMapRenderer> map_gpu_;
  std::unique_ptr<FieldCanvasArtRenderer> art_gpu_;
  std::unique_ptr<PodunkPlayerCanvasForeign> foreign_;
  PodunkSceneAnimatedLeaves *animated_leaves_ = nullptr;
  std::vector<PodunkSceneCanvasLeaf *> canvas_leaves_;
  upstream::FieldCanvasArtRuntime canvas_;
  std::map<upstream::FieldObjectId, Instance> instances_;
  std::map<uint32_t, upstream::FieldObjectId> source_objects_;
  std::map<uint32_t, upstream::FieldMapAnimationState> animations_;
  uint64_t draw_epoch_ = 0;
  bool finished_ = false, draw_started_ = false, monitors_active_ = false,
       geometry_bound_ = false;
};
} // namespace encore::ctr
