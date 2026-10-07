#pragma once
#include "field_grass_renderer.hpp"
#include "podunk_scene_native.hpp"
#include "podunk_scene_scripts.hpp"
#include <list>
namespace encore::ctr {
class PodunkConcreteSceneGrassFactory final : public PodunkSceneGrassFactory,
                                              public PodunkSceneCanvasLeaf {
public:
  ~PodunkConcreteSceneGrassFactory();
  static bool create(const upstream::GrassNativeData &,
                     const upstream::FieldData &, upstream::FieldRuntime &,
                     std::shared_ptr<upstream::FieldNodeTreeRuntime>,
                     upstream::FieldGlobalRegistry &,
                     upstream::FieldObjectSignals &,
                     upstream::FieldGeometrySpace &, PodunkPlayerPhysicsWorld &,
                     const char *asset_root, PodunkConcreteSceneGrassFactory *&,
                     std::string &);
  upstream::FieldGlobalExternalBinding binding() const override {
    return binding_;
  }
  const char *resource_class() const override { return "PackedScene"; }
  const upstream::FieldGlobalRegistry *registry() const override {
    return registry_;
  }
  const upstream::FieldData *source_data() const override { return field_; }
  const upstream::FieldNodeRecipeData &recipe() const override {
    return data_->recipe();
  }
  PodunkSceneCanvasLeaf *canvas_leaf() override { return this; }
  bool state(upstream::FieldGlobalExternalState &,
             std::string &) const override;
  bool persist_append(upstream::FieldObjectId, std::string &) override;
  bool assign_stable_canvas(upstream::FieldObjectId, std::string &) override;
  bool screen_entered(upstream::FieldObjectId, uint32_t,
                      std::string &) override;
  bool screen_exited(upstream::FieldObjectId, uint32_t, std::string &) override;
  bool owns(const upstream::FieldNodeDescriptor &) const override;
  bool owns(upstream::FieldObjectId) const override;
  bool native_construct(upstream::FieldObjectId,
                        const upstream::FieldNodeDescriptor &,
                        const upstream::FieldIdentity &,
                        std::string &) override;
  bool construct_source(upstream::FieldObjectId,
                        const upstream::FieldNodeDescriptor &,
                        const upstream::FieldIdentity &,
                        std::string &) override;
  bool bind(upstream::FieldObjectId, upstream::FieldNodeBinding &,
            std::string &) override;
  bool phase(upstream::FieldObjectId, upstream::FieldTreePhase, float, bool,
             std::string &) override;
  bool signal_declaration(upstream::FieldObjectId, std::string_view, uint32_t &,
                          std::string &) const override;
  bool handles_callback(const upstream::FieldDeferredMessage &) const override;
  bool deferred(const upstream::FieldDeferredMessage &, std::string &) override;
  bool tween_frame(uint64_t, float, bool, std::string &) override;
  bool release(upstream::FieldObjectId, std::string &) override;
  bool admit_canvas(upstream::FieldObjectId,
                    const upstream::FieldNodeDescriptor &,
                    const upstream::FieldIdentity &,
                    const upstream::FieldNodeTreeRuntime &, bool &,
                    std::string &) const override;
  const upstream::FieldNodeTreeRuntime *canvas_tree() const override {
    return tree_.get();
  }
  const upstream::FieldGlobalRegistry *canvas_registry() const override {
    return registry_;
  }
  bool owns_drawable(upstream::FieldObjectId) const override;
  bool draw_leaf(const upstream::FieldCanvasOrderSlot &,
                 const upstream::FieldTransform &, bool,
                 std::string &) override;
  bool shutdown(std::string &);

private:
  class Reference;
  struct Node {
    upstream::FieldObjectId root = 0;
    upstream::GrassNativeRole role{};
    upstream::FieldNodeBinding binding;
    bool entered = false, ready = false, source_ready = false, released = false;
  };
  struct Instance {
    uint32_t spawner = 0;
    std::array<upstream::FieldObjectId, 6> nodes{};
    std::vector<upstream::FieldObjectId> bodies;
    bool source_constructed = false, onready = false, published = false,
         geometry = false, monitor = false, timer_alive = true;
  };
  struct Tween {
    std::shared_ptr<Reference> owner, property, waiter;
    upstream::FieldObjectId root = 0, sprite = 0;
    float elapsed = 0, duration = 0;
    bool dead = false;
  };
  bool actual(upstream::FieldObjectId, Node *&, std::string &);
  bool source_spawner(upstream::FieldObjectId, uint32_t, upstream::FieldGrass &,
                      std::string &) const;
  bool synchronize(upstream::FieldObjectId, std::string &);
  bool create_tween(upstream::FieldObjectId, bool, std::string &);
  bool make_reference(const char *, std::string_view, uint32_t,
                      std::shared_ptr<Reference> &, std::string &);
  bool finish_waiter(upstream::FieldObjectId, std::string &);
  bool update_positions(upstream::FieldObjectId, std::string &);
  const upstream::GrassNativeData *data_ = nullptr;
  const upstream::FieldData *field_ = nullptr;
  upstream::FieldRuntime *core_ = nullptr;
  std::shared_ptr<upstream::FieldNodeTreeRuntime> tree_;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  upstream::FieldObjectSignals *signals_ = nullptr;
  upstream::FieldGeometrySpace *space_ = nullptr;
  PodunkPlayerPhysicsWorld *world_ = nullptr;
  upstream::FieldGlobalExternalBinding binding_;
  FieldGrassRenderer renderer_;
  std::map<upstream::FieldObjectId, Node> nodes_;
  std::map<upstream::FieldObjectId, Instance> instances_;
  std::list<Tween> tweens_;
  std::map<upstream::FieldObjectId, std::weak_ptr<Reference>> references_;
  upstream::FieldObjectId building_ = 0;
  uint64_t tween_epoch_ = 0;
  bool building_factory_ = false, poisoned_ = false;
};
} // namespace encore::ctr
