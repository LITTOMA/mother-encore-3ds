#pragma once
#include "podunk_scene_loop.hpp"
namespace encore::ctr {
// Four real native objects for the already running House's checked door.
// The old House still performs its original overlap detection. Persistent
// migration retains these exact IDs and this same source Door coroutine.
class PodunkHouseDoorContinuation final : public PodunkSceneNativeMechanism {
public:
  bool initialize(const upstream::FieldDoorData &, uint32_t,
                  upstream::HouseRuntime &, upstream::FieldGlobalRegistry &,
                  upstream::FieldDoorRuntime &, std::string &);
  bool enter(std::string &);
  bool owns(const upstream::FieldNodeDescriptor &) const override;
  bool owns(upstream::FieldObjectId) const override;
  bool construct(upstream::FieldObjectId,const upstream::FieldNodeDescriptor &,
                 const upstream::FieldIdentity &,std::string &) override;
  bool bind(upstream::FieldObjectId,upstream::FieldNodeBinding &,std::string &) override;
  bool phase(upstream::FieldObjectId,upstream::FieldTreePhase,float,bool,bool,std::string &) override;
  bool deferred(const upstream::FieldDeferredMessage &,std::string &) override;
  bool release(upstream::FieldObjectId,std::string &) override;
  bool declaration(upstream::FieldObjectId,std::string_view,uint32_t &,std::string &)const;
  bool resolve_onready(const upstream::FieldDoorDescriptor &,
                       const upstream::FieldDoorAudio &,std::string &)const;
  bool connect_body(uint32_t,std::function<bool(uint64_t,std::string &)>,std::string &);
  bool body_entered(uint64_t,std::string &);
  bool marker_world(uint32_t,upstream::Vec2 &,std::string &)const;
  bool detach(std::string &);
  bool reparent(upstream::FieldObjectId,std::string &);
  bool queue_free(uint32_t,std::string &);
  upstream::FieldObjectId object()const{return object_;}
  upstream::FieldObjectId audio_object()const;
private:
  struct Native {upstream::FieldNodeDescriptor source;bool entered=false,ready=false;};
  const upstream::FieldDoorData *data_=nullptr;
  upstream::FieldDoorDescriptor door_{};
  upstream::HouseRuntime *house_=nullptr;
  upstream::FieldGlobalRegistry *registry_=nullptr;
  upstream::FieldDoorRuntime *runtime_=nullptr;
  std::shared_ptr<upstream::FieldNodeTreeRuntime> tree_;
  upstream::FieldObjectId object_=0;
  std::map<upstream::FieldObjectId,Native> nodes_;
  std::function<bool(uint64_t,std::string &)> body_;
};
} // namespace encore::ctr
