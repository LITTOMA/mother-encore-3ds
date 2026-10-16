#include "encore/house_presents.hpp"
#include "encore/native_session.hpp"
#include "encore/house_data.hpp"
#include "encore/room_data.hpp"
#include "encore/battle_round_data.hpp"
#include "encore/session_restore.hpp"
#include "encore/content.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <fstream>
using namespace encore::upstream;
static unsigned checks=0;static std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Map present failure at %d: %s; %s\n",__LINE__,#x,error.c_str());std::exit(1);}}while(0)
struct Host final:PresentHost{
 HouseView house;std::map<std::string,bool>flags;std::vector<std::string>events;std::vector<uint32_t>texts;std::vector<SessionItem>keys;
 bool reject_grant=false;
 bool validate_text(uint32_t id,std::string_view path,std::string&e)override{for(uint32_t i=0;i<house.count(HouseSection::Dialogues);++i){const auto d=house.dialogue(i);if(d.id==id&&house.string(d.source_path)==path)return true;}e="Fixture missing text";return false;}
 bool validate_flag(std::string_view n,std::string&e)override{if(flags.count(std::string(n)))return true;e="Fixture unknown story flag";return false;}
 bool validate_item(PresentTemplate t,std::string_view n,std::string&e)override{if((n=="BatPlastic"&&!t.key_item&&t.doses==1)||(t.key_item&&t.doses==1&&(n=="MapPodunk"||n=="DogTreats"||n=="GGFDiary")))return true;e="Fixture unknown key";return false;}
 bool validate_story(std::string_view,std::string&)override{return true;}
 bool inventory_space()const override{return true;}
 bool preview_item(PresentTemplate,std::string_view,std::string&)override{return true;}
 bool start_story(std::string_view,std::string&)override{events.push_back("story");return true;}
 bool validate_sound(std::string_view n,std::string&)override{return n.substr(0,6)=="Audio/";}
 bool show_text(uint32_t id,std::string&)override{events.push_back("text");texts.push_back(id);return true;}
 bool flag(std::string_view n,bool&v,std::string&e)override{if(!validate_flag(n,e))return false;v=flags[std::string(n)];return true;}
 bool set_flag(std::string_view n,bool v,std::string&e)override{if(!validate_flag(n,e))return false;flags[std::string(n)]=v;events.push_back("story-flag");return true;}
 bool grant_item(PresentTemplate t,std::string_view n,std::string&e)override{if(reject_grant){e="Fixture rejected grant";return false;}keys.push_back({std::string(n),false,t.doses,0xabc});events.push_back("grant");return true;}
 bool play_sound(std::string_view,std::string&)override{events.push_back("sound");return true;}
};
int main(int argc,char**argv){
 const std::string base=argc>1?argv[1]:"romfs";
 PresentData data;HouseData house;RoomData room;BattleRoundData round;ItemData items;NativeSessionData session;BattleData font;
 CHECK(data.load_file((base+"/world/house/opening.encpresent").c_str(),error));
 CHECK(house.load_file((base+"/world/house/opening.enchouse").c_str(),error));
 CHECK(room.load_file((base+"/world/house/opening.encroom").c_str(),error));
 CHECK(round.load_file((base+"/battle/lamp/opening.encround").c_str(),error));
 CHECK(items.load_file((base+"/ui/opening.encitems").c_str(),error));
 CHECK(session.load_file((base+"/session/opening.encsession").c_str(),error));
 CHECK(font.load_file((base+"/battle/lamp/opening.encbattle").c_str(),error));
 Host host;host.house=house.view();for(uint32_t i=0;i<room.view().flag_count();++i)host.flags[std::string(room.view().string(room.view().flag(i).name_string))]=false;
 auto view=data.view();uint32_t map=kPresentNone;for(uint32_t i=0;i<view.count(PresentSection::Objects);++i)if(view.string(view.object(i).path)=="Objects/Present3")map=i;
 CHECK(map!=kPresentNone);CHECK(view.object(map).policy==uint32_t(PresentPolicy::ObjectFlag));
 CHECK(view.string(view.object(map).flag)=="Ninten's House/Present3");
 PresentRuntime runtime;SourceRandom rng(42);CHECK(runtime.initialize(view,host,error));CHECK(runtime.scene_ready(rng,error));
 CHECK(runtime.object_flags().empty());CHECK(runtime.pose(map).frame==0&&runtime.pose(map).sparkles);
 CHECK(runtime.start(map,error));CHECK(runtime.state()==PresentProgramState::WaitingText);
 CHECK(host.keys.size()==1&&host.keys[0].item_id=="MapPodunk");CHECK(host.texts==std::vector<uint32_t>{75});
 CHECK(host.events==std::vector<std::string>({"grant","text"}));
 CHECK(runtime.object_flags().size()==1&&runtime.object_flags()[0].value);CHECK(!runtime.pose(map).sparkles);
 CHECK(host.flags["got_dog_treats"]==false);CHECK(runtime.advance(.5,error));CHECK(runtime.pose(map).frame==4);
 CHECK(runtime.advance_text(error));CHECK(host.events==std::vector<std::string>({"grant","text","text","sound"}));CHECK(host.texts.back()==76);
 CHECK(runtime.advance_text(error));CHECK(runtime.state()==PresentProgramState::Complete);
 auto flags=runtime.object_flags();
 CHECK(!runtime.initialize(view,host,error,{{"unknown/Present3",true}}));CHECK(runtime.ready());CHECK(runtime.object_flags()[0].value);
 CHECK(!runtime.initialize(view,host,error,{flags[0],flags[0]}));CHECK(runtime.object_flags().size()==1);
 CHECK(runtime.start(map,error));CHECK(host.texts.back()==72);CHECK(host.keys.size()==1);CHECK(runtime.advance_text(error));
 PresentRuntime restored;CHECK(restored.initialize(view,host,error,flags));CHECK(restored.pose(map).frame==4&&!restored.pose(map).sparkles);
 CHECK(restored.scene_ready(rng,error));CHECK(restored.start(map,error));CHECK(host.texts.back()==72&&host.keys.size()==1);CHECK(restored.advance_text(error));
 auto state=session.defaults();state.position_x=88;state.position_y=977;state.object_flags=flags;state.key_items.push_back(host.keys[0]);
 CHECK(validate_native_session_snapshot(session,room.view(),house.view(),round.view(),items.view(),state,error));
 std::vector<uint8_t>save;CHECK(encode_session_save(state,session.compatibility(),save,error));SessionSnapshot decoded;
 CHECK(decode_session_save(save.data(),save.size(),session.compatibility(),decoded,error));
 CHECK(decoded.key_items.back().uid==0xabc&&decoded.object_flags[0].id=="Ninten's House/Present3"&&decoded.object_flags[0].value);
 PreparedSessionRestore prepared;CHECK(prepare_session_restore(session,room.view(),house.view(),round.view(),items.view(),font.view(),decoded,prepared,error));
 CHECK(prepared.state.key_items.back().uid==0xabc&&prepared.state.object_flags[0].value);
 NativeSnapshotInput live;live.state=decoded;live.stats=&prepared.stats;live.inventory=&prepared.inventory;live.storage=&prepared.storage;
 SessionSnapshot collected;CHECK(build_native_session_snapshot(session,room.view(),house.view(),round.view(),items.view(),live,collected,error));
 CHECK(collected.object_flags[0].value&&collected.key_items.back().uid==0xabc);
 CHECK(encode_session_save(collected,session.compatibility(),save,error));
 CHECK(decode_session_save(save.data(),save.size(),session.compatibility(),decoded,error));
 CHECK(decoded.object_flags[0].value&&decoded.key_items.back().uid==0xabc);
 auto bad=state;bad.object_flags.clear();CHECK(!validate_native_session_snapshot(session,room.view(),house.view(),round.view(),items.view(),bad,error));
 bad=state;bad.key_items.pop_back();CHECK(!validate_native_session_snapshot(session,room.view(),house.view(),round.view(),items.view(),bad,error));
 bad=state;bad.object_flags[0].value=false;CHECK(!validate_native_session_snapshot(session,room.view(),house.view(),round.view(),items.view(),bad,error));
 bad=state;bad.object_flags.push_back({"Nintens House/Present2",false});CHECK(!validate_native_session_snapshot(session,room.view(),house.view(),round.view(),items.view(),bad,error));
 bad=state;bad.key_items.push_back(bad.key_items.back());bad.key_items.back().uid++;CHECK(!validate_native_session_snapshot(session,room.view(),house.view(),round.view(),items.view(),bad,error));
 // Both acquisition orders are original behavior; saved order and UIDs survive.
 for(auto&f:state.flags)if(f.id=="got_dog_treats")f.value=true;
 state.key_items.push_back({"DogTreats",false,1,0xabd});
 CHECK(validate_native_session_snapshot(session,room.view(),house.view(),round.view(),items.view(),state,error));
 std::swap(state.key_items[1],state.key_items[2]);CHECK(validate_native_session_snapshot(session,room.view(),house.view(),round.view(),items.view(),state,error));
 // A failed grant cannot mark this object collected or create text.
 Host refused;refused.house=host.house;refused.flags=host.flags;refused.reject_grant=true;
 PresentRuntime fail;CHECK(fail.initialize(view,refused,error));CHECK(fail.scene_ready(rng,error));CHECK(!fail.start(map,error));
 CHECK(fail.object_flags().empty()&&refused.keys.empty()&&refused.texts.empty());CHECK(fail.pose(map).sparkles);
 if(argc>2){
  // Explicit test boundary: this is not a played save or evidence that the
  // original key acquisition chain has been completed in the emulator.
  auto fixture=session.defaults();fixture.position_x=96;fixture.position_y=735;fixture.direction_x=0;fixture.direction_y=-1;
  auto policy=std::find_if(session.key_acquisitions().begin(),session.key_acquisitions().end(),[](const auto&k){return k.item_id=="KeyBasement";});
  CHECK(policy!=session.key_acquisitions().end());for(auto&f:fixture.flags)if(f.id==policy->flag_id)f.value=true;
  fixture.key_items.push_back({policy->item_id,false,policy->doses,0xabcde});
  CHECK(validate_native_session_snapshot(session,room.view(),house.view(),round.view(),items.view(),fixture,error));
  std::vector<uint8_t>encoded;CHECK(encode_session_save(fixture,session.compatibility(),encoded,error));
  std::ofstream file(argv[2],std::ios::binary|std::ios::trunc);CHECK(file.good());file.write(reinterpret_cast<const char*>(encoded.data()),std::streamsize(encoded.size()));file.flush();CHECK(file.good());
 }
 std::printf("Map present: %u checks; source object flags, grant ordering, repeated/restored state and save identity\n",checks);
}
