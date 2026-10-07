#include "podunk_house_exit.hpp"
#include "podunk_scene_materials.hpp"
#include "podunk_scene_visibility.hpp"
#include "podunk_scene_npc_world.hpp"
#include "podunk_npc_animation.hpp"
#include "podunk_npc_return_timer.hpp"
#include "podunk_butterfly_animation.hpp"
#include "podunk_butterfly_timers.hpp"
#include "podunk_prompt_native.hpp"
#include "podunk_scene_clip_native.hpp"
#include "podunk_scene_leaf_native.hpp"
#include "podunk_scene_cameras.hpp"
#include "podunk_scene_signal_callbacks.hpp"
#include "podunk_audio_server.hpp"
#include "podunk_player_effect_owners.hpp"
#include "field_birds_renderer.hpp"
#include "field_butterfly_renderer.hpp"
#include "podunk_player_scene_services.hpp"
#include "podunk_scene_grass.hpp"
#include "podunk_house_door_continuation.hpp"
#include "podunk_player_preload_scenes.hpp"
#include "podunk_named_sfx.hpp"
#include "podunk_ready_native_bridges.hpp"
#include "podunk_ready_interaction_bridges.hpp"
#include "podunk_ready_transition_bridge.hpp"
#include "podunk_mick_session.hpp"
#include <algorithm>
#include <cmath>
#include <tuple>

namespace encore::ctr {
using namespace upstream;
namespace {
bool reject(std::string &e,const char *s) { e=s; return false; }
auto unsupported(const char *reason) {
 return [reason](auto&&...args)->bool {
   auto refs=std::forward_as_tuple(args...);
   std::get<sizeof...(args)-1>(refs)=reason;
   return false;
 };
}
}
struct PodunkHouseExit::State {
  struct BirdCanvas final : PodunkSceneCanvasLeaf {
    State *owner = nullptr;
    FieldBirdRenderer gpu;
    const FieldNodeTreeRuntime *canvas_tree() const override { return owner->tree.get(); }
    const FieldGlobalRegistry *canvas_registry() const override { return owner->continuation.registry(); }
    bool owns_drawable(FieldObjectId id) const override {
      auto *d=owner->tree->descriptor(id);
      if(!d)return false;
      for(const auto &r:owner->sources.birds().records())if(r.children[0]==d->id)return true;
      return false;
    }
    bool appearance(const FieldCanvasRecord &r,FieldObjectId id,FieldCanvasAppearance &out,std::string &e)const override {
      if(!owns_drawable(id)||r.shader!=FieldCanvasShader::Default)return reject(e,"Bird leaf source Canvas/material differs");
      const auto *d=owner->tree->descriptor(id);
      for(const auto &r:owner->sources.birds().records())if(r.children[0]==d->id){
        auto *body=owner->consumers.runtime_instances().birds->state(r.id);
        if(!body)return reject(e,"Bird source body missing");
        out.action=body->visible?FieldCanvasAction::Delegate:FieldCanvasAction::Hidden;
        e.clear();return true;
      }
      return reject(e,"Bird actual leaf source unavailable");
    }
    bool draw_leaf(const FieldCanvasOrderSlot &slot,const FieldTransform &viewport,bool snap,std::string &e)override {
      auto *d=owner->tree->descriptor(slot.object);
      if(!d||!owns_drawable(slot.object))return reject(e,"Bird draw actual source unavailable");
      for(const auto &r:owner->sources.birds().records())if(r.children[0]==d->id){
        FieldBirdDraw pose;
        auto *runtime=owner->consumers.runtime_instances().birds;
        if(!runtime->draw(r.id,pose)){e=runtime->error();return false;}
        pose.pixel_snap=snap;
        if(!gpu.draw(pose,-viewport[2].x,-viewport[2].y))return reject(e,"Bird GPU actual draw rejected");
        e.clear();return true;
      }
      return reject(e,"Bird draw outside actual checked leaves");
    }
  } bird_canvas;
  struct ButterflyCanvas final : PodunkSceneCanvasLeaf {
    State *owner=nullptr;FieldButterflyRenderer gpu;
    const FieldNodeTreeRuntime*canvas_tree()const override{return owner->tree.get();}
    const FieldGlobalRegistry*canvas_registry()const override{return owner->continuation.registry();}
    bool owns_drawable(FieldObjectId id)const override{
      const auto *d=owner->tree->descriptor(id);const auto *r=d?owner->sources.canvas().record(d->id):nullptr;
      return r&&r->owner==FieldCanvasOwner::Butterfly&&owner->sources.butterfly().binding(r->owner_id);
    }
    bool appearance(const FieldCanvasRecord&r,FieldObjectId id,FieldCanvasAppearance&out,std::string&e)const override{
      if(!owns_drawable(id)||r.shader!=FieldCanvasShader::Default)return reject(e,"Butterfly actual Canvas/material mismatch");
      const auto *state=owner->consumers.runtime_instances().butterfly->state(r.owner_id);
      if(!state)return reject(e,"Butterfly actual source body unavailable");
      out.action=state->visible?FieldCanvasAction::Delegate:FieldCanvasAction::Hidden;e.clear();return true;
    }
    bool draw_leaf(const FieldCanvasOrderSlot&slot,const FieldTransform&view,bool snap,std::string&e)override{
      const auto *d=owner->tree->descriptor(slot.object);const auto*r=d?owner->sources.canvas().record(d->id):nullptr;
      if(!r||!owns_drawable(slot.object)||view[0].x!=1||view[0].y!=0||view[1].x!=0||view[1].y!=1||snap!=owner->sources.butterfly().pixel_snap())return reject(e,"Butterfly source viewport requires translation-only canvas");
      const auto size=owner->continuation.native_root()->viewport().size;
      auto channel=[](float v){return uint32_t(std::floor(std::clamp(v,0.f,1.f)*255.f+.5f));};
      const auto color=channel(slot.color[0])|channel(slot.color[1])<<8|channel(slot.color[2])<<16|channel(slot.color[3])<<24;
      return gpu.draw(*owner->consumers.runtime_instances().butterfly,r->owner_id,
          {size.x/2-view[2].x,size.y/2-view[2].y},size.x,size.y,0,0,
          owner->tree->visible_in_tree(slot.object),color,0,e);
    }
  } butterfly_canvas;
  PodunkHouseExitInput input;
  PodunkHouseContinuation continuation;
  FieldGoodsData goods;
  PodunkProgrammeInventory programme_inventory;
  PodunkProgrammeState programme_state;
  FieldSceneSources sources;
  PodunkPlayerSources player_sources;
  FieldNativeRootData native_root;
  FieldShopData shop;
  std::shared_ptr<FieldNodeTreeRuntime> tree=std::make_shared<FieldNodeTreeRuntime>();
  FieldMapSpace map;
  FieldGeometrySpace geometry;
  PodunkSceneConsumers consumers;
  PodunkSceneLoop loop;
  PodunkSceneOperations operations;
  PodunkSceneSignalCallbacks callbacks;
  PodunkSceneNative native;
  PodunkSceneMaterials materials;
  PodunkSceneVisibility visibility;
  PodunkSceneNpcWorld npc_world;
  PodunkNpcAnimationHost npc_animation;
  PodunkNpcReturnTimers return_timers;
  PodunkButterflyAnimation butterfly_animation;
  PodunkButterflyTimers butterfly_timers;
  PodunkPromptNative prompts;
  PodunkSceneClipNative clips;
  PodunkReadyNativeBridges ready_native;
  PodunkReadyInteractionBridges ready_interaction;
  PodunkReadyTransitionBridge ready_transition;
  PodunkSceneLeafNative leaves;
  PodunkSceneCameras cameras;
  PodunkSceneAudio audio;
  PodunkAudioServer *audio_server=nullptr; // actual Registry owns singleton
  PodunkPlayerHost player;
  PodunkPlayerSceneServices player_services;
  SceneLeafNativeData leaf_data;
  PodunkPlayerPhysicsWorld physics;
  PodunkPlayerCamera player_camera;
  PodunkConcretePlayerEffectOwners effects;
  FieldDoorRuntime door;
  PodunkHouseDoorContinuation old_door;
  PodunkPlayerPreloadScenes preloads;
  PlayerPreloadScenesData preload_data;
  std::shared_ptr<const PlayerNamedSfxData> named_data;
  PodunkConcreteSceneGrassFactory *grass=nullptr;
  std::map<uint32_t,FieldGeometryShape> source_shapes;
  std::map<uint32_t,FieldObjectId> allocated;
  bool tree_paused=false,update_pending=false;
  PodunkSceneMechanismOwners owners;
  FieldCanvasArtHost canvas;
  using Args=std::vector<FieldDeferredValue>;
  using Callback=std::function<bool(const Args&,std::string&)>;
  std::map<std::pair<FieldObjectId,std::string>,Callback> methods;
  std::set<uint32_t> bird_connected,camera_area_connected,music_connected;
  std::vector<std::pair<uint64_t,std::function<bool()>>> camera_idle_waiters;
  uint64_t camera_idle_epoch=0;
  std::string failure;
  bool attempted=false,prepared=false,constructed=false,door_bound=false,
       current_published=false,attached=false,player_attached=false,
       activated=false,ended=false,failed=false,candidate=false;
  uint32_t step=0;
  uint64_t token=0;
  PodunkMickSession mick;
  bool fail(std::string &e) { failed=true; failure=e; return false; }
  bool source_ready_boundary(uint32_t stable,std::string &e) const {
    FieldObjectId id=0;
    if(!source(stable,id,e))return false;
    const auto *n=tree->state(id);const auto *d=tree->descriptor(id);
    if(!n->inside||!n->ready_notified||!n->bound)return reject(e,"Actual source Ready before native lifecycle");
    for(uint32_t i=0;i<sources.lifecycle().ready_count();++i){
      const auto row=sources.lifecycle().ready(i);
      if(row.id==stable){
        if(d->script!=sources.lifecycle().string(row.script)||d->script_sha!=row.sha)
          return reject(e,"Actual source Ready script hash differs");
        e.clear();return true;
      }
    }
    return reject(e,"Actual source Ready is outside checked lifecycle");
  }
  bool connect(uint32_t stable,std::string_view signal,std::string_view method,Callback cb,std::string &e){
    FieldObjectId actual=0;
    if(!source(stable,actual,e)||methods.count({actual,std::string(method)}))
      return reject(e,"Actual source method connection duplicate/unbound");
    if(!continuation.signals()->connect(actual,signal,actual,method,0,{},e))return false;
    methods.emplace(std::make_pair(actual,std::string(method)),std::move(cb));e.clear();return true;
  }
  bool body_arg(const Args &args,uint32_t &out,std::string &e) const {
    if(args.size()!=1||!std::holds_alternative<FieldObjectRef>(args[0]))return reject(e,"Actual source Area body argument rejected");
    auto id=std::get<FieldObjectRef>(args[0]).id;
    if(!id||id>UINT32_MAX||!const_cast<PodunkHouseContinuation&>(continuation).registry()->object_exists(id))
      return reject(e,"Actual Area ObjectID cannot be represented by source adapter");
    out=uint32_t(id);e.clear();return true;
  }
  bool player_boolean(PlayerMotionField field,bool &out,std::string &e) const {
    const auto *n=tree->state(const_cast<PodunkPlayerHost&>(player).body().object());
    PlayerInitializationMember value;
    if(!player_sources.motion||!const_cast<PodunkPlayerHost&>(player).ready_complete()||
       !n||!n->alive||!n->inside||!n->bound||
       const_cast<PodunkHouseContinuation&>(continuation).registry()->tree_owner(const_cast<PodunkPlayerHost&>(player).body().object())!=tree||
       !const_cast<PodunkPlayerHost&>(player).body().member(player_sources.motion->field(field),value,e))
      return reject(e,"Actual player source bool owner/lifecycle unavailable");
    if(value.kind!=1||!value.value||value.value->kind!=1)return reject(e,"Actual player source bool type rejected");
    out=value.value->boolean;e.clear();return true;
  }
  bool current_camera(FieldGameCameraRuntime *&out,uint32_t &stable,std::string &e) {
    FieldObjectId actual=0;
    if(!continuation.global()->core().object(FieldGlobalMemberRole::CurrentCamera,actual,e)||
       !actual||!continuation.registry()->object_exists(actual)||continuation.registry()->tree_owner(actual)!=tree)
      return reject(e,"CameraArea actual global.currentCamera owner is unavailable");
    const auto *d=tree->descriptor(actual);const auto *n=tree->state(actual);
    if(!d||!n||!n->inside||!n->bound)return reject(e,"CameraArea actual camera is not entered");
    if(player_camera.owns(actual))out=&player.children().camera();
    else if(cameras.owns(actual))out=consumers.runtime_instances().camera;
    else return reject(e,"CameraArea actual current Camera has no concrete owner");
    stable=d->id;
    const auto *state=out?out->state(stable):nullptr;
    if(!state||!state->alive||!state->ready)return reject(e,"CameraArea actual current script camera is not Ready");
    e.clear();return true;
  }
  bool shapes_ready(uint32_t stable,const std::vector<uint32_t> &shapes,std::string &e)const{
    if(!source_ready_boundary(stable,e))return false;
    for(auto source_id:shapes){FieldObjectId actual=0;
      if(!source(source_id,actual,e)||!native.owns(actual)||!tree->state(actual)->inside||
         !tree->state(actual)->bound||!tree->state(actual)->ready_notified||!source_shapes.count(source_id))
        return reject(e,"Source Area actual child shape is not bound/entered/Ready");
    }
    e.clear();return true;
  }
  template<class Data,class... Args>
  bool load(PodunkPackRole role,Data &out,std::string &e,const Args&... args) {
    std::vector<uint8_t> bytes;
    return input.continuation.destination->bundle().read(role,input.continuation.romfs_root,bytes,e)
      && out.load(bytes.data(),bytes.size(),args...,e);
  }
  template<class Data,class... Args>
  bool load_shared(PodunkPackRole role,std::shared_ptr<const Data> &out,
                   std::string &e,const Args&...args) {
    auto next=std::make_shared<Data>();
    if(!load(role,*next,e,args...))return false;
    out=std::move(next);return true;
  }
  bool source(uint32_t stable,FieldObjectId &id,std::string &e) const {
    id=tree->source_object(stable);
    if(!id){auto i=allocated.find(stable);if(i!=allocated.end())id=i->second;}
    auto *n=tree->state(id);auto *d=tree->descriptor(id);
    if(!id||!n||!n->alive||n->queued||!d||d->id!=stable||
       const_cast<PodunkHouseContinuation&>(continuation).registry()->tree_owner(id)!=tree)
      return reject(e,"House exit actual destination source object unavailable");
    e.clear();return true;
  }
  bool npc_seen_identity(uint32_t stable,const FieldNpcDialogue &row,
                         std::string &key,std::string &e) const {
    const FieldNpcDescriptor *binding=nullptr;
    for(const auto &npc:sources.npc().npcs())if(npc.id==stable)binding=&npc;
    if(!binding)return reject(e,"NPC seen source binding is absent");
    auto match=std::find_if(binding->dialogues.begin(),binding->dialogues.end(),
      [&](const auto &actual){return actual.thoughts==row.thoughts&&actual.last==row.last&&
        actual.group==row.group&&actual.ordinal==row.ordinal&&actual.flag==row.flag&&
        actual.program==row.program&&actual.source==row.source;});
    if(match==binding->dialogues.end())return reject(e,"NPC seen source row differs");
    FieldObjectId actual=0;
    if(!source(stable,actual,e))return false;
    const auto *n=tree->state(actual);const auto *d=tree->descriptor(actual);
    if(!n||!d||!n->inside||!n->ready_notified||!n->bound||
       d->path!=binding->node||d->ready!=binding->ready_ordinal)
      return reject(e,"NPC seen read before actual source Ready cursor");
    std::string path;
    if(!const_cast<PodunkHouseContinuation&>(continuation).registry()->get_path(actual,path,e))return false;
    key=path+":"+row.flag+":"+std::to_string(row.ordinal)+":"+row.program;
    e.clear();return true;
  }
};
PodunkHouseExit::PodunkHouseExit():state_(std::make_unique<State>()){}
PodunkHouseExit::~PodunkHouseExit()=default;
bool PodunkHouseExit::prepare(PodunkHouseExitInput in,std::string &e) {
  auto &s=*state_;
  if(s.attempted||!in.continuation.house||
     in.continuation.house->house.phase()!=HousePhase::Idle||!in.font||
     !in.programme||!in.programme->valid()||!in.psi||!in.psi->valid()||
     !in.basement||!in.basement->valid()||!in.choices||!in.choices->valid()||!in.choice_runtime||
     !in.printer||!in.mutable_session||
     in.mutable_session!=in.continuation.snapshot||!in.house_renderer||
     !in.text_renderer||!in.locale||!in.locale->catalog()||
     !in.inventory_audio_bank||!in.inventory_audio_bank->count()||!in.bars||!in.fade||
     !in.scene_epoch||!in.music||
     !std::isfinite(in.geometry_grid_size)||in.geometry_grid_size<=0)
    return reject(e,"House exit requires Idle live session and checked native owners");
  s.attempted=true;s.input=std::move(in);
  // This callback is used only by actual later flag mutation. Preparation
  // neither emits it nor substitutes a missing receiver with success.
  s.input.continuation.flags_updated=[&s](auto &error){return s.callbacks.emit_flags(error);};
  if(!s.continuation.initialize(s.input.continuation,e)||
     !s.sources.load(s.input.continuation.destination->bundle(),s.input.continuation.romfs_root,e))
    return s.fail(e);
  auto &r=*s.continuation.registry();auto &root=*s.continuation.native_root();
  auto &global=s.continuation.global()->core();auto &bus=*s.continuation.signals();
  if(!s.load(PodunkPackRole::Goods,s.goods,e)||
     !s.programme_inventory.prepare({&s.continuation,&s.goods,
        s.input.inventory_audio_bank,s.input.locale,s.input.continuation.clock},e))return s.fail(e);
  auto &ps=s.player_sources;
  ps.initialization=s.continuation.player_initialization_owner();
  ps.ready=s.continuation.player_ready_owner();
  if(!ps.initialization||!ps.ready||
     !s.load_shared(PodunkPackRole::PlayerMotion,ps.motion,e,*ps.initialization,*ps.ready)||
     !s.load_shared(PodunkPackRole::PlayerVisualScripts,ps.visual,e,*ps.initialization)||
     !s.load_shared(PodunkPackRole::PlayerGraphics,ps.graphics,e,*ps.visual)||
     !s.load_shared(PodunkPackRole::PlayerFetcher,ps.fetchers,e,*ps.initialization,s.sources.tree(),*global.data())||
     !s.load_shared(PodunkPackRole::PlayerChildScripts,ps.children,e,*ps.initialization,*ps.ready)||
     !s.load_shared(PodunkPackRole::PlayerEffects,ps.effects,e,*ps.initialization))return s.fail(e);
  auto resource=std::make_shared<PlayerResourcesData>();
  if(!s.load(PodunkPackRole::PlayerResources,*resource,e,*ps.initialization,*ps.graphics)||
     !resource->bind_effects(*ps.effects,e)||
     !s.load(PodunkPackRole::NativeRoot,s.native_root,e,*r.data())||
     !s.load(PodunkPackRole::Shop,s.shop,e)||
     !s.map.configure(s.sources.map(),s.sources.actions(),e)||
     !s.geometry.configure(s.sources.geometry(),s.input.geometry_grid_size,e))return s.fail(e);
  ps.resources=std::move(resource);
  for(uint32_t i=0;i<s.sources.geometry().shape_count();++i){auto shape=s.sources.geometry().shape(i);auto node=s.sources.geometry().node(shape.node);s.source_shapes.emplace(node.stable_id,shape);}
  PodunkSceneConsumerInput ci{&s.sources,&s.continuation,s.tree.get(),&s.native,
      &s.player,&s.loop.animated(),&s.map,&s.geometry,&s.shop,s.input.scene_epoch};
  if(!s.consumers.prepare(ci,e))return s.fail(e);
  auto cores=s.consumers.runtime_instances();
  if(!PodunkAudioServer::create(s.sources.audio_server(),r,bus,*s.continuation.audio(),s.audio_server,e)||
     !s.load(PodunkPackRole::PlayerPreloadScenes,s.preload_data,e,*ps.initialization)||
     !s.preloads.prepare(s.preload_data,*ps.initialization,r,e)||
     !s.load_shared(PodunkPackRole::NamedSfx,s.named_data,e,*r.data())||
     !s.continuation.bind_named_sfx(s.named_data,*s.audio_server,*s.input.music,e)||
     !s.materials.prepare(s.sources.materials(),s.sources.canvas(),*s.tree,r,
        s.sources.prompt(),*cores.prompt,s.sources.melody(),*cores.melody,
        s.input.continuation.romfs_root.c_str(),
        [&s](const auto &draw,auto camera,float w,float h,auto &error){
          return s.native.draw_default(draw,camera,w,h,error);},e))return s.fail(e);
  s.canvas.bind_owner=[&s,&r](const auto &record,auto id,auto owner,auto &error){
    auto *n=s.tree->state(id);auto *d=s.tree->descriptor(id);auto *p=s.tree->descriptor(owner);
    if(!n||!n->alive||n->queued||!d||!p||d->id!=record.id||p->id!=record.owner_id||
       r.tree_owner(id)!=s.tree||r.tree_owner(owner)!=s.tree)
      return reject(error,"House exit Canvas typed actual object binding rejected");
    error.clear();return true;
  };
  s.canvas.appearance=[&s](const auto &record,auto id,auto owner,auto &out,auto &error){
    return s.consumers.sprite_appearance(record,id,owner,out,error);
  };
  PodunkSceneLoopInput preload;
  preload.sources=&s.sources;preload.continuation=&s.continuation;preload.tree=s.tree;
  preload.native=&s.native;preload.map=&s.map;preload.geometry=&s.geometry;
  preload.asset_root=s.input.continuation.romfs_root;preload.canvas=s.canvas;
  preload.materials=&s.materials;
  if(!s.loop.prepare_native(preload,e))return s.fail(e);
  // Missing future operations fail when their source actually calls them.
  s.owners.npc.context=unsupported("Outdoor npc.context actual operation is not implemented");
  s.owners.npc.flag=unsupported("Outdoor npc.flag actual operation is not implemented");
  s.owners.npc.seen=[&s](uint32_t source,const auto &row,bool &out,auto &error){
    std::string key;
    return s.npc_seen_identity(source,row,key,error)&&
      s.continuation.characters()->flags().seen(key,out,error);
  };
  s.owners.npc.mark_seen=unsupported("Outdoor npc.mark_seen actual operation is not implemented");
  s.owners.npc.admit_program=unsupported("Outdoor npc.admit_program actual operation is not implemented");
  s.owners.npc.open_program=unsupported("Outdoor npc.open_program actual operation is not implemented");
  s.owners.npc.telepathy_effect=unsupported("Outdoor npc.telepathy_effect actual operation is not implemented");
  s.owners.npc.begin_talker=unsupported("Outdoor npc.begin_talker actual operation is not implemented");
  s.owners.npc.close_commands=unsupported("Outdoor npc.close_commands actual operation is not implemented");
  s.owners.npc.cached_raycast=unsupported("Outdoor npc.cached_raycast actual operation is not implemented");
  s.owners.npc.move_and_slide=unsupported("Outdoor npc.move_and_slide actual operation is not implemented");
  s.owners.npc.present=unsupported("Outdoor npc.present actual operation is not implemented");
  s.owners.npc.timer=unsupported("Outdoor npc.timer actual operation is not implemented");
  s.owners.enemy.context=unsupported("Outdoor enemy.context actual operation is not implemented");
  s.owners.enemy.create=unsupported("Outdoor enemy.create actual operation is not implemented");
  s.owners.enemy.wander_raycast=unsupported("Outdoor enemy.wander_raycast actual operation is not implemented");
  s.owners.enemy.move_and_slide=unsupported("Outdoor enemy.move_and_slide actual operation is not implemented");
  s.owners.enemy.roster=unsupported("Outdoor enemy.roster actual operation is not implemented");
  s.owners.enemy.battle=unsupported("Outdoor enemy.battle actual operation is not implemented");
  s.owners.enemy.present=unsupported("Outdoor enemy.present actual operation is not implemented");
  s.owners.enemy.animation=unsupported("Outdoor enemy.animation actual operation is not implemented");
  s.owners.enemy.queue_battle=unsupported("Outdoor enemy.queue_battle actual operation is not implemented");
  s.owners.enemy.damage_number=unsupported("Outdoor enemy.damage_number actual operation is not implemented");
  s.owners.enemy.reward=unsupported("Outdoor enemy.reward actual operation is not implemented");
  s.owners.tint.resolve=unsupported("Outdoor tint.resolve actual operation is not implemented");
  s.owners.tint.self_modulate=unsupported("Outdoor tint.self_modulate actual operation is not implemented");
  s.owners.sprite.create_tree=unsupported("Outdoor sprite.create_tree actual operation is not implemented");
  s.owners.sprite.rebuild_tree=unsupported("Outdoor sprite.rebuild_tree actual operation is not implemented");
  s.owners.sprite.publish=unsupported("Outdoor sprite.publish actual operation is not implemented");
  s.owners.sprite.sprite_changed=unsupported("Outdoor sprite.sprite_changed actual operation is not implemented");
  s.owners.sprite.travel=unsupported("Outdoor sprite.travel actual operation is not implemented");
  s.owners.sprite.blend=unsupported("Outdoor sprite.blend actual operation is not implemented");
  s.owners.sprite.time_scale=unsupported("Outdoor sprite.time_scale actual operation is not implemented");
  s.owners.sprite.resolve_sprite=unsupported("Outdoor sprite.resolve_sprite actual operation is not implemented");
  s.owners.sprite.current_scene=unsupported("Outdoor sprite.current_scene actual operation is not implemented");
  s.owners.sprite.sample=unsupported("Outdoor sprite.sample actual operation is not implemented");
  s.owners.sprite.reflection_create=unsupported("Outdoor sprite.reflection_create actual operation is not implemented");
  s.owners.sprite.reflection_add_child=unsupported("Outdoor sprite.reflection_add_child actual operation is not implemented");
  s.owners.sprite.reflection_valid=unsupported("Outdoor sprite.reflection_valid actual operation is not implemented");
  s.owners.sprite.reflection_queue_free=unsupported("Outdoor sprite.reflection_queue_free actual operation is not implemented");
  s.owners.emote.resolve_object=unsupported("Outdoor emote.resolve_object actual operation is not implemented");
  s.owners.emote.resolve_direction=unsupported("Outdoor emote.resolve_direction actual operation is not implemented");
  s.owners.emote.texture_geometry=unsupported("Outdoor emote.texture_geometry actual operation is not implemented");
  s.owners.emote.direction=unsupported("Outdoor emote.direction actual operation is not implemented");
  s.owners.emote.publish=unsupported("Outdoor emote.publish actual operation is not implemented");
  s.owners.emote.sound_stream=unsupported("Outdoor emote.sound_stream actual operation is not implemented");
  s.owners.emote.sound_playing=unsupported("Outdoor emote.sound_playing actual operation is not implemented");
  s.owners.emote.animation_finished=unsupported("Outdoor emote.animation_finished actual operation is not implemented");
  s.owners.dandelion.queue_source_sprite=unsupported("Outdoor dandelion.queue_source_sprite actual operation is not implemented");
  s.owners.dandelion.instance=unsupported("Outdoor dandelion.instance actual operation is not implemented");
  s.owners.dandelion.add_child=unsupported("Outdoor dandelion.add_child actual operation is not implemented");
  s.owners.dandelion.abort_unparented=unsupported("Outdoor dandelion.abort_unparented actual operation is not implemented");
  s.owners.dandelion.sprite_frame=unsupported("Outdoor dandelion.sprite_frame actual operation is not implemented");
  s.owners.dandelion.particle_backend_admitted=unsupported("Outdoor dandelion.particle_backend_admitted actual operation is not implemented");
  s.owners.dandelion.emit_particles=unsupported("Outdoor dandelion.emit_particles actual operation is not implemented");
  s.owners.dandelion.player_direction=unsupported("Outdoor dandelion.player_direction actual operation is not implemented");
  s.owners.door.observe=unsupported("Outdoor door.observe actual operation is not implemented");
  s.owners.door.resolve_onready=unsupported("Outdoor door.resolve_onready actual operation is not implemented");
  s.owners.door.connect_body=unsupported("Outdoor door.connect_body actual operation is not implemented");
  s.owners.door.prepare_destination=unsupported("Outdoor door.prepare_destination actual operation is not implemented");
  s.owners.door.release_candidate=unsupported("Outdoor door.release_candidate actual operation is not implemented");
  s.owners.door.pause_player=unsupported("Outdoor door.pause_player actual operation is not implemented");
  s.owners.door.set_entering=unsupported("Outdoor door.set_entering actual operation is not implemented");
  s.owners.door.flag_exists=unsupported("Outdoor door.flag_exists actual operation is not implemented");
  s.owners.door.write_flag=unsupported("Outdoor door.write_flag actual operation is not implemented");
  s.owners.door.music_changers=unsupported("Outdoor door.music_changers actual operation is not implemented");
  s.owners.door.stop_music=unsupported("Outdoor door.stop_music actual operation is not implemented");
  s.owners.door.play_audio=unsupported("Outdoor door.play_audio actual operation is not implemented");
  s.owners.door.fade=unsupported("Outdoor door.fade actual operation is not implemented");
  s.owners.door.emit=unsupported("Outdoor door.emit actual operation is not implemented");
  s.owners.door.marker_world=unsupported("Outdoor door.marker_world actual operation is not implemented");
  s.owners.door.set_player_global_position=unsupported("Outdoor door.set_player_global_position actual operation is not implemented");
  s.owners.door.persistent=unsupported("Outdoor door.persistent actual operation is not implemented");
  s.owners.door.schedule_deferred=unsupported("Outdoor door.schedule_deferred actual operation is not implemented");
  s.owners.door.clear_enemies=unsupported("Outdoor door.clear_enemies actual operation is not implemented");
  s.owners.door.scene_step=unsupported("Outdoor door.scene_step actual operation is not implemented");
  s.owners.door.emit_scene_changed=unsupported("Outdoor door.emit_scene_changed actual operation is not implemented");
  s.owners.door.camera_current_and_visible=unsupported("Outdoor door.camera_current_and_visible actual operation is not implemented");
  s.owners.door.fade_cut=unsupported("Outdoor door.fade_cut actual operation is not implemented");
  s.owners.door.direction_and_input=unsupported("Outdoor door.direction_and_input actual operation is not implemented");
  s.owners.door.update_party=unsupported("Outdoor door.update_party actual operation is not implemented");
  s.owners.door.unpause_player=unsupported("Outdoor door.unpause_player actual operation is not implemented");
  s.owners.door.queue_free=unsupported("Outdoor door.queue_free actual operation is not implemented");
  s.owners.door.set_respawn=unsupported("Outdoor door.set_respawn actual operation is not implemented");
  s.owners.prompt.observe=unsupported("Outdoor prompt.observe actual operation is not implemented");
  s.owners.prompt.key_name=unsupported("Outdoor prompt.key_name actual operation is not implemented");
  s.owners.prompt.connect=unsupported("Outdoor prompt.connect actual operation is not implemented");
  s.owners.prompt.publish=unsupported("Outdoor prompt.publish actual operation is not implemented");
  s.owners.prompt.visibility=unsupported("Outdoor prompt.visibility actual operation is not implemented");
  s.owners.prompt.hide_signal=unsupported("Outdoor prompt.hide_signal actual operation is not implemented");
  s.owners.prompt.native_animation=unsupported("Outdoor prompt.native_animation actual operation is not implemented");
  s.owners.bush.bind=unsupported("Outdoor bush.bind actual operation is not implemented");
  s.owners.bush.resolve_node=unsupported("Outdoor bush.resolve_node actual operation is not implemented");
  s.owners.bush.read_flag=unsupported("Outdoor bush.read_flag actual operation is not implemented");
  s.owners.bush.connect_viewport=unsupported("Outdoor bush.connect_viewport actual operation is not implemented");
  s.owners.bush.connect_hitbox=unsupported("Outdoor bush.connect_hitbox actual operation is not implemented");
  s.owners.bush.publish=unsupported("Outdoor bush.publish actual operation is not implemented");
  s.owners.bush.duplicate_sprite=unsupported("Outdoor bush.duplicate_sprite actual operation is not implemented");
  s.owners.bush.roots_frame=unsupported("Outdoor bush.roots_frame actual operation is not implemented");
  s.owners.bush.roots_add_child=unsupported("Outdoor bush.roots_add_child actual operation is not implemented");
  s.owners.bush.sprite_global_position=unsupported("Outdoor bush.sprite_global_position actual operation is not implemented");
  s.owners.bush.roots_local_position=unsupported("Outdoor bush.roots_local_position actual operation is not implemented");
  s.owners.bush.audio_play=unsupported("Outdoor bush.audio_play actual operation is not implemented");
  s.owners.bush.prompt_enabled=unsupported("Outdoor bush.prompt_enabled actual operation is not implemented");
  s.owners.bush.boolean_setting=unsupported("Outdoor bush.boolean_setting actual operation is not implemented");
  s.owners.bush.vibrate=unsupported("Outdoor bush.vibrate actual operation is not implemented");
  s.owners.bush.open_dialogue=unsupported("Outdoor bush.open_dialogue actual operation is not implemented");
  s.owners.bush.call_deferred=unsupported("Outdoor bush.call_deferred actual operation is not implemented");
  s.owners.bush.queue_free=unsupported("Outdoor bush.queue_free actual operation is not implemented");
  s.owners.interact.admit_ready=unsupported("Outdoor interact.admit_ready actual operation is not implemented");
  s.owners.interact.connect_flags=unsupported("Outdoor interact.connect_flags actual operation is not implemented");
  s.owners.interact.read_flag=unsupported("Outdoor interact.read_flag actual operation is not implemented");
  s.owners.interact.visible=unsupported("Outdoor interact.visible actual operation is not implemented");
  s.owners.interact.queue_free=unsupported("Outdoor interact.queue_free actual operation is not implemented");
  s.owners.interact.apply_serialized_offset=unsupported("Outdoor interact.apply_serialized_offset actual operation is not implemented");
  s.owners.interact.admit_programme=unsupported("Outdoor interact.admit_programme actual operation is not implemented");
  s.owners.interact.open_programme=unsupported("Outdoor interact.open_programme actual operation is not implemented");
  s.owners.interact.telepathy_effect=unsupported("Outdoor interact.telepathy_effect actual operation is not implemented");
  s.owners.present.admit_ready=unsupported("Outdoor present.admit_ready actual operation is not implemented");
  s.owners.present.admit_interaction=unsupported("Outdoor present.admit_interaction actual operation is not implemented");
  s.owners.present.read_flag=unsupported("Outdoor present.read_flag actual operation is not implemented");
  s.owners.present.write_flag=unsupported("Outdoor present.write_flag actual operation is not implemented");
  s.owners.present.inventory_space=unsupported("Outdoor present.inventory_space actual operation is not implemented");
  s.owners.present.select_item=unsupported("Outdoor present.select_item actual operation is not implemented");
  s.owners.present.prompt_enabled=unsupported("Outdoor present.prompt_enabled actual operation is not implemented");
  s.owners.present.sound=unsupported("Outdoor present.sound actual operation is not implemented");
  s.owners.present.dialogue=unsupported("Outdoor present.dialogue actual operation is not implemented");
  s.owners.dropped.admit_ready=unsupported("Outdoor dropped.admit_ready actual operation is not implemented");
  s.owners.dropped.admit_interaction=unsupported("Outdoor dropped.admit_interaction actual operation is not implemented");
  s.owners.dropped.read_flag=unsupported("Outdoor dropped.read_flag actual operation is not implemented");
  s.owners.dropped.write_flag=unsupported("Outdoor dropped.write_flag actual operation is not implemented");
  s.owners.dropped.inventory_space=unsupported("Outdoor dropped.inventory_space actual operation is not implemented");
  s.owners.dropped.player_paused=unsupported("Outdoor dropped.player_paused actual operation is not implemented");
  s.owners.dropped.select_item=unsupported("Outdoor dropped.select_item actual operation is not implemented");
  s.owners.dropped.prompt_enabled=unsupported("Outdoor dropped.prompt_enabled actual operation is not implemented");
  s.owners.dropped.prompt_force_show=unsupported("Outdoor dropped.prompt_force_show actual operation is not implemented");
  s.owners.dropped.prompt_press=unsupported("Outdoor dropped.prompt_press actual operation is not implemented");
  s.owners.dropped.prompt_connect_hide_free=unsupported("Outdoor dropped.prompt_connect_hide_free actual operation is not implemented");
  s.owners.dropped.prompt_queue_free=unsupported("Outdoor dropped.prompt_queue_free actual operation is not implemented");
  s.owners.dropped.prompt_visible=unsupported("Outdoor dropped.prompt_visible actual operation is not implemented");
  s.owners.dropped.prompt_reparent=unsupported("Outdoor dropped.prompt_reparent actual operation is not implemented");
  s.owners.dropped.prompt_position=unsupported("Outdoor dropped.prompt_position actual operation is not implemented");
  s.owners.dropped.player_position=unsupported("Outdoor dropped.player_position actual operation is not implemented");
  s.owners.dropped.publish_position=unsupported("Outdoor dropped.publish_position actual operation is not implemented");
  s.owners.dropped.publish_scale=unsupported("Outdoor dropped.publish_scale actual operation is not implemented");
  s.owners.dropped.register_collect_tween=unsupported("Outdoor dropped.register_collect_tween actual operation is not implemented");
  s.owners.dropped.dialogue=unsupported("Outdoor dropped.dialogue actual operation is not implemented");
  s.owners.dropped.queue_free=unsupported("Outdoor dropped.queue_free actual operation is not implemented");
  s.owners.sparkles.bind=unsupported("Outdoor sparkles.bind actual operation is not implemented");
  s.owners.sparkles.observe=unsupported("Outdoor sparkles.observe actual operation is not implemented");
  s.owners.sparkles.publish=unsupported("Outdoor sparkles.publish actual operation is not implemented");
  s.owners.sparkles.emit=unsupported("Outdoor sparkles.emit actual operation is not implemented");
  s.owners.openable.bind=unsupported("Outdoor openable.bind actual operation is not implemented");
  s.owners.openable.connect_area=unsupported("Outdoor openable.connect_area actual operation is not implemented");
  s.owners.openable.connect_flags=unsupported("Outdoor openable.connect_flags actual operation is not implemented");
  s.owners.openable.read_flag=unsupported("Outdoor openable.read_flag actual operation is not implemented");
  s.owners.openable.write_flag=unsupported("Outdoor openable.write_flag actual operation is not implemented");
  s.owners.openable.emit_flags=unsupported("Outdoor openable.emit_flags actual operation is not implemented");
  s.owners.openable.observe=unsupported("Outdoor openable.observe actual operation is not implemented");
  s.owners.openable.overlapping_bodies=unsupported("Outdoor openable.overlapping_bodies actual operation is not implemented");
  s.owners.openable.sprite_texture=unsupported("Outdoor openable.sprite_texture actual operation is not implemented");
  s.owners.openable.publish=unsupported("Outdoor openable.publish actual operation is not implemented");
  s.owners.openable.property=unsupported("Outdoor openable.property actual operation is not implemented");
  s.owners.openable.defer_disabled=unsupported("Outdoor openable.defer_disabled actual operation is not implemented");
  s.owners.openable.prompt_assign_enabled=unsupported("Outdoor openable.prompt_assign_enabled actual operation is not implemented");
  s.owners.openable.prompt_hide=unsupported("Outdoor openable.prompt_hide actual operation is not implemented");
  s.owners.openable.audio_stream=unsupported("Outdoor openable.audio_stream actual operation is not implemented");
  s.owners.openable.audio_playing=unsupported("Outdoor openable.audio_playing actual operation is not implemented");
  s.owners.openable.camera_shake=unsupported("Outdoor openable.camera_shake actual operation is not implemented");
  s.owners.openable.party_inventories=unsupported("Outdoor openable.party_inventories actual operation is not implemented");
  s.owners.openable.inventory_find=unsupported("Outdoor openable.inventory_find actual operation is not implemented");
  s.owners.openable.item_name=unsupported("Outdoor openable.item_name actual operation is not implemented");
  s.owners.openable.item_owner=unsupported("Outdoor openable.item_owner actual operation is not implemented");
  s.owners.openable.inventory_drop=unsupported("Outdoor openable.inventory_drop actual operation is not implemented");
  s.owners.openable.set_global_item=unsupported("Outdoor openable.set_global_item actual operation is not implemented");
  s.owners.openable.open_dialogue=unsupported("Outdoor openable.open_dialogue actual operation is not implemented");
  s.owners.payphone.bind=unsupported("Outdoor payphone.bind actual operation is not implemented");
  s.owners.payphone.read_flag=unsupported("Outdoor payphone.read_flag actual operation is not implemented");
  s.owners.payphone.connect_flags=unsupported("Outdoor payphone.connect_flags actual operation is not implemented");
  s.owners.payphone.visibility=unsupported("Outdoor payphone.visibility actual operation is not implemented");
  s.owners.payphone.queue_free=unsupported("Outdoor payphone.queue_free actual operation is not implemented");
  s.owners.payphone.find_card=unsupported("Outdoor payphone.find_card actual operation is not implemented");
  s.owners.payphone.lookup_card=unsupported("Outdoor payphone.lookup_card actual operation is not implemented");
  s.owners.payphone.reduce_or_drop=unsupported("Outdoor payphone.reduce_or_drop actual operation is not implemented");
  s.owners.payphone.read_cash=unsupported("Outdoor payphone.read_cash actual operation is not implemented");
  s.owners.payphone.add_cash=unsupported("Outdoor payphone.add_cash actual operation is not implemented");
  s.owners.payphone.set_stream=unsupported("Outdoor payphone.set_stream actual operation is not implemented");
  s.owners.payphone.play_idle=unsupported("Outdoor payphone.play_idle actual operation is not implemented");
  s.owners.payphone.set_location=unsupported("Outdoor payphone.set_location actual operation is not implemented");
  s.owners.payphone.box_open=unsupported("Outdoor payphone.box_open actual operation is not implemented");
  s.owners.payphone.box_update=unsupported("Outdoor payphone.box_update actual operation is not implemented");
  s.owners.payphone.box_close=unsupported("Outdoor payphone.box_close actual operation is not implemented");
  s.owners.payphone.audio_play=unsupported("Outdoor payphone.audio_play actual operation is not implemented");
  s.owners.payphone.open_dialogue=unsupported("Outdoor payphone.open_dialogue actual operation is not implemented");
  s.owners.butterfly.admit_ready=unsupported("Outdoor butterfly.admit_ready actual operation is not implemented");
  s.owners.butterfly.body_snapshot=unsupported("Outdoor butterfly.body_snapshot actual operation is not implemented");
  s.owners.butterfly.publish=unsupported("Outdoor butterfly.publish actual operation is not implemented");
  s.owners.butterfly.timer_start_wait=unsupported("Outdoor butterfly.timer_start_wait actual operation is not implemented");
  s.owners.butterfly.timer_cancel=unsupported("Outdoor butterfly.timer_cancel actual operation is not implemented");
  s.owners.door.resolve_onready=[&s](const FieldDoorDescriptor &d,const FieldDoorAudio &a,std::string &error){
    FieldDoorDescriptor expected;
    if(!s.sources.door().find(d.id,expected)||a.id!=d.audio||!s.shapes_ready(d.id,{d.shape},error))return false;
    for(auto stable:{d.marker,d.audio}){FieldObjectId actual=0;
      if(!s.source(stable,actual,error)||!s.tree->state(actual)->inside||!s.tree->state(actual)->ready_notified)
        return reject(error,"Door source onready child native lifecycle missing");
      if(stable==d.audio&&!s.audio.owns(actual))return reject(error,"Door source audio child owner missing");
    }
    error.clear();return true;
  };
  s.owners.door.connect_body=[&s](uint32_t stable,auto slot,std::string &error){
    const auto method=s.sources.door().body_method();
    if(method.empty()||!slot)return reject(error,"Door source body callback not admitted");
    return s.connect(stable,"body_entered",method,[&s,slot=std::move(slot)](const State::Args &args,std::string &err){
      uint32_t body=0;return s.body_arg(args,body,err)&&slot(body,err);
    },error);
  };
  s.owners.cutscene.connect_battle_to_overworld=[&s](uint32_t stable,std::string_view signal,std::string &error){
    const auto &policy=s.sources.cutscene().policy();FieldObjectId actual=0;
    if(signal!=policy.battle_signal||policy.battle_method.empty()||!s.source(stable,actual,error))return false;
    if(!s.continuation.signals()->connect(s.continuation.ui()->binding().object,signal,actual,policy.battle_method,0,{},error))return false;
    auto key=std::make_pair(actual,policy.battle_method);
    if(!s.methods.emplace(key,[&s,stable,actual](const State::Args &args,std::string &err){
      if(!args.empty())return reject(err,"Cutscene battle callback arguments differ");
      return s.consumers.runtime_instances().cutscene->battle_to_overworld(stable,err)&&s.tree->set_process(actual,false,false,err);
    }).second)return reject(error,"Cutscene source battle callback duplicate");
    return s.tree->set_process(actual,false,false,error);
  };
  s.owners.cutscene.body_is_current_player=[&s](uint32_t body,bool &out,std::string &error){
    std::shared_ptr<const GlobalLoadObjectArray> objects;
    if(!s.continuation.global()->core().array(FieldGlobalMemberRole::PartyObjects,objects,error)||!objects||objects->values.empty())return reject(error,"Cutscene actual global player unavailable");
    out=objects->values.front()==body;error.clear();return true;
  };
  s.owners.cutscene.query_ui=[&s](FieldCutsceneAreaUi &out,std::string &error){
    auto *ui=s.continuation.ui();const auto object=ui->binding().object;
    return ui->source_is_in_cutscene(object,out.cutscene,error)&&ui->source_is_in_battle(object,out.battle,error)&&ui->source_is_pause_menu_active(object,out.pause,error);
  };
  s.owners.cutscene.read_flag=[&s](std::string_view name,bool &out,std::string &error){bool present=false;if(!s.continuation.characters()->flags().read(false,name,present,out,error))return false;if(!present)out=false;error.clear();return true;};
  s.owners.cutscene.admit_ready=unsupported("Outdoor cutscene.admit_ready actual operation is not implemented");
  s.owners.cutscene.connect_battle_to_overworld=unsupported("Outdoor cutscene.connect_battle_to_overworld actual operation is not implemented");
  s.owners.cutscene.body_is_current_player=unsupported("Outdoor cutscene.body_is_current_player actual operation is not implemented");
  s.owners.cutscene.query_ui=unsupported("Outdoor cutscene.query_ui actual operation is not implemented");
  s.owners.cutscene.read_flag=unsupported("Outdoor cutscene.read_flag actual operation is not implemented");
  s.owners.cutscene.admit_programme=unsupported("Outdoor cutscene.admit_programme actual operation is not implemented");
  s.owners.cutscene.close_commands=unsupported("Outdoor cutscene.close_commands actual operation is not implemented");
  s.owners.cutscene.pause_player=unsupported("Outdoor cutscene.pause_player actual operation is not implemented");
  s.owners.cutscene.open_room_and_unpause=unsupported("Outdoor cutscene.open_room_and_unpause actual operation is not implemented");
  s.owners.birds.bind=unsupported("Outdoor birds.bind actual operation is not implemented");
  s.owners.birds.observe=unsupported("Outdoor birds.observe actual operation is not implemented");
  s.owners.birds.connect=unsupported("Outdoor birds.connect actual operation is not implemented");
  s.owners.birds.body_snapshot=unsupported("Outdoor birds.body_snapshot actual operation is not implemented");
  s.owners.birds.texture=unsupported("Outdoor birds.texture actual operation is not implemented");
  s.owners.birds.publish=unsupported("Outdoor birds.publish actual operation is not implemented");
  s.owners.birds.animation_signal=unsupported("Outdoor birds.animation_signal actual operation is not implemented");
  s.owners.birds.timer_timeout=unsupported("Outdoor birds.timer_timeout actual operation is not implemented");
  s.owners.camera_area.bind=unsupported("Outdoor camera_area.bind actual operation is not implemented");
  s.owners.camera_area.connect=unsupported("Outdoor camera_area.connect actual operation is not implemented");
  s.owners.camera_area.resolve_reference=unsupported("Outdoor camera_area.resolve_reference actual operation is not implemented");
  s.owners.camera_area.observe=unsupported("Outdoor camera_area.observe actual operation is not implemented");
  s.owners.camera_area.is_global_player=unsupported("Outdoor camera_area.is_global_player actual operation is not implemented");
  s.owners.camera_area.adjust_camareas=unsupported("Outdoor camera_area.adjust_camareas actual operation is not implemented");
  s.owners.camera_area.set_limit=unsupported("Outdoor camera_area.set_limit actual operation is not implemented");
  s.owners.camera_area.current_limits=unsupported("Outdoor camera_area.current_limits actual operation is not implemented");
  s.owners.camera_area.set_camarea_offset=unsupported("Outdoor camera_area.set_camarea_offset actual operation is not implemented");
  s.owners.camera_area.await_idle_frame=unsupported("Outdoor camera_area.await_idle_frame actual operation is not implemented");
  s.owners.camera_area.cancel_idle_frame=unsupported("Outdoor camera_area.cancel_idle_frame actual operation is not implemented");
  s.owners.music.admit_service=unsupported("Outdoor music.admit_service actual operation is not implemented");
  s.owners.music.admit_ready=unsupported("Outdoor music.admit_ready actual operation is not implemented");
  s.owners.music.set_shape_disabled=unsupported("Outdoor music.set_shape_disabled actual operation is not implemented");
  s.owners.music.context=unsupported("Outdoor music.context actual operation is not implemented");
  s.owners.music.area_enter=unsupported("Outdoor music.area_enter actual operation is not implemented");
  s.owners.music.area_exit=unsupported("Outdoor music.area_exit actual operation is not implemented");
  s.owners.music.play_explicit=unsupported("Outdoor music.play_explicit actual operation is not implemented");
  s.owners.music.tree_exit=unsupported("Outdoor music.tree_exit actual operation is not implemented");
  s.owners.music.stop_explicit=unsupported("Outdoor music.stop_explicit actual operation is not implemented");
  s.owners.music.idle_frame=unsupported("Outdoor music.idle_frame actual operation is not implemented");
  s.owners.arrows.bind=unsupported("Outdoor arrows.bind actual operation is not implemented");
  s.owners.arrows.observe=unsupported("Outdoor arrows.observe actual operation is not implemented");
  s.owners.arrows.connect_animation_finished=unsupported("Outdoor arrows.connect_animation_finished actual operation is not implemented");
  s.owners.arrows.publish_root=unsupported("Outdoor arrows.publish_root actual operation is not implemented");
  s.owners.arrows.publish_sprite=unsupported("Outdoor arrows.publish_sprite actual operation is not implemented");
  s.owners.arrows.animation_signal=unsupported("Outdoor arrows.animation_signal actual operation is not implemented");
  s.owners.arrows.sprite_signal=unsupported("Outdoor arrows.sprite_signal actual operation is not implemented");
  s.owners.arrows.await_frame_changed=unsupported("Outdoor arrows.await_frame_changed actual operation is not implemented");
  s.owners.arrows.cancel_frame_changed=unsupported("Outdoor arrows.cancel_frame_changed actual operation is not implemented");
  s.owners.arrows.control_directions=unsupported("Outdoor arrows.control_directions actual operation is not implemented");
  s.owners.actions.admit_ready=unsupported("Outdoor actions.admit_ready actual operation is not implemented");
  s.owners.actions.resolve=unsupported("Outdoor actions.resolve actual operation is not implemented");
  s.owners.actions.describe=unsupported("Outdoor actions.describe actual operation is not implemented");
  s.owners.actions.body_is_player=unsupported("Outdoor actions.body_is_player actual operation is not implemented");
  s.owners.actions.admit_reparent=unsupported("Outdoor actions.admit_reparent actual operation is not implemented");
  s.owners.actions.remove_child=unsupported("Outdoor actions.remove_child actual operation is not implemented");
  s.owners.actions.add_child=unsupported("Outdoor actions.add_child actual operation is not implemented");
  s.owners.actions.read_collision=unsupported("Outdoor actions.read_collision actual operation is not implemented");
  s.owners.actions.set_collision=unsupported("Outdoor actions.set_collision actual operation is not implemented");
  s.owners.actions.enqueue=unsupported("Outdoor actions.enqueue actual operation is not implemented");
  s.owners.actions.admit_event=unsupported("Outdoor actions.admit_event actual operation is not implemented");
  s.owners.actions.has_method=unsupported("Outdoor actions.has_method actual operation is not implemented");
  s.owners.actions.call_method=unsupported("Outdoor actions.call_method actual operation is not implemented");
  s.owners.actions.read_flag=unsupported("Outdoor actions.read_flag actual operation is not implemented");
  s.owners.actions.write_existing_flag=unsupported("Outdoor actions.write_existing_flag actual operation is not implemented");
  s.owners.stepping.admit=unsupported("Outdoor stepping.admit actual operation is not implemented");
  s.owners.stepping.admit_ready=unsupported("Outdoor stepping.admit_ready actual operation is not implemented");
  s.owners.stepping.describe_body=unsupported("Outdoor stepping.describe_body actual operation is not implemented");
  s.owners.stepping.admit_dispatch=unsupported("Outdoor stepping.admit_dispatch actual operation is not implemented");
  s.owners.stepping.set_player_run_sound=unsupported("Outdoor stepping.set_player_run_sound actual operation is not implemented");
  s.owners.stepping.set_party_shadow=unsupported("Outdoor stepping.set_party_shadow actual operation is not implemented");
  s.owners.stepping.geometry=unsupported("Outdoor stepping.geometry actual operation is not implemented");
  s.owners.transitions.bind=unsupported("Outdoor transitions.bind actual operation is not implemented");
  s.owners.transitions.context=unsupported("Outdoor transitions.context actual operation is not implemented");
  s.owners.transitions.flag=unsupported("Outdoor transitions.flag actual operation is not implemented");
  s.owners.transitions.cached_ray=unsupported("Outdoor transitions.cached_ray actual operation is not implemented");
  s.owners.transitions.apply=unsupported("Outdoor transitions.apply actual operation is not implemented");
  s.owners.camera.bind=unsupported("Outdoor camera.bind actual operation is not implemented");
  s.owners.camera.observe=unsupported("Outdoor camera.observe actual operation is not implemented");
  s.owners.camera.publish=unsupported("Outdoor camera.publish actual operation is not implemented");
  s.owners.camera.publish_canvas=unsupported("Outdoor camera.publish_canvas actual operation is not implemented");
  s.owners.camera.connect_player=unsupported("Outdoor camera.connect_player actual operation is not implemented");
  s.owners.camera.scope=unsupported("Outdoor camera.scope actual operation is not implemented");
  s.owners.camera.arrow_visible=unsupported("Outdoor camera.arrow_visible actual operation is not implemented");
  s.owners.camera.info_plates_hide=unsupported("Outdoor camera.info_plates_hide actual operation is not implemented");
  s.owners.camera.player_exit_camera=unsupported("Outdoor camera.player_exit_camera actual operation is not implemented");
  s.owners.camera.animation_signal=unsupported("Outdoor camera.animation_signal actual operation is not implemented");
  s.owners.camera.current_camera_snapshot=unsupported("Outdoor camera.current_camera_snapshot actual operation is not implemented");
  s.owners.camera.make_current=unsupported("Outdoor camera.make_current actual operation is not implemented");
  s.owners.camera.set_global_current=unsupported("Outdoor camera.set_global_current actual operation is not implemented");
  s.owners.camera.register_tween=unsupported("Outdoor camera.register_tween actual operation is not implemented");
  s.owners.camera.kill_tween=unsupported("Outdoor camera.kill_tween actual operation is not implemented");
  s.owners.camera.tween_signal=unsupported("Outdoor camera.tween_signal actual operation is not implemented");
  s.owners.camera.coroutine_completed=unsupported("Outdoor camera.coroutine_completed actual operation is not implemented");
  s.owners.camera.create_shaker=unsupported("Outdoor camera.create_shaker actual operation is not implemented");
  s.owners.camera.shaker_finished=unsupported("Outdoor camera.shaker_finished actual operation is not implemented");
  s.owners.camera.queue_free_shaker=unsupported("Outdoor camera.queue_free_shaker actual operation is not implemented");
  s.owners.camera.await_idle_frame=unsupported("Outdoor camera.await_idle_frame actual operation is not implemented");
  s.owners.camera.cancel_idle_frame=unsupported("Outdoor camera.cancel_idle_frame actual operation is not implemented");
  s.owners.camera.stopped_shaking=unsupported("Outdoor camera.stopped_shaking actual operation is not implemented");
  s.owners.door_npc.admit_ready=unsupported("Outdoor door_npc.admit_ready actual operation is not implemented");
  s.owners.door_npc.body_is_current_player=unsupported("Outdoor door_npc.body_is_current_player actual operation is not implemented");
  s.owners.door_npc.query_ui=unsupported("Outdoor door_npc.query_ui actual operation is not implemented");
  s.owners.door_npc.read_flag=unsupported("Outdoor door_npc.read_flag actual operation is not implemented");
  s.owners.door_npc.read_seen=unsupported("Outdoor door_npc.read_seen actual operation is not implemented");
  s.owners.door_npc.mark_seen=unsupported("Outdoor door_npc.mark_seen actual operation is not implemented");
  s.owners.door_npc.node_path=unsupported("Outdoor door_npc.node_path actual operation is not implemented");
  s.owners.door_npc.admit_start=unsupported("Outdoor door_npc.admit_start actual operation is not implemented");
  s.owners.door_npc.turn_player=unsupported("Outdoor door_npc.turn_player actual operation is not implemented");
  s.owners.door_npc.pause_player=unsupported("Outdoor door_npc.pause_player actual operation is not implemented");
  s.owners.door_npc.set_cutscene=unsupported("Outdoor door_npc.set_cutscene actual operation is not implemented");
  s.owners.door_npc.black_bars=unsupported("Outdoor door_npc.black_bars actual operation is not implemented");
  s.owners.door_npc.play_knock=unsupported("Outdoor door_npc.play_knock actual operation is not implemented");
  s.owners.door_npc.create_timer=unsupported("Outdoor door_npc.create_timer actual operation is not implemented");
  s.owners.door_npc.admit_dialogue=unsupported("Outdoor door_npc.admit_dialogue actual operation is not implemented");
  s.owners.door_npc.open_room_and_unpause=unsupported("Outdoor door_npc.open_room_and_unpause actual operation is not implemented");
  s.owners.melody.admit_ready=unsupported("Outdoor melody.admit_ready actual operation is not implemented");
  s.owners.melody.admit_call=unsupported("Outdoor melody.admit_call actual operation is not implemented");
  s.owners.melody.camera_screen_center=unsupported("Outdoor melody.camera_screen_center actual operation is not implemented");
  s.owners.melody.root_object=unsupported("Outdoor melody.root_object actual operation is not implemented");
  s.owners.melody.set_global_position=unsupported("Outdoor melody.set_global_position actual operation is not implemented");
  s.owners.melody.local_position=unsupported("Outdoor melody.local_position actual operation is not implemented");
  s.owners.melody.dialogue_actors=unsupported("Outdoor melody.dialogue_actors actual operation is not implemented");
  s.owners.melody.talker=unsupported("Outdoor melody.talker actual operation is not implemented");
  s.owners.melody.print_actor_key=unsupported("Outdoor melody.print_actor_key actual operation is not implemented");
  s.owners.melody.describe_actor=unsupported("Outdoor melody.describe_actor actual operation is not implemented");
  s.owners.melody.set_actor_position=unsupported("Outdoor melody.set_actor_position actual operation is not implemented");
  s.owners.melody.set_actor_active=unsupported("Outdoor melody.set_actor_active actual operation is not implemented");
  s.owners.melody.remove_child=unsupported("Outdoor melody.remove_child actual operation is not implemented");
  s.owners.melody.add_child=unsupported("Outdoor melody.add_child actual operation is not implemented");
  s.owners.melody.set_visible=unsupported("Outdoor melody.set_visible actual operation is not implemented");
  s.owners.melody.write_color=unsupported("Outdoor melody.write_color actual operation is not implemented");
  s.owners.melody.animation_started=unsupported("Outdoor melody.animation_started actual operation is not implemented");
  s.owners.melody.animation_stopped=unsupported("Outdoor melody.animation_stopped actual operation is not implemented");
  s.owners.melody.create_tween=unsupported("Outdoor melody.create_tween actual operation is not implemented");
  s.owners.melody.tween_signal=unsupported("Outdoor melody.tween_signal actual operation is not implemented");
  s.owners.vending.bind=unsupported("Outdoor vending.bind actual operation is not implemented");
  s.owners.vending.player=unsupported("Outdoor vending.player actual operation is not implemented");
  s.owners.vending.set_current_shop=unsupported("Outdoor vending.set_current_shop actual operation is not implemented");
  s.owners.vending.open_dialogue=unsupported("Outdoor vending.open_dialogue actual operation is not implemented");
  s.owners.vending.unpause=unsupported("Outdoor vending.unpause actual operation is not implemented");
  s.owners.bush.bind=[&s](const auto &data,auto&&...rest){
    auto refs=std::forward_as_tuple(rest...);auto &error=std::get<sizeof...(rest)-1>(refs);
    if(&data!=&s.sources.bush()||!data.valid())return reject(error,"bush actual checked source owner differs");
    error.clear();return true;
  };
  s.owners.sparkles.bind=[&s](const auto &data,auto&&...rest){
    auto refs=std::forward_as_tuple(rest...);auto &error=std::get<sizeof...(rest)-1>(refs);
    if(&data!=&s.sources.sparkles()||!data.valid())return reject(error,"sparkles actual checked source owner differs");
    error.clear();return true;
  };
  s.owners.openable.bind=[&s](const auto &data,auto&&...rest){
    auto refs=std::forward_as_tuple(rest...);auto &error=std::get<sizeof...(rest)-1>(refs);
    if(&data!=&s.sources.openable()||!data.valid())return reject(error,"openable actual checked source owner differs");
    error.clear();return true;
  };
  s.owners.payphone.bind=[&s](const auto &data,auto&&...rest){
    auto refs=std::forward_as_tuple(rest...);auto &error=std::get<sizeof...(rest)-1>(refs);
    if(&data!=&s.sources.payphone()||!data.valid())return reject(error,"payphone actual checked source owner differs");
    error.clear();return true;
  };
  s.owners.birds.bind=[&s](const auto &data,auto&&...rest){
    auto refs=std::forward_as_tuple(rest...);auto &error=std::get<sizeof...(rest)-1>(refs);
    if(&data!=&s.sources.birds()||!data.valid())return reject(error,"birds actual checked source owner differs");
    error.clear();return true;
  };
  s.owners.camera_area.bind=[&s](const auto &data,auto&&...rest){
    auto refs=std::forward_as_tuple(rest...);auto &error=std::get<sizeof...(rest)-1>(refs);
    if(&data!=&s.sources.camera_area()||!data.valid())return reject(error,"camera_area actual checked source owner differs");
    error.clear();return true;
  };
  s.owners.arrows.bind=[&s](const auto &data,auto&&...rest){
    auto refs=std::forward_as_tuple(rest...);auto &error=std::get<sizeof...(rest)-1>(refs);
    if(&data!=&s.sources.arrows()||!data.valid())return reject(error,"arrows actual checked source owner differs");
    error.clear();return true;
  };
  s.owners.transitions.bind=[&s](const auto &data,auto&&...rest){
    auto refs=std::forward_as_tuple(rest...);auto &error=std::get<sizeof...(rest)-1>(refs);
    if(&data!=&s.sources.transitions()||!data.valid())return reject(error,"transitions actual checked source owner differs");
    error.clear();return true;
  };
  s.owners.camera.bind=[&s](const auto &data,auto&&...rest){
    auto refs=std::forward_as_tuple(rest...);auto &error=std::get<sizeof...(rest)-1>(refs);
    if(&data!=&s.sources.camera()||!data.valid())return reject(error,"camera actual checked source owner differs");
    error.clear();return true;
  };
  s.owners.vending.bind=[&s](const auto &data,auto&&...rest){
    auto refs=std::forward_as_tuple(rest...);auto &error=std::get<sizeof...(rest)-1>(refs);
    if(&data!=&s.sources.vending()||!data.valid())return reject(error,"vending actual checked source owner differs");
    error.clear();return true;
  };
  s.owners.present.admit_ready=[&s](const FieldPresentBinding &b,std::string &error){
    if(s.sources.present().binding(b.id)!=&b||!s.source_ready_boundary(b.id,error))return false;
    const auto *body=s.consumers.runtime_instances().present->state(b.id);
    const auto *prompt=s.consumers.runtime_instances().prompt->instance(b.prompt_id);
    FieldObjectId child=0;
    if(!body||!body->child_ready||!prompt||!prompt->ready||
       !s.source(b.sparkles_id,child,error)||!s.tree->state(child)->ready_notified)
      return reject(error,"Present actual Sparkles/Prompt child not initialized");
    error.clear();return true;
  };
  s.owners.dropped.admit_ready=[&s](const FieldDroppedBinding &b,std::string &error){
    if(s.sources.dropped().binding(b.id)!=&b||!s.source_ready_boundary(b.id,error))return false;
    const auto *body=s.consumers.runtime_instances().dropped->state(b.id);
    const auto *prompt=s.consumers.runtime_instances().prompt->instance(b.prompt_id);
    if(!body||!body->child_ready||!prompt||!prompt->ready)
      return reject(error,"Dropped actual Sparkles/Prompt child not initialized");
    for(auto stable:{b.sparkles_id,b.tween_id,b.timer_id,b.animation_id}){
      FieldObjectId actual=0;
      if(!s.source(stable,actual,error)||!s.tree->state(actual)->ready_notified)
        return reject(error,"Dropped actual native child lifecycle missing");
    }
    error.clear();return true;
  };
  s.owners.music=s.input.music_service;
  s.owners.music.admit_ready=[&s](const FieldMusicChangerBinding &b,const auto &connections,std::string &error){
    if(s.sources.music().binding(b.id)!=&b||&connections!=&s.sources.music().connections()||
       !s.source_ready_boundary(b.id,error))return false;
    FieldObjectId actual=0;
    if(!s.source(b.id,actual,error)||!s.native.owns(actual))
      return reject(error,"MusicChanger actual Area owner missing");
    for(const auto &shape:b.shapes){
      if(!s.source(shape.id,actual,error)||!s.native.owns(actual)||!s.tree->state(actual)->ready_notified)
        return reject(error,"MusicChanger actual child collision owner missing");
    }
    if(s.music_connected.count(b.id))return reject(error,"MusicChanger source connections duplicated");
    for(const auto &c:connections){
      if(!s.connect(b.id,c.signal,c.method,[&s,id=b.id,role=c.role](const State::Args &args,std::string &err){
        auto *runtime=s.consumers.runtime_instances().music;
        if(role==3){if(!args.empty())return reject(err,"MusicChanger tree_exiting argument rejected");return runtime->tree_exiting(id,err);}
        uint32_t body=0;if(!s.body_arg(args,body,err))return false;
        return role==1?runtime->body_enter(id,body,err):runtime->body_exit(id,body,err);
      },error))return false;
    }
    s.music_connected.insert(b.id);
    error.clear();return true;
  };
  s.owners.music.context=[&s,&global](uint32_t body,MusicRegionContext &out,std::string &error){
    if(!s.continuation.registry()->object_exists(body))return reject(error,"MusicChanger body is not an actual live object");
    std::shared_ptr<const GlobalLoadObjectArray> objects;
    if(!global.array(FieldGlobalMemberRole::PartyObjects,objects,error)||!objects||objects->values.empty())return false;
    out.is_player=body==objects->values.front();
    // MusicChanger's source tests non-player identity first, before UI/body
    // state. The values below are observed only for that actual source player.
    if(out.is_player){
      const auto ui=s.continuation.ui()->binding().object;
      if(!s.continuation.ui()->source_is_in_battle(ui,out.in_battle,error)||
         !s.continuation.ui()->source_is_in_cutscene(ui,out.in_cutscene,error))return false;
      FieldObjectId shape=0;bool disabled=false;
      if(!s.tree->get_node(body,s.player_sources.motion->lifecycle().collision_path,shape,error)||
         !s.physics.shape_disabled(shape,disabled,error))return false;
      out.has_collisions=!disabled;
    }
    std::map<std::string,bool> flags;
    for(const auto &region:s.sources.music().music().regions())for(const auto &key:{region.appear_flag,region.disappear_flag}){
      if(key.empty()||flags.count(key))continue;
      bool present=false,value=false;
      if(!s.continuation.characters()->flags().read(false,key,present,value,error))return false;
      flags.emplace(key,present&&value);
    }
    out.flag=[flags=std::move(flags)](std::string_view key){auto i=flags.find(std::string(key));return i!=flags.end()&&i->second;};
    error.clear();return true;
  };
  s.owners.music.set_shape_disabled=[&s](uint32_t stable,bool disabled,std::string &error){
    FieldObjectId actual=0;return s.source(stable,actual,error)&&s.native.set_disabled(actual,disabled,error);
  };
  s.owners.birds.connect=[&s](const FieldBirdDescriptor &row,auto body,auto enter,auto exit,auto finished,std::string &error){
    if(s.sources.birds().record(row.id)!=&row||s.bird_connected.count(row.id))
      return reject(error,"Bird actual source connections duplicate/differ");
    FieldObjectId target=0;
    if(!s.source(row.id,target,error))return false;
    for(const auto &c:s.sources.birds().connections()){
      FieldObjectId sender=0;
      if(c.child>=row.children.size()||!s.source(row.children[c.child],sender,error))return false;
      if(c.role>=3){
        bool connected=false;
        if(!s.continuation.signals()->connected(sender,c.signal,target,c.method,connected,error)||!connected)
          return reject(error,"Bird actual Visibility source connection not installed");
        continue;
      }
      State::Callback callback;
      if(c.role==1)callback=[body](const State::Args &args,std::string &err){
        if(args.size()!=1||!std::holds_alternative<FieldObjectRef>(args[0]))
          return reject(err,"Bird Area source body signature rejected");
        if(!body(std::get<FieldObjectRef>(args[0]).id))return reject(err,"Bird actual source body callback rejected");
        err.clear();return true;
      };
      else if(c.role==2)callback=[&s,profile=row.profile,finished](const State::Args &args,std::string &err){
        if(args.size()!=1||!std::holds_alternative<std::string>(args[0]))
          return reject(err,"Bird AP source finished signature rejected");
        for(uint32_t i=1;i<=uint32_t(FieldBirdClipRole::Prepare);++i){
          auto *clip=s.sources.birds().clip(profile,FieldBirdClipRole(i));
          if(clip&&clip->name==std::get<std::string>(args[0])){
            if(!finished(clip->role))return reject(err,"Bird actual animation callback rejected");
            err.clear();return true;
          }
        }
        return reject(err,"Bird AP emitted unsupported source animation");
      };
      else return reject(error,"Bird source connection role unsupported");
      if(!s.methods.emplace(std::make_pair(target,c.method),std::move(callback)).second||
         !s.continuation.signals()->connect(sender,c.signal,target,c.method,0,{},error))return false;
    }
    (void)enter;(void)exit; // Visibility's original callbacks own these two endpoints.
    s.bird_connected.insert(row.id);error.clear();return true;
  };
  s.owners.birds.observe=[&s](uint32_t stable,FieldBirdObservation &out,std::string &error){
    auto *row=s.sources.birds().record(stable);FieldObjectId root=0,parent=0,sprite=0,notifier=0;
    if(!row||!s.source(stable,root,error)||!s.source(row->parent_id,parent,error)||
       !s.source(row->children[0],sprite,error)||!s.source(row->children[5],notifier,error)||
       !s.tree->world_transform(parent,out.parent,error))return false;
    const auto *body=s.tree->state(root);auto shape=s.source_shapes.find(row->children[3]);
    const auto *canvas=s.sources.canvas().record(row->children[0]);
    if(!body||shape==s.source_shapes.end()||!canvas||!s.npc_world.owns(root)||!s.native.owns(sprite)||
       canvas->shader!=FieldCanvasShader::Default||!(shape->second.flags&1))
      return reject(error,"Bird actual disabled body/Sprite material differs");
    out.alive=body->alive&&!body->queued;
    out.ancestors_admitted=body->bound&&s.tree->state(parent)->bound;
    out.source_signals_admitted=s.bird_connected.count(stable)!=0;
    out.disabled_body_shape=(row->flags&32)!=0&&(shape->second.flags&1)!=0;
    // This source body owns zero active shapes; no platform/floor cache exists
    // in its checked zero-shape move_and_slide branch.
    out.clear_floor_state=out.disabled_body_shape;
    out.material_admitted=canvas->shader==FieldCanvasShader::Default;
    out.ancestors_visible=true;out.canvas_color={1,1,1,1};
    for(auto p=parent;p;){
      const auto *n=s.tree->state(p);if(!n||!n->alive||n->queued||!n->bound)return reject(error,"Bird actual canvas ancestor missing");
      out.ancestors_visible=out.ancestors_visible&&(n->flags&2)!=0;
      for(size_t c=0;c<4;++c)out.canvas_color[c]*=n->modulate[c];
      p=n->canvas_parent;
    }
    if(!s.visibility.is_on_screen(notifier,out.notifier_on_screen,error))return false;
    error.clear();return true;
  };
  s.owners.birds.texture=[&s](uint32_t stable,const FieldBirdSkin &skin,std::string &error){
    if(s.sources.birds().skin(skin.index)!=&skin)return reject(error,"Bird texture is outside actual checked three atlases");
    FieldObjectId sprite=0;
    if(!s.source(stable,sprite,error)||!s.bird_canvas.owns_drawable(sprite))return false;
    // The one source core selects its checked GPU atlas. No parallel native
    // Sprite frame/texture clock or second source skin state is introduced.
    error.clear();return true;
  };
  s.owners.birds.publish=[&s](uint32_t stable,const FieldBirdState &body,std::string &error){
    const auto *row=s.sources.birds().record(stable);FieldObjectId root=0,sprite=0;
    if(!row||body.id!=stable||!s.source(stable,root,error)||!s.source(row->children[0],sprite,error))return false;
    auto local=s.tree->state(root)->local;local[2]=body.position;
    FieldCanvasAppearance pose;
    if(!s.native.sprite_snapshot(sprite,pose,error))return false;
    pose.frame=body.frame;pose.offset=body.sprite_offset;pose.flip_h=body.flip;
    return s.tree->set_local(root,local,error)&&s.tree->set_z_index(root,body.z_index,error)&&
       s.tree->set_visible(root,body.visible,error)&&s.tree->set_process(root,false,body.process,error)&&
       s.native.sprite_publish(sprite,pose,error);
  };
  s.owners.camera_area.connect=[&s](const FieldCameraAreaDescriptor &b,auto enter,auto exit,std::string &error){
    if(s.sources.camera_area().record(b.id)!=&b||s.camera_area_connected.count(b.id))
      return reject(error,"Camarea actual source connection duplicate/different");
    for(const auto &c:s.sources.camera_area().connections()){
      if(!s.connect(b.id,c.signal,c.method,[&s,role=c.role,enter,exit](const State::Args &args,std::string &err){
        uint32_t body=0;if(!s.body_arg(args,body,err))return false;
        if(!(role==1?enter(body):exit(body)))return reject(err,"Camarea actual source Area callback rejected");
        err.clear();return true;
      },error))return false;
    }
    s.camera_area_connected.insert(b.id);error.clear();return true;
  };
  s.owners.camera_area.is_global_player=[&global,&s](uint64_t body,bool &out,std::string &error){
    if(!s.continuation.registry()->object_exists(body))return reject(error,"Camarea body is not an actual live ObjectDB node");
    std::shared_ptr<const GlobalLoadObjectArray> objects;
    if(!global.array(FieldGlobalMemberRole::PartyObjects,objects,error)||!objects||objects->values.empty())return false;
    out=objects->values.front()==body;error.clear();return true;
  };
  s.owners.camera_area.adjust_camareas=[&s](int64_t delta,int64_t &out,std::string &error){
    FieldGameCameraRuntime *camera=nullptr;uint32_t id=0;
    if(!s.current_camera(camera,id,error))return false;
    if(!camera->adjust_camareas(id,delta,out)){error=camera->error();return false;}
    error.clear();return true;
  };
  s.owners.camera_area.set_limit=[&s](FieldCameraLimit limit,int32_t value,std::string &error){
    FieldGameCameraRuntime *camera=nullptr;uint32_t id=0;
    if(!s.current_camera(camera,id,error))return false;
    if(!camera->set_limit(id,limit,value)){error=camera->error();return false;}
    error.clear();return true;
  };
  s.owners.camera_area.current_limits=[&s](std::array<int32_t,4> &out,std::string &error){
    FieldGameCameraRuntime *camera=nullptr;uint32_t id=0;
    if(!s.current_camera(camera,id,error))return false;
    out=camera->state(id)->limits;error.clear();return true;
  };
  s.owners.camera_area.set_camarea_offset=[&s](Vec2 offset,std::string &error){
    FieldGameCameraRuntime *camera=nullptr;uint32_t id=0;
    if(!s.current_camera(camera,id,error))return false;
    if(!camera->set_camarea_offset(id,offset)){error=camera->error();return false;}
    error.clear();return true;
  };
  s.owners.camera_area.await_idle_frame=[&s](uint64_t token,std::function<bool()> resume,std::string &error){
    if(!token||!resume||std::any_of(s.camera_idle_waiters.begin(),s.camera_idle_waiters.end(),[token](const auto &row){return row.first==token;}))
      return reject(error,"Camarea actual idle_frame waiter duplicate/invalid");
    s.camera_idle_waiters.emplace_back(token,std::move(resume));error.clear();return true;
  };
  s.owners.camera_area.cancel_idle_frame=[&s](uint64_t token,std::string &error){
    auto i=std::find_if(s.camera_idle_waiters.begin(),s.camera_idle_waiters.end(),[token](const auto &row){return row.first==token;});
    if(i==s.camera_idle_waiters.end())return reject(error,"Camarea idle waiter has no pending source continuation");
    s.camera_idle_waiters.erase(i);error.clear();return true;
  };
  s.owners.camera_area.observe=[&s](uint32_t stable,uint32_t cached,FieldCameraAreaObservation &out,std::string &error){
    const auto *row=s.sources.camera_area().record(stable);FieldObjectId area=0,shape=0,ref=0;
    if(!row||!s.source(stable,area,error)||!s.source(row->shape_id,shape,error)||
       !s.shapes_ready(stable,{row->shape_id},error))return false;
    FieldTransform t;
    if(!s.tree->world_transform(shape,t,error))return false;
    const auto *a=s.tree->state(area);
    // Native ReferenceRect has no script or property animation in this source
    // closure. Its immutable source margins remain the actual native values;
    // its Canvas transform/lifetime are read from the same allocated object.
    if(cached&&(!s.source(cached,ref,error)||cached!=row->reference_id||
        s.tree->descriptor(ref)->native_class!="ReferenceRect"||!s.tree->descriptor(ref)->script.empty()||
        !s.native.owns(ref)))return reject(error,"Camarea cached actual ReferenceRect owner differs");
    out.alive=a->alive&&!a->queued;out.ancestors_admitted=a->bound&&s.tree->state(a->parent)->bound;
    out.signals_admitted=s.camera_area_connected.count(stable)!=0;
    out.shape_admitted=s.native.owns(shape)&&s.source_shapes.count(row->shape_id)!=0;
    out.reference_id=cached;out.reference_alive=ref&&s.tree->state(ref)->alive&&!s.tree->state(ref)->queued;
    out.local_scale={std::hypot(a->local[0].x,a->local[0].y),std::hypot(a->local[1].x,a->local[1].y)};
    out.shape_extents=row->extents;out.shape_global_position=t[2];out.reference_margins=row->reference_margins;
    out.reference_size={row->reference_margins[2]-row->reference_margins[0],row->reference_margins[3]-row->reference_margins[1]};
    out.viewport_size=s.input.viewport.logical_size;error.clear();return true;
  };
  s.owners.actions.resolve=[&s](uint32_t origin,std::string_view path,uint32_t expected,uint64_t &out,std::string &error){
    FieldObjectId actual=0;if(!s.source(origin,actual,error)||!s.tree->get_node(actual,path,out,error))return false;
    if(bool(out)!=bool(expected)||(out&&(!s.tree->descriptor(out)||s.tree->descriptor(out)->id!=expected||
        s.continuation.registry()->tree_owner(out)!=s.tree)))return reject(error,"Scene action actual NodePath/source crossed");
    error.clear();return true;
  };
  s.owners.actions.describe=[&s](uint64_t actual,FieldSceneActionNodeState &out,std::string &error){
    const auto *node=s.tree->state(actual);const auto *desc=s.tree->descriptor(actual);
    const auto *ref=desc?s.sources.actions().reference(desc->id):nullptr;
    if(!node||!desc||!ref||s.continuation.registry()->tree_owner(actual)!=s.tree||!s.continuation.registry()->object_exists(actual))
      return reject(error,"Scene action cached actual object/source reference missing");
    out={node->alive&&!node->queued,ref->kind,node->parent};error.clear();return true;
  };
  s.owners.actions.admit_ready=[&s](const FieldSceneActionBinding &b,const FieldSceneActionsData &data,std::string &error){
    if(&data!=&s.sources.actions()||data.binding(b.id)!=&b)return reject(error,"Scene action Ready foreign data");
    std::vector<uint32_t>ids;for(const auto &shape:b.shapes)ids.push_back(shape.id);
    if(!s.shapes_ready(b.id,ids,error))return false;
    for(const auto &c:data.connections())if(c.kind==b.kind){
      if(!s.connect(b.id,c.signal,c.method,[&s,id=b.id](const State::Args &args,std::string &err){
        uint32_t body=0;return s.body_arg(args,body,err)&&s.consumers.runtime_instances().actions->body_enter(id,body,err);
      },error))return false;
    }
    error.clear();return true;
  };
  s.owners.interact.admit_ready=[&s](const FieldInteractDescriptor &b,std::string &error){
    if(s.sources.interact().record(b.id)!=&b||!s.source_ready_boundary(b.id,error))return false;
    if(b.prompt){const auto *p=s.consumers.runtime_instances().prompt->instance(b.prompt);
      if(!p||!p->ready)return reject(error,"Interact actual ButtonPrompt child not Ready");}
    error.clear();return true;
  };
  s.owners.door.resolve_onready=[&s](const FieldDoorDescriptor &d,const FieldDoorAudio &a,std::string &error){
    FieldDoorDescriptor expected;
    if(!s.sources.door().find(d.id,expected)||a.id!=d.audio||!s.shapes_ready(d.id,{d.shape},error))return false;
    for(auto stable:{d.marker,d.audio}){FieldObjectId actual=0;
      if(!s.source(stable,actual,error)||!s.tree->state(actual)->inside||!s.tree->state(actual)->ready_notified)
        return reject(error,"Door source onready child native lifecycle missing");
      if(stable==d.audio&&!s.audio.owns(actual))return reject(error,"Door source audio child owner missing");
    }
    error.clear();return true;
  };
  s.owners.door.connect_body=[&s](uint32_t stable,auto slot,std::string &error){
    const auto method=s.sources.door().body_method();
    if(method.empty()||!slot)return reject(error,"Door source body callback not admitted");
    return s.connect(stable,"body_entered",method,[&s,slot=std::move(slot)](const State::Args &args,std::string &err){
      uint32_t body=0;return s.body_arg(args,body,err)&&slot(body,err);
    },error);
  };
  s.owners.cutscene.connect_battle_to_overworld=[&s](uint32_t stable,std::string_view signal,std::string &error){
    const auto &policy=s.sources.cutscene().policy();FieldObjectId actual=0;
    if(signal!=policy.battle_signal||policy.battle_method.empty()||!s.source(stable,actual,error))return false;
    if(!s.continuation.signals()->connect(s.continuation.ui()->binding().object,signal,actual,policy.battle_method,0,{},error))return false;
    auto key=std::make_pair(actual,policy.battle_method);
    if(!s.methods.emplace(key,[&s,stable,actual](const State::Args &args,std::string &err){
      if(!args.empty())return reject(err,"Cutscene battle callback arguments differ");
      return s.consumers.runtime_instances().cutscene->battle_to_overworld(stable,err)&&s.tree->set_process(actual,false,false,err);
    }).second)return reject(error,"Cutscene source battle callback duplicate");
    return s.tree->set_process(actual,false,false,error);
  };
  s.owners.cutscene.body_is_current_player=[&s](uint32_t body,bool &out,std::string &error){
    std::shared_ptr<const GlobalLoadObjectArray> objects;
    if(!s.continuation.global()->core().array(FieldGlobalMemberRole::PartyObjects,objects,error)||!objects||objects->values.empty())return reject(error,"Cutscene actual global player unavailable");
    out=objects->values.front()==body;error.clear();return true;
  };
  s.owners.cutscene.query_ui=[&s](FieldCutsceneAreaUi &out,std::string &error){
    auto *ui=s.continuation.ui();const auto object=ui->binding().object;
    return ui->source_is_in_cutscene(object,out.cutscene,error)&&ui->source_is_in_battle(object,out.battle,error)&&ui->source_is_pause_menu_active(object,out.pause,error);
  };
  s.owners.cutscene.read_flag=[&s](std::string_view name,bool &out,std::string &error){bool present=false;if(!s.continuation.characters()->flags().read(false,name,present,out,error))return false;if(!present)out=false;error.clear();return true;};
  s.owners.cutscene.admit_ready=[&s](const FieldCutsceneAreaBinding &b,const FieldCutsceneAreaPolicy &policy,std::string &error){
    if(s.sources.cutscene().binding(b.id)!=&b||&policy!=&s.sources.cutscene().policy()||
       !s.shapes_ready(b.id,{b.shape_id},error))return false;
    for(const auto &c:policy.connections){
      if(!s.connect(b.id,c.signal,c.method,[&s,id=b.id,role=c.role](const State::Args &args,std::string &err){
        uint32_t body=0;if(!s.body_arg(args,body,err))return false;
        auto *runtime=s.consumers.runtime_instances().cutscene;
        if(!(role==1?runtime->body_enter(id,body,err):runtime->body_exit(id,body,err)))return false;
        const auto *state=runtime->state(id);FieldObjectId actual=0;
        if(!state||!s.source(id,actual,err))return false;
        return s.tree->set_process(actual,false,state->processing,err);
      },error))return false;
    }
    error.clear();return true;
  };
  s.owners.door_npc.admit_ready=[&s](const FieldDoorBinding &b,const FieldDoorPolicy &policy,std::string &error){
    if(s.sources.door_npc().binding(b.id)!=&b||&policy!=&s.sources.door_npc().policy()||
       !s.shapes_ready(b.id,{b.shape_id},error))return false;
    FieldObjectId audio=0;if(!s.source(b.audio_id,audio,error)||!s.audio.owns(audio)||!s.tree->state(audio)->ready_notified)
      return reject(error,"DoorNPC actual AudioStreamPlayer child not Ready");
    for(const auto &c:policy.connections){
      if(!s.connect(b.id,c.signal,c.method,[&s,id=b.id](const State::Args &args,std::string &err){
        uint32_t body=0;return s.body_arg(args,body,err)&&s.consumers.runtime_instances().door_npc->body_enter(id,body,err);
      },error))return false;
    }
    error.clear();return true;
  };
  s.owners.stepping.admit=[&s](const FieldSteppingSoundsData &data,std::string &error){
    if(&data!=&s.sources.stepping()||!data.valid()||!s.player_sources.effects||!s.player_sources.resources)
      return reject(error,"Stepping actual Player resource/source owner missing");
    for(const auto &effect:data.effects()){
      std::array<uint8_t,32>hash{};
      if(!s.player_sources.effects->source_hash(effect.scene,hash))return reject(error,"Stepping source effect missing actual Player resource closure");
    }
    error.clear();return true;
  };
  s.owners.stepping.admit_ready=[&s](const FieldSteppingBinding &b,const auto &connections,std::string &error){
    if(s.sources.stepping().binding(b.id)!=&b||&connections!=&s.sources.stepping().connections())return reject(error,"Stepping Ready foreign data");
    std::vector<uint32_t>ids;for(const auto &shape:b.shapes)ids.push_back(shape.id);
    if(!s.shapes_ready(b.id,ids,error))return false;
    for(const auto &c:connections){
      if(!s.connect(b.id,c.signal,c.method,[&s,id=b.id,role=c.role](const State::Args &args,std::string &err){
        uint32_t body=0;if(!s.body_arg(args,body,err))return false;
        auto *runtime=s.consumers.runtime_instances().stepping;
        return role==1?runtime->body_enter(id,body,err):runtime->body_exit(id,body,err);
      },error))return false;
    }
    error.clear();return true;
  };
  s.owners.stepping.geometry=[&s](uint32_t owner,const FieldSteppingShape &shape,FieldSteppingGeometry &out,std::string &error){
    auto *row=s.sources.stepping().binding(owner);
    if(!row||std::none_of(row->shapes.begin(),row->shapes.end(),[&](const auto &r){return &r==&shape;}))return reject(error,"Stepping actual shape binding missing");
    return s.geometry.live_polygon_parts(owner,shape.id,out.attached,out.disabled,out.parts,error);
  };
  s.owners.butterfly.admit_ready=[&s](const FieldButterflyBinding &b,std::string &error){
    if(s.sources.butterfly().binding(b.id)!=&b||!s.source_ready_boundary(b.id,error))return false;
    FieldObjectId actual=0;if(!s.source(b.id,actual,error)||!s.npc_world.native_entered(actual,error))return false;
    if(!s.source(b.area_id,actual,error)||!s.native.owns(actual)||!s.tree->state(actual)->ready_notified)return reject(error,"Butterfly actual source Area is not Ready");
    const auto *sprite=static_cast<const FieldCanvasRecord*>(nullptr);
    for(const auto &r:s.sources.canvas().records())if(r.owner==FieldCanvasOwner::Butterfly&&r.owner_id==b.id){
      if(sprite)return reject(error,"Butterfly actual source Sprite mapping ambiguous");
      sprite=&r;
    }
    if(!sprite||!s.source(sprite->id,actual,error)||!s.native.owns(actual)||!s.tree->state(actual)->ready_notified)return reject(error,"Butterfly actual source Sprite is not Ready");
    // Actual four source connections use the unique visibility and body owners.
    for(const auto &c:s.sources.visibility().connections())if(c.target==b.id){
      FieldObjectId emitter=0,target=0;bool connected=false;
      if(!s.source(c.emitter,emitter,error)||!s.source(b.id,target,error)||
         !s.continuation.signals()->connected(emitter,c.signal==1?"screen_entered":"screen_exited",target,c.method,connected,error)||!connected)return reject(error,"Butterfly actual source visibility connection missing");
    }
    for(const auto &c:s.sources.butterfly().connections()){
      FieldObjectId emitter=0,target=0;if(!s.source(b.area_id,emitter,error)||!s.source(b.id,target,error))return false;
      if(!s.continuation.signals()->connect(emitter,c.signal,target,c.method,0,{},error))return false;
      if(!s.methods.emplace(std::make_pair(target,c.method),[&s,id=b.id,role=c.role](const State::Args&args,std::string&err){uint32_t body=0;if(!s.body_arg(args,body,err))return false;auto*r=s.consumers.runtime_instances().butterfly;return role==1?r->body_enter(id,body,err):r->body_exit(id,body,err);}).second)return reject(error,"Butterfly source Area method duplicate");
    }
    error.clear();return true;
  };
  s.owners.butterfly.body_snapshot=[&s](uint32_t body,FieldButterflyBody&out,std::string&error){
    if(!s.continuation.registry()->object_exists(body))return reject(error,"Butterfly body actual ObjectDB identity missing");
    std::shared_ptr<const GlobalLoadObjectArray>objects;
    if(!s.continuation.global()->core().array(FieldGlobalMemberRole::PartyObjects,objects,error)||!objects||objects->values.empty())return false;
    out.global_player=objects->values.front()==body;out.party_member_player=out.global_player;
    auto actual=s.continuation.registry()->tree_owner(body);FieldTransform world;
    if(!actual||!actual->world_transform(body,world,error))return false;
    out.world=world[2];
    if(out.party_member_player&&!s.player_boolean(PlayerMotionField::Walking,out.walking,error))return false;
    error.clear();return true;
  };
  s.owners.butterfly.publish=[&s](const FieldButterflyBinding&b,const FieldButterflyState&body,std::string&error){
    if(s.sources.butterfly().binding(b.id)!=&b||s.consumers.runtime_instances().butterfly->state(b.id)!=&body||!s.sources.butterfly().asset(body.asset)||body.frame>=s.sources.butterfly().frames())return reject(error,"Butterfly actual source Sprite state differs");
    const FieldCanvasRecord*sprite=nullptr;for(const auto&r:s.sources.canvas().records())if(r.owner==FieldCanvasOwner::Butterfly&&r.owner_id==b.id){if(sprite)return reject(error,"Butterfly source Sprite ambiguous");sprite=&r;}
    FieldObjectId id=0;if(!sprite||!s.source(sprite->id,id,error))return false;
    auto t=s.tree->state(id)->local;t[2]=body.sprite_position;
    return s.tree->set_local(id,t,error);
  };
  s.owners.npc.context=[&s](uint32_t id,FieldNpcContext &out,std::string &error){
    FieldObjectId actual=0;if(!s.source(id,actual,error))return false;
    auto *ui=s.continuation.ui();const auto object=ui->binding().object;
    if(!s.player_boolean(PlayerMotionField::Paused,out.player_paused,error)||
       !ui->source_is_in_battle(object,out.in_battle,error)||
       !ui->source_is_in_cutscene(object,out.cutscene,error)||
       !ui->source_stack_empty(object,out.stack_empty,error))return false;
    out.debug_build=s.input.actual_debug_build;
    out.ancestor_visible=s.tree->visible_in_tree(actual);error.clear();return true;
  };
  PodunkNpcWorldPorts world;
  world.source=s.owners.npc;world.player=&s.player.body();world.global=&global;world.actual_debug_build=s.input.actual_debug_build;
  world.characters=s.continuation.characters();world.signals=&bus;
  world.map_gates=[&s](uint32_t id){return s.loop.scripts().lifecycle().gate(id);};
  world.return_timer=[&s](auto npc,double seconds,auto waiter,auto &error){return s.return_timers.create(npc,seconds,waiter,error);};
  world.persistent=[&global](auto object,bool &out,auto &error){
    std::shared_ptr<const GlobalLoadObjectArray> actual;
    if(!global.array(FieldGlobalMemberRole::Persistent,actual,error)||!actual)return false;
    out=std::find(actual->values.begin(),actual->values.end(),object)!=actual->values.end();error.clear();return true;
  };
  if(!s.npc_world.prepare(s.sources.npc_world(),s.sources.tree(),s.sources.npc(),*s.tree,r,
        s.geometry,s.map,*cores.npc,*cores.sprite,s.native,s.loop.timers(),std::move(world),e)||
     !s.npc_animation.prepare(s.sources.tree(),s.sources.sprite(),s.sources.npc(),*s.tree,r,*cores.sprite,*cores.npc,e)||
     !s.return_timers.prepare(s.native_root,s.sources.npc_world(),s.sources.npc(),r,bus,*s.tree,*cores.npc,s.npc_world,e)||
     !s.butterfly_animation.prepare(s.sources.butterfly(),s.sources.tree(),*s.tree,r,*cores.butterfly,s.npc_world,e)||
     !s.butterfly_timers.prepare(s.sources.butterfly(),s.sources.tree(),s.sources.timers(),*s.tree,r,s.loop.timers(),bus,*cores.butterfly,e)||
     !s.prompts.prepare(s.sources.prompt_native(),s.sources.tree(),s.sources.prompt(),*cores.prompt,
         *s.tree,r,bus,s.continuation.characters()->runtime(),s.player,s.input.equipment,*s.input.font,s.input.continuation.romfs_root.c_str(),e)||
     !s.clips.prepare(s.sources.clips(),s.sources.tree(),*s.tree,r,bus,s.loop.timers(),s.native,
         *cores.openable,*cores.present,*cores.emote,*cores.bush,e))return s.fail(e);
  SceneLeafNativeSources ls{s.sources.tree(),s.sources.camera(),s.sources.transitions(),
      s.sources.birds(),s.sources.dropped(),s.sources.payphone(),s.sources.melody()};
  if(!s.load(PodunkPackRole::SceneLeafNative,s.leaf_data,e,ls))return s.fail(e);
  if(!s.leaves.prepare(s.leaf_data,ls,*s.tree,r,bus,s.native,*cores.camera,*cores.transitions,
         *cores.birds,*cores.dropped,*cores.payphone,*cores.melody,e)||
     !s.cameras.prepare(s.leaf_data,s.sources.tree(),*s.tree,r,root,global,*cores.camera,*cores.arrows,s.input.viewport,e))return s.fail(e);
  s.owners.npc=s.npc_world.source_host();
  s.owners.sprite_observed=[&s](auto id,const auto &pose,auto &error){return s.npc_animation.observe_sprite(id,pose,error);};
  if(!s.npc_animation.apply(s.owners.sprite,e)||!s.butterfly_animation.apply(s.owners.butterfly,e)||
     !s.butterfly_timers.apply(s.owners.butterfly,e)||!s.callbacks.prepare(s.sources.signals(),ci,e)||
     !s.operations.prepare({&s.sources,&s.continuation,s.tree.get(),&s.player,ps.motion.get(),
         &s.loop.scripts().lifecycle(),s.sources.signals().party_member_declaration(),s.input.actual_debug_build,s.owners.scene.map_possessed},e))return s.fail(e);
  s.owners.scene=s.operations.ops();
  if(!s.callbacks.apply(s.owners,e)||!s.prompts.apply(s.owners.prompt,e)||
     !s.clips.apply(s.owners.openable,s.owners.present,s.owners.emote,s.owners.bush,e)||
     !s.cameras.apply(s.owners.camera,e)||
     !s.leaves.apply(s.owners.camera,s.owners.birds,s.owners.payphone,s.owners.melody,e))return s.fail(e);
  auto audio=s.input.audio;audio.server=s.audio_server->media_host();
  audio.listeners=[&s](FieldObjectId emitter,std::vector<PodunkSceneAudioListener> &out,std::string &error){
    auto *node=s.tree->state(emitter);auto &root=*s.continuation.native_root();
    const auto &view=root.viewport();
    if(!node||!node->alive||!node->inside||s.continuation.registry()->tree_owner(emitter)!=s.tree||
       !view.inside||!view.active||!view.world_registered||view.failed||
       !s.continuation.registry()->object_exists(root.viewport_object()))
      return reject(error,"Positional audio requires the same entered World2D/Viewport");
    for(const auto &r:s.sources.tree().records())
      if(r.native_class=="AudioListener2D")
        return reject(error,"Explicit source AudioListener2D native owner is pending");
    PodunkSceneAudioListener listener;
    listener.viewport=root.viewport_object();listener.enabled=view.audio_listener_2d;
    listener.screen_size=view.size;listener.canvas=view.canvas;
    out={listener};error.clear();return true;
  };
  audio.area_bus=[&s](Vec2 position,uint32_t mask,std::string &bus,std::string &error){
    if(!std::isfinite(position.x)||!std::isfinite(position.y)||bus.empty())
      return reject(error,"Source audio Area bus query parameters rejected");
    // The complete admitted static source has no audio-bus overriding Area.
    // Newly constructed grass Areas also keep the native default override off.
    // With no eligible override there is no bus mutation; never approximate
    // a future overriding Area with a rectangle or an always-Master result.
    for(uint32_t i=0;i<s.sources.geometry().owner_count();++i){
      const auto owner=s.sources.geometry().owner(i);
      if(owner.kind==4&&(owner.flags&8)&&(owner.layer&mask))
        return reject(error,"Source audio overriding Area requires exact point query owner");
    }
    error.clear();return true;
  };
  if(!s.audio.prepare(s.sources.audio(),*s.tree,r,*s.continuation.audio(),std::move(audio),e))return s.fail(e);
  PodunkVisibilityNativeOwners vo;
  vo.animation_active=[&s](auto id,bool active,auto &error){
    if(s.butterfly_animation.owns(id))return s.butterfly_animation.set_active(id,active,error);
    return reject(error,"Visibility activation has no actual AnimationPlayer owner");
  };
  if(!s.visibility.prepare(s.sources.visibility(),s.sources.tree(),*s.tree,r,bus,root,
       s.loop.scripts(),cores,std::move(vo),e)||
     !s.ready_native.prepare({&s.sources,s.tree.get(),&r,&bus,&s.native,&s.consumers,&s.visibility,
         [&s](auto stable,auto &actual,auto &error){return s.source(stable,actual,error);}},e)||
     !s.ready_native.apply(s.owners,e)||
     !s.ready_interaction.prepare({ci,cores,&s.physics,&s.audio,ps.motion.get(),
         [&s](auto stable,auto &actual,auto &error){return s.source(stable,actual,error);},
         [&s](bool &paused,bool &update,std::string &error){paused=s.tree_paused;update=s.update_pending;error.clear();return true;}},e)||
     !s.ready_interaction.apply(s.owners,e)||
     !s.ready_transition.prepare({&s.sources,&s.continuation,&s.player,&s.player_sources,s.tree.get(),
         &s.geometry,&s.map,&s.native,[&s](uint32_t id){return s.loop.scripts().lifecycle().gate(id);},
         [&s](auto stable,auto &actual,auto &error){return s.source(stable,actual,error);}},e)||
     !s.ready_transition.apply(s.owners.transitions,e))return s.fail(e);
  PodunkMickInput mick;
  mick.continuation=&s.continuation;mick.bundle=&s.input.continuation.destination->bundle();
  mick.sources=&s.sources;mick.programmes=s.input.programme;mick.basement=s.input.basement;
  mick.psi=s.input.psi;mick.choice_data=s.input.choices;mick.choices=s.input.choice_runtime;
  mick.locale=s.input.locale;mick.printer=s.input.printer;mick.scene=&s.loop.scripts().lifecycle();mick.npc=cores.npc;
  mick.state=&s.programme_state;mick.player=&s.player;mick.player_camera=&s.player_camera;
  mick.cameras=&s.cameras;mick.audio_server=s.audio_server;mick.timers=&s.loop.timers().core();
  mick.geometry=&s.geometry;mick.physics=&s.physics;mick.bars=s.input.bars;mick.fade=s.input.fade;
  mick.house_renderer=s.input.house_renderer;mick.font=s.input.text_renderer;mick.source_font=s.input.font;
  mick.motion=ps.motion.get();mick.house=s.input.continuation.house_data;
  mick.asset_root=s.input.continuation.romfs_root;mick.controls=s.input.controls;
  mick.query_input=s.input.query_input;
  if(!s.mick.prepare(std::move(mick),e))return s.fail(e);
  s.mick.apply_npc(s.owners.npc);
  if(!s.consumers.initialize_owners(s.owners,e)||
     !s.consumers.bind_scripts(s.loop.scripts(),e))return s.fail(e);
  s.bird_canvas.owner=&s;s.butterfly_canvas.owner=&s;
  if(!s.bird_canvas.gpu.load(s.sources.birds(),s.input.continuation.romfs_root.c_str(),e)||!s.butterfly_canvas.gpu.load(s.sources.butterfly(),s.input.continuation.romfs_root.c_str(),e))return s.fail(e);
  s.prepared=true;e.clear();return true;
}
bool PodunkHouseExit::construct(std::string &e) {
  auto &s=*state_;
  if(!s.prepared||s.constructed||s.failed||s.grass)return reject(e,"House exit factory actual grass/session owner rejected");
  auto &r=*s.continuation.registry();auto &global=s.continuation.global()->core();
  auto cores=s.consumers.runtime_instances();
  if(!PodunkConcreteSceneGrassFactory::create(s.sources.grass_native(),s.sources.grass(),*cores.grass,
      s.tree,r,*s.continuation.signals(),s.geometry,s.physics,s.input.continuation.romfs_root.c_str(),s.grass,e))return s.fail(e);
  PodunkSceneLoopInput in;
  in.sources=&s.sources;in.continuation=&s.continuation;in.tree=s.tree;
  in.native=&s.native;in.visibility=&s.visibility;in.npc_world=&s.npc_world;
  in.prompts=&s.prompts;in.butterfly_animation=&s.butterfly_animation;
  in.return_timers=&s.return_timers;in.butterfly_timers=&s.butterfly_timers;
  in.clip_native=&s.clips;in.audio_server=s.audio_server;in.player=&s.player;
  in.leaf_native=&s.leaves;in.cameras=&s.cameras;
  in.house_door_continuation=&s.old_door;
  in.physics=&s.physics;in.map=&s.map;in.geometry=&s.geometry;
  in.consumers=s.consumers.consumers();in.scene_ops=s.consumers.ops();
  in.canvas=s.canvas;in.materials=&s.materials;in.grass=s.grass;
  in.asset_root=s.input.continuation.romfs_root;
  in.mechanisms={&s.visibility,&s.npc_world,&s.prompts,&s.butterfly_animation,
      &s.clips,&s.npc_animation,&s.audio,&s.leaves,&s.cameras,&s.ready_transition};
  in.foreign_candidate=[&s](const auto &node,const auto &identity){return s.mick.candidate(node,identity);};
  in.foreign_owned=[&s](auto id){return s.mick.owns(id);};
  in.foreign_construct=[&s](auto id,const auto &node,const auto &identity,auto &error){return s.mick.construct(id,node,identity,error);};
  in.foreign_bind=[&s](auto id,const auto &node,auto &binding,auto &error){return s.mick.bind(id,node,binding,error);};
  in.foreign_phase=[&s](auto id,const auto &binding,auto phase,float dt,bool paused,bool update,auto &error){return s.mick.phase(id,binding,phase,dt,paused,update,error);};
  in.foreign_deferred=[&s](const auto &message,auto &error){return s.mick.deferred(message,error);};
  in.foreign_release=[&s](auto id,auto &error){return s.mick.release(id,error);};
  in.foreign_emits_ready=[&s](auto id){return s.mick.emits_ready(id);};
  in.source_input_handled=[&s](){return s.mick.input_handled();};
  in.allocation_observed=[&s](auto id,const auto &node,const auto &identity,auto &error){
    const auto *actual=s.tree->descriptor(id);
    if(!actual||actual->id!=node.id||s.continuation.registry()->tree_owner(id)!=s.tree||
       identity.scene_id!=s.sources.tree().identity().scene_id||
       identity.source_sha256!=s.sources.tree().identity().source_sha256||
       identity.upstream_commit!=s.sources.tree().identity().upstream_commit||
       !s.allocated.emplace(node.id,id).second)
      return reject(error,"Scene allocation observer actual source identity differs");
    // The only script-created native object in this source factory is the
    // CharacterSprite's checked AnimationTree.new. Its actual native owner
    // validates the exact caller-derived descriptor and keeps its unique clock.
    // It is not a static Area script node or a new signal callback target.
    if(!s.sources.tree().record(node.id)&&s.npc_animation.owns(node)&&
        node.native_class=="AnimationTree"&&node.script.empty()){error.clear();return true;}
    return s.operations.observe_allocated(id,node,identity,error)&&
       s.callbacks.observe_allocated(id,node,identity,error);
  };
  in.source_signals=[&s](auto id,auto name,auto &arity,auto &error){
    if(s.player_camera.tween_owned(id))return s.player_camera.tween_declaration(id,name,arity,error);
    if(s.mick.owns(id))return s.mick.declaration(id,name,arity,error);
    if(s.leaves.owns(id))return s.leaves.declaration(id,name,arity,error);
    if(s.audio.owns(id))return s.audio.signal_declaration(id,name,arity,error);
    std::string transition_error;
    if(s.ready_transition.declaration(id,name,arity,transition_error)){error.clear();return true;}
    return s.callbacks.declaration(id,name,arity,error);
  };
  in.source_method_owned=[&s](const auto &message){return s.ready_native.method_owned(message)||s.ready_interaction.handles(message)||s.methods.count({message.object,message.member})||s.callbacks.handles(message);};
  in.source_methods=[&s](const auto &message,auto &error){
    if(s.ready_native.method_owned(message))return s.ready_native.dispatch(message,error);
    if(s.ready_interaction.handles(message))return s.ready_interaction.deferred(message,error);
    auto i=s.methods.find({message.object,message.member});
    return i!=s.methods.end()?i->second(message.args,error):s.callbacks.dispatch(message,error);
  };
  if(!s.loop.construct(std::move(in),e))return s.fail(e);
  if(!s.physics.prepare(*s.player_sources.initialization,*s.tree,r,s.geometry,
      *s.continuation.signals(),[&s](auto source,auto &actual,auto &error){return s.source(source,actual,error);},e))return s.fail(e);
  auto ports=std::move(s.input.camera_ports);
  ports.ui=s.continuation.ui();ports.ui_namespace=r.data();
  ports.controls=s.input.controls;ports.input=s.input.query_input;
  ports.in_battle=[&s](auto id,bool &out,auto &error){return s.continuation.ui()->source_is_in_battle(id,out,error);};
  ports.native_current=[&s](auto &id,auto &error){return s.cameras.native_current(id,error);};
  ports.make_current=[&s](auto id,auto &error){return s.cameras.make_current(id,error);};
  ports.current_snapshot=[&s](auto id,auto &out,auto &error){return s.cameras.current_snapshot(id,out,error);};
  ports.geometry_admitted=[&s](auto area,auto shape,auto &error){return s.physics.geometry_admitted(area,shape,error);};
  ports.connect_player=[&s](auto camera,auto actual_player,auto ui,const auto &data,
                           auto stop,auto pause,auto &error){
    const auto *d=s.tree->descriptor(camera);const auto *n=s.tree->state(camera);
    if(&data!=&s.player_sources.children->camera()||!d||!n||!n->inside||
       n->parent!=actual_player||actual_player!=s.player.body().object()||
       ui!=s.continuation.ui()->binding().object||!stop||!pause||
       s.player_sources.children->camera_connections().size()!=2)
      return reject(error,"Player Camera source connection owner differs");
    for(const auto &c:s.player_sources.children->camera_connections()){
      auto sender=c.role==1?ui:actual_player;
      if(s.methods.count({camera,c.method})||
         !s.continuation.signals()->connect(sender,c.signal,camera,c.method,0,{},error))return false;
      auto callback=c.role==1?stop:pause;
      s.methods.emplace(std::make_pair(camera,c.method),[callback](const State::Args &args,auto &err){
        if(!args.empty())return reject(err,"Player Camera source callback arity differs");
        if(!callback())return reject(err,"Player Camera actual source callback failed");
        err.clear();return true;
      });
    }
    error.clear();return true;
  };
  ports.listeners_admitted=[&s](auto camera,const auto &data,auto &error){
    const auto *d=s.tree->descriptor(camera);const auto *record=s.player_sources.children->record(3);
    const auto *n=s.tree->state(camera);
    if(&data!=&s.player_sources.children->camera()||!d||!record||!n||!n->inside||
       d->id!=record->id||d->script_sha!=record->script_sha||
       n->parent!=s.player.body().object()||s.player_sources.children->camera_connections().size()!=2)
      return reject(error,"Player Camera typed source listener owner rejected");
    for(const auto &c:s.player_sources.children->camera_connections()){
      uint32_t arity=0;auto sender=c.role==1?s.continuation.ui()->binding().object:s.player.body().object();
      if(!s.continuation.signal_declaration(sender,c.signal,arity,error)||arity)return false;
      if(s.methods.count({camera,c.method})){
        bool connected=false;
        if(!s.continuation.signals()->connected(sender,c.signal,camera,c.method,connected,error)||!connected)
          return reject(error,"Player Camera source listener detached from actual SignalBus");
      }
    }
    error.clear();return true;
  };
  if(!s.player_camera.prepare(*s.player_sources.children,*s.player_sources.initialization,
      *s.player_sources.motion,*s.tree,r,*s.continuation.native_root(),global,s.player.body(),
      s.player.children(),s.player.animations(),s.input.viewport,std::move(ports),e)||
     !s.player_camera.bind_tweens(s.sources.grass_native(),*s.continuation.signals(),e)||
     !s.physics.bind_camera(s.player_camera,e)||!s.cameras.register_player(s.player_camera,e)||
     !s.effects.initialize(*s.player_sources.effects,*s.player_sources.initialization,
      *s.player_sources.resources,r,global,*s.continuation.signals(),s.player.resources(),s.player.animations(),e))return s.fail(e);
  PodunkPlayerServices services;
  services.registry=&r;services.global=&global;
  services.characters=&s.continuation.characters()->runtime();
  services.character_data=s.continuation.character_data();services.statuses=s.continuation.status_runtime();
  services.random=s.continuation.random();services.audio=s.continuation.audio();
  services.map=&s.map;services.geometry=&s.geometry;services.world=&s.physics;
  services.effect_owners=&s.effects;services.audio_server=s.audio_server->media_host();
  services.camera=s.player_camera.source_host();
  services.motion.telepathy_effect=[&s](auto target,bool enabled,auto &error){
    return s.mick.telepathy_effect(target,enabled,error);
  };
  if(!s.player_services.prepare({&s.continuation,&s.player_sources,&s.player,s.tree.get(),
       &s.consumers,&s.loop.scripts(),&s.sources.lifecycle(),&s.preloads,s.continuation.named_sfx(),std::move(services),s.input.controls,s.input.query_input},e))return s.fail(e);
  if(!s.player.prepare(s.player_sources,s.player_services.services(),s.tree,s.input.continuation.romfs_root.c_str(),e)||
     !s.effects.bind_scripts(s.player.effects(),e))return s.fail(e);
  FieldObjectId actual=0;std::shared_ptr<FieldNodeTreeRuntime> owning;
  if(!s.player.instantiate(*s.player_sources.initialization,owning,actual,e)||
     owning!=s.tree||actual!=s.player.body().object()||!s.player.assembled())return s.fail(e);
  auto *node=s.tree->state(actual);
  if(!node||node->inside||node->parent||node->ready_notified)return s.fail(e);
  std::shared_ptr<const GlobalLoadObjectArray> party;
  if(!global.array(FieldGlobalMemberRole::PartyObjects,party,e)||!party||!party->values.empty()||
     !global.append_array(FieldGlobalMemberRole::PartyObjects,actual,e))return s.fail(e);
  auto local=node->local;local[2]=s.input.continuation.house->world.player().position;
  if(!s.tree->set_local(actual,local,e)||!s.native.bind_foreign(s.player,*s.player_sources.initialization,
       s.effects,*s.player_sources.effects,e)||!s.native.bind_canvas_leaf(s.bird_canvas,e)||!s.native.bind_canvas_leaf(s.butterfly_canvas,e))return s.fail(e);
  if(!s.programme_state.prepare({&s.continuation,s.programme_inventory.host(),
      s.input.programme,s.input.basement,cores.npc,s.tree.get(),
      &s.loop.scripts().lifecycle(),actual,s.input.mutable_session},e))return s.fail(e);
  auto host=s.input.house_door;
  host.observe=[&s](FieldDoorContext &out,std::string &error){
    auto &global=s.continuation.global()->core();
    out.player=s.player.body().object();
    if(!out.player||!s.continuation.registry()->object_exists(out.player)||
       !global.boolean(FieldGlobalMemberRole::EnteringDoor,out.entering,error)||
       !s.continuation.ui()->source_is_in_cutscene(s.continuation.ui()->binding().object,out.in_cutscene,error))return false;
    const auto &old=s.input.continuation.destination->exit();
    out.current_scene_name=s.current_published?std::string(s.sources.lifecycle().string(s.sources.lifecycle().area().name)):
       std::string(old.old_root_name());
    if(out.current_scene_name.empty())return reject(error,"House Door actual scene source name missing");
    error.clear();return true;
  };
  host.flag_exists=[&s](std::string_view key,bool &present,std::string &error){bool value=false;
    return s.continuation.characters()->flags().read(false,key,present,value,error);
  };
  host.write_flag=[&s](std::string_view key,bool value,std::string &error){
    return s.continuation.characters()->flags().write(false,key,value,error);
  };
  host.play_audio=[&s](uint32_t source,std::string_view path,const auto &sha,std::string &error){
    const auto &data=s.input.continuation.destination->exit();
    const auto a=data.audio(source);AudioAsset asset;std::array<uint8_t,32> proof{};
    const auto d=data.door(0);
    if(a.id!=source||(path!=data.string(d.sound)&&path!=data.string(d.end_sound))||
       !data.source_hash(path,proof)||proof!=sha||!s.old_door.audio_object()||
       !s.continuation.registry()->object_exists(s.old_door.audio_object())||
       !s.continuation.audio()->source_asset(path,asset,error)||asset.source_sha256!=sha)
      return reject(error,"House Door actual AudioStreamPlayer source/bank differs");
    return s.continuation.audio()->play(asset.stable_id,AudioLane::Effect,error);
  };
  host.emit=[&s](uint32_t source,FieldDoorSignal signal,std::string &error){
    const auto &data=s.input.continuation.destination->exit();
    auto name=data.door_signal(signal);
    if(source!=data.door(0).id||name.empty()||!s.old_door.object())
      return reject(error,"House Door signal actual source identity rejected");
    return s.continuation.signals()->emit(s.old_door.object(),name,{},error);
  };
  host.emit_scene_changed=[&s](std::string &error){
    auto &g=s.continuation.global()->core();auto name=s.input.continuation.destination->exit().scene_changed_signal();
    if(name.empty()||!g.data()||std::find(g.data()->signals().begin(),g.data()->signals().end(),name)==g.data()->signals().end())
      return reject(error,"Source scene_changed global declaration differs");
    return s.continuation.signals()->emit(g.owner(),name,{},error);
  };
  host.clear_enemies=[&s](std::string &error){return s.continuation.ui()->source_clear_on_screen_enemies(error);};
  host.set_player_global_position=[&s](uint64_t id,Vec2 position,std::string &error){
    auto *node=s.tree->state(id);
    if(id!=s.player.body().object()||!node||!node->alive)
      return reject(error,"House Door warp targets a different actual Player");
    FieldTransform parent={Vec2{1,0},Vec2{0,1},Vec2{0,0}};
    if(node->parent&&!s.tree->world_transform(node->parent,parent,error))return false;
    float det=parent[0].x*parent[1].y-parent[0].y*parent[1].x;
    if(!std::isfinite(det)||det==0)return reject(error,"House Door player parent transform singular");
    Vec2 p{position.x-parent[2].x,position.y-parent[2].y};auto local=node->local;
    local[2]={(parent[1].y*p.x-parent[1].x*p.y)/det,(-parent[0].y*p.x+parent[0].x*p.y)/det};
    return s.tree->set_local(id,local,error);
  };
  host.camera_current_and_visible=[&s](uint64_t id,std::string &error){
    auto *record=s.player_sources.children->record(3);FieldObjectId camera=0;
    if(id!=s.player.body().object()||!record||!s.tree->get_node(id,record->path,camera,error))return false;
    return s.cameras.make_current(camera,error)&&s.tree->set_visible(camera,true,error);
  };
  host.resolve_onready=[&s](const auto &d,const auto &a,auto &error){return s.old_door.resolve_onready(d,a,error);};
  host.connect_body=[&s](auto id,auto slot,auto &error){return s.old_door.connect_body(id,std::move(slot),error);};
  host.pause_player=[&s](auto id,bool stop,bool idle,auto &error){
    if(id!=s.player.body().object())return reject(error,"House Door pause targets a different actual Player");
    if(s.player.ready_complete())return s.player_services.pause(stop,idle,true,error);
    return s.input.house_door.pause_player?s.input.house_door.pause_player(id,stop,idle,error):reject(error,"Initial House Door pause has no existing live House owner");
  };
  host.set_entering=[&s](bool value,auto &error){return s.continuation.global()->core().set_boolean(FieldGlobalMemberRole::EnteringDoor,value,error);};
  host.marker_world=[&s](auto id,auto &out,auto &error){return s.old_door.marker_world(id,out,error);};
  host.persistent=[&s](auto stable,bool add,auto &error){
    if(stable!=s.input.continuation.destination->exit().door(0).id||!s.old_door.object())
      return reject(error,"House Door persistent source/object differs");
    auto &g=s.continuation.global()->core();std::shared_ptr<const GlobalLoadObjectArray> list;
    if(!g.array(FieldGlobalMemberRole::Persistent,list,error)||!list)return false;
    auto values=list->values;auto i=std::find(values.begin(),values.end(),s.old_door.object());
    if(add){if(i!=values.end())return reject(error,"House Door persistent append duplicated");values.push_back(s.old_door.object());}
    else {if(i==values.end())return reject(error,"House Door persistent erase missing actual object");values.erase(i);}
    return g.assign_array(FieldGlobalMemberRole::Persistent,std::move(values),error);
  };
  host.direction_and_input=[&s](auto id,auto dir,auto &error){return id==s.player.body().object()?s.player_services.direction_and_input(dir,error):reject(error,"House Door direction actual Player differs");};
  host.update_party=[&s](auto id,auto &error){return id==s.player.body().object()?s.player_services.update_party_member(error):reject(error,"House Door breadcrumb actual Player differs");};
  host.unpause_player=[&s](auto id,auto &error){return id==s.player.body().object()?s.player_services.unpause(true,error):reject(error,"House Door unpause actual Player differs");};
  host.queue_free=[&s](auto id,auto &error){return s.old_door.queue_free(id,error);};
  host.set_respawn=[&s](auto &error){return s.player_services.respawn(error);};
  host.prepare_destination=[&s](const auto &door,auto &candidate,auto &error){
    const PodunkBundleData *bundle=nullptr;
    if(s.candidate||!s.input.continuation.destination->destination(door.id,bundle,error)||!bundle)
      return reject(error,"House Door destination candidate source rejected");
    std::array<uint8_t,32> sha{};
    auto path=s.input.continuation.destination->exit().string(door.target_path);
    if(!s.input.continuation.destination->exit().source_hash(path,sha))return false;
    candidate={++s.token,std::string(path),bundle->identity().upstream_commit,sha};
    s.candidate=true;s.step=0;error.clear();return true;
  };
  host.release_candidate=[&s](const auto &candidate,bool committed,auto &error){
    if(!s.candidate||candidate.token!=s.token||(committed&&!s.loop.ready()))
      return reject(error,"House Door actual candidate release rejected");
    s.candidate=false;error.clear();return true;
  };
  host.scene_step=[this](auto step,const auto &candidate,auto actual,auto p,auto d,auto &error){return scene_step(step,candidate,actual,p,d,error);};
  if(!s.door.initialize(s.input.continuation.destination->exit(),std::move(host),e)||
     !s.old_door.initialize(*s.door.data(),s.door.data()->door(0).id,
       const_cast<HouseRuntime&>(s.input.continuation.house->house),r,s.door,e)||
     !s.old_door.enter(e))return s.fail(e);
  s.constructed=true;e.clear();return true;
}
bool PodunkHouseExit::scene_step(FieldDoorSceneStep step,const FieldDoorCandidate &candidate,
    uint64_t actual,Vec2 position,Vec2 direction,std::string &e) {
  auto &s=*state_;
  if(!s.constructed||s.failed||!s.candidate||candidate.token!=s.token||
     actual!=s.player.body().object()||uint32_t(step)!=s.step)
    return reject(e,"House Door actual scene operation cursor rejected");
  bool result=false;
  switch(step){
  case FieldDoorSceneStep::DetachPlayer: {
    const auto *n=s.tree->state(actual);
    if(!n||!n->alive||n->queued)break;
    // The continuation Player is the same newly reconstructed native object;
    // before first destination it is already detached from the House backend.
    result=n->parent?s.tree->remove_child(n->parent,actual,e):!n->inside;
    if(!result&&e.empty())e="House Door actual Player detach failed";
    break;
  }
  case FieldDoorSceneStep::DisablePlayerCollisions:
    {
      FieldObjectId shape=0;
      result=s.tree->get_node(actual,s.player_sources.motion->lifecycle().collision_path,shape,e)&&
        s.physics.disabled(shape,true,e);
      break;
    }
  case FieldDoorSceneStep::DetachPersistent: {
    std::shared_ptr<const GlobalLoadObjectArray> list;
    if(!s.continuation.global()->core().array(FieldGlobalMemberRole::Persistent,list,e)||!list)break;
    result=true;
    for(auto id:list->values){
      if(id!=s.old_door.object()){result=reject(e,"House continuation persistent node has no actual native owner");break;}
      if(!s.old_door.detach(e)){result=false;break;}
    }
    break;
  }
  case FieldDoorSceneStep::InstanceDestination:
    result=s.tree->object_count()!=0&&s.tree->root()!=0&&!s.tree->state(s.tree->root())->inside;
    if(!result)e="House Door destination actual factory missing";
    break;
  case FieldDoorSceneStep::AssignCurrentScene:
    result=s.operations.publish_current_scene_before_enter(e);s.current_published=result;break;
  case FieldDoorSceneStep::LeaveOldArea: {
    const auto &old=s.input.continuation.destination->exit();
    const auto &next=s.sources.lifecycle();
    // The source native House has four Present listeners. Their audited
    // reset-area/reset-region conditions are both false on this same-region
    // transition. AreaRoom.leave_for itself only emits this comparison.
    result=old.house_continuation()&&!old.old_area_has_listeners()&&
      old.old_region()==next.string(next.area().region);
    if(!result)e="House source area_left branch is outside the checked continuation";
    break;
  }
  case FieldDoorSceneStep::InitParameters:
    result=s.input.continuation.destination->exit().empty_destination_parameters();
    if(!result)e="House Door destination parameters require an actual typed setter";
    break;
  case FieldDoorSceneStep::AddRootAndReady:
    result=s.loop.attach_scene(e);s.attached=result;break;
  case FieldDoorSceneStep::AddPlayer:
    result=s.operations.attach_existing_player(e);
    if(result)result=s.player_services.pause(false,true,false,e);
    s.player_attached=result;break;
  case FieldDoorSceneStep::CreateFollowers: {
    std::shared_ptr<const GlobalLoadObjectArray> party,npcs,objects;
    auto &g=s.continuation.global()->core();
    if(!g.array(FieldGlobalMemberRole::Party,party,e)||!g.array(FieldGlobalMemberRole::PartyNpcs,npcs,e)||
       !g.array(FieldGlobalMemberRole::PartyObjects,objects,e)||!party||!npcs||!objects)break;
    result=party->values.size()==1&&npcs->values.empty()&&objects->values==std::vector<FieldObjectId>{actual};
    if(!result)e="House continuation source follower factory is pending for actual multi-member party";
    break;
  }
  case FieldDoorSceneStep::ReparentPersistent: {
    std::shared_ptr<const GlobalLoadObjectArray> list;
    if(!s.continuation.global()->core().array(FieldGlobalMemberRole::Persistent,list,e)||!list)break;
    const auto *n=s.tree->state(actual);if(!n||!n->parent)break;
    result=true;
    for(auto id:list->values){
      if(id!=s.old_door.object()){result=reject(e,"House continuation persistent node has no same-ID migration owner");break;}
      if(!s.old_door.reparent(n->parent,e)){result=false;break;}
    }
    break;
  }
  case FieldDoorSceneStep::SetTreeCurrent:
    result=s.continuation.registry()->observe_tree_current_scene(s.tree->root(),e);break;
  case FieldDoorSceneStep::UpdateKeyIndicator: {
    const auto &data=s.sources.lifecycle();
    result=s.continuation.ui()->source_update_key_indicator(
      s.continuation.characters()->runtime(),data.string(data.area().region),e);
    break;
  }
  case FieldDoorSceneStep::EnablePlayerCollisions:
    result=s.player_services.collisions(true,e);break;
  case FieldDoorSceneStep::SetPartyPosition: {
    const auto *node=s.tree->state(actual);
    if(!node||!node->parent)break;
    auto local=node->local;local[2]=position;
    if(!s.tree->set_local(actual,local,e))break;
    auto &g=s.continuation.global()->core();
    std::shared_ptr<const FieldGlobalPartySpaceArray> crumbs;
    if(!g.party_space(crumbs,e)||!crumbs)break;
    result=true;
    if(crumbs->values.size()>1)
      for(size_t i=0;i<crumbs->values.size();++i){FieldGlobalPartySpaceValue removed;
        if(!g.push_front_party_space(position,e)||!g.pop_back_party_space(removed,e)){result=false;break;}
      }
    if(result&&(direction.x!=0||direction.y!=0))result=s.player_services.direction_and_input(direction,e);
    break;
  }
  default:
    if(!s.input.house_door.scene_step)e="House Door native House/follower/UI scene step has no actual owner";
    else result=s.input.house_door.scene_step(step,candidate,actual,position,direction,e);
    break;
  }
  if(!result)return s.fail(e);
  ++s.step;
  if(step==FieldDoorSceneStep::EnablePlayerCollisions){
    if(!s.loop.activate_after_player(e))return s.fail(e);
    s.activated=true;
  }
  e.clear();return true;
}
bool PodunkHouseExit::bind_house(HouseRuntime &house,std::string &e){
  auto &s=*state_;
  if(!s.constructed||s.door_bound||s.failed||&house!=&s.input.continuation.house->house)
    return reject(e,"House Door must bind the same current live House");
  if(!house.bind_scene_doors(*s.door.data(),s.door,[this](auto &id,auto &error){
      id=player_object();if(!id)return reject(error,"House Door actual Player unavailable");error.clear();return true;},e))return s.fail(e);
  s.door_bound=true;return true;
}
bool PodunkHouseExit::request(uint32_t id,std::string &e){return state_->door.body_entered(id,player_object(),e);}
bool PodunkHouseExit::enter(uint32_t id,std::string &e){return state_->door.enter(id,player_object(),e);}
bool PodunkHouseExit::fade_in_done(std::string &e){return state_->door.fade_in_done(e);}
bool PodunkHouseExit::deferred_commit(std::string &e){return state_->door.deferred_commit(e);}
bool PodunkHouseExit::tree_changed(std::string &e){return state_->door.tree_changed(e);}
bool PodunkHouseExit::fade_out_mostly_done(std::string &e){return state_->door.fade_out_mostly_done(e);}
bool PodunkHouseExit::door_idle(std::string &e){return state_->audio_server->pump(e)&&state_->door.idle_frame(e);}
bool PodunkHouseExit::physics_frame(uint64_t epoch,float dt,bool paused,std::string &e){
  state_->tree_paused=paused;
  return state_->continuation.named_sfx()->process(true,paused,e)&&state_->player_camera.begin_frame(epoch,paused,e)&&state_->loop.physics_frame(epoch,dt,paused,e);
}
bool PodunkHouseExit::idle_frame(uint64_t epoch,float dt,bool paused,bool update,std::string &e){
  auto &s=*state_;s.tree_paused=paused;s.update_pending=update;
  if(!epoch||epoch<=s.camera_idle_epoch)return reject(e,"House exit actual idle frame repeated or out of order");
  s.camera_idle_epoch=epoch;
  if(s.activated&&!s.mick.active()&&s.door.phase()==FieldDoorPhase::Done&&
     !s.mick.activate(e))return s.fail(e);
  if(s.mick.active()&&!s.mick.idle_begin(e))return s.fail(e);
  // SceneTree idle_frame is emitted before native/script idle traversal. Taking
  // the tail here keeps any waiter created during this traversal for next frame.
  auto waiters=std::move(s.camera_idle_waiters);s.camera_idle_waiters.clear();
  for(auto &row:waiters)if(!row.second()){e=s.consumers.runtime_instances().camera_area->error();return s.fail(e);}
  if(!s.audio_server->pump(e)||!s.continuation.named_sfx()->process(false,paused,e)||
     !s.loop.idle_frame(epoch,dt,paused,update,e))return s.fail(e);
  if(!s.player_camera.idle_tail(epoch,dt,paused,e))return s.fail(e);
  return !s.mick.active()||s.mick.idle_end(epoch,dt,paused,e);
}
bool PodunkHouseExit::input(uint32_t kind,const PlayerInputEvent &event,bool accept,bool paused,std::string &e){
  auto &s=*state_;
  if(s.mick.active()&&!s.mick.begin_input(event,e))return s.fail(e);
  const bool result=s.loop.input(kind,event,accept,paused,e);
  if(s.mick.active())s.mick.end_input();
  return result;
}
bool PodunkHouseExit::draw(uint64_t epoch,float dt,float time,std::string &e){
  auto &s=*state_;
  return s.loop.draw(epoch,dt,time,e)&&(!s.mick.active()||s.mick.draw(epoch,e));
}
bool PodunkHouseExit::end(std::string &e){
  auto &s=*state_;if(s.ended)return reject(e,"House exit teardown repeated");
  if(s.tree->root()&&s.tree->state(s.tree->root())->inside&&
      !s.continuation.native_root()->remove_child(s.tree->root(),e))return s.fail(e);
  if(!s.callbacks.disconnect(e)||!s.visibility.remove_viewport(e)||!s.audio.shutdown(e)||
      !s.return_timers.shutdown(e)||!s.materials.shutdown(e)||!s.native.shutdown(e))return s.fail(e);
  s.ended=true;e.clear();return true;
}
bool PodunkHouseExit::ready()const{return state_->activated&&!state_->failed&&!state_->ended&&state_->loop.ready();}
FieldObjectId PodunkHouseExit::player_object()const{return state_->player.body().object();}
const PlayerMotionData *PodunkHouseExit::motion_data()const{return state_->player_sources.motion.get();}
bool PodunkHouseExit::player_view_position(Vec2 &out,std::string &e)const{
  const auto &s=*state_;FieldObjectId camera=0;FieldGameCameraState view;FieldTransform player;
  auto &actual=const_cast<State&>(s);
  if(!s.player_attached||!actual.cameras.native_current(camera,e)||!camera||
     !s.cameras.current_snapshot(camera,view,e)||
     !s.tree->world_transform(actual.player.body().object(),player,e))
    return reject(e,"House exit focus requires same actual entered Player and current Viewport camera");
  out={player[2].x-view.screen_center.x+s.input.viewport.logical_size.x*.5f,
       player[2].y-view.screen_center.y+s.input.viewport.logical_size.y*.5f};
  e.clear();return true;
}
PodunkHouseContinuation *PodunkHouseExit::continuation(){return state_->continuation.initialized()?&state_->continuation:nullptr;}
const FieldSceneSources *PodunkHouseExit::sources()const{return state_->sources.valid()?&state_->sources:nullptr;}
FieldSceneConsumers PodunkHouseExit::consumers()const{return state_->consumers.runtime_instances();}
const std::shared_ptr<FieldNodeTreeRuntime> &PodunkHouseExit::tree()const{return state_->tree;}
FieldGeometrySpace *PodunkHouseExit::geometry(){return &state_->geometry;}
FieldDoorRuntime *PodunkHouseExit::house_door(){return state_->door.data()?&state_->door:nullptr;}
PodunkInventoryHost *PodunkHouseExit::inventory(){return state_->programme_inventory.host();}
PodunkProgrammeState *PodunkHouseExit::programme_state(){return state_->constructed?&state_->programme_state:nullptr;}
FieldSceneHost *PodunkHouseExit::scene_lifecycle(){return state_->constructed?&state_->loop.scripts().lifecycle():nullptr;}
PodunkPlayerHost *PodunkHouseExit::player_host(){return state_->constructed?&state_->player:nullptr;}
const std::string &PodunkHouseExit::failure()const{return state_->failure;}
} // namespace encore::ctr
