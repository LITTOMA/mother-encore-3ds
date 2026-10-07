#include "house_return_camera_control_effects.hpp"
#include "encore/global_yaml_caches.hpp"
#include <3ds.h>
#include <algorithm>
#include <cmath>
namespace encore::ctr {
using namespace upstream;
namespace {
bool reject(std::string&e,const char*s){e=s;return false;}
bool same(const FieldIdentity&a,const FieldIdentity&b){return a.scene_id==b.scene_id&&a.source_sha256==b.source_sha256&&a.upstream_commit==b.upstream_commit;}
std::string hex(const std::array<uint8_t,20>&pin){const char*digits="0123456789abcdef";std::string out;for(auto v:pin){out+=digits[v>>4];out+=digits[v&15];}return out;}
HouseReturnJoyInput*joy_owner=nullptr;
struct Call {size_t&v;explicit Call(size_t&d):v(d){++v;}~Call(){--v;}};
}
HouseReturnJoyInput::~HouseReturnJoyInput(){std::string ignored;shutdown(ignored);}
bool HouseReturnJoyInput::prepare(const HouseReturnCameraControlData&d,NativeInputAdapter&input,FieldGlobalRegistry&r,std::string&e){
 if(data_||joy_owner||!d.valid()||r.poisoned()||!r.data()||d.identity().upstream_commit!=r.data()->identity().upstream_commit||hex(d.input_engine_commit())!=r.data()->engine_commit())
  return reject(e,"Actual InputDefault request owner/source/ObjectDB unavailable or duplicated");
 data_=&d;ir_=d.ir_sha256();input_=&input;registry_=&r;joy_owner=this;e.clear();return true;
}
bool HouseReturnJoyInput::start(uint32_t device,float weak,float strong,float duration,std::string&e){
 if(joy_owner!=this||!data_||!data_->valid()||data_->ir_sha256()!=ir_||!input_||!registry_||registry_->poisoned()||
    !std::isfinite(weak)||!std::isfinite(strong)||!std::isfinite(duration))return reject(e,"Actual source Input vibration receiver/parameters unavailable");
 // Exact InputDefault early-return range guard; invalid magnitude is not stored.
 if(weak<0||weak>1||strong<0||strong>1){e.clear();return true;}
 const auto now=uint64_t(double(svcGetSystemTick())*1000000.0/SYSCLOCK_ARM11);
 requests_[device]={device,weak,strong,duration,now};
 // Original InputDefault does not call a device here or reject disconnected
 // devices: it writes joy_vibration. Driver polling/physical rumble is separate.
 // CTR has no such driver, so this maps request state only, not hardware output.
 e.clear();return true;
}
bool HouseReturnJoyInput::observe(uint32_t device,HouseJoyVibrationRequest&out,std::string&e)const{
 if(joy_owner!=this||!data_||!data_->valid()||data_->ir_sha256()!=ir_||!registry_||registry_->poisoned())return reject(e,"Actual source Input request owner expired");
 auto i=requests_.find(device);out=i==requests_.end()?HouseJoyVibrationRequest{device,0,0,0,0}:i->second;e.clear();return true;
}
bool HouseReturnJoyInput::shutdown(std::string&e){if(joy_owner==this)joy_owner=nullptr;data_=nullptr;input_=nullptr;registry_=nullptr;requests_.clear();e.clear();return true;}
struct HouseReturnCameraControlEffects::Sample {
 FieldGlobalExternalBinding binding{};const FieldSceneAudioData*data=nullptr;
 std::array<uint8_t,32>ir{};AudioPlayer*audio=nullptr;FieldGlobalRegistry*registry=nullptr;
 FieldSceneAudioStream stream{};
 bool live(std::string&e)const{
  AudioAsset asset;
  if(!data||!data->valid()||data->ir_sha()!=ir||!audio||!registry||registry->poisoned()||!audio->prepared(stream.asset_id)||
     !audio->checked_asset(stream.asset_id,asset,e)||asset.source_path!="res://"+stream.source||asset.source_sha256!=stream.source_sha)
   return reject(e,"Actual House AudioStreamSample backing/checked PCM expired");return true;
 }
};
namespace {
class SampleOwner final:public FieldGlobalSourceResource {
 std::shared_ptr<HouseReturnCameraControlEffects::Sample>s_;
public:
 explicit SampleOwner(std::shared_ptr<HouseReturnCameraControlEffects::Sample>s):s_(std::move(s)){}
 FieldGlobalExternalBinding binding()const override{return s_->binding;}
 const char*resource_class()const override{return s_->stream.native_class.c_str();}
 bool state(FieldGlobalExternalState&out,std::string&e)const override{
  if(!s_->live(e))return false;out={};out.name=s_->binding.source.name;e.clear();return true;
 }
 bool deferred(const FieldDeferredMessage&,std::string&e)override{return reject(e,"Actual AudioStreamSample Resource is not a script Node");}
 bool persist_append(FieldObjectId,std::string&e)override{return reject(e,"Actual AudioStreamSample has no persistent Node array");}
 bool assign_stable_canvas(FieldObjectId,std::string&e)override{return reject(e,"Actual AudioStreamSample cannot own a UI Canvas");}
};
}
bool HouseReturnCameraControlEffects::prepare(HouseReturnCameraEffectsInput in,std::string&e){
 if(prepared_||!in.sources||!in.data||!in.audio_data||!in.tree||!in.registry||!in.controls||!in.source||!in.audio||!in.audio_player||
    !in.ui||!in.global||!in.globaldata||!in.random||!in.input||!in.dialogue||!in.world||!in.sources->valid()||!in.data->valid()||!in.audio_data->valid()||
    !same(in.sources->tree().identity(),in.data->identity())||!same(in.data->identity(),in.audio_data->identity())||
    in.audio->registry()!=in.registry||in.input->registry()!=in.registry||in.global->registry()!=in.registry||in.globaldata->registry()!=in.registry||
    in.controls->canvas_tree()!=in.tree||in.controls->canvas_registry()!=in.registry||in.controls->canvas_data()!=&in.sources->canvas()||
    in.world->content().bytes()!=in.sources->inspections().view().bytes()||in.dialogue->world()!=in.world||in.registry->poisoned())
  return reject(e,"House shaker effects require exact actual source/native/session owners");
 FieldGlobalDataMemberState rumble;
 if(!in.globaldata->read_global_member("rumble",rumble,e)||!rumble.value||rumble.value->kind!=1||!in.ui->bind_game_over_source(*in.data,e))return false;
 FieldGlobalExternalState actual_globaldata;const auto*object=in.registry->external_object(in.globaldata->globaldata_object());
 if(!in.globaldata->data()||in.globaldata->data()->identity().upstream_commit!=in.data->identity().upstream_commit||!object||
    !in.globaldata->source_state(actual_globaldata,e)||object->binding().object!=in.globaldata->globaldata_object()||
    object->binding().source.native_class!="Node"||object->binding().source.identity.upstream_commit!=in.data->identity().upstream_commit)
  return reject(e,"House shaker globaldata is not its actual retained singleton source body");
 in_=in;ir_=in.data->ir_sha256();audio_ir_=in.audio_data->ir_sha();prepared_=true;e.clear();return true;
}
bool HouseReturnCameraControlEffects::live(std::string&e)const{
 if(!prepared_||!in_.data->valid()||in_.data->ir_sha256()!=ir_||!in_.audio_data->valid()||in_.audio_data->ir_sha()!=audio_ir_||
    !in_.sources->valid()||in_.registry->poisoned()||in_.audio->registry()!=in_.registry||in_.global->registry()!=in_.registry||
    in_.globaldata->registry()!=in_.registry||in_.input->registry()!=in_.registry||in_.dialogue->world()!=in_.world||
    in_.world->content().bytes()!=in_.sources->inspections().view().bytes()||in_.controls->canvas_tree()!=in_.tree)
  return reject(e,"House shaker concrete effects borrow expired");return true;
}
bool HouseReturnCameraControlEffects::receivers(FieldObjectId control,FieldObjectId audio,bool source_call,std::string&e)const{
 if(!live(e)||!control||!audio||in_.tree->source_object(in_.data->nodes()[0].id)!=control||
    in_.tree->source_object(in_.data->nodes()[2].id)!=audio||in_.registry->tree_owner(control).get()!=in_.tree||
    in_.registry->tree_owner(audio).get()!=in_.tree||!in_.tree->state(control)||!in_.tree->state(audio)||in_.tree->state(audio)->parent!=control||
    in_.audio->source_tree(audio)!=in_.tree||in_.audio->source_data(audio)!=in_.audio_data||
    (source_call&&!in_.source->source_call_live(control,e)))return reject(e,"House shaker effects receiver is outside its actual source call/native child owner");return true;
}
bool HouseReturnCameraControlEffects::observe(FieldObjectId control,FieldObjectId audio,HouseRoomShakerEffectsState&out,std::string&e)const{
 HouseControlNativeState c;PodunkSceneAudioState a;
 if(!receivers(control,audio,false,e)||!in_.controls->snapshot(control,c,e)||!in_.audio->state(audio,a,e))return false;
 out={};out.tree=in_.tree;out.registry=in_.registry;out.ui=in_.ui;out.global=in_.global;out.random=in_.random;out.audio_owner=in_.audio;
 out.control=control;out.audio=audio;out.control_constructed=c.constructed;out.audio_constructed=in_.audio->owns(audio);out.audio_ready=a.ready;out.bus=a.bus;
 if(sample_){if(!sample_->live(e)||in_.registry->source_resource(sample_->binding.object)==nullptr||a.source_stream!=sample_->stream.id)return reject(e,"House shaker loaded Resource differs from actual audio assignment");
  out.stream=sample_->binding.object;out.source_sound=sample_->stream.source;out.sound_sha=sample_->stream.source_sha;}
 e.clear();return true;
}
bool HouseReturnCameraControlEffects::load_sound(FieldObjectId control,FieldObjectId audio,std::string_view path,const std::array<uint8_t,32>&sha,std::string&e){
 if(depth_||!receivers(control,audio,true,e)||path!=in_.data->sound())return reject(e,"House shaker load requires its actual Ready source receiver");Call call(depth_);
 const FieldSceneAudioStream*stream=nullptr;for(const auto&s:in_.audio_data->streams())if(s.source==path&&s.source_sha==sha&&s.native_class=="AudioStreamSample"){if(stream)return reject(e,"House shaker source load has ambiguous metadata");stream=&s;}
 if(!stream||!in_.audio_player->prepare(stream->asset_id,e))return reject(e,"House shaker actual source Sample/checked PCM unavailable");
 if(sample_){if(sample_->stream.id!=stream->id||!sample_->live(e))return reject(e,"House shaker ResourceLoader cache source changed");return in_.audio->set_stream(audio,stream->id,e);}
 auto s=std::make_shared<Sample>();s->data=in_.audio_data;s->ir=audio_ir_;s->audio=in_.audio_player;s->registry=in_.registry;s->stream=*stream;
 FieldGlobalExternalSpec spec;spec.identity=in_.data->identity();spec.identity.source_sha256=sha;spec.source_sha=sha;
 spec.source=std::string(path);spec.name=spec.source;spec.stable_id=stream->id;spec.role=4;spec.native_class=stream->native_class;
 FieldObjectId id=0;if(!in_.registry->allocate_object(id,e))return false;s->binding={id,spec,0x454e0067,1};
 if(!in_.registry->publish_source_resource(spec,id,std::make_unique<SampleOwner>(s),e)){std::string ignored;in_.registry->retire_object(id,ignored);return false;}
 sample_=s;return in_.audio->set_stream(audio,stream->id,e);
}
bool HouseReturnCameraControlEffects::play(FieldObjectId control,FieldObjectId audio,std::string&e){
 HouseRoomShakerEffectsState s;if(depth_||!receivers(control,audio,true,e)||!observe(control,audio,s,e)||!s.stream||!s.audio_ready)return reject(e,"House shaker play lacks its actual assigned Sample/native Ready");Call call(depth_);return in_.audio->play(audio,0,e);
}
bool HouseReturnCameraControlEffects::source_game_over(FieldObjectId ui,bool&out,std::string&e)const{
 const auto control=in_.tree?in_.tree->source_object(in_.data->nodes()[0].id):0;
 if(!live(e)||!in_.source->source_call_live(control,e)||ui!=in_.ui->binding().object)return reject(e,"House shaker is_game_over receiver/source scope differs");return in_.ui->source_is_game_over(ui,out,e);
}
bool HouseReturnCameraControlEffects::source_joy(FieldObjectId global,uint32_t device,double weak,double strong,double duration,std::string&e){
 const auto control=in_.tree?in_.tree->source_object(in_.data->nodes()[0].id):0;FieldGlobalDataMemberState gate;
 if(depth_||!live(e)||!in_.source->source_call_live(control,e)||global!=in_.global->owner()||
    !in_.globaldata->read_global_member("rumble",gate,e)||!gate.value||gate.value->kind!=1||
    device!=in_.data->joy_device()||weak!=in_.data->joy_weak()||strong!=in_.data->joy_strong()||duration!=in_.data->length())
  return reject(e,"House shaker global.start_joy_vibration actual receiver/gate/arguments differ");
 if(!gate.value->boolean){e.clear();return true;}Call call(depth_);return in_.input->start(device,float(weak),float(strong),float(duration),e);
}
bool HouseReturnCameraControlEffects::source_frame_closed(const OpeningWorld&w,std::string&e)const{
 if(!live(e)||&w!=in_.world||depth_||!in_.source->source_closed(e)||!in_.ui->source_business_closed(e))return reject(e,"House shaker exact unbind/admission has live source callbacks/waiters/Timer");
 HouseReturnDialogueReceipt r;if(!in_.dialogue->inventory_frame(r,e)||!r.source_closed||r.callback_depth||r.pending_callbacks||r.ready_waiting||r.world!=&w||r.registry!=in_.registry||r.lifecycle_tree!=in_.tree||r.random!=in_.random)return reject(e,"House shaker binding requires actual closed same native programme owners");e.clear();return true;
}
bool HouseReturnCameraControlEffects::bind_world(std::string&e){if(bound_||!live(e)||!in_.world->bind_room_shaker_owner(*this,e))return false;bound_=true;e.clear();return true;}
bool HouseReturnCameraControlEffects::unbind_world(std::string&e){if(!bound_||!live(e)||!in_.world->unbind_room_shaker_owner(*this,e))return false;bound_=false;e.clear();return true;}
bool HouseReturnCameraControlEffects::periodic(uint32_t index,RoomBinding&out,std::string&e)const{
 const auto&room=in_.world->content();if(index>=room.binding_count())return reject(e,"House shaker actual Room binding index missing");out=room.binding(index);
 if(out.kind!=uint16_t(RoomBindingKind::PeriodicCameraShake)||out.target_index>=room.resource_count())return reject(e,"House shaker binding is not an actual source delayed_start");
 const auto asset=room.resource(out.target_index);std::array<uint8_t,32>sha{};
 if(!in_.data->source_hash(in_.data->sound(),sha)||room.string(asset.path_string)!="res://"+in_.data->sound()||asset.sha256!=sha||
    out.value!=in_.data->magnitude()||out.duration!=in_.data->delay()||
    room.rule_f64(RoomRuleKey::RoomShakeWaitSeconds)!=in_.data->wait()||room.rule_f64(RoomRuleKey::RoomShakeWaitMarginSeconds)!=in_.data->margin()||
    room.rule_f64(RoomRuleKey::RoomShakeLengthSeconds)!=in_.data->length()||
    float(room.rule_f64(RoomRuleKey::RoomShakeDirectionX))!=in_.data->direction().x||float(room.rule_f64(RoomRuleKey::RoomShakeDirectionY))!=in_.data->direction().y)
  return reject(e,"House shaker Room resource/source defaults differ from actual source body");return true;
}
bool HouseReturnCameraControlEffects::invoke_room_binding(OpeningWorld&w,uint32_t index,std::string&e){
 if(depth_||!bound_||!live(e)||&w!=in_.world||w.room_shaker_owner()!=this||w.house_programme_owner()!=in_.dialogue||index>=w.content().binding_count())return reject(e,"House shaker deferred call has foreign World/source owner");
 HouseReturnDialogueReceipt r;if(!in_.dialogue->inventory_frame(r,e)||r.world!=&w||r.registry!=in_.registry||r.lifecycle_tree!=in_.tree||r.random!=in_.random||r.ready_waiting||r.programme!=w.story_program_index()||r.generation!=w.story_generation())return reject(e,"House shaker deferred source VM/Room generation differs");
 const auto binding=w.content().binding(index);RoomBinding periodic_binding;const auto control=in_.tree->source_object(in_.data->nodes()[0].id);
 Call call(depth_);
 if(binding.kind==uint16_t(RoomBindingKind::PeriodicCameraShake)){if(!periodic(index,periodic_binding,e))return false;return in_.source->delayed_start(control,binding.duration,e);}
 if(binding.kind==uint16_t(RoomBindingKind::StopRoomShaker)){if(!periodic(binding.target_index,periodic_binding,e))return false;return in_.source->stop_shake(control,e);}
 return reject(e,"House shaker source owner refuses unrelated World binding");
}
bool HouseReturnCameraControlEffects::release_stream_after_delete(std::string&e){
 if(bound_||depth_||!live(e)||in_.source->callback_depth()||in_.source->pending_waiters())return reject(e,"House shaker Resource still borrowed by live source callbacks/waiters/World");
 const auto control=in_.tree->source_object(in_.data->nodes()[0].id),audio=in_.tree->source_object(in_.data->nodes()[2].id);
 if((control&&in_.registry->object_exists(control))||(audio&&in_.registry->object_exists(audio))||(audio&&in_.audio->owns(audio)))return reject(e,"House shaker stream release requires actual source/native Audio deletion first");
 if(sample_&&!in_.registry->retire_object(sample_->binding.object,e))return false;sample_.reset();e.clear();return true;
}
}
