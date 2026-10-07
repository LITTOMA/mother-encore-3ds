#include "house_return_dialogue_native.hpp"
#include "house_return_npc_runtime.hpp"
#include <algorithm>
#include <set>

namespace encore::ctr {
using namespace upstream;
namespace {
bool reject(std::string &e,const char *s){e=s;return false;}
bool same(const FieldIdentity&a,const FieldIdentity&b){return a.scene_id==b.scene_id&&
 a.upstream_commit==b.upstream_commit&&a.source_sha256==b.source_sha256;}
struct SourceCall {size_t &depth;explicit SourceCall(size_t&d):depth(d){++depth;}
 ~SourceCall(){--depth;}SourceCall(const SourceCall&)=delete;};
std::string_view source_path(std::string_view s){return s.substr(0,6)=="res://"?s.substr(6):s;}
}
bool HouseReturnDialogueNativeOwner::state(PodunkMickHouseNativeState &out,
    std::string &e)const{
 if(!prepared_||!in_.session||!in_.house||!in_.driver||!in_.tree||
    !in_.source||!in_.source_tree||!in_.doors||!in_.text.valid())
  return reject(e,"House native owner is not explicitly prepared with actual borrowed inputs");
 if(!in_.session->house_native_state(*in_.house,out,e))return false;
 if(out.registry!=owners_.registry||out.ui!=owners_.ui||out.dialogue!=owners_.dialogue||
    out.root!=owners_.root||out.lifecycle!=owners_.lifecycle||out.native!=owners_.native||
    out.old_programme!=owners_.old_programme||out.recipe!=owners_.recipe||
    out.life!=owners_.life||out.choices!=owners_.choices||out.choice_data!=owners_.choice_data||
    out.random!=owners_.random||out.uid_ledger!=owners_.uid_ledger||
    out.global_data!=owners_.global_data||out.audio_data!=owners_.audio_data||
    out.input_sound_node!=owners_.input_sound_node||!out.retired||
    out.registry->current_scene()!=in_.tree->root()||
    out.dialogue->lifecycle_tree()!=in_.tree.get()||
    !in_.source->matches(*in_.doors,in_.house->world.content(),in_.text,e))
  return reject(e,"House native owner lost an actual retained source/receiver borrow");
 FieldIdentity identity{};
 const auto *root=in_.tree->state(in_.tree->root());
 if(!root||!root->alive||!root->bound||!root->inside||root->queued||
    !root->ready_notified||root->ready_first||in_.house->scene_ready_pending()||
    out.registry->tree_owner(in_.tree->root())!=in_.tree||
    !in_.tree->object_identity(in_.tree->root(),identity)||
    !same(identity,in_.source_tree->identity()))
  return reject(e,"House native owner requires actual mapped full House Tree Ready");
 if(room_installed_){const auto *life=out.dialogue->lifecycle();
  if(!life||!life->room_bound()||life->room_source().bytes()!=in_.house->world.content().bytes()||
     life->room_source().byte_size()!=in_.house->world.content().byte_size()||
     life->house_source().bytes()!=in_.text.bytes()||life->house_source().byte_size()!=in_.text.byte_size())
   return reject(e,"House native owner lost its actual installed lifecycle source context");}
 e.clear();return true;
}
bool HouseReturnDialogueNativeOwner::actor(FieldObjectId id,uint32_t &house_index,
    uint32_t &source,std::string &e)const{
 if(!id)return reject(e,"House native actor lookup cannot invent a null NPC");
 const HouseReentryActor *found=nullptr;
 for(const auto &a:in_.source->actors()){
  FieldObjectId actual=0;
  if(!in_.tree->get_node(in_.tree->root(),a.node,actual,e))return false;
  if(actual==id){if(found)return reject(e,"House native actor binding is ambiguous");found=&a;}
 }
 const auto *n=in_.tree->state(id);const auto *d=in_.tree->descriptor(id);
 FieldIdentity identity{};
 if(!found||!n||!d||!n->alive||!n->bound||!n->inside||!n->ready_notified||
    n->queued||d->path!=found->node||in_.source_tree->record(d->id)==nullptr||
    owners_.registry->tree_owner(id)!=in_.tree||
    !in_.tree->object_identity(id,identity)||!same(identity,in_.source_tree->identity())||
    found->house_index>=in_.text.count(HouseSection::Npcs)||
    found->room_index>=in_.house->world.content().actor_instance_count()||
    in_.house->world.content().actor_instance(found->room_index).stable_id!=found->id)
  return reject(e,"House native actor is not the exact existing House/Room/full Tree binding");
 house_index=found->house_index;source=d->id;e.clear();return true;
}
bool HouseReturnDialogueNativeOwner::context(const HouseReturnDialogueContext &c,
    FieldProgrammeContext &out,std::string &e)const{
 PodunkMickHouseNativeState s;
 if(!state(s,e))return false;
 const auto room=in_.house->world.content();
 if(c.house!=in_.house||c.house_root!=in_.tree->root()||
    c.room.bytes()!=room.bytes()||c.room.byte_size()!=room.byte_size()||
    c.text.bytes()!=in_.text.bytes()||c.text.byte_size()!=in_.text.byte_size()||
    c.programme>=room.program_count()||!c.generation)
  return reject(e,"House native action has a foreign immutable programme context");
 FieldProgrammeContext n;
 n.dialogue_object=c.dialogue;n.actor_object=c.talker;n.thoughts=false;
 if(c.talker){uint32_t index=0,source=0;
  if(!actor(c.talker,index,source,e))return false;
  if(index!=c.original_npc)return reject(e,"House native talker differs from original actual NPC index");
  n.source_npc=source;
 }else if(c.original_npc!=kRoomNoIndex)
  return reject(e,"House native nullable talker disagrees with actual House programme caller");
 out=n;e.clear();return true;
}
bool HouseReturnDialogueNativeOwner::room_actor(RoomView room,uint32_t programme,
    const FieldProgrammeContext &c,std::string &e)const{
 PodunkMickHouseNativeState s;
 if(!state(s,e)||room.bytes()!=in_.house->world.content().bytes()||
    room.byte_size()!=in_.house->world.content().byte_size()||programme>=room.program_count()||
    c.thoughts||bool(c.source_npc)!=bool(c.actor_object))
  return reject(e,"House lifecycle borrowed a different Room or non-source NPC context");
 if(c.actor_object){uint32_t index=0,source=0;
  if(!actor(c.actor_object,index,source,e)||source!=c.source_npc)return false;
 }
 e.clear();return true;
}
bool HouseReturnDialogueNativeOwner::prepare(HouseReturnDialogueNativeInput in,
    std::string &e){
 if(prepared_||in_.house||!in.session||!in.driver||!in.house||!in.tree||
    !in.source||!in.source_tree||!in.doors||!in.text.valid()||
    !in.source->valid()||!in.source_tree->valid())
  return reject(e,"House native fixed owner inputs are incomplete or already installed");
 PodunkMickHouseNativeState actual;
 if(!in.session->house_native_state(*in.house,actual,e))return false;
 if(!actual.registry||!actual.ui||!actual.dialogue||!actual.root||!actual.lifecycle||!actual.native||
    !actual.old_programme||!actual.recipe||!actual.life||!actual.choice_data||
    !actual.choices||!actual.random||!actual.uid_ledger||!actual.global_data||
    !actual.audio_data||actual.input_sound_node.empty()||actual.callback_depth||
    actual.callback_receiver||actual.input_live||actual.notifying||actual.printer_owner||
    actual.pending_messages||actual.business_pending||!actual.objects.empty()||!actual.wait_connections.empty()||
    !actual.root->observes_closed_printer(in.house->presentation,e)||
    !actual.dialogue->observes_closed_printer(in.house->presentation,e)||
    !actual.native->observes_closed_printer(in.house->presentation,e)||
    !actual.ui->source_business_closed(e)||
    !actual.lifecycle->source_frame_closed(e))
  return reject(e,"House native receiver installation requires all actual source owners closed");
 in_=std::move(in);owners_=std::move(actual);prepared_=true;
 PodunkMickHouseNativeState checked;
 if(!state(checked,e)){prepared_=false;in_={};owners_={};return false;}
 // These are the same concrete coroutine/function-state/signal owners. Only
 // the programme and House region/talker receivers change at this boundary.
 FieldDialogueRoomHost room;
 room.lifecycle=owners_.lifecycle->host();
 room.lifecycle.start_programme=[this](FieldObjectId id,uint32_t gen,std::string &error){
  auto *life=owners_.dialogue->lifecycle();
  if(!programme_bound_||!life||life->object()!=id||life->generation()!=gen)
   return reject(error,"House real Ready callback has no exact Room programme receiver");
  return in_.driver->native_ready(id,error);
 };
 const auto base_admit=room.lifecycle.admit_step;
 room.lifecycle.admit_step=[this,base_admit](const auto &step,const auto &ctx,std::string &error){
  if(step.op==FieldDialogueOp::KeyUpdate)
   return owners_.ui->source_update_key_indicator(*owners_.global_data,in_.source->target_region(),error);
  return base_admit(step,ctx,error);
 };
 const auto base_manager=room.lifecycle.manager;
 room.lifecycle.manager=[this,base_manager](const auto &step,FieldObjectId id,std::string &error){
  if(step.op==FieldDialogueOp::KeyUpdate)
   return owners_.ui->source_update_key_indicator(*owners_.global_data,in_.source->target_region(),error);
  return base_manager(step,id,error);
 };
 room.admit_actor=[this](RoomView r,uint32_t p,const auto &c,std::string &error){return room_actor(r,p,c,error);};
 room.bind_programme=[this](RoomView r,uint32_t p,FieldObjectId id,const auto &c,std::string &error){
  const auto &current=in_.driver->context();FieldProgrammeContext expected;
  if(!programme_bound_||!context(current,expected,error)||r.bytes()!=current.room.bytes()||
     r.byte_size()!=current.room.byte_size()||p!=current.programme||id!=current.dialogue||
     c.dialogue_object!=expected.dialogue_object||c.actor_object!=expected.actor_object||
     c.source_npc!=expected.source_npc||c.thoughts!=expected.thoughts)
   return reject(error,"House actual SetProgramme callback differs from current native/Room caller");
  return true;
 };
 room.stop_talker=[this](FieldObjectId id,uint32_t source,std::string &error){
  uint32_t index=0,actual_source=0;
  if(!actor(id,index,actual_source,error)||actual_source!=source)
   return reject(error,"House StopTalker callback received another source body");
  if(npc_source_){
   if(!npc_source_->owns(id)||!npc_source_->stop_interaction(id,error))return false;
   // The real NPC owns its SceneTreeTimer return waiter. Projection here must
   // not schedule HousePresentation's separate legacy return countdown.
   return (in_.house->presentation.set_npc_talking(index,false)&&
           in_.house->world.set_story_talking(false))||
       reject(error,"House source StopTalker projection rejected its actual NPC");
  }
  return in_.house->presentation.stop_npc_interaction(index)||
      reject(error,"House original StopTalker consumer rejected its actual NPC");
 };
 if(!in_.session->bind_house_talker(*in_.house,*this,e)||
    !owners_.dialogue->bind_room_source(*in_.source,*in_.doors,
      in_.house->world.content(),in_.text,*in_.source_tree,std::move(room),e))return false;
 room_installed_=true;
 e.clear();return true;
}
bool HouseReturnDialogueNativeOwner::driver_input(HouseReturnDialogueInput &out,
    std::string &e)const{
 PodunkMickHouseNativeState s;
 if(!room_installed_)return reject(e,"House native lifecycle installation has not completed");
 if(!state(s,e))return false;
 HouseReturnDialogueInput n;
 n.registry=s.registry;n.tree=in_.tree;n.house=in_.house;n.text=in_.text;
 n.source=in_.source;n.source_tree=in_.source_tree;n.doors=in_.doors;
 n.lifecycle=s.life;n.recipe=s.recipe;n.session=in_.session;n.ui=s.ui;n.script=s.root;
 n.random=s.random;n.uid_ledger=s.uid_ledger;n.choice_data=s.choice_data;n.choices=s.choices;
 n.native=const_cast<HouseReturnDialogueNativeOwner*>(this);out=std::move(n);e.clear();return true;
}
bool HouseReturnDialogueNativeOwner::bind_programme(std::string &e){
 PodunkMickHouseNativeState s;
 if(programme_bound_||!room_installed_||!state(s,e)||in_.house->world.house_programme_owner()!=in_.driver)
  return reject(e,"House Root programme binding requires its actual typed World owner first");
 PodunkDialogueProgrammeBinding old;
 if(!s.old_programme->programme_binding(old,e)||
    !in_.session->rebind_house_programme(*in_.house,old,*in_.driver,e))return false;
 programme_bound_=true;e.clear();return true;
}
bool HouseReturnDialogueNativeOwner::set_talking(FieldObjectId id,bool talking,
    std::string &e){
 PodunkMickHouseNativeState s;if(!state(s,e))return false;
 const auto &c=in_.driver->context();uint32_t index=0,source=0;
 HouseUiDialogueState ui;
 if(!programme_bound_||!s.callback_depth||c.dialogue!=s.printer_owner||
    c.talker!=id||!s.ui->source_dialogue_state(ui,e)||ui.talker!=id||
    !actor(id,index,source,e)||c.original_npc!=index)
  return reject(e,"House talking callback is outside its actual Root/printer/talker source frame");
 if(npc_source_&&(!npc_source_->owns(id)||!npc_source_->set_talking(id,talking,e)))return false;
 return (in_.house->presentation.set_npc_talking(index,talking)&&
         in_.house->world.set_story_talking(talking))||
      reject(e,"House actual talking source consumer rejected");
}
bool HouseReturnDialogueNativeOwner::bind_npc_source(HouseReturnNpcRuntime&source,std::string&e){
 PodunkMickHouseNativeState s;
 if(npc_source_||!programme_bound_||callback_depth_||selected_||!state(s,e)||
    in_.house->house.programme_owner()!=in_.driver||
    s.callback_depth||s.callback_receiver||s.input_live||s.notifying||s.printer_owner||
    s.pending_messages||s.business_pending||!s.objects.empty()||!s.wait_connections.empty()||
    !source.source_frame_closed()||source.house()!=in_.house||source.tree()!=in_.tree.get()||
    source.registry()!=owners_.registry||source.native_dialogue()!=this||!source.sources()||
    &source.sources()->reentry()!=in_.source||&source.sources()->tree()!=in_.source_tree||
    source.runtime().data()!=&source.sources()->npcs()||!source.runtime().error().empty()||
    !in_.driver->source_frame_closed(in_.house->world,e))
  return reject(e,"House NPC borrower bind requires the same real Ready owner and closed source boundary");
 for(const auto&v:source.runtime().npcs()){
  const auto object=in_.tree->source_object(v.id);
  const auto*n=in_.tree->state(object);
  if(!v.ready||v.destroyed||v.queued_free||!object||!source.owns(object)||
     !n||!n->inside||!n->bound||!n->ready_notified||n->ready_first)
   return reject(e,"House NPC borrower lacks an actual full-tree Ready script receiver");
 }
 npc_source_=&source;e.clear();return true;
}
bool HouseReturnDialogueNativeOwner::admit_npc_programme(const HouseReturnNpcRuntime&source,
    const HouseSourceNpcProgramme&r,std::string&e)const{
 PodunkMickHouseNativeState s;
 if(npc_source_!=&source||!programme_bound_||selected_||callback_depth_||!state(s,e)||
    r.source!=&source.runtime()||r.tree!=in_.tree.get()||r.tree_data!=in_.source_tree||
    r.registry!=owners_.registry||r.reentry!=in_.source||r.doors!=in_.doors||r.thoughts||
    !source.owns(r.object)||!in_.driver->source_frame_closed(in_.house->world,e)||
    !in_.house->house.admit_source_npc_programme(r,e))
  return reject(e,"House NPC native admission borrowed another live programme/source receiver");
 HouseSourceNpcProgramme actual;
 if(!source.programme_input(r.object,r.programme,false,actual,e)||
    actual.source_id!=r.source_id||actual.original_npc!=r.original_npc)
  return reject(e,"House NPC native admission is outside its actual source programme callback");
 auto next=in_.driver->context();
 next.dialogue=0;next.talker=r.object;next.programme=r.programme;next.original_npc=r.original_npc;
 next.generation=in_.house->world.source_generation()+1;if(!next.generation)++next.generation;
 const auto p=next.room.program(next.programme);
 if(!p.stable_id||!p.command_count||p.first_command>next.room.command_count()||
    p.command_count>next.room.command_count()-p.first_command)
  return reject(e,"House NPC native programme command span is invalid");
 const auto rng=owners_.random->state(),draws=owners_.random->raw_draw_count();
 const auto ledger=*owners_.uid_ledger;
 for(uint32_t pc=0;pc<p.command_count;++pc){
  const auto command=next.room.command(p.first_command+pc);
  if(command.opcode==uint16_t(DialogueActionKind::AwaitChoices)){
   if(!owners_.choice_data->validate_program(command.target_index,
        next.room.string(p.source_path_string),p.command_count,e))return false;
   const auto&group=owners_.choice_data->groups()[command.target_index];
   auto phrase=[&](uint32_t target){return target>pc&&target<p.command_count&&
       next.room.command(p.first_command+target-1).phrase!=next.room.command(p.first_command+target).phrase;};
   if(!phrase(group.cancel_target_pc))return reject(e,"House NPC choice cancel is not an actual source forward phrase");
   for(const auto&option:group.options)if(!phrase(option.target_pc))
    return reject(e,"House NPC choice target is not an actual source forward phrase");
  }
  if(!admit_command(next,pc,command,e))return false;
 }
 if(rng!=owners_.random->state()||draws!=owners_.random->raw_draw_count()||ledger!=*owners_.uid_ledger)
  return reject(e,"House NPC native source admission changed the actual entropy/UID owners");
 e.clear();return true;
}
bool HouseReturnDialogueNativeOwner::observe(const HouseReturnDialogueContext &c,
    HouseReturnDialogueReceipt &out,std::string &e)const{
 PodunkMickHouseNativeState s;if(!state(s,e))return false;
 const auto *life=s.dialogue->lifecycle();
 std::vector<PodunkDialogueFactoryState> factories;
 std::vector<PodunkDialogueRootSourceState> roots;
 PodunkDialogueCoroutineState coroutines;
 std::vector<FieldObjectId> awaiting;
 HouseUiDialogueState ui;
 if(!life||!s.dialogue->source_factories(factories,e)||!s.root->observe_instances(roots,e)||
    !s.lifecycle->source_coroutines(coroutines,e)||!life->waiting_owners(awaiting,e)||
    !s.ui->source_dialogue_state(ui,e))return false;
 std::set<FieldObjectId> objects(s.objects.begin(),s.objects.end()),native,root_ids;
 for(const auto &f:factories){
  root_ids.insert(f.root);
  if(f.tree!=in_.tree.get()||f.script.object!=f.root||f.script.printer!=&in_.house->presentation)
   return reject(e,"House factory receipt borrowed another actual Tree/script/printer");
  for(const auto &n:f.objects)if(!native.insert(n.second).second)
   return reject(e,"House factory receipt contains duplicate actual native ownership");
 }
 std::set<FieldObjectId> scripts;
 for(const auto &r:roots){if(!scripts.insert(r.script.object).second||
    r.script.printer!=&in_.house->presentation||!root_ids.count(r.script.object))
   return reject(e,"House Root script map differs from the actual retained factory map");}
 if(native!=objects||scripts!=root_ids)
  return reject(e,"House dialogue maps do not enumerate the same actual live source objects");
 HouseReturnDialogueReceipt n;
 n.registry=s.registry;n.lifecycle_tree=s.dialogue->lifecycle_tree();n.house=in_.house;
 n.world=&in_.house->world;n.runtime=&in_.house->house;n.printer=&in_.house->presentation;
 n.session=in_.session;n.script=s.root;n.random=s.random;n.uid_ledger=s.uid_ledger;n.choices=s.choices;
 n.dialogue_objects=s.objects;n.wait_receivers=coroutines.receivers;
 std::set<FieldObjectId> history(s.history.begin(),s.history.end());
 history.insert(coroutines.history.begin(),coroutines.history.end());
 for(const auto &w:s.wait_connections){
  if(!objects.count(w.receiver)||!objects.count(w.emitter))
   return reject(e,"House actual WaitTimer signal ledger contains unknown dialogue owners");
  n.wait_receivers.push_back(w.receiver);history.insert(w.receiver);history.insert(w.emitter);
 }
 n.wait_receivers.insert(n.wait_receivers.end(),awaiting.begin(),awaiting.end());
 n.message_targets.assign(history.begin(),history.end());
 // A real retained Fade restoration coroutine can outlive Root removal. Its
 // owner remains scheduled by the House tail until that actual wait closes.
 n.pending_callbacks=s.registry->pending_messages_to(history)+(s.business_pending?1:0);
 n.callback_depth=s.callback_depth+coroutines.callback_depth+callback_depth_;
 n.dialogue=life->object();n.canvas=ui.stable_canvas;n.talker=ui.talker;
 n.programme=life->programme_index();n.generation=life->generation();
 n.world_generation=in_.house->world.source_generation();n.lifecycle_phase=life->phase();
 n.ready_waiting=n.lifecycle_phase==FieldDialogueLifecyclePhase::WaitingReady;
 n.pending_choice_events=s.choices->pending_events();
 n.podunk_retired=s.retired;
 n.source_closed=n.lifecycle_phase==FieldDialogueLifecyclePhase::Closed&&factories.empty()&&
    roots.empty()&&s.objects.empty()&&s.wait_connections.empty()&&coroutines.receivers.empty()&&
    awaiting.empty()&&!s.printer_owner&&!s.notifying&&!s.input_live&&!n.callback_depth&&
    !n.pending_callbacks&&!n.pending_choice_events&&!selected_;
 if(n.source_closed&&(!s.root->observes_closed_printer(in_.house->presentation,e)||
    !s.dialogue->observes_closed_printer(in_.house->presentation,e)||
    !s.native->observes_closed_printer(in_.house->presentation,e)||
    !s.ui->source_business_closed(e)||
    !s.lifecycle->source_frame_closed(e)||!life->source_frame_closed(e)))return false;
 if(c.dialogue&&s.input_live&&s.notifying==c.dialogue&&s.callback_receiver==c.dialogue&&
    s.callback_depth&&s.frame.phase==FieldTreePhase::Input){
  PodunkDialogueFrame actual;
  if(!s.root->source_frame(c.dialogue,actual,e)||actual.phase!=s.frame.phase||
     actual.action!=s.frame.action||actual.pressed!=s.frame.pressed)
   return reject(e,"House source input receipt differs from its actual Root frame endpoint");
  PodunkDialogueScriptState script;
  if(!s.root->state(c.dialogue,script,e))return false;
  auto *v=s.dialogue->visual(c.dialogue);
  const auto *cursor=v?v->cursor_state(script.references[5]):nullptr;
  n.input_callback=actual.pressed;
  n.choice_callback=(s.choices->active()||selected_)&&cursor&&actual.pressed;
  if(cursor)n.cursor_index=cursor->index;
 }
 if(selected_){
  n.selected_choice_pc=selected_->target_pc;
  n.choice_event_consumed=selected_dialogue_==c.dialogue&&selected_generation_==c.generation;
 }
 n.inspected=true;out=std::move(n);e.clear();return true;
}
bool HouseReturnDialogueNativeOwner::admit_command(const HouseReturnDialogueContext &c,
    uint32_t pc,const RoomCommand &cmd,std::string &e)const{
 FieldProgrammeContext native;if(!context(c,native,e))return false;
 const auto p=c.room.program(c.programme);
 if(cmd.opcode>uint16_t(DialogueActionKind::AnimateSpecialActor))
  return reject(e,"House native admission received an unknown source opcode");
 if(pc>=p.command_count)return reject(e,"House native admission PC is outside its immutable programme");
 const auto actual=c.room.command(p.first_command+pc);
 if(actual.opcode!=cmd.opcode||actual.actor_index!=cmd.actor_index||actual.target_index!=cmd.target_index||
    actual.phrase!=cmd.phrase||actual.flags!=cmd.flags||actual.auxiliary_index!=cmd.auxiliary_index||
    actual.vector.x!=cmd.vector.x||actual.vector.y!=cmd.vector.y||actual.value!=cmd.value||actual.duration!=cmd.duration)
  return reject(e,"House native admission received a copied/non-source instruction");
 using K=DialogueActionKind;
 switch(K(cmd.opcode)){
 case K::BeginCutscene:case K::StopInteraction:case K::SetTalker:
 case K::CutsceneEnded:case K::DialogueDone:{
  if((K(cmd.opcode)==K::StopInteraction&&!(cmd.flags&1))||K(cmd.opcode)==K::SetTalker)
   return reject(e,"House actor-dictionary talker lifecycle requires its actual source actor owner");
  DialogueAction a{K(cmd.opcode),cmd.actor_index,cmd.phrase,cmd.vector,cmd.value,
                   cmd.duration,cmd.target_index,cmd.auxiliary_index,cmd.flags};
  return owners_.dialogue->admit_room(c.programme,a,native,e);
 }
 case K::ShowDialogue:{HouseDialogue t;
  if(!(cmd.flags&1))return reject(e,"House actor phrase requires a mapped source actor dictionary");
  return in_.driver->source_text(cmd.target_index,t,e);}
 case K::AwaitChoices:
  if(cmd.target_index>=owners_.choice_data->groups().size())
   return reject(e,"House choice command refers to an unknown source group");
  return owners_.choice_data->validate_program(cmd.target_index,c.room.string(p.source_path_string),p.command_count,e);
 case K::AwaitDialogue:case K::Jump:case K::BranchFlag:case K::BranchLeader:
 case K::YieldIdle:case K::AwaitTimer:case K::StartWait:case K::SetFlag:
 case K::MusicFadeOut:case K::PlayMusicImmediate:case K::PlaySound:
 case K::ShakeCamera:case K::ChangeCamera:case K::MoveCamera:case K::ReturnCamera:
 case K::OverworldBattleMusic:
  e.clear();return true; // Existing checked World consumers; no native duplicate.
 default:return reject(e,"House native command has no actual Root/actor/business source consumer");
 }
}
bool HouseReturnDialogueNativeOwner::open(const HouseReturnDialogueContext &c,
    FieldObjectId &out,std::string &e){
 FieldProgrammeContext native;
 if(!programme_bound_||!in_.driver->native_open_live()||selected_||!context(c,native,e)||c.dialogue)
  return reject(e,"House native open requires its bound closed actual programme caller");
 return owners_.dialogue->open_room(c.programme,native,c.generation,out,e);
}
bool HouseReturnDialogueNativeOwner::source_started(const HouseReturnDialogueContext &c,
    std::string &e){
 SourceCall call(callback_depth_);FieldProgrammeContext native;
 if(!programme_bound_||!in_.driver->native_start_live()||!context(c,native,e)||
    c.generation!=in_.house->world.source_generation()||
    !owners_.dialogue->admit_ready(c.dialogue,c.generation,e))return false;
 return owners_.root->begin(c.dialogue,c.generation,e);
}
bool HouseReturnDialogueNativeOwner::apply_native(const HouseReturnDialogueContext &c,
    uint32_t pc,const DialogueAction &a,std::string &e){
 SourceCall call(callback_depth_);FieldProgrammeContext native;
 if(!programme_bound_||!in_.driver->native_action_live()||!context(c,native,e)||
    !owners_.dialogue->admit_ready(c.dialogue,c.generation,e))return false;
 using K=DialogueActionKind;
 switch(a.kind){
 case K::BeginCutscene:case K::StopInteraction:case K::SetTalker:
 case K::CutsceneEnded:case K::DialogueDone:return owners_.dialogue->apply(a,native,e);
 case K::ShowDialogue:{
  HouseDialogue t;std::string name;
  if(!in_.driver->source_text(a.target_index,t,e)||!owners_.global_data->data()||
     !owners_.global_data->character_nickname(owners_.global_data->data()->first_character(),name,e)||
     !owners_.root->phrase_begin(c.dialogue,e))return false;
  if(!in_.house->presentation.present_story_dialogue(t.first_segment,t.segment_count,name))
   return reject(e,in_.house->presentation.error());
  return owners_.root->presented_text(c.dialogue,c.room,c.room.program(c.programme).first_command+pc,t,e);
 }
 case K::AwaitChoices:{
  const auto p=c.room.program(c.programme);
  return owners_.choices->prepare(*owners_.choice_data,a.target_index,
      c.room.string(p.source_path_string),p.command_count,e);
 }
 default:{
  // Re-run the exact immutable admission; accepted actions are executed once
  // by OpeningWorld after this source hook returns.
  const auto p=c.room.program(c.programme);
  return admit_command(c,pc,c.room.command(p.first_command+pc),e);
 }
 }
}
bool HouseReturnDialogueNativeOwner::input_sound(const HouseReturnDialogueContext &c,
    DialogueChoiceSound sound,std::string &e)const{
 FieldObjectId id=0;
 if(!in_.tree->get_node(c.dialogue,owners_.input_sound_node,id,e))return false;
 auto *audio=owners_.dialogue->audio(c.dialogue);
 const auto *state=audio?audio->state(id):nullptr;
 const auto *asset=state?owners_.audio_data->asset(state->stream):nullptr;
 if(!state||!state->entered||!state->ready||!asset||
    source_path(asset->source)!=source_path(owners_.choice_data->sound(sound)))
  return reject(e,"House choice InputSound does not own its exact checked source stream");
 e.clear();return true;
}
bool HouseReturnDialogueNativeOwner::select_choices(const HouseReturnDialogueContext &c,
    int32_t index,bool confirm,bool cancel,uint32_t &pc,std::string &e){
 SourceCall call(callback_depth_);HouseReturnDialogueReceipt actual;
 if(selected_||!observe(c,actual,e)||!actual.choice_callback||actual.cursor_index!=index||
    actual.pending_choice_events||(!confirm&&!cancel)||
    !owners_.choices->source_cursor_selection(index,e))
  return reject(e,"House selection lacks the current source Cursor callback/empty event queue");
 if(!owners_.choices->step(0,{0,0,confirm,cancel},e))return false;
 const auto *event=owners_.choices->peek_event();
 const auto sound=cancel?DialogueChoiceSound::Cancel:DialogueChoiceSound::Accept;
 if(owners_.choices->pending_events()!=1||!event||event->kind!=DialogueChoicesEventKind::Selected||
    !event->clear_dialogue||!event->sound_after_target||event->sound!=sound||
    event->cancelled!=cancel||!input_sound(c,event->sound,e))
  return reject(e,"House source choice produced an unknown/unowned event tail");
 DialogueChoicesEvent consumed;
 if(!owners_.choices->poll_event(consumed))return reject(e,"House source Selected event vanished");
 selected_=consumed;selected_dialogue_=c.dialogue;selected_generation_=c.generation;
 in_.house->presentation.clear_story_text();pc=consumed.target_pc;e.clear();return true;
}
bool HouseReturnDialogueNativeOwner::choice_target_committed(const HouseReturnDialogueContext &c,
    std::string &e){
 SourceCall call(callback_depth_);PodunkMickHouseNativeState s;
 if(!state(s,e)||!selected_||selected_dialogue_!=c.dialogue||selected_generation_!=c.generation||
    !s.input_live||s.notifying!=c.dialogue||!s.callback_depth||
    s.frame.phase!=FieldTreePhase::Input||owners_.choices->pending_events()||
    !input_sound(c,selected_->sound,e))
  return reject(e,"House choice sound tail lost its actual committed source callback");
 if(!owners_.dialogue->audio_event(c.dialogue,{HouseAudioKind::Confirm,{},1},e))return false;
 selected_.reset();selected_dialogue_=0;selected_generation_=0;e.clear();return true;
}
} // namespace encore::ctr
