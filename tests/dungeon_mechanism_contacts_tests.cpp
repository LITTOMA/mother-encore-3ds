#include "encore/dungeon_mechanism_contacts.hpp"
#include "encore/dungeon_mechanism_audio.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
using namespace encore::upstream;
namespace {unsigned checks=0;std::string error;
void check(bool v,const char*s){++checks;if(!v){std::fprintf(stderr,"Mechanism contacts %u: %s (%s)\n",checks,s,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
}
int main(int argc,char**argv){
 CHECK(argc==3);DungeonMechanismPresentationData data;CHECK(data.load_file(argv[1],error));
 std::vector<DungeonSwitchFlag>flags;const auto definitions=data.bindings().graph();
 for(uint32_t i=0;i<definitions.switch_count();++i)flags.push_back({definitions.switch_node(i).id,false});
 DungeonMechanismContacts contacts;DungeonMechanismState state;DungeonMechanismAudioRouter audio;
 DungeonMechanismContactSource body{1,4353,true,true},attack{2,2,true,true};std::array<uint8_t,128>overlaps{};
 CHECK(!contacts.prepare({},1,error)&&!contacts.prepare(data,0,error)&&!contacts.valid());
 CHECK(!contacts.begin(1,body,attack,state,false,true,error)&&!contacts.end(1,body,attack,overlaps.data(),5,error));
 std::ifstream input(argv[2]);CHECK(input);std::string line;unsigned previous=UINT32_MAX,rows=0,cases=0;uint64_t epoch=0;
 std::vector<DungeonMechanismCallback>timers;
 while(std::getline(input,line)){
  unsigned ci,pi,ready,target,bat,inside,paused,can,bl,bm,ae,al,am,monitor,on,hits,denied;std::istringstream in(line);
  CHECK(in>>ci>>pi>>ready>>target>>bat>>inside>>paused>>can>>bl>>bm>>ae>>al>>am>>monitor>>on>>hits>>denied);std::string trailing;CHECK(!(in>>trailing));
  if(ci!=previous){CHECK(pi==0);previous=ci;++cases;++epoch;contacts={};state={};audio={};timers.clear();
   CHECK(contacts.prepare(data,epoch,error)&&contacts.count()==5&&contacts.shape_index(5)==SIZE_MAX);
   CHECK(!contacts.prepare(data,epoch,error)&&state.prepare(data.bindings(),epoch,flags,error)&&audio.prepare(data,epoch,error));
   for(const auto&e:state.take_events())if(e.kind==DungeonMechanismEventKind::Request&&e.request.kind==DungeonSwitchEventKind::ReadyTimer)timers.push_back({e.epoch,e.callback_ticket});
   CHECK(timers.size()==5);
  }
  if(ready){for(const auto&t:timers)CHECK(state.timer_finished(t,error));timers.clear();state.take_events();}
  body={bl,bm,true,true};attack={al,am,bool(ae),bool(monitor)};overlaps.fill(0);bool found=false;
  for(size_t i=0;i<contacts.count();++i){const auto&s=data.shapes()[contacts.shape_index(i)];if(s.owner==target){CHECK((s.role==DungeonMechanismShapeRole::Interaction)==bool(bat));overlaps[i]=uint8_t(inside);found=true;}}
  CHECK(found);unsigned got_hits=0,got_denied=0;
  for(unsigned tick=0;tick<4;++tick){
   CHECK(!contacts.begin(epoch+1,body,attack,state,paused,can,error)&&!contacts.sampling());
   auto bad=attack;bad.layer=0x100000;CHECK(!contacts.begin(epoch,body,bad,state,paused,can,error)&&!contacts.sampling());
   DungeonMechanismState foreign;CHECK(!contacts.begin(epoch,body,attack,foreign,paused,can,error)&&!contacts.sampling());
   CHECK(contacts.begin(epoch,body,attack,state,paused,can,error)&&contacts.sampling());
   CHECK(!contacts.begin(epoch,body,attack,state,paused,can,error)&&contacts.sampling());
   const auto events=state.take_events();std::vector<DungeonMechanismSoundCommand>sounds;CHECK(audio.plan(events,sounds,error));
   for(const auto&e:events)if(e.kind==DungeonMechanismEventKind::Request){if(e.request.kind==DungeonSwitchEventKind::SwitchHit)++got_hits;if(e.request.kind==DungeonSwitchEventKind::DeniedAudio)++got_denied;}
   CHECK(!contacts.end(epoch,body,attack,nullptr,contacts.count(),error)&&contacts.sampling());
   CHECK(!contacts.end(epoch,body,attack,overlaps.data(),contacts.count()-1,error)&&contacts.sampling());
   auto invalid=overlaps;invalid[0]=2;CHECK(!contacts.end(epoch,body,attack,invalid.data(),contacts.count(),error)&&contacts.sampling());
   CHECK(contacts.end(epoch,body,attack,overlaps.data(),contacts.count(),error)&&!contacts.sampling());
   CHECK(!contacts.end(epoch,body,attack,overlaps.data(),contacts.count(),error));
  }
  CHECK(got_hits==hits&&got_denied==denied);unsigned actual=0;const auto graph=data.bindings().graph();
  for(uint32_t i=0;i<graph.switch_count();++i){DungeonMechanismPose pose;CHECK(state.inspect(graph.switch_node(i).id,pose,error));actual|=unsigned(pose.on)<<i;}
  CHECK(actual==on);++rows;
 }
 CHECK(input.eof()&&rows==49&&cases==22);
 // Byte-identical independently loaded data is still a different live owner.
 DungeonMechanismPresentationData second;CHECK(second.load_file(argv[1],error));DungeonMechanismState other;CHECK(other.prepare(second.bindings(),epoch,flags,error));
 CHECK(!contacts.begin(epoch,body,attack,other,false,true,error)&&!contacts.sampling());
 // Disabling a source cancels an unpublished enter, then reenable can enter.
 DungeonMechanismContacts transient;DungeonMechanismState target_state;CHECK(transient.prepare(data,999,error)&&target_state.prepare(data.bindings(),999,flags,error));
 for(const auto&e:target_state.take_events()){
  if(e.kind==DungeonMechanismEventKind::Request&&e.request.kind==DungeonSwitchEventKind::ReadyTimer)CHECK(target_state.timer_finished({e.epoch,e.callback_ticket},error));
 }
 target_state.take_events();
 body={1,4353,true,true};attack={2,2,true,true};overlaps.fill(0);for(size_t i=0;i<transient.count();++i)if(data.shapes()[transient.shape_index(i)].role==DungeonMechanismShapeRole::Interaction){overlaps[i]=1;break;}
 CHECK(transient.begin(999,body,attack,target_state,false,true,error)&&transient.end(999,body,attack,overlaps.data(),transient.count(),error));
 attack.enabled=false;CHECK(transient.begin(999,body,attack,target_state,false,true,error));CHECK(target_state.take_events().empty());
 CHECK(transient.end(999,body,attack,overlaps.data(),transient.count(),error));attack.enabled=true;
 CHECK(transient.begin(999,body,attack,target_state,false,true,error)&&target_state.take_events().empty());
 CHECK(transient.end(999,body,attack,overlaps.data(),transient.count(),error));
 // A downstream graph capacity failure must preserve the pending enter and
 // every mechanism state/event. It can be retried against a prepared consumer.
 DungeonMechanismContacts pending;DungeonMechanismState crowded,healthy;
 CHECK(pending.prepare(data,1000,error)&&crowded.prepare(data.bindings(),1000,flags,error)&&healthy.prepare(data.bindings(),1000,flags,error));
 for(auto*receiver:{&crowded,&healthy}){
  for(const auto&e:receiver->take_events()){
   if(e.kind==DungeonMechanismEventKind::Request&&e.request.kind==DungeonSwitchEventKind::ReadyTimer)CHECK(receiver->timer_finished({e.epoch,e.callback_ticket},error));
  }
  receiver->take_events();
 }
 overlaps.fill(0);const auto owner=definitions.switch_node(1).id;
 for(size_t i=0;i<pending.count();++i)if(data.shapes()[pending.shape_index(i)].owner==owner)overlaps[i]=1;
 CHECK(pending.begin(1000,body,attack,crowded,false,true,error)&&pending.end(1000,body,attack,overlaps.data(),pending.count(),error));
 unsigned filled=0;while(filled<1024&&crowded.operate(owner,DungeonSwitchMethod::Hit,true,false,true,error))++filled;
 CHECK(filled>0&&filled<1024&&error.find("capacity")!=std::string::npos);const auto before=crowded.event_count();DungeonMechanismPose pose,after;
 CHECK(crowded.inspect(owner,pose,error));CHECK(!pending.begin(1000,body,attack,crowded,false,true,error)&&!pending.sampling());
 CHECK(crowded.event_count()==before&&crowded.inspect(owner,after,error)&&pose.on==after.on);
 CHECK(pending.begin(1000,body,attack,healthy,false,true,error)&&pending.sampling());
 unsigned retried=0;for(const auto&e:healthy.take_events())if(e.kind==DungeonMechanismEventKind::Request&&e.request.kind==DungeonSwitchEventKind::SwitchHit)++retried;
 CHECK(retried==1);
 std::printf("Mechanism contacts: %u checks;49 native settled observations/22 cases,checked source filters,enter/stay/reentry,paused/not-ready consumption,one-way plate,denied sound,atomic phase/epoch/data-owner rejection; no live map or exact-frame claim\n",checks);
}
