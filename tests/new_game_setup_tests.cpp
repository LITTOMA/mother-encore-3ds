#include "encore/new_game_setup.hpp"
#include "encore/session_restore.hpp"
#include "encore/fresh_house.hpp"
#include "encore/battle_action_presentation.hpp"
#include "encore/content.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <limits>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Naming line %d: %s: %s\n",__LINE__,#x,error.c_str());std::exit(1);}}while(0)
void input(NewGameSetup&m,NamingInput in){CHECK(m.step(.1,in,error));}
void choose(NewGameSetup&m,uint32_t desired){
 const auto&p=m.data()->panels[m.panel()];std::vector<int>parent(p.size(),-1),directions(p.size());std::deque<uint32_t>q{m.selected()};parent[m.selected()]=int(m.selected());while(!q.empty()){auto n=q.front();q.pop_front();for(unsigned d=0;d<4;++d){auto v=p[n].neighbors[d];if(parent[v]<0){parent[v]=int(n);directions[v]=int(d);q.push_back(v);}}}CHECK(parent[desired]>=0);
 std::vector<int>path;for(auto n=desired;n!=m.selected();n=uint32_t(parent[n]))path.push_back(directions[n]);std::reverse(path.begin(),path.end());for(int d:path)input(m,{d==0?-1:d==1?1:0,d==2?-1:d==3?1:0});CHECK(m.selected()==desired);
}
void type(NewGameSetup&m,const std::string&s){for(char c:s){bool found=false;for(unsigned panel=0;panel<m.data()->panels.size();++panel){const auto&p=m.data()->panels[panel];auto it=std::find_if(p.begin(),p.end(),[&](const auto&k){return k.kind==0&&k.value==std::string(1,c);});if(it!=p.end()){if(panel!=m.panel())input(m,{0,0,false,false,true});choose(m,uint32_t(it-p.begin()));input(m,{0,0,true});found=true;break;}}CHECK(found);}}
void fix(std::vector<uint8_t>&b){auto crc=encore::crc32(b.data()+24,b.size()-24);for(unsigned i=0;i<4;++i)b[16+i]=uint8_t(crc>>(8*i));}
struct Resolver {const NewGameSetupData*data;std::string name;};
bool resolve(void*opaque,RoundView view,uint32_t index,std::string&out){auto&r=*static_cast<Resolver*>(opaque);return r.data->battle_text(view,index,r.name,out);}
}
int main(int argc,char**argv){
 const std::string root=argc>1?argv[1]:"romfs/data";NewGameSetupData data;CHECK(data.load_file((root+"/opening.encnewgame").c_str(),error));CHECK(data.valid()&&data.maximum==7&&data.defaults.size()==7&&data.fields.size()==6&&data.fields.back().maximum==13);
 StartupSettingsData settings;CHECK(settings.load_file((root+"/opening.encsettings").c_str(),error));
 NewGameSetup menu;CHECK(menu.open(data,settings,error));CHECK(menu.name().empty()&&menu.selected()==0&&menu.panel()==0);input(menu,{0,0,false,true});CHECK(menu.active());CHECK(menu.step(.5,{},error));input(menu,{0,0,false,true});CHECK(menu.phase()==NamingPhase::Cancelled);
 CHECK(menu.open(data,settings,error));type(menu,"Minnie");input(menu,{0,0,false,false,false,false,true});CHECK(menu.active()&&menu.prompt()==data.texts[1]);for(unsigned i=0;i<6;++i)input(menu,{0,0,false,true});type(menu,"       ");input(menu,{0,0,false,false,false,false,true});CHECK(menu.active()&&menu.prompt()==data.texts[1]);
 CHECK(menu.open(data,settings,error));input(menu,{0,0,false,false,false,true});CHECK(data.panels[0][menu.selected()].kind==1);for(size_t i=0;i<8;++i){input(menu,{0,0,true});CHECK(menu.name()==data.defaults[i%7]);}
 CHECK(menu.open(data,settings,error));type(menu,"PortKid");type(menu,"X");CHECK(menu.name()=="PortKid");input(menu,{0,0,false,false,false,false,true});CHECK(menu.active()&&menu.field_index()==1);
 // Source previous/duplicate rules use pending values, never invented defaults.
 type(menu,"portkid");input(menu,{0,0,false,false,false,false,true});CHECK(menu.field_index()==1&&menu.prompt()==data.texts[2]);
 for(unsigned i=0;i<7;++i)input(menu,{0,0,false,true});input(menu,{0,0,false,true});CHECK(menu.field_index()==0&&menu.name()=="PortKid");
 input(menu,{0,0,false,false,false,false,true});CHECK(menu.field_index()==1&&menu.name().empty());
 for(unsigned field=1;field<5;++field){CHECK(menu.field_index()==field);input(menu,{0,0,false,false,false,true});for(size_t i=0;i<8;++i){input(menu,{0,0,true});CHECK(menu.name()==data.fields[field].defaults[i%7]);}input(menu,{0,0,false,false,false,false,true});}
 CHECK(menu.field_index()==5&&menu.name().empty());type(menu,"PortKid");input(menu,{0,0,false,false,false,false,true});CHECK(menu.active()&&menu.prompt()==data.texts[2]);
 for(unsigned i=0;i<7;++i)input(menu,{0,0,false,true});type(menu,"Apple pie");input(menu,{0,0,false,false,false,false,false,true});CHECK(menu.field_index()==4);input(menu,{0,0,false,false,false,false,true});CHECK(menu.field_index()==5&&menu.name()=="Apple pie");
 type(menu,"12345");CHECK(menu.name()=="Apple pie1234");for(unsigned i=0;i<4;++i)input(menu,{0,0,false,true});input(menu,{0,0,false,false,false,false,true});CHECK(menu.phase()==NamingPhase::Settings);
 // Settings B returns to food; submenus own cancellation and preview changes.
 input(menu,{0,0,false,true});CHECK(menu.phase()==NamingPhase::Editing&&menu.field_index()==5&&menu.name()=="Apple pie");input(menu,{0,0,false,false,false,false,true});CHECK(menu.phase()==NamingPhase::Settings);
 const auto initial_settings=menu.settings();input(menu,{0,0,true});CHECK(menu.phase()==NamingPhase::SettingOption);input(menu,{0,1});CHECK(menu.option()==1);input(menu,{0,0,false,true});CHECK(menu.settings().text_speed==initial_settings.text_speed);
 input(menu,{0,0,true});input(menu,{0,-1});CHECK(menu.option()==2);input(menu,{0,0,true});CHECK(menu.settings().text_speed==settings.speeds[2]);
 input(menu,{0,1});input(menu,{0,0,true});input(menu,{0,1});CHECK(menu.preview_flavor()==settings.flavors[1]&&menu.settings().menu_flavor==initial_settings.menu_flavor);input(menu,{0,0,false,true});CHECK(menu.preview_flavor()==initial_settings.menu_flavor);
 input(menu,{0,0,true});input(menu,{0,-1});input(menu,{0,-1});input(menu,{0,0,true});CHECK(menu.settings().menu_flavor==settings.flavors[5]);
 input(menu,{0,1});input(menu,{0,0,true});input(menu,{0,-1});input(menu,{0,0,true});CHECK(menu.settings().button_prompts==settings.prompts[3]);
 input(menu,{0,1});input(menu,{0,0,true});CHECK(menu.phase()==NamingPhase::Confirmation);const auto saved_values=menu.values();input(menu,{0,0,false,true});CHECK(menu.phase()==NamingPhase::Editing&&menu.field_index()==0&&menu.values()==saved_values&&menu.settings().text_speed==settings.speeds[2]);
 for(unsigned i=0;i<6;++i)input(menu,{0,0,false,false,false,false,true});CHECK(menu.phase()==NamingPhase::Settings);for(unsigned i=0;i<3;++i)input(menu,{0,1});input(menu,{0,0,true});input(menu,{0,1});input(menu,{0,0,true});CHECK(menu.phase()==NamingPhase::Editing&&menu.values()==saved_values&&menu.field_index()==0);
 for(unsigned i=0;i<6;++i)input(menu,{0,0,false,false,false,false,true});for(unsigned i=0;i<3;++i)input(menu,{0,1});input(menu,{0,0,true});CHECK(menu.phase()==NamingPhase::Confirmation&&menu.confirmation()==0);input(menu,{0,0,true});CHECK(menu.phase()==NamingPhase::Accepted);
 auto named=menu;NativeSessionData session;RoomData room;HouseData house;BattleRoundData round;ItemData items;BattleData battle;RestoreData restore;PhoneData phone;
 CHECK(session.load_file((root+"/opening.encsession").c_str(),error));CHECK(room.load_file((root+"/opening.encroom").c_str(),error));CHECK(house.load_file((root+"/opening.enchouse").c_str(),error));CHECK(round.load_file((root+"/doll-entry.encround").c_str(),error));CHECK(items.load_file((root+"/opening.encitems").c_str(),error));CHECK(battle.load_file((root+"/opening.encbattle").c_str(),error));CHECK(restore.load_file((root+"/opening.encrestore").c_str(),room.view(),house.view(),error));CHECK(phone.load_file((root+"/opening.encphone").c_str(),error));
 SourceRandom live_random{19};const auto old_random=live_random.state();std::vector<uint32_t>live_ledger;
 auto staged_random=live_random;auto staged_ledger=live_ledger;SessionSnapshot state;unsigned clock_calls=0;
 LoadRngClockProvider clock=[&](LoadRngClockSample&sample,std::string&){sample={1700000000,++clock_calls};return true;};
 CHECK(stage_new_game_startup(session,restore,staged_random,staged_ledger,clock,state,error));CHECK(clock_calls==4&&staged_ledger.size()==4&&live_random.state()==old_random&&live_ledger.empty());
 CHECK(state.key_items[0].uid==staged_ledger[0]&&state.characters[0].inventory[0].uid==staged_ledger[1]&&state.characters[2].inventory[0].uid==staged_ledger[2]&&state.characters[2].inventory[1].uid==staged_ledger[3]);
 auto failed_random=live_random;auto failed_ledger=live_ledger;SessionSnapshot untouched=session.defaults();const auto old_untouched=untouched.characters[0].nickname;
 LoadRngClockProvider badclock=[](LoadRngClockSample&,std::string&e){e="expected clock failure";return false;};CHECK(!stage_new_game_startup(session,restore,failed_random,failed_ledger,badclock,untouched,error));CHECK(failed_random.state()==old_random&&failed_ledger.empty()&&untouched.characters[0].nickname==old_untouched);
 const auto staged_ninten_uid=state.characters[0].inventory[0].uid;CHECK(named.apply(state,session,error));CHECK(state.characters.size()==5&&state.party==session.defaults().party&&state.favorite_food=="Apple pie"&&state.characters[0].inventory[0].uid==staged_ninten_uid);CHECK(state.characters[0].nickname=="PortKid"&&session.defaults().characters[0].nickname=="Ninten");
 PreparedSessionRestore prepared;CHECK(prepare_session_restore(session,room.view(),house.view(),round.view(),items.view(),battle.view(),state,prepared,error));NativeSnapshotInput live{prepared.state,&prepared.stats,&prepared.inventory};SessionSnapshot recorded;CHECK(build_native_session_snapshot(session,room.view(),house.view(),round.view(),items.view(),live,recorded,error));std::vector<uint8_t>bytes;CHECK(encode_session_save(recorded,session.compatibility(),bytes,error));SessionSnapshot decoded;CHECK(decode_session_save(bytes.data(),bytes.size(),session.compatibility(),decoded,error));CHECK(decoded.settings.text_speed==settings.speeds[2]&&decoded.settings.menu_flavor==settings.flavors[5]&&decoded.settings.button_prompts==settings.prompts[3]);CHECK(decoded.characters[0].nickname=="PortKid"&&decoded.favorite_food=="Apple pie"&&decoded.characters.size()==5&&decoded.characters[2].inventory[1].uid==staged_ledger[3]);CHECK(prepare_session_restore(session,room.view(),house.view(),round.view(),items.view(),battle.view(),decoded,prepared,error));SourceRandom random{19};std::unique_ptr<FreshHouseState>fresh;CHECK(prepare_fresh_house(prepared,restore,room.view(),house.view(),battle.view(),phone.view(),random,{400,240},fresh,error));CHECK(fresh->finish_scene_ready());
 // Existing schema/default saves are still decoded without rewriting bytes.
 CHECK(encode_session_save(session.defaults(),session.compatibility(),bytes,error));const auto old=bytes;CHECK(decode_session_save(bytes.data(),bytes.size(),session.compatibility(),decoded,error));CHECK(decoded.characters[0].nickname=="Ninten");CHECK(encode_session_save(decoded,session.compatibility(),bytes,error));CHECK(bytes==old);
 for(const auto&filename:{"opening.encround","doll-entry.encround","pillow-entry.encround"}){
  CHECK(round.load_file((root+"/"+filename).c_str(),error));for(uint32_t i=0;i<round.view().count(RoundSection::Texts);++i){std::string value;CHECK(data.battle_text(round.view(),i,"Ninten",value));CHECK(value==round.view().string(round.view().text(i).text));CHECK(data.battle_text(round.view(),i,"PortKid",value));CHECK(value.find("Ninten")==std::string::npos);}
  CHECK(!data.battle_text(round.view(),round.view().count(RoundSection::Texts),"PortKid",error));
 }
 CHECK(round.load_file((root+"/opening.encround").c_str(),error));BattleActionPresentation presentation;CHECK(presentation.begin(round.view(),battle.view(),random));const auto text=round.view().skill(round.view().binding().basic_skill).dialog;BattleRoundCue cue;cue.kind=BattleRoundCueKind::Dialogue;cue.text=text;CHECK(presentation.emit(cue,random));CHECK(presentation.dialogue().text()=="Ninten attacks!");Resolver binding{&data,"PortKid"};presentation.set_text_resolver(resolve,&binding);CHECK(presentation.emit(cue,random));CHECK(presentation.dialogue().text()=="PortKid attacks!");
 // Unknown values never become loadable merely because SessionSettings exists.
 auto invalid_settings=session.defaults();invalid_settings.settings.text_speed=.04;CHECK(!session.supports_settings(invalid_settings.settings));invalid_settings=session.defaults();invalid_settings.settings.menu_flavor="Unknown";CHECK(!session.supports_settings(invalid_settings.settings));invalid_settings=session.defaults();invalid_settings.settings.button_prompts="Unknown";CHECK(!session.supports_settings(invalid_settings.settings));
 CHECK(encore::read_file((root+"/opening.encsettings").c_str(),bytes,1024*1024,error));auto invalid_pack=bytes;invalid_pack[8]=99;CHECK(!settings.load(invalid_pack.data(),invalid_pack.size(),error));invalid_pack=bytes;invalid_pack.back()^=1;CHECK(!settings.load(invalid_pack.data(),invalid_pack.size(),error));invalid_pack=bytes;invalid_pack[24]=0xff;fix(invalid_pack);CHECK(!settings.load(invalid_pack.data(),invalid_pack.size(),error));for(auto cut:{size_t(0),size_t(23),bytes.size()-1})CHECK(!settings.load(bytes.data(),cut,error));CHECK(settings.valid());
 // Malformed bytes preserve the previous checked owner and fail closed.
 CHECK(encore::read_file((root+"/opening.encnewgame").c_str(),bytes,1024*1024,error));auto bad=bytes;bad.back()^=1;CHECK(!data.load(bad.data(),bad.size(),error));CHECK(data.valid());bad=bytes;bad[8]=4;CHECK(!data.load(bad.data(),bad.size(),error));bad=bytes;bad[24]=0;fix(bad);CHECK(!data.load(bad.data(),bad.size(),error));for(auto cut:{size_t(0),size_t(23),bytes.size()-1})CHECK(!data.load(bytes.data(),cut,error));
 CHECK(menu.open(data,settings,error));CHECK(!menu.step(std::numeric_limits<double>::quiet_NaN(),{},error));CHECK(!menu.apply(state,session,error));
 std::printf("Startup names/settings/confirmation: %u checks; keyboard, cancel/default/blocked names, live battle resolver, Record codec and fresh LOAD passed.\n",checks);
}
