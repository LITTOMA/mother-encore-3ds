#include "encore/native_session.hpp"
#include "encore/content.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <vector>
using namespace encore::upstream;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"NativeSession check failed at %d: %s; %s\n",__LINE__,#x,error.c_str());std::exit(1);}}while(0)
static void put(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(v>>(8*i));}
static void fix(std::vector<uint8_t>&b){put(b,12,uint32_t(b.size()));put(b,16,encore::crc32(b.data()+24,b.size()-24));}
int main(int argc,char**argv){
 const std::string root=argc>1?argv[1]:"romfs/data";std::string error;
 NativeSessionData data;RoomData room;HouseData house;BattleRoundData round;ItemData items;InventoryState inventory;
 CHECK(data.load_file((root+"/opening.encsession").c_str(),error));CHECK(room.load_file((root+"/opening.encroom").c_str(),error));CHECK(house.load_file((root+"/opening.enchouse").c_str(),error));CHECK(round.load_file((root+"/doll-entry.encround").c_str(),error));CHECK(items.load_file((root+"/opening.encitems").c_str(),error));CHECK(inventory.initialize(items.view()));
 auto validate=[&](const SessionSnapshot&s){return validate_native_session_snapshot(data,room.view(),house.view(),round.view(),items.view(),s,error);};
 CHECK(data.leader_id()=="ninten");CHECK(data.saved_flag_id()=="saved");CHECK(data.earned_cash_flag_id()=="earned_cash");CHECK(data.defaults().flags.size()==178);CHECK(validate(data.defaults()));
 const auto&initial=data.defaults().characters[0];CHECK(initial.nickname=="Ninten");CHECK(initial.permanent_boosts.empty());CHECK(initial.affinity_multipliers.size()==1&&initial.affinity_multipliers[0].id=="asthma"&&initial.affinity_multipliers[0].value==1);CHECK(initial.inventory[0].item_id=="BaseballCap"&&initial.inventory[0].equipped&&initial.inventory[0].doses==1);CHECK(data.defaults().key_items[0].item_id=="CashCard"&&data.defaults().key_items[0].uid!=initial.inventory[0].uid);
 // Additive startup roster never broadens the playable party. Old singleton
 // defaults are retained byte-for-byte; inactive values are frozen source data.
 CHECK(data.defaults().characters.size()==1);CHECK(data.startup_characters().size()==5);
 auto startup=data.defaults();startup.characters=data.startup_characters();CHECK(validate(startup));
 CHECK(startup.characters[1].character_id=="ana"&&startup.characters[1].hp==42&&startup.characters[1].pp==31);
 CHECK(startup.characters[2].character_id=="lloyd"&&startup.characters[2].hp==82&&startup.characters[2].pp==0&&startup.characters[2].learned_skills==std::vector<std::string>{"spy"});
 CHECK(startup.characters[3].character_id=="pippi"&&startup.characters[3].hp==83);CHECK(startup.characters[4].character_id=="teddy"&&startup.characters[4].hp==83);
 for(const auto&character:startup.characters)CHECK(character.nickname.empty());
 CHECK(startup.characters[2].inventory.size()==2&&startup.characters[2].inventory[0].item_id=="KickMeNote"&&startup.characters[2].inventory[1].item_id=="GlassesCelluloid");
 startup.characters[1].nickname="Ann";startup.characters[2].nickname="Lou";startup.characters[3].nickname="Pip";startup.characters[4].nickname="Ted";
 startup.characters[2].inventory[0].uid=4094;startup.characters[2].inventory[1].uid=4095;CHECK(validate(startup));
 const std::vector<std::function<void(SessionSnapshot&)>>bad_startup={
 [](auto&s){s.characters.pop_back();},[](auto&s){s.characters.erase(s.characters.begin()+1);},[](auto&s){std::swap(s.characters[3],s.characters[4]);},[](auto&s){s.party.push_back(s.characters[1].character_id);},[](auto&s){s.party[0]=s.characters[1].character_id;},[](auto&s){s.characters[1].level++;},[](auto&s){s.characters[1].experience++;},[](auto&s){s.characters[1].hp--;},[](auto&s){s.characters[1].pp--;},[](auto&s){s.characters[1].nickname=std::string(256,'x');},[](auto&s){s.characters[1].permanent_boosts.push_back({"maxhp",1});},[](auto&s){s.characters[1].affinity_multipliers.push_back({"asthma",1});},[](auto&s){s.characters[1].status.push_back({"asthma",0});},[](auto&s){s.characters[2].learned_skills.clear();},[](auto&s){s.characters[2].inventory[0].equipped=false;},[](auto&s){s.characters[2].inventory[0].doses++;},[](auto&s){s.characters[2].inventory[0].item_id="unknown";},[](auto&s){std::swap(s.characters[2].inventory[0],s.characters[2].inventory[1]);},[](auto&s){s.characters[2].inventory[0].uid=s.key_items[0].uid;},[](auto&s){s.characters.push_back(s.characters.back());}
 };
 for(const auto&edit:bad_startup){auto bad=startup;edit(bad);CHECK(!validate(bad));}
 NativeSnapshotInput input;input.state=data.defaults();input.inventory=&inventory;BattleSessionStats live;input.stats=&live;
 const auto&row=data.levels().back();live.level=row.level;live.experience=11;live.hp=19;live.pp=row.stats[1];live.maxhp=row.stats[0];live.maxpp=row.stats[1];live.offense=row.stats[2];live.defense=row.stats[3];live.speed=row.stats[4];live.iq=row.stats[5];live.guts=row.stats[6];live.bank=15;live.cash=0;live.earned_cash=15;live.learned_skills={std::string(round.view().string(round.view().encounter().learned_skill))};
 input.state.position_x=148;input.state.position_y=704;input.state.direction_x=0;input.state.direction_y=-1;input.state.saved_at="2026-10-02T08:00:00Z";input.state.playtime_seconds=450;
 for(auto&flag:input.state.flags)if(flag.id=="mimmie_door_opened"||flag.id=="poltergeist"||flag.id=="doll_defeated"||flag.id=="doll_melody"||flag.id=="phone_ring"||flag.id=="talked_to_dad"||flag.id==data.earned_cash_flag_id())flag.value=true;
 for(uint32_t i=0;i<room.view().battle_count();++i)input.state.encountered.push_back({std::string(room.view().string(room.view().battle(i).enemy_string)),true});
 input.state.seen_dialogue_flags.push_back({std::string(house.view().string(house.view().npc(0).seen_key)),true});
 SessionSnapshot output;auto build=[&](){return build_native_session_snapshot(data,room.view(),house.view(),round.view(),items.view(),input,output,error);};CHECK(build());CHECK(output.characters[0].level==2&&output.characters[0].experience==11&&output.characters[0].hp==19&&output.characters[0].pp==27);CHECK(output.characters[0].learned_skills==row.skills);CHECK(output.characters[0].permanent_boosts.empty());CHECK(output.bank==15&&output.cash==0&&output.earned_cash==15);CHECK(validate(output));
 auto flag_value=[&](const SessionSnapshot&s,const std::string&id){for(auto&f:s.flags)if(f.id==id)return f.value;return false;};CHECK(!flag_value(output,data.saved_flag_id()));CHECK(!flag_value(input.state,data.saved_flag_id()));
 // First disk write preserves false; successful main-layer commit changes live
 // flag afterwards. A second snapshot captures true without codec mutation.
 std::vector<uint8_t>encoded;CHECK(encode_session_save(output,data.compatibility(),encoded,error));SessionSnapshot restored;CHECK(decode_session_save(encoded.data(),encoded.size(),data.compatibility(),restored,error));CHECK(validate(restored));CHECK(!flag_value(restored,data.saved_flag_id()));
 for(auto&f:input.state.flags)if(f.id==data.saved_flag_id())f.value=true;
 CHECK(build());CHECK(flag_value(output,data.saved_flag_id()));
 live.earned_cash=0;for(auto&f:input.state.flags)if(f.id==data.earned_cash_flag_id())f.value=false;CHECK(build());CHECK(output.bank==15&&output.earned_cash==0&&!flag_value(output,data.earned_cash_flag_id()));
 const auto good=output;
 // Mom room is now a source-connected native route; basement remains unported.
 auto mom=good;mom.position_x=496;mom.position_y=96;CHECK(validate(mom));
 std::vector<std::function<void(SessionSnapshot&)>>bad={
 [](auto&s){s.scene_id="unknown";},[](auto&s){s.scene_label="unknown";},[](auto&s){s.source_version="unknown";},[](auto&s){s.party[0]="nickname";},[](auto&s){s.characters[0].character_id="unknown";},[](auto&s){s.characters.push_back(s.characters[0]);},[](auto&s){s.characters[0].permanent_boosts.push_back({"defense",5});},[](auto&s){s.characters[0].status.push_back({"unknown",0});},[](auto&s){s.characters[0].affinity_multipliers[0].value=2;},[](auto&s){s.characters[0].level=3;},[](auto&s){s.characters[0].experience=27;},[](auto&s){s.characters[0].hp=66;},[](auto&s){s.characters[0].pp=28;},[](auto&s){s.characters[0].learned_skills.push_back("unknown");},[](auto&s){s.characters[0].inventory[0].item_id="unknown";},[](auto&s){s.characters[0].inventory[0].equipped=false;},[](auto&s){s.characters[0].inventory[0].doses=2;},[](auto&s){s.key_items[0].uid=s.characters[0].inventory[0].uid;},[](auto&s){s.key_items[0].item_id="unknown";},[](auto&s){s.storage=s.key_items;},[](auto&s){s.keys[0].value=1;},[](auto&s){s.rare_drops.push_back({"unknown",1});},[](auto&s){s.object_flags.push_back({"unknown",true});},[](auto&s){s.flags.pop_back();},[](auto&s){s.flags[0].id="unknown";},[](auto&s){s.seen_dialogue_flags.push_back({"unknown",true});},[](auto&s){s.encountered.push_back({"unknown",true});},[](auto&s){s.position_x=1e6;},[](auto&s){s.position_x=96;s.position_y=1000;},[](auto&s){s.direction_x=.2;s.direction_y=.3;},[](auto&s){s.settings.text_speed=.1;},[](auto&s){s.settings.menu_flavor="unknown";},[](auto&s){s.settings.button_prompts="unknown";},[](auto&s){s.settings.description=false;},[](auto&s){s.run_sound="unknown";},[](auto&s){s.shadow_effect="unknown";}
 };
 for(const auto&edit:bad){auto s=good;edit(s);CHECK(!validate(s));}
 // Registered but unsupported story state must not become a valid save merely
 // because its name exists in the complete source registry.
 auto unrelated=good;bool changed=false;for(auto&f:unrelated.flags)if(std::find(data.mutable_flags().begin(),data.mutable_flags().end(),f.id)==data.mutable_flags().end()&&!f.value){f.value=true;changed=true;break;}CHECK(changed);CHECK(!validate(unrelated));
 live.defense+=5;CHECK(!build());CHECK(output.characters[0].hp==good.characters[0].hp&&output.saved_at==good.saved_at);live.defense-=5;live.learned_skills.push_back("unknown");CHECK(!build());live.learned_skills.pop_back();input.state.characters[0].inventory[0].item_id="unknown";CHECK(!build());input.state.characters[0].inventory=initial.inventory;input.state.characters[0].learned_skills.push_back("unknown");CHECK(!build());input.state.characters[0].learned_skills=initial.learned_skills;CHECK(build());
 // Live battle snapshots overlay only Ninten, retaining the named inactive
 // records and opaque source-allocated IDs exactly, then round-trip in memory.
 input.state.characters=startup.characters;CHECK(build());CHECK(output.characters.size()==5);
 auto inactive_bytes=[&](const SessionSnapshot&state){auto frozen=data.defaults();frozen.characters=state.characters;frozen.characters[0]=data.defaults().characters[0];std::vector<uint8_t>result;CHECK(encode_session_save(frozen,data.compatibility(),result,error));return result;};
 CHECK(inactive_bytes(output)==inactive_bytes(input.state));
 CHECK(encode_session_save(output,data.compatibility(),encoded,error));CHECK(decode_session_save(encoded.data(),encoded.size(),data.compatibility(),restored,error));CHECK(validate(restored));CHECK(inactive_bytes(restored)==inactive_bytes(output));
 auto previous=output;input.state.characters[1].hp++;CHECK(!build());CHECK(inactive_bytes(output)==inactive_bytes(previous));input.state.characters[1].hp--;
 std::vector<uint8_t>bytes;CHECK(encore::read_file((root+"/opening.encsession").c_str(),bytes,1024*1024,error));
 for(size_t n=0;n<bytes.size();++n)CHECK(!data.load(bytes.data(),n,error));
 CHECK(data.valid()&&data.leader_id()=="ninten");
 for(size_t i=0;i<bytes.size();++i){auto mutation=bytes;mutation[i]^=1;CHECK(!data.load(mutation.data(),mutation.size(),error));}
 for(auto entry:std::vector<std::pair<size_t,uint32_t>>{{8,4},{20,4},{8,1},{20,1},{24,0},{36,0xffffffff}}){auto mutation=bytes;put(mutation,entry.first,entry.second);fix(mutation);CHECK(!data.load(mutation.data(),mutation.size(),error));}
 auto trailing=bytes;trailing.push_back(0);fix(trailing);CHECK(!data.load(trailing.data(),trailing.size(),error));CHECK(data.load(bytes.data(),bytes.size(),error));CHECK(validate(good));
 // A schema1/capability1 resource still loads without injecting the new
 // startup roster, as required by the archived rules6 migration resource.
 auto template_state=data.defaults();template_state.characters=data.startup_characters();std::vector<uint8_t>startup_bytes;CHECK(encode_session_save(template_state,data.compatibility(),startup_bytes,error));
 size_t settings_bytes=4+8*data.text_speeds().size()+4+4;for(const auto&v:data.menu_flavors())settings_bytes+=4+v.size();for(const auto&v:data.button_prompts())settings_bytes+=4+v.size();
 auto legacy_resource=bytes;legacy_resource.resize(legacy_resource.size()-settings_bytes-startup_bytes.size()-4);put(legacy_resource,8,1);put(legacy_resource,20,1);fix(legacy_resource);
 NativeSessionData legacy;CHECK(legacy.load(legacy_resource.data(),legacy_resource.size(),error));CHECK(legacy.startup_characters().size()==1);CHECK(legacy.defaults().characters.size()==1);
 std::vector<uint8_t>current_defaults,legacy_defaults;CHECK(encode_session_save(data.defaults(),data.compatibility(),current_defaults,error));CHECK(encode_session_save(legacy.defaults(),legacy.compatibility(),legacy_defaults,error));CHECK(current_defaults==legacy_defaults);
 CHECK(validate_native_session_snapshot(legacy,room.view(),house.view(),round.view(),items.view(),good,error));CHECK(!validate_native_session_snapshot(legacy,room.view(),house.view(),round.view(),items.view(),startup,error));
 std::printf("NativeSession: %u checks; source defaults, actual level2 stats, stable identities, complete snapshot and fail-closed scope\n",checks);
}
