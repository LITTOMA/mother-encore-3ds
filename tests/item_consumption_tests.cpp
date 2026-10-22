#include "encore/field_menus.hpp"
#include "encore/content.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>

using namespace encore::upstream;
static unsigned checks=0;
static std::string error;
#define CHECK(x) do { ++checks; if(!(x)){std::fprintf(stderr,"Item consume check %d: %s; %s\n",__LINE__,#x,error.c_str());std::exit(1);} } while(0)
static std::vector<uint8_t>read(const std::string&path){std::ifstream file(path,std::ios::binary);CHECK(bool(file));return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};}
struct Reader {
 const uint8_t*p;size_t n;
 uint32_t u(){CHECK(n>=4);uint32_t v=uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;p+=4;n-=4;return v;}
 int64_t q(){uint64_t bits=u();bits|=uint64_t(u())<<32;int64_t v;std::memcpy(&v,&bits,8);return v;}
 uint32_t count(uint32_t maximum){auto value=u();CHECK(value<=maximum);return value;}
 std::string text(){auto size=count(4096);CHECK(size<=n);std::string value(reinterpret_cast<const char*>(p),size);p+=size;n-=size;return value;}
 void character(SessionCharacter&c){
  c.hp=q();c.pp=q();CHECK(c.hp>=0&&c.pp>=0);c.status.clear();
  auto statuses=count(16);for(uint32_t i=0;i<statuses;++i){SessionStatus s;s.status_id=text();s.passive_healing_turns=q();CHECK(!s.status_id.empty()&&s.passive_healing_turns>=0);c.status.push_back(std::move(s));}
  c.inventory.clear();auto items=count(16);CHECK(items);
  for(uint32_t i=0;i<items;++i){SessionItem item;item.item_id=text();item.uid=u();item.doses=q();auto equipped=count(1);item.equipped=equipped!=0;CHECK(!item.item_id.empty()&&item.doses>0&&item.doses<=65535);c.inventory.push_back(std::move(item));}
 }
};
static void same(const SessionSnapshot&a,const SessionSnapshot&b,const SessionSaveCompatibility&compatibility){std::vector<uint8_t>x,y;CHECK(encode_session_save(a,compatibility,x,error));CHECK(encode_session_save(b,compatibility,y,error));CHECK(x==y);}

int main(int argc,char**argv){
 CHECK(argc==3||argc==4);const std::string root=argv[1],fixture=argv[2];
 FieldMenusData data,legacy;NativeSessionData session;StartupSettingsData settings;
 CHECK(data.load_file((fixture+"/menu-v3.encmenus").c_str(),error));CHECK(data.has_consume_effects());
 CHECK(legacy.load_file((fixture+"/menu-v2.encmenus").c_str(),error));CHECK(!legacy.has_consume_effects());
 CHECK(session.load_file((root+"/session/opening.encsession").c_str(),error));CHECK(settings.load_file((root+"/ui/opening.encsettings").c_str(),error));
 const auto source=read(argc==4?argv[3]:fixture+"/original.bin");CHECK(source.size()>=16&&source.size()<=32*1024*1024);
 CHECK(std::memcmp(source.data(),"ENCCNSR1",8)==0);Reader input{source.data()+8,source.size()-8};CHECK(input.u()==1);const auto count=input.count(1024);CHECK(count);
 FieldMenusState first_state;
 for(uint32_t i=0;i<count;++i){
  CHECK(input.u()==i);auto before=session.defaults(),expected=session.defaults();input.character(before.characters[0]);input.character(expected.characters[0]);
  const auto selected=input.count(15),maxhp=input.count(INT32_MAX),maxpp=input.count(INT32_MAX),kind=input.count(4),value=input.u(),doses=input.count(65535),sound=input.count(1);
  CHECK(kind&&maxhp);const auto message=input.text(),audio=input.text(),slot=input.text();
  std::vector<FieldMenuConsumeSignal>signals;auto signal_count=input.count(2);
  for(uint32_t n=0;n<signal_count;++n){auto type=input.count(2);CHECK(type);signals.push_back({FieldMenuConsumeSignalKind(type),input.q(),input.q()});}
  FieldMenusState state;state.session=before;state.stats={{int32_t(maxhp),int32_t(maxpp),0,0,0,0,0}};state.preferences=data.defaults;
  if(i==0)first_state=state;
  FieldMenusState after;FieldMenuConsumeResult result;CHECK(consume_field_menu_item(data,state,selected,after,result,error));
  same(after.session,expected,session.compatibility());CHECK(uint32_t(result.kind)==kind&&result.value==value&&result.retained_doses==doses&&result.eat_sound==bool(sound));
  CHECK(result.message_key==message&&!result.message.empty());CHECK(result.signals.size()==signals.size());
  for(size_t n=0;n<signals.size();++n)CHECK(result.signals[n].kind==signals[n].kind&&result.signals[n].value==signals[n].value&&result.signals[n].maximum==signals[n].maximum);
  if(sound)CHECK(data.consume_sound==audio&&data.consume_slot==slot);else CHECK(audio.empty()&&slot.empty());
  // Native UI calls the same checked effect and applies only owned changes.
  FieldMenusState live=state;unsigned commits=0;bool reject=false;
  FieldMenusHost host;
  host.read=[&](FieldMenusState&out,std::string&){out=live;return true;};
  host.commit=[&](const FieldMenusState&observed,const FieldMenusChanges&changes,std::string&e){if(reject){e="fixture rejects commit";return false;}SessionSnapshot merged;if(!merge_field_menu_session(observed,changes,live.session,merged,e))return false;live.session=std::move(merged);++commits;return true;};
  host.equip=[](uint32_t,bool,uint32_t,std::string&){return false;};host.locale=[](std::string_view,std::string&){return true;};
  FieldMenus menu;CHECK(menu.initialize(data,settings,host,error));CHECK(menu.open(1,error));
  CHECK(menu.input(1,0,false,false,false,0,0,error));CHECK(menu.selection()==selected);
  CHECK(menu.input(0,0,true,false,false,0,0,error));CHECK(menu.phase()==FieldMenusPhase::Actions);
  CHECK(menu.input(0,0,true,false,false,0,0,error));CHECK(menu.phase()==FieldMenusPhase::Target);
  menu.take_sounds();reject=true;CHECK(!menu.input(0,0,true,false,false,0,0,error));CHECK(commits==0);same(live.session,before,session.compatibility());
  reject=false;CHECK(menu.input(0,0,true,false,false,0,0,error));CHECK(commits==1&&menu.phase()==FieldMenusPhase::Message);same(live.session,expected,session.compatibility());
  const auto sounds=menu.take_sounds();CHECK(std::count(sounds.begin(),sounds.end(),11u)==int(sound));
  // A stale-health callback cannot apply a dose/HP/status partial commit.
  FieldMenusChanges changes;changes.inventory=after.session.characters[0].inventory;changes.keys=before.key_items;changes.settings=before.settings;changes.preferences=data.defaults;changes.health_observed=true;
  changes.hp=after.session.characters[0].hp;changes.status=after.session.characters[0].status;
  auto current=before;current.characters[0].hp=before.characters[0].hp?before.characters[0].hp-1:1;auto untouched=current;
  SessionSnapshot out;CHECK(!merge_field_menu_session(state,changes,current,out,error));same(current,untouched,session.compatibility());
 }
 CHECK(input.n==0);
 // All newly decoded resource failures retain the previous checked owner.
 auto bytes=read(fixture+"/menu-v3.encmenus");for(size_t n=0;n<bytes.size();++n)CHECK(!data.load(bytes.data(),n,error));CHECK(data.valid()&&data.has_consume_effects());
 unsigned bad_menus=0;{std::ifstream file(fixture+"/negative-menu-count.txt");CHECK(bool(file>>bad_menus));}CHECK(bad_menus>=20);
 for(unsigned n=0;n<bad_menus;++n){auto bad=read(fixture+"/bad-menu-"+std::to_string(n)+".encmenus");CHECK(!data.load(bad.data(),bad.size(),error));CHECK(data.valid()&&data.has_consume_effects()&&data.consume_effects.size()==4);}
 std::string expanded="retained";CHECK(expand_field_item_text("{owner}: {value}","[Hero]",50,expanded,error));CHECK(expanded=="[Hero]: 50");
 for(const auto*text:{"{button:0}","{unknown}","{value", "}","[image]"}){expanded="retained";CHECK(!expand_field_item_text(text,"owner",1,expanded,error));CHECK(expanded=="retained");}
 FieldMenusState before;before.session=session.defaults();before.stats={{30,26,0,0,0,0,0}};before.preferences=data.defaults;
 FieldMenusState out=before;FieldMenuConsumeResult result;result.message="retained";CHECK(!consume_field_menu_item(legacy,before,0,out,result,error));CHECK(result.message=="retained");
 auto refuses=[&](const FieldMenusData&rules,const FieldMenusState&state,uint32_t selected){auto retained=first_state;FieldMenuConsumeResult unchanged;unchanged.message="retained";CHECK(!consume_field_menu_item(rules,state,selected,retained,unchanged,error));CHECK(unchanged.message=="retained");same(retained.session,first_state.session,session.compatibility());};
 refuses(data,first_state,UINT32_MAX);
 auto bad_state=first_state;bad_state.session.characters[0].inventory[1].equipped=true;refuses(data,bad_state,1);
 bad_state=first_state;bad_state.session.characters[0].inventory[1].doses=0;refuses(data,bad_state,1);
 bad_state=first_state;bad_state.session.characters[0].inventory[1].doses=65535;refuses(data,bad_state,1);
 bad_state=first_state;bad_state.session.characters[0].hp=first_state.stats[0]+1;refuses(data,bad_state,1);
 bad_state=first_state;bad_state.session.characters[0].status.push_back({"unknown_status",0});refuses(data,bad_state,1);
 auto bad_data=data;bad_data.consume_lane=0;refuses(bad_data,first_state,1);
 bad_data=data;bad_data.consume_effects.push_back(bad_data.consume_effects.front());refuses(bad_data,first_state,1);
 bad_data=data;bad_data.consume_effects.front().hp=0;refuses(bad_data,first_state,1);
 bad_data=data;bad_data.consume_effects.front().messages[1].en="{unknown}";refuses(bad_data,first_state,1);
 std::printf("Original item consumption: %u cases, %u checks; kernel, menu, atomic failure and source requests\n",count,checks);
}
