#include "encore/fresh_house.hpp"
#include <cmath>
namespace encore::upstream {
bool prepare_fresh_house(const PreparedSessionRestore&saved,const RestoreData&data,RoomView room,HouseView house,BattleView font,PhoneView phone,SourceRandom&random,Vec2 viewport,std::unique_ptr<FreshHouseState>&output,std::string&error,const FreshHouseAdapters&adapters){
 auto fail=[&](const char*message){error=message;return false;};
 if(!saved.valid()||!data.valid()||!data.matches(room,house)||!font.valid()||!phone.valid())return fail("Fresh scene needs matching checked restore resources");
 auto next=std::make_unique<FreshHouseState>();
 if(adapters.prepare_world&&!adapters.prepare_world(*next,error))return false;
 if(!next->world.initialize_restored(room,saved.story_flags,saved.reviewed_flag_mutations,saved.position,saved.direction,viewport)){error=std::string("Fresh scene world initialization rejected: ")+next->world.error();return false;}
 next->world.attach_random(random);next->world.set_party_leader(saved.state.party.front());
 if(!next->presentation.begin(house,font,random))return fail(next->presentation.error());
 for(const auto&event:data.npc_event_positions()){
  if(next->world.story_flag(event.condition.flag_name)!=event.condition.expected_value)continue;
  const auto npc=house.npc(event.npc_index);
  if(!next->presentation.restore_npc_pose(event.npc_index,event.position,npc.default_direction)||!next->world.set_initial_actor_pose(event.actor_index,event.position,npc.default_direction))return fail("Fresh NPC event position rejected");
 }
 if(!next->house.initialize(house,next->world,next->presentation))return fail(next->house.error());
 if(adapters.bind_house&&!adapters.bind_house(*next,error))return false;
 if(!next->phone.initialize(phone)||!next->house.bind_phone(next->phone))return fail("Fresh phone initialization rejected");
 if(!next->house.restore_seen_dialogue(saved.seen_dialogue_keys)||!next->house.set_player_nickname(saved.state.characters.front().nickname))return fail("Fresh dialogue state rejected");
 if(adapters.prepare_music&&!adapters.prepare_music(*next,data,error))return false;
 for(const auto&area:data.music_areas()){
  if(next->music_claims_&&next->music_claims_(area))continue;
  if(std::abs(saved.position.x-area.center.x)>area.extents.x||std::abs(saved.position.y-area.center.y)>area.extents.y)continue;
  bool enabled=true;for(const auto&condition:area.conditions)enabled&=next->world.story_flag(condition.flag_name)==condition.expected_value;
  if(!enabled)continue;
  if(!area.supported)return fail("Restored position requires an unported source music area");
  if(next->music_pending_)return fail("Ambiguous source music-area entry order");
  next->music_pending_=true;next->music_resource_=area.room_resource_index;next->music_gain_=area.volume_db;next->music_fade_=area.fadein_seconds;
 }
 next->ready_pending_=true;output=std::move(next);error.clear();return true;
}
bool FreshHouseState::finish_scene_ready(){
 if(!ready_pending_)return world.healthy();
 if(ready_adapter_){std::string error;if(!ready_adapter_(error))return false;ready_adapter_={};}
 if(!world.end_scene_frame())return false;
 if(music_ready_){std::string error;if(!music_ready_(error))return false;music_ready_={};}
 if(music_pending_&&!world.start_area_music(music_resource_,music_gain_,music_fade_))return false;
 ready_pending_=music_pending_=false;return true;
}
}
