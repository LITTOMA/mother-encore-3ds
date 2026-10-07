#pragma once
#include "encore/field_canvas_art.hpp"
#include "encore/field_geometry_space.hpp"
#include "encore/field_map_space.hpp"
#include "encore/field_object_signals.hpp"
#include "encore/field_scene_host.hpp"
#include "encore/source_random.hpp"
#include "podunk_native_root.hpp"
#include "podunk_player_physics_world.hpp"
#include <memory>
#include <set>

class FieldMapRenderer;
class FieldCanvasArtRenderer;
namespace encore::upstream { class HouseReturnSources; }
namespace encore::ctr {
class HouseReturnGeometryNative;
class PodunkPlayerHost;
class PodunkConcretePlayerEffectOwners;
class PodunkPlayerCanvasForeign;
class PodunkSceneAnimatedLeaves;
class PodunkSceneGrassFactory;
class PodunkSceneNative;
// Evidence from the actual source remove_child, never a replacement Door.
// Only the concrete native owner may create or advance this receipt.
class PodunkSceneDoorTransfer {
public:
  upstream::FieldObjectId door()const{return geometry_.door;}
  upstream::FieldObjectId shape()const{return geometry_.shape;}
  upstream::FieldObjectId marker()const{return marker_;}
  upstream::FieldObjectId audio()const{return audio_;}
  upstream::FieldPhysicsRid rid()const{return geometry_.rid;}
  const upstream::FieldNodeTreeRuntime*old_tree()const{return old_;}
  const upstream::FieldNodeTreeRuntime*destination_tree()const{return next_;}
  const auto&objects()const{return objects_;}
private:
  friend class PodunkSceneNative;
  const PodunkSceneNative*owner_=nullptr;
  upstream::FieldNodeTreeRuntime*old_=nullptr,*next_=nullptr;
  upstream::FieldPersistentDoorGeometry geometry_{};
  upstream::FieldObjectId marker_=0,audio_=0;
  std::vector<upstream::FieldObjectId>objects_;
  std::map<upstream::FieldObjectId,upstream::FieldNodeDescriptor>descriptors_;
  std::map<upstream::FieldObjectId,upstream::FieldNodeState>states_;
  bool shape_disabled_=false;
};
struct PodunkSceneControlState {
 const upstream::FieldNodeTreeRuntime*tree=nullptr;
 const upstream::FieldGlobalRegistry*registry=nullptr;
 upstream::FieldObjectId object=0;uint32_t source_id=0;
 upstream::FieldIdentity identity{};std::string native_class;
 upstream::Vec2 position{},size{};bool constructed=false,bound=false,entered=false,ready=false;
};
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
// Podunk owns its native collision bodies/shapes and GPU pages here. The
// House path borrows its existing concrete geometry owner and consumes its
// checked Canvas/TileMap pages and Sparkles body in the same native tree.
// Other script/Control/material owners keep their actual source lifecycle.
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
  // Detached destination preparation borrows the same World/physics spaces;
  // House geometry is constructed and synchronized by its existing owner.
  bool prepare_house(const upstream::HouseReturnSources &,
               upstream::FieldNodeTreeRuntime &,upstream::FieldGlobalRegistry &,
               PodunkNativeRoot &,upstream::FieldMapSpace &,
               upstream::FieldGeometrySpace &,HouseReturnGeometryNative &,
               upstream::SourceRandom &,const char *asset_root,
               upstream::FieldCanvasArtHost,PodunkSceneMaterialOwner *,
               upstream::FieldCanvasControlOwner *,std::string &);
  bool bind_replaced_house(const upstream::FieldNodeTreeRuntime &old_tree,
               upstream::FieldObjectId old_root,upstream::FieldObjectId house_root,
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
  bool bind_grass(PodunkSceneGrassFactory &, std::string &);
  bool bind_foreign(PodunkPlayerHost &,
                    const upstream::PlayerInitializationData &,
                    PodunkConcretePlayerEffectOwners &,
                    const upstream::PlayerEffectsData &, std::string &);
  bool phase(upstream::FieldObjectId, upstream::FieldTreePhase, std::string &);
  // Same real SceneTree notification/idle cursor. Source Ready below owns
  // only the checked AnimatedSprite script body, never another source class.
  bool phase_house(upstream::FieldObjectId,upstream::FieldTreePhase,float,
                   bool paused,bool actual_update_pending,std::string &);
  bool deferred_house(const upstream::FieldDeferredMessage &,std::string &);
  // Exact Sparkles source attachment: no constructor/Enter/Exit body in the
  // checked source, and the original _ready is consumed by phase_house.
  bool house_sparkles_binding(upstream::FieldObjectId,
                             upstream::FieldNodeBinding &,std::string &)const;
  bool house_signal_declaration(upstream::FieldObjectId,std::string_view,
                                uint32_t &,std::string &)const;
  const upstream::FieldCanvasArtData *canvas_data() const override {
    return art_;
  }
  const upstream::FieldNodeTreeRuntime *canvas_tree() const override {
    return tree_;
  }
  const upstream::FieldMapView *tile_map_data()const override{return house_map_;}
  bool tile_sort_children(upstream::FieldObjectId,
                          std::vector<upstream::FieldCanvasNativeTileChild>&,
                          std::string &)const override;
  bool sprite_snapshot(upstream::FieldObjectId,
                       upstream::FieldCanvasAppearance &,
                       std::string &) const override;
  bool animated_snapshot(upstream::FieldObjectId,std::string &,uint32_t &,
                       upstream::FieldCanvasAppearance &,std::string &)const override;
  bool color_rect_snapshot(upstream::FieldObjectId,upstream::Vec2 &,
                       upstream::FieldColor &,std::string &)const override;
  // Read the same actual TextureRect/ColorRect native instance. This grants
  // no construction, lifecycle or source property mutation.
  bool control_state(upstream::FieldObjectId,PodunkSceneControlState&,
                     std::string&)const;
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
  // AFTER source remove_child/ExitNative, BEFORE old scene free/replacement.
  bool retain_detached_door(const upstream::FieldDoorData &,uint32_t,
                           PodunkSceneDoorTransfer &,std::string &);
  // Inside Registry's transfer callback: actual Tree/ObjectDB transfer has
  // completed; source add_child (and native Enter) has not happened yet.
  bool rebind_persistent_door(PodunkSceneDoorTransfer &,
      upstream::FieldNodeTreeRuntime &old_tree,upstream::FieldNodeTreeRuntime &next,
      const std::vector<upstream::FieldObjectId>&,std::string &);
  bool observe_persistent_door(const PodunkSceneDoorTransfer &,
                              upstream::FieldObjectId actual_parent,std::string &)const;
  // Caller has waited for the actual GPU fence and exited the real tree.
  bool shutdown(std::string &);

private:
  enum class Kind { Node, Canvas, Map, Body, Area, Shape, Sprite,TextureRect,Animated,ColorRect };
  struct Instance {
    upstream::FieldNodeTreeRuntime *tree=nullptr;
    const upstream::FieldDoorData *persistent_door=nullptr;
    upstream::FieldObjectId persistent_root=0;
    Kind kind = Kind::Node;
    uint32_t source = 0, geometry_node = UINT32_MAX, owner = UINT32_MAX,
             shape = UINT32_MAX, map = UINT32_MAX;
    bool entered = false, ready = false, bound = false, disabled = false,
         monitored = false, synchronized = false, space_disabled = false;
    upstream::FieldTransform space_local{};
    upstream::FieldCanvasAppearance sprite{};
    upstream::FieldCanvasAnimatedState animated{};
    upstream::FieldColor color{};
    bool source_ready=false;
  };
  bool actual(upstream::FieldObjectId, const upstream::FieldNodeDescriptor *&,
              const upstream::FieldNodeState *&, std::string &) const;
  bool synchronize(upstream::FieldObjectId, Instance &, std::string &);
  bool material(upstream::FieldObjectId, const upstream::FieldCanvasRecord &,
                std::string &) const;
  bool prepare_images(const upstream::FieldMapView &,
                const upstream::FieldCanvasArtData &,const char *,std::string &);
  const upstream::FieldMapView *draw_map_source()const;
  bool house_live(std::string &)const;
  bool collect_house_tiles(upstream::FieldMapRect,std::vector<uint32_t>&,
                           std::string &)const;
  bool tile_pose(uint32_t,upstream::FieldMapDraw &,std::string &)const;
  upstream::FieldCanvasAnimationHost animation_host();
  const upstream::FieldNodeTreeData *data_ = nullptr;
  upstream::FieldNodeTreeRuntime *tree_ = nullptr;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  PodunkNativeRoot *root_ = nullptr;
  upstream::FieldMapSpace *map_ = nullptr;
  upstream::FieldGeometrySpace *geometry_ = nullptr;
  const upstream::FieldCanvasArtData *art_ = nullptr;
  const upstream::HouseReturnSources *house_sources_=nullptr;
  const upstream::FieldMapView *house_map_=nullptr;
  HouseReturnGeometryNative *house_geometry_=nullptr;
  upstream::SourceRandom *house_random_=nullptr;
  upstream::FieldCanvasControlOwner *house_controls_=nullptr;
  const upstream::FieldNodeTreeRuntime *house_old_tree_=nullptr;
  upstream::FieldObjectId house_old_root_=0,house_root_=0,animation_call_object_=0;
  upstream::FieldCanvasNativeCall animation_call_=upstream::FieldCanvasNativeCall::SetProperty;
  bool animation_call_active_=false,house_replaced_=false;
  std::set<uint32_t>house_deleted_;
  upstream::FieldCanvasArtHost art_host_;
  PodunkSceneMaterialOwner *materials_ = nullptr;
  upstream::FieldObjectSignals *sprite_signals_ = nullptr;
  PodunkPlayerPhysicsWorld *world_ = nullptr;
  std::unique_ptr<FieldMapRenderer> map_gpu_;
  std::unique_ptr<FieldCanvasArtRenderer> art_gpu_;
  std::unique_ptr<PodunkPlayerCanvasForeign> foreign_;
  PodunkSceneGrassFactory *grass_ = nullptr;
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
