#pragma once
#include "house_return_npc_runtime.hpp"
#include "podunk_npc_animation.hpp"
#include "podunk_scene_native.hpp"
#include "podunk_scene_timers.hpp"
#include "encore/field_scene_tree_timer.hpp"

namespace encore::ctr {
struct HouseReturnNpcPresentationInput {
  const upstream::HouseReturnSources *sources=nullptr;
  upstream::FieldNodeTreeRuntime *tree=nullptr;
  upstream::FieldGlobalRegistry *registry=nullptr;
  upstream::FieldObjectSignals *signals=nullptr;
  HouseReturnNpcRuntime *npcs=nullptr;
  upstream::FieldSpriteRuntime *sprites=nullptr;
  PodunkNpcAnimationHost *animations=nullptr;
  PodunkSceneNative *native_sprites=nullptr;
  PodunkSceneTimers *node_timers=nullptr;
  // Borrow the retained global list. This owner never initializes, advances,
  // shuts down or replaces it, including when the House subtree is deleted.
  upstream::FieldSceneTreeTimers *scene_timers=nullptr;
};
// Concrete npc.gd presentation and timer operations. NPC movement, shapes,
// Shadow, UI, flags and source lifecycle remain with their actual owners.
// Fixed address: ports and live timer receivers borrow this object.
class HouseReturnNpcPresentation final {
public:
  HouseReturnNpcPresentation()=default;
  HouseReturnNpcPresentation(const HouseReturnNpcPresentation&)=delete;
  HouseReturnNpcPresentation&operator=(const HouseReturnNpcPresentation&)=delete;
  bool prepare(HouseReturnNpcPresentationInput,std::string&);
  // Only the two source operations are replaced. No callback here grants Ready.
  bool apply(HouseReturnNpcPorts&,std::string&);
  // After the actual NPC core is initialized, before Sprite construction.
  // Native publish/sprite_changed/reflection endpoints must already be real.
  // The one checked NPC graph installs its execution ports in that host.
  bool initialize_sprites(upstream::FieldSpriteHost,std::string&);
  // Actual full factory, before Enter. Restores source WanderTimer connections;
  // it does not execute child/parent Ready or start a timer.
  bool finish_factory(std::string&);
  bool sprite(upstream::FieldObjectId,upstream::FieldNpcPresentation,
      const upstream::FieldNpcDescriptor&,const upstream::FieldNpcInstance&,
      std::string&);
  bool timer(upstream::FieldObjectId,upstream::FieldObjectId,
      upstream::FieldNpcTimer,double,uint64_t,std::string&);
  bool handles_callback(const upstream::FieldDeferredMessage&)const;
  bool deferred(const upstream::FieldDeferredMessage&,std::string&);
  // Call after the retained list's sole actual idle tail; only remove expired
  // weak receipts. There is deliberately no idle(delta) or second countdown.
  bool collect_expired(std::string&);
private:
  struct Waiting {
    upstream::FieldObjectId target=0;
    uint32_t source=0;
    uint64_t waiter=0;
    std::weak_ptr<upstream::FieldSceneTreeTimer> timer;
    bool returned=false;
  };
  bool borrows(std::string&)const;
  bool node(uint32_t,upstream::FieldObjectId&,bool,std::string&)const;
  bool npc(upstream::FieldObjectId,uint32_t&,
      const upstream::FieldNpcDescriptor*&,const upstream::FieldNpcInstance*&,
      bool,std::string&)const;
  bool create_return(upstream::FieldObjectId,uint32_t,
      const upstream::FieldNpcInstance&,double,uint64_t,std::string&);
  HouseReturnNpcPresentationInput in_{};
  std::map<upstream::FieldObjectId,Waiting> waiting_;
  std::string return_method_,wander_method_;
  bool prepared_=false,applied_=false,sprites_initialized_=false,factory_=false;
};
}
