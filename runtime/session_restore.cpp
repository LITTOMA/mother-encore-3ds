#include "encore/session_restore.hpp"
#include <algorithm>

namespace encore::upstream { namespace {
bool reject(std::string& error,const char*message){error=message;return false;}
bool display_text(BattleView font,const std::string&text,std::string&error){
 for(unsigned char byte:text){
  if(byte<32||byte>126)return reject(error,"Session restore name requires unsupported text codepoint");
  bool found=false;
  for(uint32_t i=0;i<font.count(BattleSection::Glyphs);++i){
   const auto glyph=font.glyph(i);
   if(glyph.codepoint==byte&&glyph.advance>0){found=true;break;}
  }
  if(!found)return reject(error,"Session restore name glyph unavailable in checked font");
 }
 return true;
}
}

bool prepare_session_restore(const NativeSessionData&data,RoomView room,
 HouseView house,RoundView round,ItemView items,BattleView font,
 const SessionSnapshot&snapshot,PreparedSessionRestore&output,std::string&error){
 // All validation and construction uses a detached local candidate. No live
 // world, presentation, menu, save slot, or random state is reachable here.
 if(!validate_native_session_snapshot(data,room,house,round,items,snapshot,error))return false;
 if(!font.valid()||!font.count(BattleSection::Glyphs))return reject(error,"Session restore requires checked font glyphs");
 if(!display_text(font,snapshot.player_name,error)||!display_text(font,snapshot.favorite_food,error)||
    !display_text(font,snapshot.scene_label,error))return false;
 for(const auto&character:snapshot.characters)if(!display_text(font,character.nickname,error))return false;

 PreparedSessionRestore next;next.state=snapshot;
 next.position={float(snapshot.position_x),float(snapshot.position_y)};
 next.direction={float(snapshot.direction_x),float(snapshot.direction_y)};
 const auto&character=snapshot.characters.front();
 const auto row=std::find_if(data.levels().begin(),data.levels().end(),
  [&](const NativeSessionLevel&level){return level.level==character.level;});
 if(row==data.levels().end())return reject(error,"Session restore source level row unavailable");
 auto&stats=next.stats;
 stats.level=uint32_t(character.level);stats.experience=uint32_t(character.experience);
 stats.hp=int32_t(character.hp);stats.pp=int32_t(character.pp);
 stats.maxhp=row->stats[0];stats.maxpp=row->stats[1];stats.offense=row->stats[2];
 stats.defense=row->stats[3];stats.speed=row->stats[4];stats.iq=row->stats[5];stats.guts=row->stats[6];
 stats.cash=uint32_t(snapshot.cash);stats.bank=uint32_t(snapshot.bank);stats.earned_cash=uint32_t(snapshot.earned_cash);
 stats.learned_skills=character.learned_skills;

 std::vector<ItemInstance>instances;instances.reserve(character.inventory.size());
 for(const auto&saved:character.inventory){
  uint32_t resolved=item_no_index;
  for(uint32_t i=0;i<items.count(ItemSection::Definitions);++i){
   if(items.string(items.definition(i).source)==saved.item_id){
    if(resolved!=item_no_index)return reject(error,"Session restore ambiguous item identity");
    resolved=i;
   }
  }
  if(resolved==item_no_index)return reject(error,"Session restore item identity unavailable");
  instances.push_back({saved.uid,resolved,saved.equipped?1u:0u,uint32_t(saved.doses)});
 }
 if(!next.inventory.restore(items,instances,error))return false;

 next.story_flags.reserve(room.flag_count());next.reviewed_flag_mutations.reserve(room.flag_count());
 for(uint32_t i=0;i<room.flag_count();++i){
  const auto name=room.string(room.flag(i).name_string);
  const auto flag=std::find_if(snapshot.flags.begin(),snapshot.flags.end(),
   [&](const SessionFlag&value){return value.id==name;});
  if(flag==snapshot.flags.end())return reject(error,"Session restore story flag binding unavailable");
  next.story_flags.push_back(flag->value);
  next.reviewed_flag_mutations.push_back(std::any_of(data.mutable_flags().begin(),data.mutable_flags().end(),[&](const auto&allowed){return allowed==name;}));
 }
 for(const auto&saved:snapshot.seen_dialogue_flags){
  bool resolved=false;
  auto bind=[&](uint32_t key){if(!house.string(key).empty()&&house.string(key)==saved.id){resolved=true;if(saved.value)next.seen_dialogue_keys.insert(key);}};
  for(uint32_t i=0;i<house.count(HouseSection::Npcs);++i)bind(house.npc(i).seen_key);
  for(uint32_t i=0;i<house.count(HouseSection::Overrides);++i)bind(house.override_dialogue(i).seen_key);
  if(!resolved)return reject(error,"Session restore seen dialogue binding unavailable");
 }
 next.valid_=true;output=std::move(next);error.clear();return true;
}
}
