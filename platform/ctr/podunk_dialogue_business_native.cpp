#include "podunk_dialogue_business_native.hpp"
#include <algorithm>
#include <cmath>

namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e,const char *s){e=s;return false;}
bool same_source(const HouseUiContinuationData &ui,const FieldGlobalConstructorData &global){
  std::array<uint8_t,32> a{},b{};
  return ui.identity().upstream_commit==global.identity().upstream_commit &&
    ui.source_hash(ui.business_policy().global_script,a) &&
    global.source_hash(ui.business_policy().global_script,b) && a==b;
}
}
bool PodunkDialogueBusinessNative::prepare(PodunkDialogueBusinessInput in,std::string &e){
  if(in_.registry || !in.data || !in.data->valid() || !in.data->dialogue_continuation() ||
     !in.life || !in.life->valid() || !in.scene || !in.scene->valid() ||
     !in.registry || !in.signals || in.signals->registry()!=in.registry ||
     !in.global || !in.global->data() || !same_source(*in.data,*in.global->data()) ||
     !in.global_data || in.global_data->registry()!=in.registry || !in.ui ||
     in.registry->external_object(in.ui->binding().object)!=in.ui ||
     !in.root_script || !in.dialogue || !in.player || !in.bars || !in.fade ||
     !in.sounds || in.sounds->registry()!=in.registry ||
     in.data->identity().upstream_commit!=in.life->commit() ||
     in.scene->identity().upstream_commit!=in.life->commit())
    return fail(e,"Dialogue business requires the actual same source continuation owners");
  const auto &p=in.data->business_policy();
  const auto *phone=in.global->data()->member(FieldGlobalMemberRole::PhoneLocation);
  if(!phone || phone->name!=p.phone_member || p.widgets.size()!=3 ||
     p.end_signal.empty() || p.close_sound_source.empty() || p.close_sound_name.empty() ||
     in.scene->string(in.scene->area().region).empty())
    return fail(e,"Dialogue business source widget/region/global member binding differs");
  if(!in.ui->source_business_closed(e))return false;
  if(!in.bars->update(in.bars->target_open(),0))
    return fail(e,"Dialogue Blackbars actual checked rendered body absent");
  if(!in.fade->bind_dialogue_restore(*in.data,e))return false;
  in_=in;e.clear();return true;
}
bool PodunkDialogueBusinessNative::checked_step(const FieldDialogueStep &step,std::string &e)const{
  if(!in_.life || std::none_of(in_.life->steps().begin(),in_.life->steps().end(),
      [&](const auto &s){return s.stage==step.stage && s.op==step.op &&
        s.role==step.role && s.value==step.value && s.text==step.text;}))
    return fail(e,"Dialogue business unknown source lifecycle step");
  e.clear();return true;
}
bool PodunkDialogueBusinessNative::observe(FieldObjectId id,FieldDialogueObservation &out,std::string &e)const{
  if(!in_.registry || !in_.ui->source_business_closed(e))return false;
  if(id && !in_.root_script->source_observation(id,out,e))return false;
  e.clear();return true;
}
bool PodunkDialogueBusinessNative::admit(const FieldDialogueStep &step,
    const FieldProgrammeContext &context,std::string &e)const{
  if(!checked_step(step,e) || !in_.ui->source_business_closed(e))return false;
  using O=FieldDialogueOp;
  switch(step.op){
  case O::KeyClose:case O::BlackBars:case O::InfoHide:case O::CashClose:
  case O::PhoneLocation:case O::EmitCutsceneEnded:case O::ReturnCamera:case O::ReturnOffset:
    break;
  case O::KeyUpdate:
    return in_.ui->source_update_key_indicator(*in_.global_data,
      in_.scene->string(in_.scene->area().region),e);
  case O::TelepathyRestore:
    // This source lifecycle step restores the effect; positive activation
    // enters set_telepathy_effect through the actual player/NPC source call.
    if(step.value!=0)return fail(e,"Positive telepathy Fade effect is not admitted");
    break;
  default:return fail(e,"Dialogue operation is not an owned business source method");
  }
  if(context.dialogue_object && !in_.registry->tree_owner(context.dialogue_object))
    return fail(e,"Dialogue business context does not reference an actual factory node");
  e.clear();return true;
}
bool PodunkDialogueBusinessNative::manager(const FieldDialogueStep &step,
    FieldObjectId id,std::string &e){
  if(!checked_step(step,e) || !in_.registry->object_exists(id))
    return fail(e,"Dialogue business manager root is not alive");
  using O=FieldDialogueOp;
  switch(step.op){
  case O::KeyClose:return in_.ui->source_close_closed_widget(1,e);
  case O::CashClose:return in_.ui->source_close_closed_widget(2,e);
  case O::InfoHide:return in_.ui->source_close_closed_widget(3,e);
  case O::BlackBars:
    if(!in_.bars->update(step.value!=0,0))return fail(e,"Dialogue actual Blackbars toggle rejected");
    e.clear();return true;
  case O::KeyUpdate:return in_.ui->source_update_key_indicator(*in_.global_data,
      in_.scene->string(in_.scene->area().region),e);
  default:return fail(e,"Unknown dialogue business manager action");
  }
}
bool PodunkDialogueBusinessNative::global(const FieldDialogueStep &step,
    FieldObjectId id,FieldObjectId talker,std::string &e){
  if(!checked_step(step,e) || !in_.registry->object_exists(id) || talker)
    return fail(e,"Dialogue business global action root/arguments rejected");
  if(step.op==FieldDialogueOp::PhoneLocation)
    return in_.global->set_string(FieldGlobalMemberRole::PhoneLocation,step.text,e);
  if(step.op==FieldDialogueOp::EmitCutsceneEnded){
    const auto object=in_.global->owner();
    if(step.text!=in_.data->business_policy().end_signal || !in_.registry->object_exists(object))
      return fail(e,"Dialogue source global completion emitter differs");
    return in_.signals->emit(object,step.text,{},e);
  }
  return fail(e,"Unknown dialogue business global method");
}
bool PodunkDialogueBusinessNative::close_sound(std::string &e){
  if(!in_.registry)return fail(e,"Dialogue close sound actual owner absent");
  const auto &p=in_.data->business_policy();FieldObjectId voice=0;
  return in_.sounds->play_sfx(p.close_sound_source,p.close_sound_name,voice,e);
}
bool PodunkDialogueBusinessNative::restore_telepathy(std::string &e){
  if(!in_.registry)return fail(e,"Dialogue restore actual Fade owner absent");
  // The owning Fade resumes precisely its own audited false-spin continuation.
  // This is not an invented public Fade node or a fake SignalBus emitter.
  return in_.fade->restore_cut([this](std::string &error){
    return in_.fade->stop_dialogue_spin(error);
  },e);
}
bool PodunkDialogueBusinessNative::idle(uint64_t epoch,float dt,std::string &e){
  if(!in_.registry)return fail(e,"Dialogue business idle before owning prepare");
  return in_.fade->restore_idle(epoch,dt,e);
}
bool PodunkDialogueBusinessNative::current_camera(FieldGameCameraRuntime *&out,uint32_t &source,std::string &e)const{
  if(!in_.registry)return fail(e,"Dialogue current camera before actual owning prepare");
  FieldObjectId actual=0;
  if(!in_.global->object(FieldGlobalMemberRole::CurrentCamera,actual,e))return false;
  auto tree=in_.registry->tree_owner(actual);
  const auto *descriptor=tree ? tree->descriptor(actual) : nullptr;
  const auto *native=tree ? tree->state(actual) : nullptr;
  if(!descriptor || !native || !native->inside || !native->bound ||
     !in_.registry->object_exists(actual) || descriptor->native_class!="Camera2D")
    return fail(e,"Dialogue return camera is not the actual live current Camera2D");
  FieldGameCameraRuntime *camera=&in_.player->children().camera();
  const auto *player_camera=camera->data() ? camera->data()->record(descriptor->id) : nullptr;
  if(player_camera){
    FieldObjectId resolved=0;
    if(in_.player->tree()!=tree.get() ||
       !tree->get_node(in_.player->body().object(),player_camera->node,resolved,e) ||
       resolved!=actual)
      return fail(e,"Dialogue current player camera is a different source instance");
  }else{
    FieldObjectId root=actual;
    while(root){
      const auto *d=tree->descriptor(root);const auto *n=tree->state(root);
      if(d && d->id==in_.life->factory_scene_id())break;
      root=n ? n->parent : 0;
    }
    auto *visual=root ? in_.dialogue->visual(root) : nullptr;
    camera=visual ? &visual->camera() : nullptr;
    const auto match=visual ? visual->objects().find(descriptor->id) :
      std::map<uint32_t,FieldObjectId>::const_iterator{};
    if(!visual || match==visual->objects().end() || match->second!=actual)
      return fail(e,"Dialogue current camera has a different actual factory owner");
  }
  const auto *state=camera ? camera->state(descriptor->id) : nullptr;
  if(!state || !state->alive || !state->ready || !camera->data()->record(descriptor->id))
    return fail(e,"Dialogue current camera has no matching actual source runtime");
  out=camera;source=descriptor->id;e.clear();return true;
}
bool PodunkDialogueBusinessNative::set_telepathy_effect(bool enabled,FieldObjectId target,std::string &e){
  if(!enabled)return restore_telepathy(e);
  if(!in_.registry || !target || !in_.registry->object_exists(target))
    return fail(e,"Telepathy focus is not an actual same ObjectDB target");
  auto tree=in_.registry->tree_owner(target);const auto *body=tree ? tree->state(target) : nullptr;
  if(!body || !body->inside || !body->bound || !(body->flags&1))
    return fail(e,"Telepathy focus target is not an actual entered CanvasItem");
  FieldTransform world{};
  if(!tree->world_transform(target,world,e))return false;
  FieldGameCameraRuntime *camera=nullptr;uint32_t source=0;
  if(!current_camera(camera,source,e))return false;
  const auto *state=camera->state(source);const auto &p=in_.data->business_policy();
  // Source focus_object recenters its same ColorRect then subtracts the actual
  // current camera screen center. Source composition is adapted at draw time.
  const Vec2 focus{world[2].x-state->screen_center.x+p.screen_size.x/2,
                   world[2].y-state->screen_center.y+p.screen_size.y/2};
  return in_.fade->telepathy_cut(focus,e);
}
bool PodunkDialogueBusinessNative::return_camera(bool offset,double duration,std::string &e){
  if(!std::isfinite(duration) || duration<=0 || duration>120)
    return fail(e,"Dialogue source camera return duration rejected");
  FieldGameCameraRuntime *camera=nullptr;uint32_t source=0;
  if(!current_camera(camera,source,e))return false;
  uint64_t token=0;
  const bool ok=offset ? camera->return_offset(source,float(duration),token) :
                         camera->return_camera(source,float(duration),token);
  if(!ok){e=camera->error();return false;}
  e.clear();return true;
}
} // namespace encore::ctr
