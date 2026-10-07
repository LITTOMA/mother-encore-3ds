#include "house_return_camera_control.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>

namespace encore::ctr {
using namespace upstream;
namespace {
bool reject(std::string&e,const char*m){e=m;return false;}
bool same(const FieldIdentity&a,const FieldIdentity&b){return a.scene_id==b.scene_id&&a.source_sha256==b.source_sha256&&a.upstream_commit==b.upstream_commit;}
bool nonzero(const std::array<uint8_t,32>&a){return std::any_of(a.begin(),a.end(),[](auto x){return x!=0;});}
struct Callback {size_t&d;explicit Callback(size_t&v):d(v){++d;}~Callback(){--d;}};
}
bool HouseReturnCameraControl::prepare(HouseReturnCameraControlInput in,std::string&e){
 if(prepared_||!in.sources||!in.data||!in.tree||!in.registry||!in.signals||!in.global||!in.random||!in.ui||!in.player||!in.node_timers||!in.scene_timers||!in.controls||!in.effects)
  return reject(e,"RoomShaker requires concrete same-session source/native owners");
 if(!in.sources->valid()||!in.data->valid()||!same(in.data->identity(),in.sources->tree().identity())||in.data->tree_ir_sha256()!=in.sources->canvas().tree_ir_sha()||
  in.global->registry()!=in.registry||in.signals->registry()!=in.registry||in.node_timers->registry()!=in.registry||in.scene_timers->registry()!=in.registry||
  in.controls->canvas_tree()!=in.tree||in.controls->canvas_registry()!=in.registry||in.controls->canvas_data()!=&in.sources->canvas()||
  in.registry->poisoned()||(in.tree->object_domain()&&in.tree->object_domain()!=in.registry->kernel()))
  return reject(e,"RoomShaker same House source/Tree/ObjectDB/clock receipt differs");
 std::array<uint8_t,32>sha{},actual_sha{};const auto ui=in.ui->binding();
 if(!in.global->data()||!in.data->source_hash(in.global->data()->owner_source(),sha)||
  !in.global->data()->source_hash(in.global->data()->owner_source(),actual_sha)||actual_sha!=sha||
  !in.data->source_hash(ui.source.script,sha)||sha!=ui.source.script_sha||
  ui.source.identity.upstream_commit!=in.data->identity().upstream_commit||in.registry->external_object(ui.object)!=in.ui)
  return reject(e,"RoomShaker actual continued global/UiManager source fingerprints differ");
 if(!in.player->body().data()||!in.data->source_hash(in.player->body().data()->player_source(),sha)||
  !in.player->body().data()->source_hash(in.player->body().data()->player_source(),actual_sha)||sha!=actual_sha||
  in.player->body().data()->identity().upstream_commit!=in.data->identity().upstream_commit)
  return reject(e,"RoomShaker actual retained Player.get_state source fingerprint differs");
 in_=in;prepared_=true;e.clear();return true;
}
bool HouseReturnCameraControl::borrows(std::string&e)const{
 if(!prepared_||failed_||!in_.data->valid()||!in_.sources->valid()||in_.registry->poisoned()||in_.tree->object_domain()!=in_.registry->kernel()||
  in_.global->registry()!=in_.registry||in_.signals->registry()!=in_.registry||in_.node_timers->registry()!=in_.registry||in_.scene_timers->registry()!=in_.registry||
  in_.controls->canvas_tree()!=in_.tree||in_.controls->canvas_registry()!=in_.registry||in_.controls->canvas_data()!=&in_.sources->canvas())
  return reject(e,"RoomShaker actual owner borrow expired");return true;
}
bool HouseReturnCameraControl::owns(const FieldNodeDescriptor&d)const{
 if(!in_.data)return false;const auto&r=in_.data->nodes()[0];std::array<uint8_t,32>sha{};
 return d.id==r.id&&d.path==r.path&&d.native_class==r.native_class&&d.ready==r.ready&&d.script==in_.data->script()&&in_.data->source_hash(d.script,sha)&&sha==d.script_sha;
}
bool HouseReturnCameraControl::owns(FieldObjectId id)const{return object_&&id==object_;}
bool HouseReturnCameraControl::node(uint32_t stable,FieldObjectId&out,bool ready,std::string&e)const{
 if(!borrows(e))return false;out=in_.tree->source_object(stable);const auto*n=in_.tree->state(out);const auto*d=in_.tree->descriptor(out);FieldIdentity identity;
 if(!out||!n||!n->alive||!d||d->id!=stable||!in_.registry->object_exists(out)||in_.registry->tree_owner(out).get()!=in_.tree||
  !in_.tree->object_identity(out,identity)||!same(identity,in_.data->identity())||(ready&&(!n->inside||!n->bound||!n->ready_notified)))
  return reject(e,"RoomShaker actual source node/lifecycle unavailable");return true;
}
bool HouseReturnCameraControl::live(FieldObjectId id,bool ready,std::string&e)const{
 FieldObjectId exact=0;if(!object_||id!=object_||!node(in_.data->nodes()[0].id,exact,ready,e)||exact!=id||!owns(*in_.tree->descriptor(id))||
  (ready&&(!ready_||!entered_)))return reject(e,"RoomShaker source receiver/Ready differs");
 HouseControlNativeState c;if(!in_.controls->snapshot(id,c,e)||!c.constructed||c.object!=id||(ready&&(!c.ready||!c.entered)))return reject(e,"RoomShaker actual Control native receiver missing");
 return true;
}
bool HouseReturnCameraControl::construct(FieldObjectId id,const FieldNodeDescriptor&d,const FieldIdentity&identity,std::string&e){
 FieldObjectId exact=0;HouseControlNativeState c;
 if(!borrows(e)||object_||!owns(d)||!same(identity,in_.data->identity())||!node(d.id,exact,false,e)||exact!=id||
  in_.tree->state(id)->inside||in_.tree->state(id)->parent||in_.tree->state(id)->bound||
  !in_.controls->snapshot(id,c,e)||!c.constructed||c.object!=id)return reject(e,"RoomShaker source construct requires actual native Control before parent/Ready");
 object_=id;e.clear();return true;
}
bool HouseReturnCameraControl::bind(FieldObjectId id,const FieldNodeBinding&b,std::string&e){
 if(!live(id,false,e)||bound_||b.stable_id!=in_.data->nodes()[0].id||!same(b.identity,in_.data->identity())||
  b.script_sha!=in_.tree->descriptor(id)->script_sha||b.class_index!=in_.tree->descriptor(id)->class_index||b.native_class!="Control")
  return reject(e,"RoomShaker source/native binding differs");bound_=true;e.clear();return true;
}
bool HouseReturnCameraControl::source_call_live(FieldObjectId id,std::string&e)const{
 if(!depth_||!live(id,false,e))return reject(e,"RoomShaker effect requires its actual source callback receiver");e.clear();return true;
}
bool HouseReturnCameraControl::source_closed(std::string&e)const{
 if(!borrows(e)||depth_||!waiting_.empty())return reject(e,"RoomShaker source callback/delayed waiter is still live");
 if(deleted_object_&&!object_){
  for(auto id:{deleted_object_,deleted_timer_,deleted_audio_})if(in_.tree->state(id)||in_.registry->object_exists(id))return reject(e,"RoomShaker actual deleted-owner receipt still has a live node");
  if(in_.controls->owns(deleted_object_)||in_.node_timers->owns(deleted_timer_))return reject(e,"RoomShaker deleted native owner is still retained");
 }else if(!object_||!in_.node_timers->state(timer_)||in_.node_timers->state(timer_)->processing)return reject(e,"RoomShaker native Timer is still live or receiver absent");
 e.clear();return true;
}
bool HouseReturnCameraControl::effects(bool loaded,std::string&e)const{
 HouseRoomShakerEffectsState state;PodunkSceneAudioState audio;std::array<uint8_t,32>sha{};
 if(!in_.effects->observe(object_,audio_,state,e)||state.tree!=in_.tree||state.registry!=in_.registry||state.ui!=in_.ui||state.global!=in_.global||state.random!=in_.random||
  !state.audio_owner||!state.audio_owner->owns(audio_)||!state.audio_owner->state(audio_,audio,e)||audio.object!=audio_||!audio.ready||audio.bus!=in_.data->bus()||
  state.control!=object_||state.audio!=audio_||!state.audio_constructed||!state.audio_ready||state.bus!=in_.data->bus())return reject(e,"RoomShaker effect receipt is not its actual same native/source owners");
 if(loaded&&(!in_.data->source_hash(in_.data->sound(),sha)||!state.stream||!in_.registry->object_exists(state.stream)||
  state.source_sound!=in_.data->sound()||state.sound_sha!=sha||
  !([&]{const auto*r=in_.registry->source_resource(state.stream);if(!r)return false;const auto b=r->binding();return b.object==state.stream&&b.source.source==in_.data->sound()&&b.source.source_sha==sha&&b.source.identity.upstream_commit==in_.data->identity().upstream_commit&&std::string_view(r->resource_class())=="AudioStreamSample";})()))return reject(e,"RoomShaker actual Audio stream Resource/source load missing");
 return true;
}
bool HouseReturnCameraControl::finish_factory(std::string&e){
 if(factory_||!bound_||!live(object_,false,e)||!node(in_.data->nodes()[1].id,timer_,false,e)||!node(in_.data->nodes()[2].id,audio_,false,e)||
  in_.tree->state(timer_)->parent!=object_||in_.tree->state(audio_)->parent!=object_||!in_.tree->state(timer_)->bound||!in_.tree->state(audio_)->bound||
  !in_.node_timers->owns(timer_)||!in_.node_timers->state(timer_))return reject(e,"RoomShaker complete source/native factory unavailable");
 const auto*state=in_.node_timers->state(timer_);
 // RoomShaker reusable Timer has native defaults, independently checked by the
 // existing House Timer pack/native owner. Do not change them at factory time.
 if(state->one_shot||state->autostart||state->mode!=1||state->processing||state->left!=-1)
  return reject(e,"RoomShaker native Timer constructor differs from original reusable");
 bool connected=false;if(!in_.signals->connected(timer_,"timeout",object_,"_on_Timer_timeout",connected,e)||connected||
  !in_.signals->connect(timer_,"timeout",object_,"_on_Timer_timeout",FieldSignalPersist,{},e))return reject(e,"RoomShaker source Persist timeout connection differs/repeated");
 factory_=true;e.clear();return true;
}
bool HouseReturnCameraControl::phase(FieldObjectId id,FieldTreePhase phase,std::string&e){
 if(!factory_||!live(id,false,e))return false;const auto*n=in_.tree->state(id);
 if(phase==FieldTreePhase::EnterScript){if(entered_||!n->inside||!n->bound)return reject(e,"RoomShaker actual Enter cursor differs");entered_=true;}
 else if(phase==FieldTreePhase::ReadyScript){
  if(ready_||!entered_||!n->inside||!n->ready_notified||!node(in_.data->nodes()[1].id,timer_,true,e)||!node(in_.data->nodes()[2].id,audio_,true,e)||!effects(false,e))return reject(e,"RoomShaker source Ready/child native owner unavailable or replayed");
  Callback call(depth_);std::array<uint8_t,32>sha{};
  if(!in_.node_timers->set_wait(timer_,float(in_.data->wait()),e)||!in_.data->source_hash(in_.data->sound(),sha)||
   !in_.effects->load_sound(id,audio_,in_.data->sound(),sha,e)||!effects(true,e)){failed_=true;return false;}
  if(in_.data->auto_start()&&!in_.node_timers->start(timer_,0,e)){failed_=true;return false;}ready_=true;
 }else if(phase==FieldTreePhase::ExitScript){if(!entered_||!n->inside)return reject(e,"RoomShaker Exit cursor differs");entered_=false;}
 else return reject(e,"RoomShaker source has no process/native notification method");
 e.clear();return true;
}
bool HouseReturnCameraControl::timeout(FieldObjectId id,std::string&e){
 if(!live(id,true,e))return false;bool battle=false,game_over=false;
 const auto ui=in_.ui->binding().object;if(!in_.ui->source_is_in_battle(ui,battle,e))return false;if(battle){e.clear();return true;}
 if(!in_.effects->source_game_over(ui,game_over,e))return false;if(game_over){e.clear();return true;}
 if(in_.player->registry()!=in_.registry||in_.player->tree()!=in_.tree||!in_.player->ready_complete())return reject(e,"RoomShaker actual global Player owner unavailable");
 std::shared_ptr<const GlobalLoadObjectArray>party;
 if(!in_.global->array(FieldGlobalMemberRole::PartyObjects,party,e)||!party||party->values.empty()||party->values.front()!=in_.player->body().object())return reject(e,"RoomShaker global.get_player actual first party object differs");
 PlayerInitializationMember state;
 if(!in_.player->body().member(in_.data->player_state_member(),state,e)||state.kind!=2||!state.value||state.value->kind!=2)return reject(e,"RoomShaker actual Player.get_state source member missing");
 if(state.value->integer==in_.data->player_camera()){e.clear();return true;}return vibrate(id,e);
}
bool HouseReturnCameraControl::vibrate(FieldObjectId id,std::string&e){
 if(!live(id,true,e)||!effects(true,e))return false;Callback call(depth_);
 FieldObjectId camera=0;if(!in_.global->object(FieldGlobalMemberRole::CurrentCamera,camera,e))return false;
 auto&owner=in_.player->children();const auto actual=owner.actual(3);const auto*d=in_.tree->descriptor(actual);const auto*n=in_.tree->state(actual);auto&core=owner.camera();const auto*data=core.data();
 if(in_.player->registry()!=in_.registry||in_.player->tree()!=in_.tree||!camera||camera!=actual||!d||!n||!n->alive||!n->inside||!n->bound||!n->ready_notified||!owner.onready_complete(3)||
  !data||!data->valid()||core.random()!=in_.random||d->script!=data->script()||d->script_sha!=data->script_sha()||!in_.registry->object_exists(camera)||in_.registry->tree_owner(camera).get()!=in_.tree||
  !core.state(d->id)||!core.state(d->id)->ready||data->player_states()[0]!=in_.data->player_camera())return reject(e,"RoomShaker global.currentCamera has no mapped same actual Player Camera source owner");
 uint64_t token=0;if(!core.shake_camera(d->id,in_.data->magnitude(),in_.data->length(),in_.data->direction(),data->tuning(FieldCameraTuning::ShakeInterval),data->tuning(FieldCameraTuning::ShakeWeight),true,token)){e=core.error();return false;}
 if(!in_.effects->play(id,audio_,e)||!in_.effects->source_joy(in_.global->owner(),in_.data->joy_device(),in_.data->joy_weak(),in_.data->joy_strong(),in_.data->length(),e))return false;
 // Global rand_range consumes the same session stream only after all source
 // side effects returned. The native setter performs real_t conversion once.
 const auto wait=in_.random->rand_range(in_.data->wait()-in_.data->margin(),in_.data->wait()+in_.data->margin());
 return in_.node_timers->set_wait(timer_,float(wait),e);
}
bool HouseReturnCameraControl::start_shake(FieldObjectId id,std::string&e){
 if(!live(id,true,e))return false;Callback call(depth_);float left=0;
 if(!in_.node_timers->time_left(timer_,left,e))return false;
 if(left==0){if(!timeout(id,e)||!in_.node_timers->start(timer_,0,e))return false;}e.clear();return true;
}
bool HouseReturnCameraControl::stop_shake(FieldObjectId id,std::string&e){
 if(!live(id,true,e))return false;Callback call(depth_);return in_.node_timers->stop(timer_,e);
}
bool HouseReturnCameraControl::delayed_start(FieldObjectId id,double seconds,std::string&e){
 if(!live(id,true,e)||!std::isfinite(seconds)||std::abs(seconds)>std::numeric_limits<float>::max())return reject(e,"RoomShaker actual delayed_start argument invalid");
 Callback call(depth_);std::shared_ptr<FieldSceneTreeTimer>timer;
 if(!in_.scene_timers->create_timer(float(seconds),true,timer,e))return false;const auto object=timer->binding().object;
 if(!object||timer->registry()!=in_.registry||!in_.scene_timers->owns(object)||waiting_.count(object)||
  !in_.signals->connect(object,"timeout",id,"start_shake",FieldSignalOneShot,{},e))return reject(e,"RoomShaker actual source yield waiter connection unavailable");
 waiting_.emplace(object,Wait{timer,false});e.clear();return true;
}
bool HouseReturnCameraControl::deferred(const FieldDeferredMessage&m,std::string&e){
 if(m.kind!=FieldDeferredKind::Call||!live(m.object,true,e))return reject(e,"RoomShaker callback is not its actual source receiver");
 bool result=false;Callback call(depth_);
 if(m.member=="_on_Timer_timeout"){
  if(!m.args.empty()||!in_.signals->emitting_to(timer_,"timeout",object_,m.member))return reject(e,"RoomShaker timeout is outside actual Timer source signal frame");result=timeout(m.object,e);
 }else if(m.member=="start_shake"){
  if(!m.args.empty())return reject(e,"RoomShaker start_shake arguments unknown");const auto emitting=in_.scene_timers->emitting();auto i=waiting_.find(emitting);
  if(emitting){if(i==waiting_.end()||i->second.returned||i->second.timer.expired()||!in_.signals->emitting_to(emitting,"timeout",object_,m.member))return reject(e,"RoomShaker delayed source coroutine owner differs");i->second.returned=true;}
  result=start_shake(m.object,e);
 }else if(m.member=="stop_shake"&&m.args.empty())result=stop_shake(m.object,e);
 else if(m.member=="vibrate"&&m.args.empty())result=vibrate(m.object,e);
 else if(m.member=="delayed_start"){
  double seconds=in_.data->delay();if(m.args.size()>1)return reject(e,"RoomShaker delayed_start arity unknown");
  if(!m.args.empty()){if(auto*v=std::get_if<double>(&m.args[0]))seconds=*v;else if(auto*v=std::get_if<int64_t>(&m.args[0]))seconds=double(*v);else return reject(e,"RoomShaker source delay Variant is not numeric");}
  result=delayed_start(m.object,seconds,e);
 }else return reject(e,"RoomShaker unknown source method/arguments");
 if(!result)failed_=true;return result;
}
bool HouseReturnCameraControl::collect_expired(std::string&e){
 if(!borrows(e)||depth_)return reject(e,"RoomShaker waiter collection inside source callback");
 for(auto i=waiting_.begin();i!=waiting_.end();){if(i->second.timer.expired()){if(!i->second.returned)return reject(e,"RoomShaker expired timer lost actual source continuation");i=waiting_.erase(i);}else ++i;}e.clear();return true;
}
bool HouseReturnCameraControl::release_deleted(FieldObjectId id,std::string&e){
 if(!borrows(e)||id!=object_||depth_||entered_||in_.tree->state(id)||in_.registry->object_exists(id)||in_.controls->owns(id))return reject(e,"RoomShaker release requires actual source/native/ObjectDB deletion");
 // Deleting the actual script receiver releases its coroutine states; the
 // global timer list retains its native References until the sole idle tail.
 deleted_object_=object_;deleted_timer_=timer_;deleted_audio_=audio_;
 waiting_.clear();object_=timer_=audio_=0;ready_=bound_=factory_=false;e.clear();return true;
}
}
