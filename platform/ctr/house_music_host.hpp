#pragma once
#include "encore/fresh_house.hpp"
#include "encore/basement_progression.hpp"
#include "music_region_service.hpp"
#include <algorithm>
#include <cmath>
#include <memory>
#include <set>
namespace encore::ctr {
// Per-scene owner. Preparation and Fresh SceneReady only admit a candidate;
// activate is the explicit post-scene-swap boundary that changes audio state.
class HouseMusicHost final {
 struct Contact {const upstream::BasementMusicBinding*geometry=nullptr;const upstream::MusicRegionBinding*binding=nullptr;bool inside=false,retired=false;};
 struct State {upstream::FreshHouseState*scene=nullptr;MusicRegionService*service=nullptr;const AudioPlayer*audio=nullptr;std::vector<Contact>contacts;bool ready=false,active=false;uint64_t epoch=0;};
 std::shared_ptr<State>state_;
 static bool fail(std::string&e,const char*s){e=s;return false;}
 static bool flag_exists(upstream::RoomView room,std::string_view name){if(name.empty())return true;for(uint32_t i=0;i<room.flag_count();++i)if(room.string(room.flag(i).name_string)==name)return true;return false;}
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
  if(state_||!scene.world.healthy()||!restore.valid()||!music.valid()||!geometry.valid()||music.regions().size()!=geometry.music_regions().size()||restore.music_areas().size()!=music.regions().size())return fail(e,"House music candidate resources rejected");
  const auto room=scene.world.content();const auto rs=room.scene();if(rs.actor_hull_count<3)return fail(e,"House music player hull unavailable");
  const char*hex="0123456789abcdef";std::string pin;for(size_t i=56;i<76;++i){pin+=hex[room.bytes()[i]>>4];pin+=hex[room.bytes()[i]&15];}
  if(!geometry.bind_reviewed_commit(pin,e))return false;
  auto candidate=std::make_shared<State>();candidate->scene=&scene;candidate->service=&service;candidate->audio=&audio;
  std::set<uint32_t>ordinals;
  for(const auto&g:geometry.music_regions()){
   if(room.string(rs.source_scene_string)!=g.scene||!flag_exists(room,g.parent_disappear_flag)||!ordinals.insert(g.source_ordinal).second)return fail(e,"House music scene/lifecycle binding rejected");
   const auto binding=std::find_if(music.regions().begin(),music.regions().end(),[&](const auto&r){return r.source_path==g.node;});
   if(binding==music.regions().end()||binding->id!=g.region_id||binding->track_id!=g.track_id||binding->volume_db!=g.volume_db||binding->fadein_seconds!=g.fadein_seconds||binding->fadeout_seconds!=g.fadeout_seconds||!flag_exists(room,binding->appear_flag)||!flag_exists(room,binding->disappear_flag))return fail(e,"House music region identity/tuning rejected");
   const auto track=std::find_if(music.tracks().begin(),music.tracks().end(),[&](const auto&t){return t.id==g.track_id;});const auto area=std::find_if(restore.music_areas().begin(),restore.music_areas().end(),[&](const auto&a){return a.source_path==g.node;});
   if(track==music.tracks().end()||area==restore.music_areas().end()||track->source_path!=g.music||area->resource_path!=g.music||area->source_sha256!=track->source_sha||float(area->center.x)!=g.geometry.x||float(area->center.y)!=g.geometry.y||float(area->extents.x)!=g.geometry.z||float(area->extents.y)!=g.geometry.w||float(area->volume_db)!=g.volume_db||float(area->fadein_seconds)!=g.fadein_seconds||float(area->fadeout_seconds)!=g.fadeout_seconds)return fail(e,"House restore/music source geometry rejected");
   std::vector<std::pair<std::string,bool>>expected;if(!g.parent_disappear_flag.empty())expected.emplace_back(g.parent_disappear_flag,false);if(!binding->appear_flag.empty())expected.emplace_back(binding->appear_flag,true);if(!binding->disappear_flag.empty())expected.emplace_back(binding->disappear_flag,false);
   if(expected.size()!=area->conditions.size())return fail(e,"House restore music flag coverage rejected");for(size_t i=0;i<expected.size();++i)if(expected[i].first!=area->conditions[i].flag_name||expected[i].second!=area->conditions[i].expected_value)return fail(e,"House restore music flag binding rejected");
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
