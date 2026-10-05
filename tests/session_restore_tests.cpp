#include "encore/session_restore.hpp"
#include "encore/content.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>

using namespace encore::upstream;
namespace {
unsigned checks=0;
void check(bool value,const char*expression,int line,const std::string&error){
 ++checks;if(!value){std::fprintf(stderr,"SessionRestore line %d: %s; %s\n",line,expression,error.c_str());std::exit(1);}
}
#define CHECK(value) check(bool(value),#value,__LINE__,error)
uint32_t get(const std::vector<uint8_t>&bytes,size_t at){return uint32_t(bytes[at])|uint32_t(bytes[at+1])<<8|uint32_t(bytes[at+2])<<16|uint32_t(bytes[at+3])<<24;}
void put(std::vector<uint8_t>&bytes,size_t at,uint32_t value){for(unsigned i=0;i<4;++i)bytes[at+i]=uint8_t(value>>(i*8));}
void fix_battle_crc(std::vector<uint8_t>&bytes){put(bytes,16,0);put(bytes,16,encore::crc32(bytes.data(),bytes.size()));}
}
int main(int argc,char**argv){
 const std::string root=argc>1?argv[1]:"romfs/data";std::string error;
 NativeSessionData data;RoomData room;HouseData house;BattleRoundData round;ItemData items;BattleData font;
 CHECK(data.load_file((root+"/opening.encsession").c_str(),error));
 CHECK(room.load_file((root+"/opening.encroom").c_str(),error));
 CHECK(house.load_file((root+"/opening.enchouse").c_str(),error));
 CHECK(round.load_file((root+"/doll-entry.encround").c_str(),error));
 CHECK(items.load_file((root+"/opening.encitems").c_str(),error));
 CHECK(font.load_file((root+"/opening.encbattle").c_str(),error));
 PreparedSessionRestore prepared;CHECK(!prepared.valid());
 auto prepare=[&](const SessionSnapshot&state){return prepare_session_restore(data,room.view(),house.view(),round.view(),items.view(),font.view(),state,prepared,error);};
 CHECK(prepare(data.defaults()));CHECK(prepared.valid());
 CHECK(prepared.stats.level==data.levels().front().level);
 CHECK(prepared.stats.hp==data.defaults().characters.front().hp);
 CHECK(prepared.stats.defense==data.levels().front().stats[3]);
 CHECK(prepared.seen_dialogue_keys.empty());

 auto saved=data.defaults();auto&character=saved.characters.front();const auto&row=data.levels().back();
 character.level=row.level;character.experience=row.minimum_exp+1;character.hp=19;character.pp=3;character.learned_skills=row.skills;
 character.nickname="Ana";character.inventory.front().uid=0;
 saved.key_items.front().uid=UINT32_MAX;saved.player_name="Player";saved.favorite_food="Bread";
 saved.position_x=148.25;saved.position_y=704.5;saved.direction_x=-std::sqrt(.5);saved.direction_y=std::sqrt(.5);
 saved.cash=UINT32_MAX;saved.bank=15;saved.earned_cash=7;saved.playtime_seconds=1234.75;saved.saved_at="2026-10-02T08:00:00Z";
 for(auto&flag:saved.flags)if(flag.id=="mimmie_door_opened"||flag.id=="poltergeist"||flag.id=="doll_defeated"||flag.id=="doll_melody"||flag.id=="phone_ring"||flag.id=="talked_to_dad"||flag.id==data.earned_cash_flag_id())flag.value=true;
 std::set<std::string>seen_ids;std::vector<uint32_t>seen_offsets;
 for(uint32_t i=0;i<house.view().count(HouseSection::Npcs);++i){const auto offset=house.view().npc(i).seen_key;const std::string name(house.view().string(offset));if(!name.empty()&&seen_ids.insert(name).second)seen_offsets.push_back(offset);}
 for(uint32_t i=0;i<house.view().count(HouseSection::Overrides);++i){const auto offset=house.view().override_dialogue(i).seen_key;const std::string name(house.view().string(offset));if(!name.empty()&&seen_ids.insert(name).second)seen_offsets.push_back(offset);}
 CHECK(seen_offsets.size()>1);
 for(size_t i=0;i<seen_offsets.size();++i)saved.seen_dialogue_flags.push_back({std::string(house.view().string(seen_offsets[i])),i%2==0});
 for(uint32_t i=0;i<room.view().battle_count();++i)saved.encountered.push_back({std::string(room.view().string(room.view().battle(i).enemy_string)),true});
 // Save registries are identified by names, never serialized vector positions.
 std::reverse(saved.flags.begin(),saved.flags.end());
 std::vector<uint8_t>original_bytes;CHECK(encode_session_save(saved,data.compatibility(),original_bytes,error));
 SessionSnapshot decoded;CHECK(decode_session_save(original_bytes.data(),original_bytes.size(),data.compatibility(),decoded,error));
 CHECK(prepare(decoded));CHECK(prepared.valid());
 CHECK(prepared.stats.level==row.level&&prepared.stats.experience==row.minimum_exp+1);
 CHECK(prepared.stats.hp==19&&prepared.stats.pp==3&&prepared.stats.maxhp==row.stats[0]&&prepared.stats.maxpp==row.stats[1]);
 CHECK(prepared.stats.offense==row.stats[2]&&prepared.stats.defense==row.stats[3]&&prepared.stats.speed==row.stats[4]&&prepared.stats.iq==row.stats[5]&&prepared.stats.guts==row.stats[6]);
 CHECK(prepared.stats.cash==UINT32_MAX&&prepared.stats.bank==15&&prepared.stats.earned_cash==7&&prepared.stats.learned_skills==row.skills);
 CHECK(prepared.position.x==148.25f&&prepared.position.y==704.5f);
 CHECK(prepared.direction.x==float(saved.direction_x)&&prepared.direction.y==float(saved.direction_y));
 CHECK(prepared.inventory.size()==character.inventory.size()&&prepared.inventory.instance(0).id==0);
 CHECK(prepared.inventory.instance(0).equipped==1&&prepared.inventory.instance(0).doses==1);
 CHECK(items.view().string(items.view().definition(prepared.inventory.instance(0).definition).source)==character.inventory.front().item_id);
 CHECK(prepared.state.key_items.front().uid==UINT32_MAX&&prepared.state.characters.front().nickname=="Ana");
 CHECK(prepared.state.playtime_seconds==1234.75&&prepared.state.player_name=="Player"&&prepared.state.favorite_food=="Bread");
 CHECK(prepared.story_flags.size()==room.view().flag_count());
 for(uint32_t i=0;i<room.view().flag_count();++i){const auto name=room.view().string(room.view().flag(i).name_string);const auto saved_flag=std::find_if(saved.flags.begin(),saved.flags.end(),[&](const SessionFlag&flag){return flag.id==name;});CHECK(saved_flag!=saved.flags.end());CHECK(prepared.story_flags[i]==saved_flag->value);}
 for(size_t i=0;i<seen_offsets.size();++i)CHECK(prepared.seen_dialogue_keys.count(seen_offsets[i])==size_t(i%2==0));
 auto encoded=[&](const SessionSnapshot&state){std::vector<uint8_t>bytes;CHECK(encode_session_save(state,data.compatibility(),bytes,error));return bytes;};
 CHECK(encoded(prepared.state)==original_bytes);CHECK(encoded(saved)==original_bytes);
 const auto old_flags=prepared.story_flags;const auto old_seen=prepared.seen_dialogue_keys;
 auto untouched=[&](){CHECK(prepared.valid()&&encoded(prepared.state)==original_bytes);CHECK(prepared.story_flags==old_flags&&prepared.seen_dialogue_keys==old_seen);CHECK(prepared.stats.hp==19&&prepared.stats.pp==3&&prepared.stats.level==row.level&&prepared.stats.cash==UINT32_MAX);CHECK(prepared.inventory.size()==1&&prepared.inventory.instance(0).id==0&&prepared.inventory.instance(0).equipped==1&&prepared.inventory.instance(0).doses==1);CHECK(prepared.position.x==148.25f&&prepared.direction.x==float(saved.direction_x));};
 const std::vector<std::function<void(SessionSnapshot&)>>bad={
  [](auto&s){s.scene_id="unknown";},[](auto&s){s.scene_label="unknown";},[](auto&s){s.source_version="unknown";},
  [](auto&s){s.characters.front().character_id="unknown";},[](auto&s){s.party.front()="unknown";},
  [](auto&s){s.characters.front().level=3;},[](auto&s){s.characters.front().experience=UINT32_MAX;},
  [](auto&s){s.characters.front().hp=INT32_MAX;},[](auto&s){s.characters.front().pp=INT32_MAX;},
  [](auto&s){s.characters.front().status.push_back({"unknown",0});},[](auto&s){s.characters.front().permanent_boosts.push_back({"defense",5});},
  [](auto&s){s.characters.front().affinity_multipliers.front().value=2;},[](auto&s){s.characters.front().learned_skills.push_back("unknown");},
  [](auto&s){s.characters.front().nickname="TooLongName";},[](auto&s){s.characters.front().nickname="\xc3\xa9";},
  [](auto&s){s.characters.front().nickname="\n";},[](auto&s){s.player_name="\t";},[](auto&s){s.favorite_food="\xc3\xa9";},
  [](auto&s){s.characters.front().inventory.front().item_id="unknown";},[](auto&s){s.characters.front().inventory.front().doses=2;},
  [](auto&s){s.key_items.front().uid=s.characters.front().inventory.front().uid;},
  [](auto&s){s.flags.pop_back();},[](auto&s){s.flags.front().id="unknown";},[](auto&s){s.flags.push_back(s.flags.front());},
  [](auto&s){s.seen_dialogue_flags.push_back({"unknown",true});},[](auto&s){s.encountered.push_back({"unknown",true});},
  [](auto&s){s.position_x=1e6;},[](auto&s){s.direction_x=.5;s.direction_y=.25;},
  [](auto&s){s.settings.text_speed=.1;},[](auto&s){s.settings.menu_flavor="unknown";},[](auto&s){s.settings.button_prompts="unknown";},
  [](auto&s){s.playtime_seconds=std::numeric_limits<double>::quiet_NaN();},[](auto&s){s.storage=s.key_items;},
  [](auto&s){s.object_flags.push_back({"unknown",true});},[](auto&s){s.rare_drops.push_back({"unknown",1});},
  [](auto&s){s.run_sound="unknown";},[](auto&s){s.shadow_effect="unknown";},[](auto&s){s.rng_policy=SessionRngPolicy(2);}
 };
 for(const auto&edit:bad){auto state=saved;edit(state);CHECK(!prepare(state));CHECK(!error.empty());untouched();}
 CHECK(!prepare_session_restore(NativeSessionData{},room.view(),house.view(),round.view(),items.view(),font.view(),saved,prepared,error));untouched();
 CHECK(!prepare_session_restore(data,RoomView{},house.view(),round.view(),items.view(),font.view(),saved,prepared,error));untouched();
 CHECK(!prepare_session_restore(data,room.view(),HouseView{},round.view(),items.view(),font.view(),saved,prepared,error));untouched();
 CHECK(!prepare_session_restore(data,room.view(),house.view(),RoundView{},items.view(),font.view(),saved,prepared,error));untouched();
 CHECK(!prepare_session_restore(data,room.view(),house.view(),round.view(),ItemView{},font.view(),saved,prepared,error));untouched();
 CHECK(!prepare_session_restore(data,room.view(),house.view(),round.view(),items.view(),BattleView{},saved,prepared,error));untouched();

 // A checked font is not proof that every dynamic user name can be rendered.
 // Remove one required glyph through a separate, CRC-correct fixture pack.
 std::vector<uint8_t>font_bytes;CHECK(encore::read_file((root+"/opening.encbattle").c_str(),font_bytes,1024*1024,error));
 const auto glyph_base=get(font_bytes,64+11*16+4);bool removed=false;
 for(uint32_t i=0;i<font.view().count(BattleSection::Glyphs);++i)if(font.view().glyph(i).codepoint=='A'){put(font_bytes,glyph_base+i*36,0x1000);removed=true;break;}
 CHECK(removed);fix_battle_crc(font_bytes);BattleData incomplete_font;CHECK(incomplete_font.load(font_bytes.data(),font_bytes.size(),error));
 CHECK(!prepare_session_restore(data,room.view(),house.view(),round.view(),items.view(),incomplete_font.view(),saved,prepared,error));
 CHECK(error.find("glyph unavailable")!=std::string::npos);untouched();

 // Derived session round-trips through Record without replaying rewards, healing,
 // reequipping, reallocating IDs, resetting names, or losing retained metadata.
 NativeSnapshotInput live{prepared.state,&prepared.stats,&prepared.inventory,&prepared.storage};SessionSnapshot recorded;
 CHECK(build_native_session_snapshot(data,room.view(),house.view(),round.view(),items.view(),live,recorded,error));
 CHECK(encoded(recorded)==original_bytes);
 CHECK(prepare_session_restore(data,room.view(),house.view(),round.view(),items.view(),font.view(),prepared.state,prepared,error));
 CHECK(encoded(prepared.state)==original_bytes);
 for(const Vec2 direction:std::vector<Vec2>{{0,1},{1,0},{0,-1},{-1,0},{1,1},{1,-1},{-1,1},{-1,-1}}){auto state=saved;state.direction_x=direction.x;state.direction_y=direction.y;CHECK(prepare(state));CHECK(prepared.direction.x==direction.x&&prepared.direction.y==direction.y);}

 InventoryState inventory;CHECK(inventory.initialize(items.view()));const auto initial=inventory.instance(0);
 std::vector<ItemInstance>instances{initial};instances.front().id=0;instances.front().doses=65535;instances.front().equipped=0;
 CHECK(inventory.restore(items.view(),instances,error));CHECK(inventory.instance(0).id==0&&inventory.instance(0).doses==65535&&!inventory.instance(0).equipped);
 auto inventory_untouched=[&](){CHECK(inventory.valid()&&inventory.size()==1&&inventory.instance(0).id==0&&inventory.instance(0).doses==65535&&!inventory.instance(0).equipped);};
 CHECK(!inventory.restore(ItemView{},instances,error));inventory_untouched();
 for(const auto&edit:std::vector<std::function<void(std::vector<ItemInstance>&)>>{
  [&](auto&v){v.front().definition=items.view().count(ItemSection::Definitions);},[](auto&v){v.front().doses=0;},[](auto&v){v.front().doses=65536;},
  [](auto&v){v.front().equipped=2;},[](auto&v){v.push_back(v.front());},
  [](auto&v){v.front().equipped=1;auto duplicate=v.front();duplicate.id=1;v.push_back(duplicate);},
  [&](auto&v){v.resize(items.view().metadata().capacity+1);}
 }){auto invalid=instances;edit(invalid);CHECK(!inventory.restore(items.view(),invalid,error));inventory_untouched();}
 CHECK(inventory.restore(items.view(),{},error));CHECK(inventory.valid()&&inventory.size()==0);
 CHECK(items.view().initial_instance(0).id==initial.id&&items.view().initial_instance(0).equipped==initial.equipped&&items.view().initial_instance(0).doses==initial.doses);
 std::printf("SessionRestore: %u checks; detached validation, source stat rows, stable identities, retained metadata, glyph rejection, Record round-trip and rollback\n",checks);
}
