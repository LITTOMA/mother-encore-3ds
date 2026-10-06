#pragma once
#include "encore/fresh_house.hpp"
#include "encore/basement_progression.hpp"
#include "music_region_service.hpp"
#include "encore/room_music_admission.hpp"
#include <algorithm>
#include <cmath>
#include <memory>
namespace encore::ctr {
// Per-scene owner. Preparation and Fresh SceneReady only admit a candidate;
// activate is the explicit post-scene-swap boundary that changes audio state.
class HouseMusicHost final {
 struct Contact {const upstream::BasementMusicBinding*geometry=nullptr;const upstream::MusicRegionBinding*binding=nullptr;bool inside=false,retired=false;};
 struct State {upstream::FreshHouseState*scene=nullptr;MusicRegionService*service=nullptr;const AudioPlayer*audio=nullptr;std::vector<Contact>contacts;bool ready=false,active=false;uint64_t epoch=0;};
 std::shared_ptr<State>state_;
 static bool fail(std::string&e,const char*s){e=s;return false;}
 static bool overlaps(upstream::RoomView room,upstream::Vec2 player,const upstream::BattleValue&g){
  const auto scene=room.scene();const upstream::Vec2 rect[]={{g.x-g.z,g.y-g.w},{g.x+g.z,g.y-g.w},{g.x+g.z,g.y+g.w},{g.x-g.z,g.y+g.w}};
  auto separated=[&](upstream::Vec2 axis){float amin=INFINITY,amax=-INFINITY,bmin=INFINITY,bmax=-INFINITY;
   for(uint32_t i=0;i<scene.actor_hull_count;++i){const auto p=room.vertex(scene.actor_hull_first+i);const float dot=(p.x+player.x)*axis.x+(p.y+player.y)*axis.y;amin=std::min(amin,dot);amax=std::max(amax,dot);}
   for(const auto p:rect){const float dot=p.x*axis.x+p.y*axis.y;bmin=std::min(bmin,dot);bmax=std::max(bmax,dot);}return amax<bmin||bmax<amin;};
  if(separated({1,0})||separated({0,1}))return false;
  for(uint32_t i=0;i<scene.actor_hull_count;++i){const auto a=room.vertex(scene.actor_hull_first+i),b=room.vertex(scene.actor_hull_first+(i+1)%scene.actor_hull_count);if(separated({a.y-b.y,b.x-a.x}))return false;}return true;
 }
 bool contacts(upstream::MusicRegionContext context,std::string&e){
  auto&s=*state_;auto&world=s.scene->world;context.is_player=true;context.in_cutscene=context.in_cutscene||world.cutscene_active();context.flag=[&world](std::string_view f){return world.story_flag(f);};
  for(auto&c:s.contacts){const auto&g=*c.geometry;if(c.retired)continue;
   if(!g.parent_disappear_flag.empty()&&world.story_flag(g.parent_disappear_flag)){if(!s.service->tree_exit(s.epoch,g.node,e))return false;c.retired=true;c.inside=false;continue;}
   const bool inside=!c.binding->disabled&&overlaps(world.content(),world.player().position,g.geometry);
   if(inside==c.inside)continue;c.inside=inside;
   if(inside){if(!s.service->area_enter(s.epoch,g.node,context,*s.audio,e))return false;}
   else if(!s.service->area_exit(s.epoch,g.node,context,e))return false;
  }e.clear();return true;
 }
public:
 bool prepare(upstream::FreshHouseState&scene,const upstream::RestoreData&restore,const upstream::MusicRegionData&music,const upstream::BasementProgressionData&geometry,MusicRegionService&service,const AudioPlayer&audio,std::string&e){
  if(state_||!scene.world.healthy())return fail(e,"House music candidate owner rejected");
  const auto room=scene.world.content();
  if(!upstream::admit_house_music_bindings(room,restore,geometry,music,e))return false;
  auto candidate=std::make_shared<State>();candidate->scene=&scene;candidate->service=&service;candidate->audio=&audio;
  for(const auto&g:geometry.music_regions()){
   const auto binding=std::find_if(music.regions().begin(),music.regions().end(),[&](const auto&r){return r.source_path==g.node;});
   candidate->contacts.push_back({&g,&*binding,false,false});
  }
  std::sort(candidate->contacts.begin(),candidate->contacts.end(),[](const Contact&a,const Contact&b){return a.geometry->source_ordinal<b.geometry->source_ordinal;});
  if(!scene.bind_music_adapter([candidate](const upstream::RestoreMusicArea&a){return std::any_of(candidate->contacts.begin(),candidate->contacts.end(),[&](const Contact&c){return c.geometry->node==a.source_path;});},[candidate](std::string&e){candidate->ready=true;e.clear();return true;}))return fail(e,"House music candidate ready owner already bound");
  state_=std::move(candidate);e.clear();return true;
 }
 bool activate(upstream::MusicRegionContext context,std::string&e){if(!state_||!state_->ready||state_->active||!state_->scene->world.healthy())return fail(e,"House music activate requires committed ready scene");auto&s=*state_;if(s.service->scene_epoch()==UINT64_MAX)return fail(e,"House music scene epoch exhausted");const auto epoch=s.service->scene_epoch()+1;if(!s.service->commit_scene(epoch,*s.audio,e)||!s.service->bind_room_history(s.scene->world.content(),e))return false;s.epoch=epoch;s.active=true;return contacts(context,e);}
 // The prior source idle wait completes before this frame's physical callbacks;
 // new exit waits are left pending until the following call.
 bool idle(upstream::MusicRegionContext context,std::string&e){if(!state_||!state_->active)return fail(e,"House music idle has no active source scene");if(!state_->service->idle_frame(state_->epoch,e))return false;return contacts(context,e);}
 bool finish(std::string&e){if(!state_){e.clear();return true;}auto&s=*state_;if(s.active){for(auto&c:s.contacts)if(!c.retired){if(!s.service->tree_exit(s.epoch,c.geometry->node,e))return false;c.retired=true;}if(!s.service->finish_scene(s.epoch,e))return false;s.active=false;}state_.reset();e.clear();return true;}
 bool active()const{return state_&&state_->active;}uint64_t epoch()const{return state_?state_->epoch:0;}
};
}
