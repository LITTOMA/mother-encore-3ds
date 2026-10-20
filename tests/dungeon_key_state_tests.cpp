#include "encore/dungeon_key_state.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <initializer_list>
#include <sstream>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
void check(bool ok,const char*why){++checks;if(!ok){std::fprintf(stderr,"Dungeon key state %u: %s (%s)\n",checks,why,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
using Projection=std::array<int,19>;
struct Row {unsigned index=0;std::string action;Projection value{};};
struct Harness {
 DungeonKeysView data;DungeonKeyRuntime model;uint32_t key=0,lock=0;bool audio=false;
 int notices=0,notice_visible=0,notice_disabled=0,notice_opened=0,notice_audio=0,indicators=0,indicator_value=-1,texts=0;
 explicit Harness(DungeonKeysView v):data(v){for(uint32_t i=0;i<data.node_count();++i){auto n=data.node(i);if(!key&&n.kind==DungeonKeyKind::Pickup)key=n.id;if(!lock&&n.kind==DungeonKeyKind::Lock)lock=n.id;}CHECK(key&&lock);}
 void prepare(int count,bool got_key,bool opened_lock){
  std::vector<DungeonKeyFlagValue>flags;
  for(uint32_t i=0;i<data.node_count();++i){auto id=data.node(i).id;flags.push_back({id,(id==key&&got_key)||(id==lock&&opened_lock)});}
  std::reverse(flags.begin(),flags.end()); // Saved identities must not depend on table order.
  CHECK(model.prepare(data,data.scene_id(),data.text(data.policy().region),count,flags,error));audio=false;
 }
 void consume(){
  notices=notice_visible=notice_disabled=notice_opened=notice_audio=indicators=texts=0;indicator_value=-1;
  for(const auto&event:model.events()){
   CHECK(event.target.generation==model.generation());
   switch(event.kind){
   case DungeonKeyEventKind::FlagsUpdated:
    ++notices;notice_visible=event.state.visible;notice_disabled=event.state.collision_disabled;notice_opened=event.state.opened;notice_audio=event.target.id==key&&audio;CHECK(event.flag);break;
   case DungeonKeyEventKind::PickupAudioRequested:CHECK(event.target.id==key);audio=true;break;
   case DungeonKeyEventKind::Deleted:if(event.target.id==key)audio=false;break;
   case DungeonKeyEventKind::IndicatorRequested:++indicators;indicator_value=event.counter;break;
   case DungeonKeyEventKind::LockedDialogue:++texts;CHECK(data.text(data.policy().locked_dialogue)=="Reusable/locklocked");break;
   case DungeonKeyEventKind::CounterChanged:case DungeonKeyEventKind::FlagChanged:
   case DungeonKeyEventKind::Hidden:case DungeonKeyEventKind::CollisionDisableDeferred:
   case DungeonKeyEventKind::CollisionDisabled:case DungeonKeyEventKind::DeleteQueued:
   case DungeonKeyEventKind::LockAnimationRequested:case DungeonKeyEventKind::PromptDisabled:break;
   default:CHECK(false);
   }
  }
  model.clear_events();
 }
 Projection capture(bool pickup){
  DungeonKeyState state;bool key_flag=false,lock_flag=false;CHECK(model.state(model.handle(pickup?key:lock),state,error));CHECK(model.flag(model.handle(key),key_flag,error));CHECK(model.flag(model.handle(lock),lock_flag,error));
  return {model.counter(),int(key_flag),int(lock_flag),int(state.alive),state.alive&&state.queued,state.alive&&state.visible,state.alive&&state.collision_disabled,state.alive&&state.prompt_enabled,state.alive&&state.opened,state.alive?int(state.requested_animation):0,state.alive&&pickup&&audio,notices,notice_visible,notice_disabled,notice_opened,notice_audio,indicators,indicator_value,texts};
 }
 std::vector<Projection>run(const std::vector<Row>&rows){
  std::vector<Projection>result;
  auto sequence=[&](std::initializer_list<DungeonKeyEventKind>wanted){CHECK(model.events().size()==wanted.size());size_t i=0;for(auto kind:wanted)CHECK(model.events()[i++].kind==kind);};
  for(const auto&row:rows){
   switch(row.index){
   case 0:prepare(0,false,false);break;
   case 1:CHECK(model.pickup_body_entered(model.handle(key),false,false,audio,error));break;
   case 2:CHECK(model.pickup_body_entered(model.handle(key),true,true,audio,error));break;
   case 3:CHECK(model.pickup_body_entered(model.handle(key),true,false,audio,error));sequence({DungeonKeyEventKind::CounterChanged,DungeonKeyEventKind::FlagChanged,DungeonKeyEventKind::FlagsUpdated,DungeonKeyEventKind::IndicatorRequested,DungeonKeyEventKind::PickupAudioRequested,DungeonKeyEventKind::Hidden,DungeonKeyEventKind::CollisionDisableDeferred});break;
   case 4:CHECK(model.pickup_body_entered(model.handle(key),true,false,audio,error));break;
   case 5:CHECK(model.end_scene_frame(error));break;
   case 6:CHECK(model.pickup_finished(model.handle(key),error));break;
   case 7:CHECK(model.end_scene_frame(error));break;
   case 8:prepare(1,true,false);break;
   case 9:CHECK(model.end_scene_frame(error));break;
   // These are explicit counter-service inputs from the original component
   // fixture, not claimed reachable save checkpoints or normal scene loads.
   case 10:prepare(0,true,false);CHECK(model.end_scene_frame(error));model.clear_events();break;
   case 11:CHECK(model.lock_interact(model.handle(lock),error));sequence({DungeonKeyEventKind::LockedDialogue});break;
   case 12:prepare(1,true,false);CHECK(model.end_scene_frame(error));model.clear_events();CHECK(model.lock_interact(model.handle(lock),error));sequence({DungeonKeyEventKind::CounterChanged,DungeonKeyEventKind::LockAnimationRequested,DungeonKeyEventKind::FlagChanged,DungeonKeyEventKind::FlagsUpdated,DungeonKeyEventKind::IndicatorRequested});break;
   case 13:CHECK(model.lock_open_method(model.handle(lock),error));break;
   case 14:CHECK(model.lock_interact(model.handle(lock),error));break;
   case 17:prepare(0,true,true);CHECK(model.end_scene_frame(error));break;
   default:CHECK(false);
   }
   consume();auto actual=capture(row.index<10);
   if(actual!=row.value){std::fprintf(stderr,"Original reference row %u %s\n",row.index,row.action.c_str());for(size_t i=0;i<actual.size();++i)if(actual[i]!=row.value[i])std::fprintf(stderr," column %zu actual%d expected%d\n",i,actual[i],row.value[i]);}
   CHECK(actual==row.value);result.push_back(actual);
  }
  return result;
 }
};
}
int main(int argc,char**argv){
 CHECK(argc==3);DungeonKeysData data;CHECK(data.load_file(argv[1],error));std::ifstream input(argv[2]);CHECK(bool(input));std::string line;CHECK(bool(std::getline(input,line)));std::vector<Row>rows;
 while(std::getline(input,line)){if(line.empty())continue;std::istringstream stream(line);Row row;CHECK(bool(stream>>row.index>>row.action));for(auto&value:row.value)CHECK(bool(stream>>value));std::string extra;CHECK(!(stream>>extra));rows.push_back(row);}
 CHECK(rows.size()==16);Harness first(data.view()),second(data.view());const auto a=first.run(rows),b=second.run(rows);CHECK(a==b);
 Harness negative(data.view());std::vector<DungeonKeyFlagValue>flags;for(uint32_t i=0;i<data.view().node_count();++i)flags.push_back({data.view().node(i).id,false});DungeonKeyRuntime empty;
 CHECK(!empty.pickup_body_entered({},true,false,false,error));CHECK(!empty.end_scene_frame(error));
 negative.prepare(0,false,false);negative.consume();const auto generation=negative.model.generation();const auto old=negative.model.handle(negative.key);const auto before=negative.capture(true);
 for(int count:{-1,4}){CHECK(!negative.model.prepare(data.view(),data.view().scene_id(),data.view().text(data.view().policy().region),count,flags,error));CHECK(negative.model.generation()==generation&&negative.capture(true)==before);}
 {auto bad=flags;bad[1].id=bad[0].id;CHECK(!negative.model.prepare(data.view(),4,"Catacombs",0,bad,error));CHECK(negative.capture(true)==before);}
 {auto bad=flags;bad[0].id=0;CHECK(!negative.model.prepare(data.view(),4,"Catacombs",0,bad,error));CHECK(negative.capture(true)==before);}
 {auto bad=flags;bad.pop_back();CHECK(!negative.model.prepare(data.view(),4,"Catacombs",0,bad,error));CHECK(negative.capture(true)==before);}
 CHECK(!negative.model.prepare(data.view(),3,"Catacombs",0,flags,error));CHECK(!negative.model.prepare(data.view(),4,"wrong",0,flags,error));CHECK(negative.capture(true)==before);
 CHECK(!negative.model.prepare({},4,"Catacombs",0,flags,error));CHECK(negative.model.generation()==generation&&negative.capture(true)==before);
 CHECK(!negative.model.lock_interact(negative.model.handle(negative.key),error));CHECK(!negative.model.pickup_finished(negative.model.handle(negative.lock),error));CHECK(!negative.model.pickup_body_entered({1,generation},true,false,false,error));CHECK(negative.capture(true)==before);
 negative.prepare(0,false,false);negative.consume();CHECK(!negative.model.pickup_body_entered(old,true,false,false,error));CHECK(negative.model.counter()==0);
 negative.prepare(3,false,false);negative.consume();const auto bounded=negative.capture(true);CHECK(!negative.model.pickup_body_entered(negative.model.handle(negative.key),true,false,false,error));CHECK(negative.capture(true)==bounded&&negative.model.events().empty());
 negative.prepare(0,false,false);negative.consume();CHECK(negative.model.pickup_finished(negative.model.handle(negative.key),error));CHECK(negative.model.end_scene_frame(error));negative.consume();const auto deleted=negative.capture(true);CHECK(!negative.model.pickup_finished(negative.model.handle(negative.key),error));CHECK(negative.capture(true)==deleted);
 negative.prepare(0,false,false);negative.consume();bool full=false;
 for(unsigned i=0;i<1025;++i)if(!negative.model.lock_interact(negative.model.handle(negative.lock),error)){CHECK(i==1024);full=true;break;}
 CHECK(full&&negative.model.events().size()==1024&&negative.model.counter()==0);bool unchanged=false;CHECK(negative.model.flag(negative.model.handle(negative.lock),unchanged,error)&&!unchanged);
 // A capacity failure after tentative counter/flag writes must roll back the
 // whole method, including deferred work and the previously queued events.
 negative.prepare(0,false,false);negative.consume();
 for(unsigned i=0;i<1021;++i)CHECK(negative.model.lock_interact(negative.model.handle(negative.lock),error));
 const auto pending=negative.model.events().size();const auto untouched=negative.capture(true);
 CHECK(!negative.model.pickup_body_entered(negative.model.handle(negative.key),true,false,false,error));
 CHECK(negative.model.events().size()==pending&&negative.capture(true)==untouched);
 negative.model.clear_events();CHECK(negative.model.end_scene_frame(error));CHECK(negative.model.events().empty());
 std::printf("Dungeon key state: %u checks;16 frozen original-method projections twice, stable identity/generation, source gates/order/deferred lifecycle and atomic rejection; detached component only\n",checks);
}
