#pragma once
#include "encore/field_global_registry.hpp"
#include "encore/field_object_signals.hpp"
#include "encore/field_scene_sources.hpp"
#include "encore/grass_native.hpp"
#include <map>
namespace encore::ctr {
class PodunkSceneCanvasLeaf;
// Original onready preload owns a checked PackedScene before spawner Ready.
// The same concrete factory owns dynamic native/source nodes through deletion.
class PodunkSceneGrassFactory : public upstream::FieldGlobalSourceResource {
public:
  virtual const upstream::FieldGlobalRegistry *registry() const = 0;
  virtual const upstream::FieldData *source_data() const = 0;
  virtual const upstream::FieldNodeRecipeData &recipe() const = 0;
  virtual PodunkSceneCanvasLeaf *canvas_leaf() = 0;
  virtual bool owns(const upstream::FieldNodeDescriptor &) const = 0;
  virtual bool owns(upstream::FieldObjectId) const = 0;
  virtual bool native_construct(upstream::FieldObjectId,const upstream::FieldNodeDescriptor&,const upstream::FieldIdentity&,std::string&) = 0;
  virtual bool construct_source(upstream::FieldObjectId,const upstream::FieldNodeDescriptor&,const upstream::FieldIdentity&,std::string&) = 0;
  virtual bool bind(upstream::FieldObjectId,upstream::FieldNodeBinding&,std::string&) = 0;
  virtual bool phase(upstream::FieldObjectId,upstream::FieldTreePhase,float,bool,std::string&) = 0;
  virtual bool signal_declaration(upstream::FieldObjectId,std::string_view,uint32_t&,std::string&) const = 0;
  virtual bool handles_callback(const upstream::FieldDeferredMessage&) const = 0;
  virtual bool tween_frame(uint64_t,float,bool,std::string&) = 0;
  virtual bool release(upstream::FieldObjectId,std::string&) = 0;
  virtual bool admit_canvas(upstream::FieldObjectId,const upstream::FieldNodeDescriptor&,const upstream::FieldIdentity&,const upstream::FieldNodeTreeRuntime&,bool&drawable,std::string&) const = 0;
  virtual bool screen_entered(upstream::FieldObjectId, uint32_t,
                              std::string &) = 0;
  virtual bool screen_exited(upstream::FieldObjectId, uint32_t,
                             std::string &) = 0;
};
struct PodunkSceneScriptGap {
  uint32_t source = 0, ordinal = 0;
  upstream::FieldSceneRole role{};
  std::string path, script, reason;
};
// Source bodies borrow the already initialized typed cores. Native node
// allocation, native lifecycle, animation/audio clocks and GPU ownership are
// handled by their concrete owners. This adapter never grants their Ready.
class PodunkSceneScripts {
public:
  bool prepare(const upstream::FieldSceneSources &,
               upstream::FieldSceneConsumers, upstream::FieldSceneHostOps,
               upstream::FieldNodeTreeRuntime &,
               upstream::FieldGlobalRegistry &, upstream::FieldObjectSignals &,
               PodunkSceneGrassFactory *, std::string &);
  bool owns(const upstream::FieldNodeDescriptor &) const;
  bool owns(upstream::FieldObjectId) const;
  bool construct_source(upstream::FieldObjectId,
                        const upstream::FieldNodeDescriptor &,
                        const upstream::FieldIdentity &, std::string &);
  bool bind(upstream::FieldObjectId, const upstream::FieldNodeDescriptor &,
            upstream::FieldNodeBinding &, std::string &);
  // Only script phases. The caller separately executes the same node's native
  // phase; passing a native phase here is an error, not an empty admission.
  bool phase(upstream::FieldObjectId, const upstream::FieldNodeBinding &,
             upstream::FieldTreePhase, float actual_delta, std::string &);
  bool deferred(const upstream::FieldDeferredMessage &, std::string &);
  bool release(upstream::FieldObjectId, const upstream::FieldNodeBinding &,
               std::string &);
  bool collect_deleted(std::string &);
  bool grass_screen(upstream::FieldObjectId, bool entered, std::string &);
  bool npc_screen(upstream::FieldObjectId, bool entered, std::string &);
  bool npc_interact(upstream::FieldObjectId, bool telepathy, std::string &);
  bool landmark_recheck(upstream::FieldObjectId, std::string &);
  // Input owner supplies the real event's source action-pressed result. Generic
  // Input phases cannot manufacture an acceptance from an unrelated event.
  bool transition_accept(upstream::FieldObjectId, bool source_action_pressed,
                         std::string &);
  bool transition_native_idle(upstream::FieldObjectId, double, bool can_process,
                              std::string &);
  bool object_for_source(uint32_t, upstream::FieldObjectId &,
                         std::string &) const;
  bool admission(upstream::FieldObjectId,
                 upstream::FieldSceneScriptAdmission &) const;
  upstream::FieldSceneHost &lifecycle() { return lifecycle_; }
  const std::vector<PodunkSceneScriptGap> &gaps() const { return gaps_; }
  bool poisoned() const { return poisoned_; }

private:
  struct Instance {
    upstream::FieldSceneReady source;
    upstream::FieldNodeBinding binding;
    bool bound = false, ready = false, released = false, deleted = false;
  };
  bool actual(upstream::FieldObjectId, Instance *&, std::string &);
  bool supported(upstream::FieldSceneRole) const;
  bool construct_body(const upstream::FieldSceneReady &, std::string &);
  bool grass_preload(std::string &) const;
  bool grass_dynamic_factory(std::string &) const;
  bool grass_preview(upstream::FieldObjectId, const upstream::FieldSceneReady &,
                     upstream::FieldObjectId &, std::string &) const;
  bool fail(std::string &, const char *) const;
  const upstream::FieldSceneSources *sources_ = nullptr;
  upstream::FieldSceneConsumers consumers_;
  upstream::FieldNodeTreeRuntime *tree_ = nullptr;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  upstream::FieldObjectSignals *signals_ = nullptr;
  PodunkSceneGrassFactory *grass_factory_ = nullptr;
  upstream::FieldSceneHost lifecycle_;
  std::map<uint32_t, upstream::FieldSceneReady> roster_;
  std::map<upstream::FieldObjectId, Instance> instances_;
  std::map<uint32_t, upstream::FieldObjectId> objects_;
  std::vector<PodunkSceneScriptGap> gaps_;
  bool poisoned_ = false;
};
} // namespace encore::ctr
