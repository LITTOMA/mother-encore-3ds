#include "podunk_dialogue_lifecycle_ports.hpp"
#include <algorithm>
#include <cmath>

namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e, const char *message) { e = message; return false; }
bool same(const FieldIdentity &a, const FieldIdentity &b) {
  return a.scene_id == b.scene_id && a.upstream_commit == b.upstream_commit &&
         a.source_sha256 == b.source_sha256;
}
}
class PodunkDialogueLifecyclePorts::Wait final : public FieldGlobalNativeReference {
public:
  Wait(PodunkDialogueLifecyclePorts &o, FieldGlobalExternalBinding b)
      : owner(o), source(std::move(b)) {}
  ~Wait() {
    if (owner.in_.registry && owner.in_.registry->object_exists(source.object)) {
      std::string e;
      owner.in_.registry->retire_object(source.object, e);
    }
  }
  FieldGlobalExternalBinding binding() const override { return source; }
  const char *native_class() const override { return "GDScriptFunctionState"; }
  const FieldGlobalRegistry *registry() const override { return owner.in_.registry; }
  bool checked_source_hash(std::string_view p, std::array<uint8_t,32> &h) const override {
    return owner.in_.ui_data && owner.in_.ui_data->source_hash(p,h);
  }
  bool dispatch(const FieldDeferredMessage &m, std::string &e) override {
    return owner.resume(source.object,m,e);
  }
  PodunkDialogueLifecyclePorts &owner;
  FieldGlobalExternalBinding source;
  FieldObjectId emitter=0, instance=0;
  uint32_t generation=0;
  Kind kind=Kind::Ready;
  std::string signal;
  std::function<bool()> callback;
  std::function<bool(int64_t)> done;
  bool resumed=false, executing=false;
};
PodunkDialogueLifecyclePorts::~PodunkDialogueLifecyclePorts() {
  std::string e; shutdown(e);
}
bool PodunkDialogueLifecyclePorts::source_frame_closed(std::string &e)const{
 if(!in_.registry||in_.registry->poisoned()||callback_depth_||callback_receiver_||!waiting_.empty()||
    in_.registry->pending_messages_to(wait_history_))
  return fail(e,"Dialogue reentry rejects retained source Ready/Done/Animation wait owners");
 e.clear();return true;
}
bool PodunkDialogueLifecyclePorts::source_coroutines(PodunkDialogueCoroutineState &out,
    std::string &e)const{
 if(!in_.registry||in_.registry->poisoned()||!in_.signals||
    in_.signals->registry()!=in_.registry)
  return fail(e,"Dialogue coroutine receipt has no actual Registry/signal owner");
 PodunkDialogueCoroutineState n;
 n.callback_depth=callback_depth_;n.callback_receiver=callback_receiver_;
 n.history.assign(wait_history_.begin(),wait_history_.end());
 for(const auto &entry:waiting_){
  const auto &w=entry.second;
  bool connected=false;
  if(!w||w->source.object!=entry.first||w->registry()!=in_.registry||
     in_.registry->native_reference(entry.first).get()!=w.get()||!w->emitter||
     !w->generation||!w->instance||!in_.registry->object_exists(w->emitter)||
     !in_.registry->object_exists(w->instance)||
     !in_.signals->connected(w->emitter,w->signal,entry.first,"_signal_callback",connected,e)||
     (!connected&&!w->executing))
   return fail(e,"Dialogue coroutine receipt contains stale/unknown actual waiter ownership");
  n.receivers.push_back(entry.first);
 }
 out=std::move(n);e.clear();return true;
}
bool PodunkDialogueLifecyclePorts::prepare(PodunkDialogueLifecycleInput in,
                                           std::string &e) {
  if (in_.registry || !in.ui_data || !in.ui_data->valid() ||
      !in.ui_data->dialogue_continuation() || !in.life || !in.life->valid() ||
      !in.recipe || !in.recipe->valid() || !in.programmes || !in.programmes->valid() || !in.motion || !in.motion->valid() ||
      !in.registry || !in.signals || in.signals->registry()!=in.registry ||
      !in.ui || !in.player || !in.dialogue || !in.root_script || !in.programme ||
      !in.business || in.business->registry()!=in.registry ||
      in.life->factory_ir_sha()!=in.recipe->ir_sha256() ||
      in.life->commit()!=in.ui_data->identity().upstream_commit ||
      in.motion->identity().upstream_commit!=in.life->commit() ||
      in.registry->external_object(in.ui->binding().object)!=in.ui)
    return fail(e,"Dialogue lifecycle actual owners/source bindings absent");
  std::array<uint8_t,32> hash{}, other{};
  for (const auto &p : {in.ui_data->source_script(),
                       in.ui_data->dialogue_policy().dialogue_script}) {
    if (!in.ui_data->source_hash(p,hash) || !in.life->source_hash(p,other) || hash!=other)
      return fail(e,"Dialogue coroutine source proof differs");
  }
  if(!in.root_script->bind_observation_defaults(*in.ui_data,e))return false;
  in_=in; e.clear(); return true;
}
bool PodunkDialogueLifecyclePorts::player(bool &paused, std::string &e) const {
  if (!in_.registry || in_.player->registry()!=in_.registry ||
      !in_.player->ready_complete() || !in_.player->body().constructed() ||
      !in_.registry->object_exists(in_.player->body().object()))
    return fail(e,"Dialogue actual live Player Ready/body absent");
  PlayerInitializationMember value;
  if (!in_.player->body().member(in_.motion->field(PlayerMotionField::Paused),value,e))
    return false;
  if (value.kind!=1 || !value.value || value.value->kind!=1)
    return fail(e,"Dialogue Player paused member is not source bool");
  paused=value.value->boolean; e.clear(); return true;
}
bool PodunkDialogueLifecyclePorts::checked_step(const FieldDialogueStep &step,
                                                std::string &e) const {
  if (!in_.life || std::none_of(in_.life->steps().begin(),in_.life->steps().end(),
      [&](const auto &s) { return s.stage==step.stage && s.op==step.op &&
        s.role==step.role && s.value==step.value && s.text==step.text; }))
    return fail(e,"Dialogue lifecycle unknown source step");
  return true;
}
bool PodunkDialogueLifecyclePorts::observe(FieldObjectId id,
                                             FieldDialogueObservation &out,
                                             std::string &e) const {
  HouseUiDialogueState ui;
  FieldDialogueObservation actual;
  if (!in_.registry || !in_.ui->source_dialogue_state(ui,e) ||
      !in_.business->observe(id,actual,e) || !player(actual.player_paused,e))
    return false;
  actual.stable_canvas=ui.stable_canvas;
  actual.dialogue_present=ui.current_dialogue!=0;
  actual.talker=ui.talker;
  actual.talker_valid=ui.talker && in_.registry->object_exists(ui.talker);
  actual.in_ui_stack=std::find(ui.stack.begin(),ui.stack.end(),id)!=ui.stack.end();
  if (id) {
    auto tree=in_.registry->tree_owner(id);
    const auto *desc=tree ? tree->descriptor(id) : nullptr;
    FieldIdentity identity{};
    if (!desc || !tree->object_identity(id,identity) ||
        !same(identity,in_.recipe->identity()) ||
        desc->id!=in_.recipe->identity().scene_id)
      return fail(e,"Dialogue observation source root differs");
    if(!in_.root_script->source_observation(id,actual,e))return false;
    auto *native=in_.dialogue->ui(id);
    FieldObjectId box=0,name=0;
    if (!tree->get_node(id,in_.life->node(2),box,e) ||
        !tree->get_node(id,in_.life->node(3),name,e)) return false;
    const auto *box_state=native ? native->control(box) : nullptr;
    const auto *name_state=native ? native->control(name) : nullptr;
    if (!box_state || !name_state || !std::isfinite(box_state->rect.y))
      return fail(e,"Dialogue actual native box/name state absent");
    actual.dialogue_y=box_state->rect.y;
    actual.name_nonempty=!name_state->text.empty();
  }
  out=actual; e.clear(); return true;
}
bool PodunkDialogueLifecyclePorts::wait(FieldObjectId emitter,uint32_t generation,
    std::string_view signal,Kind kind,std::function<bool()> callback,
    std::function<bool(int64_t)> done,std::string &e) {
  auto tree=in_.registry ? in_.registry->tree_owner(emitter) : nullptr;
  const auto *desc=tree ? tree->descriptor(emitter) : nullptr;
  FieldIdentity identity{};
  const auto *ref=in_.life ? in_.life->reference(kind==Kind::Animation ? 7 : 1) : nullptr;
  const auto expected=kind==Kind::Ready ? in_.life->ready_signal() :
                      kind==Kind::Done ? in_.life->done_signal() : in_.life->animation_signal();
  if (!tree || !desc || !ref || desc->id!=ref->id || !generation ||
      !tree->object_identity(emitter,identity) || !same(identity,in_.recipe->identity()) ||
      signal!=expected || (kind==Kind::Done ? !done : !callback))
    return fail(e,"Dialogue actual source yield emitter/signature rejected");
  for (const auto &v:waiting_)
    if (!v.second->resumed && v.second->emitter==emitter &&
        v.second->generation==generation && v.second->kind==kind)
      return fail(e,"Dialogue duplicate source coroutine waiter");
  FieldGlobalExternalBinding b;
  if (!in_.registry->allocate_object(b.object,e)) return false;
  b.family=0x454e0064; b.capability=4; b.source.identity=in_.ui_data->identity();
  b.source.stable_id=ref->id; b.source.role=5;
  b.source.native_class="GDScriptFunctionState";
  b.source.source=kind==Kind::Animation ? in_.ui_data->dialogue_policy().dialogue_script :
                                         in_.ui_data->source_script();
  if (!in_.ui_data->source_hash(b.source.source,b.source.source_sha)) return false;
  b.source.identity.source_sha256=b.source.source_sha;
  b.source.script=b.source.source; b.source.script_sha=b.source.source_sha;
  auto owner=std::make_shared<Wait>(*this,b);
  owner->emitter=emitter; owner->generation=generation; owner->kind=kind;
  owner->instance=in_.ui->binding().object;
  if (kind==Kind::Animation && !tree->get_node(emitter,"..",owner->instance,e)) return false;
  owner->signal=std::string(signal); owner->callback=std::move(callback); owner->done=std::move(done);
  if (!owner->instance || !in_.registry->object_exists(owner->instance) ||
      !in_.registry->publish_native_reference(b.source,b.object,owner,e)) return false;
  waiting_.emplace(b.object,owner);wait_history_.insert(b.object);
  if (!in_.signals->connect(emitter,signal,b.object,"_signal_callback",FieldSignalOneShot,
                            {FieldObjectRef{b.object}},e)) {
    waiting_.erase(b.object); return false;
  }
  e.clear(); return true;
}
bool PodunkDialogueLifecyclePorts::resume(FieldObjectId id,
    const FieldDeferredMessage &m,std::string &e) {
  const auto at=waiting_.find(id);
  auto owner=at==waiting_.end() ? nullptr : at->second;
  if (!owner || owner->resumed || owner->executing || m.object!=id ||
      m.kind!=FieldDeferredKind::Call || m.member!="_signal_callback" || m.args.empty() ||
      !std::holds_alternative<FieldObjectRef>(m.args.back()) ||
      std::get<FieldObjectRef>(m.args.back()).id!=id ||
      !in_.registry->object_exists(owner->instance) ||
      in_.registry->native_reference(id).get()!=owner.get() ||
      !in_.signals->emitting_to(owner->emitter,owner->signal,id,"_signal_callback"))
    return fail(e,"Dialogue invalid/dead/reentrant source FunctionState resume");
  bool connected=false;
  if (!in_.signals->connected(owner->emitter,owner->signal,id,"_signal_callback",connected,e) ||
      !connected) return fail(e,"Dialogue resume is outside actual signal connection");
  if (owner->kind==Kind::Ready ? m.args.size()!=1 : m.args.size()!=2)
    return fail(e,"Dialogue coroutine signal argument count differs");
  int64_t response=0;
  if (owner->kind==Kind::Done) {
    if (!std::holds_alternative<int64_t>(m.args[0]))
      return fail(e,"Dialogue done signal result is not source integer");
    response=std::get<int64_t>(m.args[0]);
  }
  if (owner->kind==Kind::Animation &&
      (!std::holds_alternative<std::string>(m.args[0]) ||
       std::get<std::string>(m.args[0])!=in_.life->close_clip().name))
    return fail(e,"Dialogue yielded animation signal does not match Close");
  // A callback can disconnect/retire its waiter before unwinding. Keep actual
  // receiver liveness independent of waiting_'s retained ownership map.
  struct ActiveCallback {
    uint32_t &depth;FieldObjectId &receiver;FieldObjectId previous;
    ActiveCallback(uint32_t &d,FieldObjectId &r,FieldObjectId id):depth(d),receiver(r),previous(r){++depth;receiver=id;}
    ~ActiveCallback(){receiver=previous;--depth;}
  } callback(callback_depth_,callback_receiver_,id);
  owner->executing=true;
  const bool ok=owner->kind==Kind::Done ? owner->done(response) : owner->callback();
  owner->executing=false; owner->resumed=true;
  // Keep the actual Ref alive until emit completes its native ONE_SHOT removal.
  if (!ok) return fail(e,"Dialogue source coroutine continuation rejected");
  e.clear(); return true;
}
bool PodunkDialogueLifecyclePorts::disconnect(FieldObjectId emitter,
                                               uint32_t generation,std::string &e) {
  for (auto i=waiting_.begin();i!=waiting_.end();) {
    auto owner=i->second;
    if ((owner->emitter!=emitter && owner->instance!=emitter) || owner->generation!=generation) { ++i; continue; }
    bool connected=false;
    if (!in_.registry->object_exists(owner->emitter)) {
      if(!in_.signals->release(owner->emitter,e))return false;
    } else if (!in_.signals->connected(owner->emitter,owner->signal,i->first,"_signal_callback",connected,e) ||
        (connected && !in_.signals->disconnect(owner->emitter,owner->signal,i->first,"_signal_callback",e)))
      return false;
    const auto retired=owner->source.object;
    i=waiting_.erase(i);owner.reset();
    if(!in_.registry->retire_object(retired,e))return false;
  }
  e.clear(); return true;
}
bool PodunkDialogueLifecyclePorts::shutdown(std::string &e) {
  while (!waiting_.empty()) {
    const auto emitter=waiting_.begin()->second->emitter;
    const auto generation=waiting_.begin()->second->generation;
    if (!disconnect(emitter,generation,e)) return false;
  }
  in_={}; e.clear(); return true;
}
FieldDialogueLifecycleHost PodunkDialogueLifecyclePorts::host() {
  FieldDialogueLifecycleHost h;
  h.admit_factory=[this](const auto &d,std::string &e) {
    return &d==in_.life && in_.dialogue->admit_factory(e);
  };
  h.admit_parent=[this](FieldObjectId id,std::string &e) {return in_.ui->source_dialogue_parent(id,e);};
  h.admit_step=[this](const auto &s,const auto &ctx,std::string &e) {
    if (!checked_step(s,e)) return false;
    using O=FieldDialogueOp;
    switch (s.op) {
    case O::StoreDialogue: case O::PauseMenuInactive: case O::StackPush:
    case O::UiCutscene: case O::ClearDialogue: case O::RemoveUi:
    case O::GlobalCutscene: case O::SetTalker:
    case O::DeferredAdd: case O::SetProgramme: case O::ConnectName:
    case O::InputRelease: case O::ArrowHide: case O::VisibleCharacters:
    case O::TextClear: case O::BulletClear: case O::VoiceVolume:
    case O::TextHide: case O::NameClose: case O::ResetPhrase: case O::CloseBox:
    case O::UnpauseIfOwned: case O::EmitDone:
    case O::StopTalker: return in_.dialogue->admit_factory(e);
    default: return in_.business->admit(s,ctx,e);
    }
  };
  h.observe=[this](FieldObjectId id,auto &o,std::string &e) {return observe(id,o,e);};
  h.pause_player=[this](bool stop,bool idle,std::string &e) {
    bool paused=false; return player(paused,e) && in_.player->motion().pause(stop,idle,true,e);
  };
  h.unpause_player=[this](std::string &e) {
    bool paused=false; return player(paused,e) && in_.player->motion().unpause(true,e);
  };
  h.manager=[this](const auto &s,FieldObjectId id,std::string &e) {
    if (!checked_step(s,e)) return false;
    using O=FieldDialogueOp;
    switch (s.op) {
    case O::StoreDialogue: case O::PauseMenuInactive: case O::StackPush:
    case O::UiCutscene: case O::ClearDialogue: case O::RemoveUi:
      return in_.ui->source_dialogue_step(s,id,e);
    default: return in_.business->manager(s,id,e);
    }
  };
  h.global=[this](const auto &s,FieldObjectId id,FieldObjectId talker,std::string &e) {
    if (!checked_step(s,e)) return false;
    if (s.op==FieldDialogueOp::GlobalCutscene || s.op==FieldDialogueOp::SetTalker)
      return in_.ui->source_dialogue_global_step(s,id,talker,e);
    return in_.business->global(s,id,talker,e);
  };
  h.native=[this](const auto &s,FieldObjectId id,std::string &e) {return in_.dialogue->native_step(s,id,e);};
  h.play_animation=[this](FieldObjectId id,const auto &c,std::string &e) {return in_.dialogue->play_animation(id,c,e);};
  h.close_sound=[this](std::string &e) {return in_.business->close_sound(e);};
  h.restore_telepathy=[this](std::string &e) {return in_.business->restore_telepathy(e);};
  h.return_camera=[this](bool offset,double seconds,std::string &e) {return in_.business->return_camera(offset,seconds,e);};
  h.connect_ready=[this](FieldObjectId id,uint32_t gen,std::string_view sig,std::function<bool()> cb,std::string &e) {
    return wait(id,gen,sig,Kind::Ready,std::move(cb),{},e);
  };
  h.connect_done=[this](FieldObjectId id,uint32_t gen,std::string_view sig,std::function<bool(int64_t)> cb,std::string &e) {
    return wait(id,gen,sig,Kind::Done,{},std::move(cb),e);
  };
  h.connect_animation=[this](FieldObjectId id,uint32_t gen,std::string_view sig,std::function<bool()> cb,std::string &e) {
    return wait(id,gen,sig,Kind::Animation,std::move(cb),{},e);
  };
  h.start_programme=[this](FieldObjectId id,uint32_t gen,std::string &e) {
    return in_.root_script->begin(id,gen,e) && in_.programme->dialogue_ready(id,gen,e);
  };
  h.bind_programme=[this](const auto &data,uint32_t programme,FieldObjectId id,const auto &ctx,std::string &e) {
    const auto &actual=in_.programme->programme();
    if (&data!=in_.programmes || actual.program_index()!=programme || actual.context().dialogue_object!=id || actual.context().source_npc!=ctx.source_npc ||
        actual.context().actor_object!=ctx.actor_object || actual.context().thoughts!=ctx.thoughts ||
        !data.record(programme)) return fail(e,"Dialogue current programme source context differs");
    return true;
  };
  h.emit_done=[this](FieldObjectId id,std::string_view signal,int64_t result,std::string &e) {
    if (signal!=in_.life->done_signal()) return fail(e,"Dialogue non-source done signal");
    return in_.signals->emit(id,signal,{result},e);
  };
  h.disconnect=[this](FieldObjectId id,uint32_t gen,std::string &e) {return disconnect(id,gen,e);};
  return h;
}
} // namespace encore::ctr
