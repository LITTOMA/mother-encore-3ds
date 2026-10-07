#include "house_return_target.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string&e,const std::string&s){e=s;return false;}
bool same(const FieldIdentity&a,const FieldIdentity&b){
  return a.scene_id==b.scene_id&&a.upstream_commit==b.upstream_commit&&
         a.source_sha256==b.source_sha256;
}
bool source_phase(FieldTreePhase p){
  return p==FieldTreePhase::EnterScript||p==FieldTreePhase::ReadyScript||
    p==FieldTreePhase::ExitScript||p==FieldTreePhase::Idle||p==FieldTreePhase::Physics||
    p==FieldTreePhase::Input||p==FieldTreePhase::UnhandledInput||
    p==FieldTreePhase::UnhandledKeyInput;
}
std::string_view node_signal(FieldTreePhase p){
  switch(p){
  case FieldTreePhase::TreeEntered:return "tree_entered";
  case FieldTreePhase::TreeExiting:return "tree_exiting";
  case FieldTreePhase::TreeExited:return "tree_exited";
  case FieldTreePhase::ReadySignal:return "ready";
  case FieldTreePhase::VisibilityChanged:return "visibility_changed";
  default:return {};
  }
}
bool base_only(FieldTreePhase p){
  return !node_signal(p).empty()||p==FieldTreePhase::NodeAdded||
    p==FieldTreePhase::NodeRemoved||p==FieldTreePhase::ChildEntered||
    p==FieldTreePhase::ChildExiting;
}
}
struct HouseReturnTarget::State {
  enum class Source {None,Room,Ladder,Prompt,Npc,Tint,Interact,Sprite,Sparkles,Shaker,Unknown};
  enum class Native {Canvas,Geometry,Kinematic,Ray,Timer,Visibility,Prompt,Animation,Control,Audio,Unknown};
  struct Record {Source source=Source::Unknown;Native native=Native::Unknown;
    bool control=false,constructed=false,source_constructed=false,bound=false,
      source_ready=false,native_ready=false;};
  struct Deleted {FieldObjectId object=0;Record record;};
  HouseReturnTargetInput in;
  HouseReturnPlayerSceneOwner player_scene_owner;
  PodunkSceneDoorTransfer held_door;
  FieldSceneRetirement old_retirement;
  std::vector<FieldObjectId>held_deleted;
  std::shared_ptr<FieldNodeTreeRuntime> tree=std::make_shared<FieldNodeTreeRuntime>();
  FieldGlobalRegistry*r=nullptr;FieldObjectSignals*signals=nullptr;PodunkNativeRoot*root=nullptr;
  HouseReturnScripts scripts;
  HouseReturnGeometryNative geometry;
  HouseReturnKinematicNative kinematic;
  HouseReturnRayNative rays;
  PodunkSceneNative native;
  PodunkSceneTimers timers;
  PodunkSceneVisibility visibility;
  HouseReturnControlsNative controls;
  HouseReturnGuiNative gui;
  HouseReturnButtonPromptNative prompts;
  HouseReturnPromptMaterialOwner prompt_materials;
  HouseReturnInteractDialog interact;
  HouseReturnInteractProgrammeBridge interact_programmes;
  HouseReturnLadder ladder;
  HouseReturnTintRuntime tint;
  HouseReturnNpcRuntime npcs;
  HouseReturnNpcPresentation presentation;
  HouseReturnUiContext ui;
  FieldSpriteRuntime sprites;
  PodunkNpcAnimationHost animations;
  HouseReturnDialogue driver;
  HouseReturnDialogueNativeOwner dialogue_native;
  HouseReturnInventoryOwner inventory;
  HouseReturnJoyInput joy_input;
  HouseReturnCameraControl shaker;
  HouseReturnCameraControlEffects shaker_effects;
  std::map<uint32_t,Record>records;
  std::map<FieldObjectId,Record>objects;
  std::vector<HouseReturnTargetGap>gaps;
  std::vector<Deleted>deleted;
  FieldDoorCandidate candidate;
  FieldObjectId old_root=0,player_parent=0;
  uint64_t token=0,frame=0;
  float idle=0,physics=0;
  bool audio_prepared=false,audio_retired=false,joy_prepared=false,
    effects_prepared=false,effects_bound=false,shaker_deleted=false;
  bool paused=false,pending=false,staged=false,prepared=false,owners=false,
    presenter=false,factory=false,old_left=false,old_freed=false,replaced=false,
    attached=false,player_attached=false,active=false,committed=false,poisoned=false;

  bool error(std::string&e,const std::string&m){poisoned=true;return fail(e,m);}
  const FieldNodeDescriptor*source(uint32_t id)const{return in.sources->tree().record(id);}
  bool actual(FieldObjectId id,std::string&e)const{
    FieldIdentity identity;const auto*d=tree->descriptor(id);const auto*n=tree->state(id);
    if(!d||!n||!n->alive||!r||r->poisoned()||r->tree_owner(id)!=tree||
      !r->object_exists(id)||!tree->object_identity(id,identity))
      return fail(e,"House Target receiver lacks its actual Tree/ObjectDB owner");
    if(in.player->owns(id))return true;
    const auto*original=source(d->id);
    if(!original||original->path!=d->path||original->script!=d->script||
      original->script_sha!=d->script_sha||!same(identity,in.sources->tree().identity())||
      tree->source_object(d->id)!=id)
      return fail(e,"House Target receiver differs from its checked full source tree");
    return true;
  }
  bool candidate_owned(const FieldDoorCandidate&c,std::string&e)const{
    if(!prepared||!candidate.token||c.token!=candidate.token||c.path!=candidate.path||
       c.pin!=candidate.pin||c.source_sha!=candidate.source_sha)
      return fail(e,"House Target received a foreign or expired owned candidate");
    return true;
  }
  bool matching(const FieldDoorCandidate&c,std::string&e)const{
    if(!candidate_owned(c,e))return false;
    return !poisoned||fail(e,"House Target poisoned candidate admits cleanup only");
  }
  void gap(const FieldNodeDescriptor&d,const std::string&why){
    gaps.push_back({d.id,d.path,d.script,d.native_class,why});
  }
  bool sprite_node(const FieldNodeDescriptor&d)const{
    const auto*s=in.sources->sprites().record(d.id);std::array<uint8_t,32>sha{};
    return s&&s->node==d.path&&s->parent_id==d.parent&&s->ready_ordinal==d.ready&&
      !d.script.empty()&&in.sources->tree().source_hash(d.script,sha)&&sha==d.script_sha;
  }
  Record classify(const FieldNodeDescriptor&d)const{
    Record v;v.control=in.sources->controls().record(d.id)!=nullptr;
    const auto&a=in.sources->canvas();const auto*art=a.record(d.id);
    if(d.script.empty())v.source=Source::None;
    else if(scripts.mapped_source(d.id)!=HouseReturnScripts::Role::Unmapped)v.source=Source::Room;
    else if(d.id==in.ladder->id()&&d.path==in.ladder->node()&&
      d.script==in.ladder->text(HouseLadderText::Script))v.source=Source::Ladder;
    else if(in.sources->button_prompts().core().record(d.id)&&
      d.script==in.sources->button_prompts().wait().source&&
      d.script_sha==in.sources->button_prompts().wait().source_sha)v.source=Source::Prompt;
    else if([&]{const auto&c=in.sources->camera_control();const auto&n=c.nodes()[0];
      std::array<uint8_t,32>sha{};
      return c.valid()&&d.id==n.id&&d.path==n.path&&d.ready==n.ready&&
        d.native_class==n.native_class&&d.script==c.script()&&
        c.source_hash(d.script,sha)&&sha==d.script_sha;}())v.source=Source::Shaker;
    else if(in.sources->npc_world().npc(d.id))v.source=Source::Npc;
    else if(in.sources->tint().record(d.id))v.source=Source::Tint;
    else if(in.sources->interact().record(d.id))v.source=Source::Interact;
    else if(sprite_node(d))v.source=Source::Sprite;
    else if(art&&art->kind==2&&art->owner==FieldCanvasOwner::Sparkles&&
      art->owner_id==d.id&&art->owner_script==d.script&&art->owner_sha==d.script_sha&&
      !art->ready_method.empty()&&d.script_methods==1)v.source=Source::Sparkles;
    if(const auto*n=in.sources->scene_audio().node(d.id);n&&n->path==d.path&&
      d.script.empty()&&((n->kind==1&&d.native_class=="AudioStreamPlayer")||
      (n->kind==2&&d.native_class=="AudioStreamPlayer2D")))v.native=Native::Audio;
    else if(in.sources->button_prompts().native().record(d.id))v.native=Native::Prompt;
    else if(d.native_class=="StaticBody2D"||d.native_class=="Area2D"||
      d.native_class=="CollisionShape2D"||d.native_class=="CollisionPolygon2D"){
      const auto&g=in.sources->geometry();
      for(uint32_t j=0;j<g.node_count();++j)if(g.node(j).stable_id==d.id)v.native=Native::Geometry;
    }else if(in.sources->npc_world().body(d.id))v.native=Native::Kinematic;
    else if(in.sources->npc_world().ray(d.id))v.native=Native::Ray;
    else if(in.sources->timers().record(in.sources->tree().identity(),d.id))v.native=Native::Timer;
    else if(d.native_class=="VisibilityNotifier2D"||d.native_class=="VisibilityEnabler2D"){
      // The concrete visibility owner must additionally admit its real record
      // during prepare/construct. Class text never grants a notification.
      v.native=Native::Visibility;
    }else if(d.native_class=="AnimationPlayer"&&d.script.empty()){
      const auto*p=in.sources->sprites().record(d.parent);
      if(p&&p->kind==FieldSpriteKind::Character)v.native=Native::Animation;
    }else if(v.control&&d.native_class=="Control")v.native=Native::Control;
    else if(d.native_class=="Node"||d.native_class=="Node2D"||
      d.native_class=="Position2D"||d.native_class=="YSort"||d.native_class=="TileMap"||
      d.native_class=="Sprite"||d.native_class=="TextureRect"||
      d.native_class=="ColorRect"||d.native_class=="AnimatedSprite")v.native=Native::Canvas;
    return v;
  }
  bool prepare_owners(std::string&e){
    if(owners||!prepared||!gaps.empty())return fail(e,"House Target owner preparation lacks complete source admission");
    const auto&s=*in.sources;
    if(!player_scene_owner.prepare({in.sources,tree.get(),r,&npcs,&interact,&prompts,
      in.player,in.continuation},e))return false;
    if(!geometry.prepare(s,*tree,*r,*signals,*in.space,*in.world,e)||
      !kinematic.prepare(s,*tree,*r,*signals,*in.space,*in.world,geometry,e)||
      !rays.prepare(s,s.npc_world(),s.npcs(),*tree,*r,*signals,*in.space,*in.world,kinematic,e)||
      !timers.prepare(s.timers(),s.tree(),*tree,*r,*signals,e)||
      !controls.prepare(s.controls(),s.tree(),s.canvas(),*tree,*r,*root,prompts,native,e)||
      !gui.prepare(controls,*signals,*root,e)||
      !prompt_materials.prepare(s.button_prompts(),s.tree(),*tree,*r,prompts,e))return false;
    HouseButtonPromptNativeInput pi;
    pi.data=&s.button_prompts();pi.source=&s.tree();pi.tree=tree.get();pi.registry=r;pi.signals=signals;
    pi.global_data=&in.characters->runtime();pi.global_owner=in.global;pi.player=in.player;
    pi.global=in.global->owner();pi.equipment=in.equipment;pi.font=in.font;pi.asset_root=in.asset_root.c_str();
    pi.interact=&interact;pi.materials=&prompt_materials;pi.source_calls=&player_scene_owner;
    if(!prompts.prepare(pi,e)||!interact_programmes.prepare(interact,*in.house,*tree,*r,e))return false;
    HouseReturnInteractInput ii;
    ii.sources=&s;ii.data=&s.interact();ii.doors=in.doors;ii.tree=tree.get();ii.registry=r;
    ii.signals=signals;ii.geometry=&geometry;ii.global=in.global;ii.global_data=in.characters;
    ii.house=in.house;ii.text=in.text;ii.dialogue=&driver;ii.prompts=&prompts;ii.programmes=&interact_programmes;
    HouseReturnTintInput ti;
    ti.sources=&s;ti.data=&s.tint();ti.tree=tree.get();ti.registry=r;ti.signals=signals;
    ti.canvas=&native;ti.kinematic=&kinematic;
    if(!interact.prepare(ii,e)||!tint.prepare(ti,e)||
      !native.prepare_house(s,*tree,*r,*root,*in.map,*in.space,geometry,
        *in.continuation->random(),in.asset_root.c_str(),in.canvas,in.materials,&controls,e)||
      !native.bind_canvas_leaf(controls,e)||!native.bind_sprite_signals(*signals,e)||
      !ui.prepare({&s,tree.get(),in.house,in.continuation,in.session,in.player,&driver,in.actual_debug_build,&npcs},e)||
      !ladder.prepare(*in.ladder,s.tree(),s.geometry(),in.player_sources,*tree,*r,*signals,
        *in.space,*in.player,e))return false;
    if(!joy_input.prepare(s.camera_control(),*in.retained_input,*r,e))return false;
    joy_prepared=true;
    HouseReturnCameraControlInput ci;
    ci.sources=&s;ci.data=&s.camera_control();ci.tree=tree.get();ci.registry=r;ci.signals=signals;
    ci.global=in.global;ci.random=in.continuation->random();ci.ui=in.continuation->ui();
    ci.player=in.player;ci.node_timers=&timers;ci.scene_timers=in.scene_timers;
    ci.controls=&controls;ci.effects=&shaker_effects;
    if(!shaker.prepare(ci,e))return false;
    owners=true;e.clear();return true;
  }
  bool prepare_presenter(std::string&e){
    if(presenter)return true;
    if(!owners||tree->object_domain()!=r->kernel())return fail(e,"House Target staged NPC presenter has no actual kernel");
    if(!audio_prepared){
      if(!in.retained_audio->prepare_house(in.sources->scene_audio(),*tree,*r,e))return false;
      audio_prepared=true;
      if(!in.retained_audio->bind_house_spatial(*in.sources,*root,*tree,e))return false;
    }
    HouseReturnNpcPorts ports=in.npc;
    if(!presentation.prepare({in.sources,tree.get(),r,signals,&npcs,&sprites,&animations,&native,
        &timers,in.scene_timers},e)||!ui.install(ports,e)||!presentation.apply(ports,e))return false;
    HouseReturnNpcInput ni;
    ni.sources=in.sources;ni.data=&in.sources->npcs();ni.world_data=&in.sources->npc_world();
    ni.doors=in.doors;ni.tree=tree.get();ni.registry=r;ni.signals=signals;ni.space=in.space;
    ni.kinematic=&kinematic;ni.geometry=&geometry;ni.rays=&rays;ni.global=in.global;
    ni.characters=in.characters;ni.random=in.continuation->random();ni.uid_ledger=in.continuation->uid_ledger();
    ni.house=in.house;ni.text=in.text;ni.session=in.session;ni.dialogue=&driver;
    ni.dialogue_native=&dialogue_native;ni.player=in.player->body().object();ni.ports=std::move(ports);
    if(!npcs.prepare(std::move(ni),e)||!presentation.initialize_sprites(in.sprite,e)||
      !visibility.prepare_house(*in.sources,*tree,*r,*signals,*root,npcs,&animations,e))return false;
    presenter=true;e.clear();return true;
  }
  bool allocated(FieldObjectId id,const FieldNodeDescriptor&d,const FieldIdentity&i,std::string&e){
    if(!r->publish_allocated_node(tree,id,[this](const auto&m,auto&e){return deferred(m,e);},e)||
      !prepare_presenter(e))return false;
    if(animations.owns(d)&&!source(d.id))return animations.construct(id,d,i,e);
    auto at=records.find(d.id);
    if(at==records.end()||!same(i,in.sources->tree().identity())||objects.count(id))
      return fail(e,"House Target attempted unadmitted actual native allocation");
    auto v=at->second;bool ok=false;
    switch(v.native){
    case Native::Canvas:ok=native.construct(id,d,i,e);break;
    case Native::Geometry:ok=geometry.construct(id,d,i,e);break;
    case Native::Kinematic:ok=kinematic.construct(id,d,i,e);break;
    case Native::Ray:ok=rays.construct(id,d,i,e);break;
    case Native::Timer:ok=timers.construct(id,d,i,e);break;
    case Native::Visibility:ok=visibility.construct(id,d,i,e);break;
    case Native::Prompt:ok=prompts.construct(id,d,i,e);break;
    case Native::Animation:ok=animations.construct(id,d,i,e);break;
    case Native::Audio:ok=in.retained_audio->construct(id,d,i,e);break;
    case Native::Control:ok=true;break; // Constructed by the Control owner below.
    case Native::Unknown:return fail(e,"House Target unknown native constructor: "+d.path);
    }
    if(!ok||(v.control&&!controls.construct(id,d,i,e)))return false;
    v.constructed=true;objects.emplace(id,v);e.clear();return true;
  }
  bool construct_source(FieldObjectId id,const FieldNodeDescriptor&d,const FieldIdentity&i,std::string&e){
    if(animations.owns(id)&&!source(d.id)){e.clear();return true;}
    auto at=objects.find(id);if(at==objects.end()||!at->second.constructed||!actual(id,e))return false;
    auto&v=at->second;bool ok=false;
    switch(v.source){
    case Source::None:ok=d.script.empty();break;
    case Source::Room:ok=scripts.construct(id,e);break;
    case Source::Shaker:ok=shaker.construct(id,d,i,e);break;
    case Source::Ladder:ok=ladder.construct(id,e);break;
    case Source::Prompt:ok=prompt_materials.construct(id,d,i,e)&&prompts.construct(id,d,i,e);break;
    case Source::Npc:ok=npcs.construct(id,d,i,e);break;
    case Source::Tint:ok=tint.construct(id,d,i,e);break;
    case Source::Interact:ok=interact.construct(id,d,i,e);break;
    case Source::Sprite:ok=sprites.create(d.id);if(!ok)e=sprites.error();break;
    case Source::Sparkles:{FieldNodeBinding b;ok=native.house_sparkles_binding(id,b,e);break;}
    case Source::Unknown:return fail(e,"House Target unmapped actual source constructor: "+d.path);
    }
    if(!ok)return false;
    v.source_constructed=true;e.clear();return true;
  }
  bool bind(FieldObjectId id,const FieldNodeDescriptor&d,FieldNodeBinding&out,std::string&e){
    if(in.player->owns(id))return in.player->bind(id,d,out,e);
    if(animations.owns(id)&&!source(d.id))return animations.bind(id,out,e);
    auto at=objects.find(id);
    if(at==objects.end()||!at->second.constructed||!at->second.source_constructed||at->second.bound)
      return fail(e,"House Target combined bind precedes its real native/source constructors");
    auto&v=at->second;
    FieldNodeBinding b{in.sources->tree().identity(),d.id,d.class_index,0x454e003c,3,d.script_sha,d.native_class};
    // Preserve each native mechanism's exact schema receipt (notably Ray).
    switch(v.native){
    case Native::Audio:if(!in.retained_audio->bind(id,b,e))return false;break;
    case Native::Ray:if(!rays.bind(id,b,e))return false;break;
    case Native::Timer:if(!timers.bind(id,b,e))return false;break;
    case Native::Visibility:if(!visibility.bind(id,b,e))return false;break;
    case Native::Prompt:if(!prompts.bind(id,b,e))return false;break;
    case Native::Animation:if(!animations.bind(id,b,e))return false;break;
    default:break;
    }
    switch(v.source){
    case Source::Shaker:if(!shaker.bind(id,b,e))return false;break;
    case Source::Prompt:if(!prompts.bind(id,b,e))return false;break;
    case Source::Npc:if(!npcs.bind(id,b,e))return false;break;
    case Source::Tint:if(!tint.bind(id,b,e))return false;break;
    case Source::Interact:if(!interact.bind(id,b,e))return false;break;
    case Source::Sparkles:if(!native.house_sparkles_binding(id,b,e))return false;break;
    case Source::Ladder:if(!ladder.source_constructed(id))return fail(e,"House Target Ladder source instance absent");break;
    case Source::Room:if(scripts.mapped_source(d.id)==HouseReturnScripts::Role::Unmapped)return false;break;
    case Source::Sprite:if(!sprites.instance(d.id))return fail(e,"House Target actual Sprite source body absent");break;
    case Source::None:break;
    case Source::Unknown:return fail(e,"House Target unknown source binding");
    }
    if((v.native==Native::Canvas&&!native.bind(id,b,e))||
      (v.native==Native::Geometry&&!geometry.bind(id,b,e))||
      (v.native==Native::Kinematic&&!kinematic.bind(id,b,e))||
      (v.control&&!controls.bind(id,b,e)))return false;
    v.bound=true;out=std::move(b);e.clear();return true;
  }
  bool source_call(FieldObjectId id,Record&v,FieldTreePhase p,std::string&e){
    const auto*d=tree->descriptor(id);bool ok=false;
    switch(v.source){
    case Source::None:ok=d&&d->script.empty();break;
    case Source::Room:ok=scripts.script_phase(id,p,e);break;
    case Source::Shaker:ok=shaker.phase(id,p,e);break;
    case Source::Npc:ok=npcs.source_phase(id,p,p==FieldTreePhase::Physics?physics:idle,paused,e);break;
    case Source::Tint:ok=tint.source_phase(id,p,e);break;
    case Source::Interact:ok=interact.source_phase(id,p,e);break;
    case Source::Prompt:ok=prompts.phase(id,p,idle,paused,pending,e);break;
    case Source::Sparkles:ok=native.phase_house(id,p,idle,paused,pending,e);break;
    case Source::Ladder:
      ok=d&&!d->script_methods&&ladder.source_constructed(id);break;
    case Source::Sprite:{
      const auto*s=d?in.sources->sprites().record(d->id):nullptr;
      if(!s||!sprites.instance(d->id))return fail(e,"House Target Sprite source body is missing");
      if(p==FieldTreePhase::ReadyScript)ok=sprites.ready(d->id);
      else if(p==FieldTreePhase::Idle&&s->kind==FieldSpriteKind::Fetcher)ok=sprites.process_fetcher(d->id);
      else if((p==FieldTreePhase::EnterScript&&!(d->script_methods&2))||
              (p==FieldTreePhase::ExitScript&&!(d->script_methods&4)))ok=true;
      else return fail(e,"House Target Sprite source phase has no mapped body");
      if(!ok)e=sprites.error();break;
    }
    case Source::Unknown:return fail(e,"House Target attempted an unmapped source notification");
    }
    if(!ok)return false;
    if(p==FieldTreePhase::ReadyScript)v.source_ready=true;
    e.clear();return true;
  }
  bool dispatch(FieldObjectId id,const FieldNodeBinding&b,FieldTreePhase p,std::string&e){
    if(in.player->owns(id))return in.player->phase(id,b,p,e);
    const auto*d=tree->descriptor(id);
    if(!d||r->tree_owner(id)!=tree||!r->object_exists(id))return fail(e,"House Target lifecycle receiver has no actual owner");
    if(p==FieldTreePhase::NodeAdded||p==FieldTreePhase::NodeRemoved||
      p==FieldTreePhase::ChildEntered||p==FieldTreePhase::ChildExiting||
      p==FieldTreePhase::ReadyNative||p==FieldTreePhase::ReadyScript)
      if(!root->node_notification(id,p,e))return false;
    if(std::find(held_door.objects().begin(),held_door.objects().end(),id)!=held_door.objects().end()){
      if(source_phase(p))return d->script.empty()||
        in.retained_scripts->phase(id,b,p,p==FieldTreePhase::Physics?physics:idle,e);
      if(!base_only(p)){
        if(in.retained_native->owns(id)){
          if(!in.retained_native->phase(id,p,e))return false;
        }else if(in.retained_audio->owns(id)){
          if(!in.retained_audio->phase(id,p,idle,paused,pending,e))return false;
        }else return fail(e,"House persistent Door has lost its original native owner");
      }
      return emit_node(id,p,e);
    }
    if(animations.owns(id)&&!source(d->id)){
      if(source_phase(p))return d->script.empty()||fail(e,"House dynamic native graph gained an unknown script");
      if(!base_only(p)&&!animations.phase(id,p,idle,paused,pending,e))return false;
      return emit_node(id,p,e);
    }
    auto at=objects.find(id);if(at==objects.end()||!at->second.bound)return fail(e,"House Target lifecycle before actual combined binding");
    auto&v=at->second;
    if(source_phase(p))return source_call(id,v,p,e);
    const float dt=p==FieldTreePhase::PhysicsInternal?physics:idle;bool ok=false,owns_signals=false;
    switch(v.native){
    case Native::Canvas:ok=native.phase_house(id,p,dt,paused,pending,e);break;
    case Native::Geometry:
      ok=geometry.phase(id,p,e);owns_signals=!node_signal(p).empty()&&p!=FieldTreePhase::VisibilityChanged;
      if(ok&&(p==FieldTreePhase::ChildEntered||p==FieldTreePhase::ChildExiting)){
        const auto*n=tree->state(id);
        owns_signals=n&&n->parent&&geometry.owns(n->parent);
      }
      break;
    case Native::Kinematic:ok=kinematic.phase(id,p,e);owns_signals=!node_signal(p).empty();break;
    case Native::Ray:ok=rays.phase(id,p,paused,e);owns_signals=!node_signal(p).empty()||
      p==FieldTreePhase::ChildEntered||p==FieldTreePhase::ChildExiting;break;
    case Native::Timer:ok=base_only(p)||timers.phase(id,p,dt,paused,e);break;
    case Native::Visibility:ok=base_only(p)||visibility.phase(id,p,dt,paused,pending,e);break;
    case Native::Prompt:ok=base_only(p)||prompts.phase(id,p,dt,paused,pending,e);break;
    case Native::Animation:ok=base_only(p)||animations.phase(id,p,dt,paused,pending,e);break;
    case Native::Audio:ok=base_only(p)||in.retained_audio->phase(id,p,dt,paused,pending,e);break;
    case Native::Control:ok=true;break;
    case Native::Unknown:return fail(e,"House Target unowned native lifecycle");
    }
    if(!ok)return false;
    if(v.control){
      if(p==FieldTreePhase::PostEnterNative){
        const auto*c=in.sources->controls().record(d->id);
        if(c&&c->pose_owner==0&&!controls.parent_rect_changed(native,tree->state(id)->parent,e))return false;
      }
      if((!base_only(p)||p==FieldTreePhase::VisibilityChanged)&&!controls.phase(id,p,e))return false;
      if((p==FieldTreePhase::EnterNative||p==FieldTreePhase::ExitNative)&&
        !root->house_gui_control(id,p,e))return false;
      if(p==FieldTreePhase::Deleting)v.control=false;
    }
    if(p==FieldTreePhase::ReadyNative)v.native_ready=true;
    if(p==FieldTreePhase::Deleting&&v.native!=Native::Canvas&&v.native!=Native::Geometry&&
      v.native!=Native::Kinematic){
      const auto&g=in.sources->geometry();
      for(uint32_t i=0;i<g.node_count();++i)if(g.node(i).stable_id==d->id)
        if(!geometry.observe_deleted(id,e))return false;
    }
    return owns_signals||emit_node(id,p,e);
  }
  bool emit_node(FieldObjectId id,FieldTreePhase p,std::string&e){
    const auto signal=node_signal(p);
    if(!signal.empty())return signals->emit(id,signal,{},e);
    if(p==FieldTreePhase::ChildEntered||p==FieldTreePhase::ChildExiting){
      const auto*n=tree->state(id);
      if(n&&n->parent&&kinematic.owns(n->parent))return kinematic.child_phase(id,p,e);
      if(n&&n->parent)return signals->emit(n->parent,p==FieldTreePhase::ChildEntered?
        "child_entered_tree":"child_exiting_tree",{FieldObjectRef{id}},e);
    }
    e.clear();return true;
  }
  bool release_node(FieldObjectId id,const FieldNodeBinding&,std::string&e){
    if(in.player->owns(id))return fail(e,"House target cannot delete the retained source Player implicitly");
    if(std::find(held_door.objects().begin(),held_door.objects().end(),id)!=held_door.objects().end()){
      const auto*n=tree->state(id);
      if(!n)return fail(e,"House retained Door release lost the actual pre-delete node");
      if(in.retained_scripts->owns(id)&&!in.retained_scripts->release(id,n->binding,e))return false;
      if(in.retained_audio->owns(id)&&!in.retained_audio->release(id,e))return false;
      held_deleted.push_back(id);e.clear();return true;
    }
    auto at=objects.find(id);
    if(at==objects.end()){
      if(animations.owns(id))return animations.release(id,e);
      return fail(e,"House Target unowned actual pre-delete receiver");
    }
    const auto v=at->second;
    // These specific source owners require the still-live, exited Tree object.
    // The remaining owners below require its actual erase/ObjectDB collection.
    if(v.source==Source::Npc&&!npcs.release_deleted(id,e))return false;
    if(v.source==Source::Prompt&&!prompts.release_deleted(id,e))return false;
    if(v.source==Source::Sprite){const auto*d=tree->descriptor(id);
      if(!d||!sprites.destroy(d->id)){e=sprites.error();return false;}}
    if(v.native==Native::Visibility&&!visibility.release(id,e))return false;
    if(v.native==Native::Animation&&!animations.release(id,e))return false;
    if(v.native==Native::Audio&&!in.retained_audio->release(id,e))return false;
    deleted.push_back({id,v});objects.erase(at);e.clear();return true;
  }
  bool deferred(const FieldDeferredMessage&m,std::string&e){
    if(in.player->owns(m.object))return in.player->deferred(m,e);
    if(in.retained_door&&in.retained_door->handles(m))return in.retained_door->deferred(m,e);
    if(std::find(held_door.objects().begin(),held_door.objects().end(),m.object)!=held_door.objects().end()){
      if(in.retained_audio->owns(m.object))return in.retained_audio->deferred(m,e);
      if(in.retained_scripts->owns(m.object))return in.retained_scripts->deferred(m,e);
      return fail(e,"House persistent Door method has no original source receiver");
    }
    if(!r->object_exists(m.object)||r->tree_owner(m.object)!=tree)
      return fail(e,"House Target message addressed a foreign ObjectDB owner");
    if(shaker.owns(m.object))return shaker.deferred(m,e);
    if(in.retained_audio->owns(m.object))return in.retained_audio->deferred(m,e);
    if(presentation.handles_callback(m))return presentation.deferred(m,e);
    if(visibility.handles_method(m))return visibility.deferred(m,e);
    if(npcs.handles_callback(m))return npcs.deferred(m,e);
    if(prompts.handles_method(m))return prompts.dispatch(m,e);
    if(interact.handles_method(m))return interact.dispatch(m,e);
    if(tint.handles_method(m))return tint.dispatch(m,e);
    if(rays.owns(m.object))return rays.deferred(m,e);
    if(timers.owns(m.object))return timers.deferred(m,e);
    if(animations.owns(m.object))return animations.deferred(m,e);
    const auto*d=tree->descriptor(m.object);const auto at=d?records.find(d->id):records.end();
    if(at!=records.end()){
      if(at->second.source==Source::Room)return scripts.dispatch(m,e);
      if(at->second.source==Source::Ladder)return ladder.dispatch(m,e);
      if(at->second.source==Source::Sparkles)return native.deferred_house(m,e);
    }
    return fail(e,"House Target source/native method has no actual consumer: "+m.member);
  }
};

HouseReturnTarget::HouseReturnTarget():state_(std::make_unique<State>()){}
HouseReturnTarget::~HouseReturnTarget()=default;
const std::vector<HouseReturnTargetGap>&HouseReturnTarget::missing()const{return state_->gaps;}
const HouseReentryData*HouseReturnTarget::data()const{return state_->in.sources?&state_->in.sources->reentry():nullptr;}
const FieldGlobalRegistry*HouseReturnTarget::registry()const{return state_->r;}
const std::shared_ptr<FieldNodeTreeRuntime>&HouseReturnTarget::tree()const{return state_->tree;}
HouseReturnNpcRuntime&HouseReturnTarget::npcs(){return state_->npcs;}
HouseReturnDialogue&HouseReturnTarget::dialogue(){return state_->driver;}
HouseReturnDialogueNativeOwner&HouseReturnTarget::dialogue_native(){return state_->dialogue_native;}
HouseReturnInventoryOwner&HouseReturnTarget::house_inventory(){return state_->inventory;}
PodunkSceneNative&HouseReturnTarget::canvas(){return state_->native;}
HouseReturnGeometryNative&HouseReturnTarget::geometry(){return state_->geometry;}
HouseReturnKinematicNative&HouseReturnTarget::kinematic(){return state_->kinematic;}
HouseReturnRayNative&HouseReturnTarget::rays(){return state_->rays;}
PodunkSceneTimers&HouseReturnTarget::timers(){return state_->timers;}
HouseReturnControlsNative&HouseReturnTarget::controls(){return state_->controls;}
HouseReturnCameraControl&HouseReturnTarget::room_shaker(){return state_->shaker;}

bool HouseReturnTarget::stage(HouseReturnTargetInput in,std::string&e){
  auto&s=*state_;
  if(s.staged||s.in.sources||!in.sources||!in.sources->valid()||!in.ladder||!in.ladder->valid()||
    !in.doors||!in.old_scene||!in.old_scene->valid()||!in.callbacks||!in.session_data||
    !in.room.valid()||!in.text.valid()||!in.drawer.valid()||!in.house||!in.continuation||
    !in.continuation->initialized()||!in.session||!in.global||!in.characters||!in.inventory||
    !in.map||!in.space||!in.world||!in.player||!in.camera||!in.scene_timers||!in.old_tree||
    !in.font||!in.retained_audio||!in.retained_native||!in.retained_scripts||!in.retained_door||
    !in.retained_player_services||!in.retained_input||
    in.asset_root.empty()||!in.player_sources.initialization||
    !in.player_sources.ready||!in.player_sources.motion)
    return fail(e,"House Target requires all actual immutable and retained source owners");
  auto*r=in.continuation->registry();auto*signals=in.continuation->signals();auto*root=in.continuation->native_root();
  const auto&re=in.sources->reentry();const auto identity=in.sources->tree().identity();
  const auto*retained_sources=in.retained_player_services->sources();
  if(!r||!signals||!root||r->poisoned()||signals->registry()!=r||root->kernel_object()!=r->kernel()||
    in.player->registry()!=r||in.world->registry()!=r||in.camera->registry()!=r||
    in.player->tree()!=in.old_tree.get()||in.world->tree()!=in.old_tree.get()||
    in.camera->tree()!=in.old_tree.get()||in.world->geometry_space()!=in.space||
    in.retained_player_services->tree()!=in.old_tree.get()||
    in.retained_player_services->registry()!=r||in.retained_player_services->player()!=in.player||
    !retained_sources||retained_sources->initialization!=in.player_sources.initialization||
    retained_sources->ready!=in.player_sources.ready||retained_sources->motion!=in.player_sources.motion||
    in.scene_timers->registry()!=r||!in.player->ready_complete()||
    in.characters->runtime().registry()!=r||!in.continuation->random()||!in.continuation->uid_ledger()||
    !same(in.old_scene->identity(),in.doors->identity())||
    in.old_scene->source_scene()!=re.source_scene()||!re.matches(*in.doors,in.room,in.text,e)||
    !in.ladder->matches(in.sources->tree(),in.sources->geometry(),*in.player_sources.initialization,
      *in.player_sources.ready,*in.player_sources.motion,e))
    return fail(e,"House Target retained Tree/Player/world/source domains differ");
  s.in=std::move(in);s.r=r;s.signals=signals;s.root=root;s.old_root=s.in.old_tree->root();
  const auto*old=s.in.old_tree->state(s.old_root);FieldIdentity old_identity;
  if(!old||!old->inside||r->tree_owner(s.old_root)!=s.in.old_tree||
    !s.in.old_tree->object_identity(s.old_root,old_identity)||!same(old_identity,s.in.old_scene->identity()))
    return s.error(e,"House Target old scene is not its actual live source root");
  FieldObjectId old_door=0,script_door=0;FieldDoorDescriptor original_door;
  old_door=s.in.old_tree->source_object(re.door_id());
  if(!old_door||!s.in.doors->find(re.door_id(),original_door)||
    s.in.retained_native->canvas_tree()!=s.in.old_tree.get()||
    !s.in.retained_native->owns(old_door)||!s.in.retained_scripts->owns(old_door)||
    !s.in.retained_scripts->object_for_source(re.door_id(),script_door,e)||script_door!=old_door||
    !s.in.retained_audio->owns(s.in.old_tree->source_object(original_door.audio))||
    s.in.retained_door->registry()!=r||s.in.retained_door->source_data()!=s.in.doors||
    !s.in.retained_door->source_runtime()||s.in.retained_door->source_runtime()->data()!=s.in.doors)
    return s.error(e,"House Target lacks the same actual original Door native/source/audio owners");
  HouseReturnScripts::Input si;
  si.reentry=&re;si.nodes=&s.in.sources->tree();si.doors=s.in.doors;si.room=s.in.room;si.house=s.in.text;
  si.tree=s.tree.get();si.registry=r;si.signals=signals;si.global=s.in.global;
  si.globaldata=&s.in.characters->runtime();si.flags=&s.in.characters->flags();si.callbacks=s.in.callbacks;
  if(!s.scripts.prepare(si,e))return s.error(e,e);
  // No construction/Ready/RNG/GPU or Registry allocation happens in this pass.
  for(const auto&d:s.in.sources->tree().records()){
    const auto v=s.classify(d);s.records.emplace(d.id,v);
    if(v.source==State::Source::Unknown)s.gap(d,"Source constructor/lifecycle/method consumer is not implemented");
    if(v.native==State::Native::Unknown)s.gap(d,"Native source properties/clock owner is not implemented");
  }
  const auto&root_source=s.in.sources->tree().records().front();
  // These are observed concrete interface gaps, not booleans a caller may
  // toggle. Removing any gap requires its real consumer and checked endpoint.
  s.gap(root_source,"CharacterSprite sprite_changed requires the actual House Emotes source consumer");
  s.staged=true;(void)identity;e.clear();return true;
}
bool HouseReturnTarget::prepare(const FieldDoorDescriptor&d,FieldDoorCandidate&out,std::string&e){
  auto&s=*state_;const auto*re=data();FieldDoorDescriptor actual;
  if(!s.staged||s.poisoned||s.prepared||!re||d.id!=re->door_id()||
    !s.in.doors->find(d.id,actual)||actual.target_path!=d.target_path||
    s.in.doors->string(d.target_path)!=re->target_scene()||!re->empty_target_params()||
    actual.node!=d.node||actual.marker!=d.marker||actual.audio!=d.audio||actual.shape!=d.shape||
    actual.target.x!=d.target.x||actual.target.y!=d.target.y||
    actual.direction.x!=d.direction.x||actual.direction.y!=d.direction.y)
    return fail(e,"House Target accepts only its exact supported source Door destination");
  if(!s.gaps.empty())return fail(e,"House Target source closure incomplete before allocation: "+
    s.gaps.front().node+" : "+s.gaps.front().reason);
  if(s.token==std::numeric_limits<uint64_t>::max())return fail(e,"House Target candidate counter exhausted");
  FieldDoorCandidate c;c.token=++s.token;c.path=re->target_scene();
  c.pin=s.in.sources->tree().identity().upstream_commit;c.source_sha=s.in.sources->tree().identity().source_sha256;
  s.candidate=c;s.prepared=true;out=std::move(c);e.clear();return true;
}
bool HouseReturnTarget::instantiate(const FieldDoorCandidate&c,PodunkDoorScene&out,std::string&e){
  auto&s=*state_;if(!s.matching(c,e)||s.factory||s.tree->object_count()||!s.prepare_owners(e))return false;
  if(!s.root->bind_house_gui(s.gui,e))return s.error(e,e);
  FieldNodeTreeHost h;h.object_domain=s.r->kernel();
  h.allocate_object=[&s](auto&id,auto&e){return s.r->allocate_object(id,e);};
  h.allocate_fast_name=[&s](auto&id,auto&e){return s.r->allocate_fast_name(id,e);};
  h.native_allocated=[&s](auto id,const auto&d,const auto&i,auto&e){return s.allocated(id,d,i,e);};
  h.construct_source=[&s](auto id,const auto&d,const auto&i,auto&e){return s.construct_source(id,d,i,e);};
  h.bind=[&s](auto id,const auto&d,auto&b,auto&e){return s.bind(id,d,b,e);};
  h.dispatch=[&s](auto id,const auto&b,auto p,auto&e){return s.dispatch(id,b,p,e);};
  h.deferred=[&s](const auto&m,auto&e){return s.deferred(m,e);};
  h.enqueue_global=[&s](auto m,auto&e){return s.r->enqueue(std::move(m),e);};
  h.flush_global=[&s](auto&e){return s.r->flush_messages(e);};
  h.object_exists=[&s](auto id){return s.r->object_exists(id);};
  h.input_registration=[&s](auto id,auto kind,auto on,auto&e){return s.root->input_registration(id,kind,on,e);};
  h.external_pause_process=[](auto){return false;};
  h.external_path=[&s](auto id,auto p,auto&out,auto&e){return s.r->resolve_path(id,p,out,e);};
  h.release=[&s](auto id,const auto&b,auto&e){return s.release_node(id,b,e);};
  if(!s.tree->initialize(s.in.sources->tree(),std::move(h),e))return s.error(e,e);
  for(const auto&d:s.in.sources->tree().records()){
    const auto id=s.tree->source_object(d.id);
    if(!id||!s.tree->bind_source_object(id,e))return s.error(e,e);
    if(s.geometry.owns(id)&&!d.script.empty()&&
      !s.geometry.bind_script_receiver(id,[&s](const auto&m,auto&e){return s.deferred(m,e);},e))return s.error(e,e);
  }
  if(!s.interact.finish_factory(e)||!s.prompts.finish_factory(e)||!s.prompt_materials.finish_factory(e)||
    !s.controls.finish_factory(e)||!s.shaker.finish_factory(e)||!s.tint.finish_factory(e)||!s.geometry.finish_factory(e)||
    !s.kinematic.finish_factory(e)||!s.rays.finish_factory(e)||!s.native.finish_factory(e)||
    !s.presentation.finish_factory(e)||!s.visibility.finish_factory(e)||!s.ladder.connect_source(e)||
    !s.tree->get_node(s.tree->root(),data()->player_parent(),s.player_parent,e))return s.error(e,e);
  HouseReturnInventoryInput ii{&s.driver,&s.in.house->world,&s.in.house->house,s.in.global,
    s.in.characters,s.in.inventory,s.in.session_data,s.in.drawer};
  if(!s.inventory.prepare(ii,e)||
    !s.in.house->house.bind_drawer(s.in.drawer,s.inventory))return s.error(e,e.empty()?"House actual drawer source binding failed":e);
  s.factory=true;out={s.tree,s.tree->root(),s.player_parent,s.in.sources->tree().identity()};e.clear();return true;
}
bool HouseReturnTarget::leave_old(FieldObjectId id,FieldSceneHost&,std::string&e){
  auto&s=*state_;if(s.poisoned||!s.factory||s.old_left||id!=s.old_root||s.r->tree_owner(id)!=s.in.old_tree)
    return fail(e,"House Target leave_for is not the actual old scene boundary");
  const auto*d=s.in.old_tree->descriptor(id);FieldIdentity identity;
  if(!d||!s.in.old_tree->object_identity(id,identity)||!same(identity,s.in.old_scene->identity())||
    s.in.old_scene->string(s.in.old_scene->area().region)!=data()->source_region()||
    data()->area_left_arguments()!=1)
    return fail(e,"House Target leave_for source scene/region/signal proof differs");
  // Actual AreaRoom.leave_for only emits the checked source area_left value.
  if(!s.signals->emit(id,data()->area_left_signal(),
    {data()->source_region()!=data()->target_region()},e))return s.error(e,e);
  s.old_left=true;e.clear();return true;
}
bool HouseReturnTarget::free_old(std::shared_ptr<FieldNodeTreeRuntime>old,FieldObjectId id,std::string&e){
  auto&s=*state_;if(s.poisoned||!s.factory||!s.old_left||s.old_freed||old!=s.in.old_tree||id!=s.old_root)
    return fail(e,"House Target free received a foreign or unordered old scene");
  const auto*player=old->state(s.in.player->body().object());
  if(!player||player->inside||player->parent)return fail(e,"House Target old free precedes actual Player detach");
  std::shared_ptr<const GlobalLoadObjectArray> persistent;
  if(!s.in.global->array(FieldGlobalMemberRole::Persistent,persistent,e)||!persistent)return false;
  for(auto p:persistent->values){auto owner=s.r->tree_owner(p);const auto*n=owner?owner->state(p):nullptr;
    if(!n||n->inside||n->parent)return fail(e,"House Target old free precedes actual persistent source detach");}
  if(persistent->values.size()!=1||s.in.retained_door->source_data()!=s.in.doors||
    s.in.retained_door->registry()!=s.r||!s.in.retained_door->source_runtime()||
    !s.in.retained_native->retain_detached_door(*s.in.doors,data()->door_id(),s.held_door,e)||
    s.held_door.door()!=persistent->values.front()||s.held_door.old_tree()!=old.get())
    return s.error(e,e.empty()?"House Target persistent roster is not the actual original returning Door":e);
  if(!s.in.retained_scripts->stage_old_scene_retirement(id,*s.in.doors,
    *s.in.retained_door->source_runtime(),s.held_door.door(),s.old_retirement,e))return s.error(e,e);
  if(!s.r->detach_scene(id,e)||!old->queue_free(id,e)||!old->flush_delete_queue(e))return s.error(e,e);
  s.r->collect_dead_tree_objects();
  if(old->state(id)||s.r->object_exists(id)||old->lifecycle_pending())
    return s.error(e,"House Target actual old native scene deletion did not complete");
  s.old_freed=true;
  std::vector<FieldGeometryNodeUpdate>updates,bodies;
  if(!s.geometry.replacement_updates(updates,e)||!s.kinematic.replacement_updates(bodies,e))return s.error(e,e);
  for(const auto&body:bodies){
    auto at=std::find_if(updates.begin(),updates.end(),[&](const auto&v){return v.stable_id==body.stable_id;});
    if(at==updates.end()||(at->fields&2)||body.fields!=(1u|8u)||
      !(at->fields&1)||at->local.x.x!=body.local.x.x||at->local.x.y!=body.local.x.y||
      at->local.y.x!=body.local.y.x||at->local.y.y!=body.local.y.y||
      at->local.origin.x!=body.local.origin.x||at->local.origin.y!=body.local.origin.y||
      ((at->fields&8)&&(at->layer!=body.layer||at->mask!=body.mask)))
      return s.error(e,"House native geometry/body replacement snapshots disagree");
    at->fields|=8;at->layer=body.layer;at->mask=body.mask;
  }
  if(!s.in.space->replace_house_world(*data(),s.in.sources->geometry(),*old,id,*s.tree,updates,e)||
    !s.in.map->replace_house_tiles(*data(),*old,id,*s.tree,e))return s.error(e,e);
  for(const auto&d:s.in.sources->tree().records())if(!d.script.empty()){
    const auto object=s.tree->source_object(d.id);const auto*n=s.tree->state(object);
    const auto&g=s.in.sources->geometry();
    for(uint32_t j=0;j<g.node_count();++j)if(g.node(j).stable_id==d.id)
      if(!n||!n->bound||!s.in.space->bind_script(d.id,d.script_sha,n->binding.family,n->binding.capability,e))return s.error(e,e);
  }
  if(!s.geometry.bind_replaced_space(*old,id,s.tree->root(),e)||
    !s.kinematic.bind_replaced_space(*old,id,s.tree->root(),e)||
    !s.rays.bind_replaced_space(*old,id,s.tree->root(),e)||
    !s.native.bind_replaced_house(*old,id,s.tree->root(),e))return s.error(e,e);
  s.replaced=true;e.clear();return true;
}
bool HouseReturnTarget::attach_root(const PodunkDoorScene&scene,std::string&e){
  auto&s=*state_;if(s.poisoned||!s.factory||!s.replaced||s.attached||scene.tree!=s.tree||
    scene.root!=s.tree->root()||scene.player_parent!=s.player_parent||!same(scene.identity,s.in.sources->tree().identity()))
    return fail(e,"House Target root attachment precedes actual full factory/replacement");
  HouseReturnDialogueNativeInput ni{s.in.session,&s.driver,s.in.house,s.tree,s.in.text,
    data(),&s.in.sources->tree(),s.in.doors};
  HouseReturnDialogueInput di;
  if(!s.in.retained_scripts->commit_old_scene_retirement(s.old_retirement,*s.tree,scene.root,e)||
    !s.dialogue_native.prepare_staged(ni,e)||
    !s.dialogue_native.staged_driver_input(di,e)||
    !s.driver.bind_staged(std::move(di),s.dialogue_native,s.in.old_tree,s.old_root,
      *s.in.player,e))return s.error(e,e);
  HouseReturnCameraEffectsInput ci;
  ci.sources=s.in.sources;ci.data=&s.in.sources->camera_control();
  ci.audio_data=&s.in.sources->scene_audio();ci.tree=s.tree.get();ci.registry=s.r;
  ci.controls=&s.controls;ci.source=&s.shaker;ci.audio=s.in.retained_audio;
  ci.audio_player=s.in.continuation->audio();ci.ui=s.in.continuation->ui();
  ci.global=s.in.global;ci.globaldata=&s.in.characters->runtime();
  ci.random=s.in.continuation->random();ci.input=&s.joy_input;
  ci.dialogue=&s.driver;ci.world=&s.in.house->world;
  if(!s.shaker_effects.prepare(ci,e))return s.error(e,e);
  s.effects_prepared=true;
  if(!s.r->attach_scene(scene.root,e)||s.tree->lifecycle_pending())return s.error(e,e);
  const auto*n=s.tree->state(scene.root);
  const auto receipt=s.objects.find(scene.root);
  if(!n||!n->inside||!n->ready_notified||receipt==s.objects.end()||
    !receipt->second.source_ready||!receipt->second.native_ready)
    return s.error(e,"House Target real root Enter/Ready did not finish");
  if(!source_factory_ready(e))return s.error(e,e);
  if(!s.in.house->finish_scene_ready())return s.error(e,"House actual source Ready adapter or scene boundary failed");
  if(!s.driver.finish_ready(e)||!s.in.house->house.bind_programme_owner(s.driver,e)||
    !s.dialogue_native.bind_programme(e)||!s.inventory.bind(e)||
    !s.dialogue_native.bind_inventory_source(s.inventory,e)||!s.dialogue_native.bind_npc_source(s.npcs,e))return s.error(e,e);
  if(!s.shaker_effects.bind_world(e))return s.error(e,e);
  s.effects_bound=true;s.attached=true;e.clear();return true;
}
bool HouseReturnTarget::attach_player(const PodunkDoorScene&scene,PodunkPlayerHost&player,std::string&e){
  auto&s=*state_;if(s.poisoned||!s.attached||s.player_attached||&player!=s.in.player||scene.tree!=s.tree||
    scene.root!=s.tree->root()||scene.player_parent!=s.player_parent||!player.ready_complete())
    return fail(e,"House Target transfer requires the same completed retained Player");
  const auto id=player.body().object();const auto*n=s.in.old_tree->state(id);
  if(!n||n->inside||n->parent||n->ready_first||s.r->tree_owner(id)!=s.in.old_tree)
    return fail(e,"House Target Player is not its actual detached retained subtree");
  if(!s.r->persistent_reparent_before_enter(id,s.player_parent,
    [&s,id](auto&old,auto&next,const auto&nodes,auto&e){
      if(&old!=s.in.old_tree.get()||&next!=s.tree.get()||nodes.empty()||nodes.front()!=id)
        return fail(e,"House Target Player transfer receipt changed actual trees/subtree");
      for(auto object:nodes)if(!s.in.player->owns(object))
        return fail(e,"House Target Player transfer contains a foreign source object");
      auto resolver=[&s](uint32_t stable,FieldObjectId&object,std::string&e){
        object=s.tree->source_object(stable);
        if(!object||s.r->tree_owner(object)!=s.tree)return fail(e,"House Target World resolver has no actual source object");
        e.clear();return true;
      };
      // Registry ownership and complete source subtree migration have occurred;
      // every existing native/script borrower rebinds BEFORE source add_child.
      return s.in.world->rebind_tree(next,resolver,e)&&s.in.camera->rebind_tree(next,e)&&
        s.in.player->rebind_tree(s.tree,e)&&
        s.in.retained_player_services->rebind_house(s.player_scene_owner,old,next,nodes,e);
    },e))return s.error(e,e);
  const auto*after=s.tree->state(id);
  if(!after||!after->inside||after->parent!=s.player_parent||after->ready_first||
    s.in.old_tree->state(id)||s.r->tree_owner(id)!=s.tree||!player.ready_complete())
    return s.error(e,"House Target same Player source transfer/retained Ready proof failed");
  s.player_attached=true;e.clear();return true;
}
bool HouseReturnTarget::reparent_persistent(const PodunkDoorScene&scene,FieldObjectId id,std::string&e){
  auto&s=*state_;if(s.poisoned||!s.player_attached||scene.tree!=s.tree||scene.player_parent!=s.player_parent||
    !id||!s.r->object_exists(id))return fail(e,"House Target persistent reparent has no actual ordered receiver");
  if(id!=s.held_door.door()||s.held_door.old_tree()!=s.in.old_tree.get())
    return fail(e,"House Target persistent object is not its real retained Door receipt");
  if(!s.r->persistent_reparent_before_enter(id,s.player_parent,
    [&s,id](auto&old,auto&next,const auto&nodes,auto&e){
      if(&old!=s.in.old_tree.get()||&next!=s.tree.get())return fail(e,"House Door actual migration trees changed");
      return s.in.retained_native->rebind_persistent_door(s.held_door,old,next,nodes,e)&&
        s.in.retained_scripts->rebind_persistent_door(*s.in.doors,
          *s.in.retained_door->source_runtime(),id,old,next,nodes,e)&&
        s.in.retained_audio->rebind_voices(next,nodes,e)&&
        s.in.retained_door->rebind_persistent_door(old,next,nodes,e);
    },e)||!s.in.retained_native->observe_persistent_door(s.held_door,s.player_parent,e))return s.error(e,e);
  e.clear();return true;
}
PodunkPlayerSceneServices*HouseReturnTarget::player_services(){
  return state_->owners&&!state_->poisoned?state_->in.retained_player_services:nullptr;
}
bool HouseReturnTarget::update_key_indicator(std::string&e){
  auto&s=*state_;
  if(s.poisoned||!s.attached||!s.player_attached)return fail(e,"House key indicator precedes actual Root/Player Enter");
  return s.player_scene_owner.update_key_indicator(e);
}
bool HouseReturnTarget::make_player_camera_current(FieldObjectId id,std::string&e){
  auto&s=*state_;if(s.poisoned||!s.player_attached||id!=s.in.player->body().object()||
    s.in.camera->tree()!=s.tree.get()||s.in.camera->registry()!=s.r||!s.in.camera->native_ready())
    return fail(e,"House Target camera selection has no actual retained Player Camera owner");
  return s.in.camera->native_select(true,e)&&s.tree->set_visible(s.in.camera->object(),true,e);
}
bool HouseReturnTarget::activate_after_player(std::string&e){
  auto&s=*state_;if(!s.attached||!s.player_attached||s.active||s.poisoned||s.tree->lifecycle_pending())
    return fail(e,"House Target activation precedes actual source/Player transfer");
  if(!s.npcs.activate_geometry(e)||!s.kinematic.activate_sources(e)||
    !s.geometry.activate_monitors(e)||!s.ladder.bind_geometry(e)||
    !s.tree->flush_transform_notifications(e)||!s.visibility.update_world(0,e))return s.error(e,e);
  s.active=true;e.clear();return true;
}
bool HouseReturnTarget::begin_frame(uint64_t epoch,float idle,float physics,bool paused,bool pending,std::string&e){
  auto&s=*state_;if(!ready()||!epoch||epoch<=s.frame||!std::isfinite(idle)||idle<0||
    !std::isfinite(physics)||physics<0)return fail(e,"House Target frame boundary lacks actual active owners");
  s.frame=epoch;s.idle=idle;s.physics=physics;s.paused=paused;s.pending=pending;
  return s.in.retained_audio->tree_pause(paused,e)&&
    s.in.player->begin_frame(epoch,idle,physics,paused,pending,e);
}
bool HouseReturnTarget::source_factory_ready(std::string&e)const{
  const auto&s=*state_;
  if(!s.factory||!s.replaced||!s.old_freed||s.poisoned||!s.gaps.empty()||
    s.tree->lifecycle_pending()||s.r->current_scene()!=s.tree->root()||
    s.r->tree_owner(s.tree->root())!=s.tree)
    return fail(e,"House source Ready validator lacks the actual complete factory/current-scene boundary");
  for(const auto&source:s.in.sources->tree().records()){
    const auto object=s.tree->source_object(source.id);
    if(!object||!s.objects.count(object))
      return fail(e,"House source Ready validator lost an actual initial source record");
  }
  for(const auto&v:s.objects){
    const auto*n=s.tree->state(v.first);const auto*d=s.tree->descriptor(v.first);
    const auto*source=d?s.in.sources->tree().record(d->id):nullptr;FieldIdentity identity;
    if(!n||!n->alive||!n->inside||!n->bound||!n->ready_notified||n->ready_first||
      !d||!source||source->path!=d->path||source->script!=d->script||
      source->script_sha!=d->script_sha||!s.tree->object_identity(v.first,identity)||
      !same(identity,s.in.sources->tree().identity())||s.r->tree_owner(v.first)!=s.tree||
      !v.second.constructed||!v.second.source_constructed||!v.second.bound||
      !v.second.source_ready||!v.second.native_ready)
      return fail(e,"House source Ready validator found an uncompleted actual native/source receiver");
  }
  e.clear();return true;
}
bool HouseReturnTarget::begin_physics(uint64_t epoch,float delta,std::string&e){
  auto&s=*state_;
  if(!ready()||!epoch||!std::isfinite(delta)||delta<0)
    return fail(e,"House native physics kickoff lacks the real active House epoch");
  return s.kinematic.begin_physics(epoch,delta,e);
}
bool HouseReturnTarget::deferred(const FieldDeferredMessage&m,std::string&e){return state_->deferred(m,e);}
bool HouseReturnTarget::declaration(FieldObjectId id,std::string_view name,uint32_t&argc,std::string&e)const{
  const auto&s=*state_;
  if(!s.r||s.r->tree_owner(id)!=s.tree||!s.r->object_exists(id))return fail(e,"House Target signal belongs to a different actual tree");
  if(s.in.player->owns(id))return s.in.player->declaration(id,name,argc,e);
  if(name=="tree_entered"||name=="tree_exiting"||name=="tree_exited"||name=="ready"||
     name=="visibility_changed"){argc=0;e.clear();return true;}
  if(name=="child_entered_tree"||name=="child_exiting_tree"){argc=1;e.clear();return true;}
  if(std::find(s.held_door.objects().begin(),s.held_door.objects().end(),id)!=s.held_door.objects().end()){
    if(s.in.retained_audio->owns(id))return s.in.retained_audio->signal_declaration(id,name,argc,e);
    return s.in.retained_door->declaration(id,name,argc,e);
  }
  const auto*d=s.tree->descriptor(id);
  if(d&&s.scripts.mapped_source(d->id)==HouseReturnScripts::Role::AreaRoom){
    if(name==data()->area_left_signal()){argc=data()->area_left_arguments();e.clear();return true;}
    if(name==data()->ready().switch_signal){argc=3;e.clear();return true;}
  }
  if(s.in.retained_audio->owns(id))return s.in.retained_audio->signal_declaration(id,name,argc,e);
  if(s.geometry.owns(id))return s.geometry.declaration(id,name,argc,e);
  if(s.prompts.owns(id))return s.prompts.declaration(id,name,argc,e);
  if(s.visibility.owns(id))return s.visibility.signal_declaration(id,name,argc,e);
  if(s.timers.owns(id)&&name=="timeout"){argc=0;e.clear();return true;}
  if(s.controls.owns(id)&&s.gui.declaration(id,name,argc,e))return true;
  if(s.native.owns(id))return s.native.house_signal_declaration(id,name,argc,e);
  return fail(e,"House Target unknown actual signal declaration");
}
bool HouseReturnTarget::collect_deleted(std::string&e){
  auto&s=*state_;if(!s.r||s.tree->lifecycle_pending())return fail(e,"House Target collection precedes actual deletion/lifecycle flush");
  s.r->collect_dead_tree_objects();
  if(s.held_door.door()&&!s.in.retained_scripts->collect_deleted(e))return s.error(e,e);
  for(auto object:s.held_deleted){
    if(s.tree->state(object)||s.r->object_exists(object)||!s.signals->release(object,e))
      return s.error(e,e.empty()?"House original Door signal release preceded actual erase":e);
  }
  s.held_deleted.clear();
  for(const auto&v:s.deleted){const auto id=v.object;
    if(s.tree->state(id)||s.r->object_exists(id))return fail(e,"House Target post-delete object still belongs to ObjectDB");
    if((v.record.source==State::Source::Room&&!s.scripts.release_deleted(id,e))||
      (v.record.source==State::Source::Ladder&&!s.ladder.release_deleted(e))||
      (v.record.source==State::Source::Tint&&!s.tint.release_deleted(id,e))||
      (v.record.source==State::Source::Interact&&!s.interact.release_deleted(id,e))||
      (v.record.source==State::Source::Prompt&&!s.prompt_materials.release_deleted(id,e))||
      (v.record.native==State::Native::Geometry&&!s.geometry.release_deleted(id,e))||
      (v.record.native==State::Native::Kinematic&&!s.kinematic.release_deleted(id,e))||
      (v.record.native==State::Native::Ray&&!s.rays.release_deleted(id,e))||
      (v.record.control&&!s.controls.release_deleted(id,e))||!s.signals->release(id,e))return s.error(e,e);
  }
  // Control Deleting has already released the actual native receiver. The
  // source body keeps its deletion receipt until Timer/Audio erasure is proven.
  for(const auto&v:s.deleted)if(v.record.source==State::Source::Shaker){
    if(!s.shaker.release_deleted(v.object,e))return s.error(e,e);
    s.shaker_deleted=true;
  }
  s.deleted.clear();
  if(s.audio_prepared&&!s.audio_retired&&s.old_freed&&s.held_door.door()){
    bool door_deleted=true;
    for(auto id:s.held_door.objects())if(s.tree->state(id)||s.in.old_tree->state(id)||
      s.r->object_exists(id)||s.in.retained_audio->owns(id))door_deleted=false;
    if(door_deleted){
      if(!s.in.retained_audio->retire_previous_scene(*s.in.old_tree,e))return s.error(e,e);
      s.audio_retired=true;
    }
  }
  e.clear();return true;
}
bool HouseReturnTarget::finish_idle_tail(std::string&e){
  auto&s=*state_;
  if(!s.attached||s.poisoned||s.tree->lifecycle_pending()||s.in.scene_timers->emitting())
    return fail(e,"House shaker idle-tail collection precedes the actual closed global timer tail");
  return s.shaker.collect_expired(e);
}
bool HouseReturnTarget::unbind_room_shaker(std::string&e){
  auto&s=*state_;
  if(!s.effects_prepared||!s.effects_bound)return fail(e,"House shaker has no actual World binding to release");
  if(!s.shaker_effects.unbind_world(e))return false;
  s.effects_bound=false;e.clear();return true;
}
bool HouseReturnTarget::release_room_shaker_resources(std::string&e){
  auto&s=*state_;
  if(!s.effects_prepared||s.effects_bound||!s.shaker_deleted)
    return fail(e,"House shaker Resource release precedes exact World unbind/source deletion");
  if(!s.shaker_effects.release_stream_after_delete(e)||!s.joy_input.shutdown(e))return false;
  s.joy_prepared=false;e.clear();return true;
}
bool HouseReturnTarget::draw(uint64_t epoch,float delta,float shader_time,std::string&e){
  auto&s=*state_;if(!ready())return fail(e,"House Target cannot draw an unadmitted/unentered House");
  FieldMapGateQuery gates=[&s](uint32_t stable){
    const auto id=s.tree->source_object(stable);const auto*n=s.tree->state(id);
    if(!s.in.sources->tree().record(stable))return FieldMapGateState::Pending;
    if(!n)return FieldMapGateState::Deleted;
    if(!n->alive||!n->bound||!n->inside||!n->ready_notified)return FieldMapGateState::Pending;
    return s.tree->visible_in_tree(id)?FieldMapGateState::Visible:FieldMapGateState::Hidden;
  };
  return s.native.begin_draw(epoch,delta,shader_time,e)&&s.native.draw(gates,e);
}
bool HouseReturnTarget::ready()const{
  const auto&s=*state_;if(!s.active||!s.attached||!s.player_attached||s.poisoned||!s.gaps.empty()||
    s.tree->lifecycle_pending()||!s.in.player->ready_complete())return false;
  for(const auto&entry:s.objects){const auto*n=s.tree->state(entry.first);
    if(!n||!n->inside||!n->bound||!n->ready_notified||!entry.second.native_ready||
      (entry.second.source!=State::Source::None&&!entry.second.source_ready))return false;}
  return true;
}
bool HouseReturnTarget::release(const FieldDoorCandidate&c,bool committed,std::string&e){
  auto&s=*state_;if(!s.candidate_owned(c,e))return false;
  if(committed){
    if(s.poisoned)return fail(e,"House Target poisoned candidate cannot commit");
    if(!ready())return fail(e,"House Target cannot commit a candidate before actual complete execution");
    s.committed=true;s.candidate={};s.prepared=false;e.clear();return true;
  }
  if(s.old_left||s.old_freed||s.attached||s.player_attached||s.tree->lifecycle_pending())
    return fail(e,"House Target cannot discard a candidate after actual old-scene/lifecycle mutation");
  if(s.tree->object_count()&&!s.tree->root())
    return s.error(e,"House Target partial failed factory has no deletable root; retain all actual owners");
  if(s.tree->root()){
    if(!s.tree->queue_free(s.tree->root(),e)||!s.tree->flush_delete_queue(e)||!collect_deleted(e)||
      !s.root->clear_house_gui(s.gui,e)||!s.prompt_materials.shutdown(e)||!s.native.shutdown(e))return s.error(e,e);
  }
  if(s.audio_prepared&&!s.audio_retired){
    if(s.effects_prepared||s.effects_bound||s.shaker.callback_depth()||s.shaker.pending_waiters())
      return s.error(e,"House candidate still owns actual shaker effects/callbacks/waiters");
    if(s.shaker_deleted&&!s.shaker.source_closed(e))return s.error(e,e);
    if(!s.in.retained_audio->cancel_house_prepare(*s.tree,*s.signals,e))return s.error(e,e);
    s.audio_prepared=false;
  }
  if(s.joy_prepared){if(!s.joy_input.shutdown(e))return s.error(e,e);s.joy_prepared=false;}
  s.candidate={};s.prepared=false;e.clear();return true;
}
} // namespace encore::ctr
