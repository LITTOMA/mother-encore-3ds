#include "encore/dungeon_switch_state.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <initializer_list>
#include <sstream>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;void check(bool v,const char*s){++checks;if(!v){std::fprintf(stderr,"Dungeon switch state %u: %s (%s)\n",checks,s,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
using Projection=std::array<double,71>;
struct Row {unsigned index=0;std::string action;Projection value{};};
struct Harness {
 DungeonSwitchesView data;DungeonSwitchRuntime model;bool paused=false,cutscene=false;
 std::vector<bool>audio;std::vector<DungeonSwitchToken>ready,wall;DungeonSwitchToken camera;
 unsigned player_events=1,ui_events=1,audio_events=1;std::array<double,5>camera_values{};
 explicit Harness(DungeonSwitchesView v):data(v),audio(v.switch_count()){}
 uint32_t id(std::string_view path)const{for(uint32_t i=0;i<data.switch_count();++i)if(data.text(data.switch_node(i).path)==path)return data.switch_node(i).id;return 0;}
 uint32_t index(uint32_t id)const{for(uint32_t i=0;i<data.switch_count();++i)if(data.switch_node(i).id==id)return i;CHECK(false);return 0;}
 void consume(){
  for(auto event:model.events()){
   CHECK(event.target.generation==model.generation());
   switch(event.kind){
   case DungeonSwitchEventKind::ReadyTimer:CHECK(event.duration==data.policy().ready_delay);ready.push_back(event.token);break;
   case DungeonSwitchEventKind::WallTimer:CHECK(event.duration==data.policy().post_delay);wall.push_back(event.token);break;
   case DungeonSwitchEventKind::SwitchAudio:audio[index(event.target.id)]=true;break;
   case DungeonSwitchEventKind::Pause:paused=true;player_events=player_events*4+1;break;
   case DungeonSwitchEventKind::Unpause:paused=false;player_events=player_events*4+2;break;
   case DungeonSwitchEventKind::Cutscene:cutscene=event.value;ui_events=ui_events*4+(event.value?2:1);break;
   case DungeonSwitchEventKind::MusicFade:CHECK(event.duration==data.policy().music_fade);audio_events=audio_events*4+1;break;
   case DungeonSwitchEventKind::MusicResume:audio_events=audio_events*4+2;break;
   case DungeonSwitchEventKind::DeniedAudio:CHECK(data.text(data.policy().denied_audio)=="Audio/Sound effects/M3/bump.wav"&&data.text(data.policy().denied_channel)=="switch");audio_events=audio_events*4+3;break;
   case DungeonSwitchEventKind::CameraMove:case DungeonSwitchEventKind::CameraReturn:
    CHECK(camera_values[0]==0);camera=event.token;camera_values={1,event.kind==DungeonSwitchEventKind::CameraMove?1.:2.,event.x,event.y,event.duration};break;
   case DungeonSwitchEventKind::SwitchAnimation:case DungeonSwitchEventKind::ObjectAnimation:case DungeonSwitchEventKind::SwitchHit:
   case DungeonSwitchEventKind::FlagChanged:case DungeonSwitchEventKind::FlagsUpdated:case DungeonSwitchEventKind::ObjectStateChanged:break;
   default:CHECK(false);
   }
  }
  model.clear_events();
 }
 void begin_row(){player_events=ui_events=audio_events=1;camera_values={};}
 void prepare(bool saved){std::vector<DungeonSwitchFlag>flags;for(uint32_t i=0;i<data.switch_count();++i){auto n=data.switch_node(i);DungeonSwitchState state;if(saved)CHECK(model.switch_state(model.handle(n.id),state,error));flags.push_back({n.id,saved&&state.on});}std::reverse(flags.begin(),flags.end());CHECK(model.prepare(data,data.scene_id(),data.text(data.policy().region),flags,error));audio.assign(data.switch_count(),false);ready.clear();wall.clear();camera={};consume();}
 void timers(bool after_wall){auto pending=after_wall?wall:ready;if(after_wall)CHECK(pending.size()==1);else CHECK(pending.size()==data.switch_count());(after_wall?wall:ready).clear();for(auto token:pending)CHECK(model.timer_finished(token,error));consume();}
 void animations(){for(uint32_t i=0;i<data.object_count();++i)CHECK(model.object_animation_finished(model.handle(data.object(i).id),error));consume();}
 void operate(uint32_t target,DungeonSwitchMethod method=DungeonSwitchMethod::Interact,bool contact=true,bool allowed=true){CHECK(model.operate(model.handle(target),method,contact,paused,allowed,error));consume();}
 void camera_done(bool return_pending){auto token=camera;camera={};CHECK(model.camera_finished(token,error));if(return_pending){CHECK(model.idle_frame(error));CHECK(model.idle_frame(error));}consume();}
 Projection capture(){Projection out{};size_t at=0;for(uint32_t i=0;i<data.switch_count();++i){DungeonSwitchState v;CHECK(model.switch_state(model.handle(data.switch_node(i).id),v,error));for(double n:{double(v.on),double(v.disabled),double(v.ready),double(v.animation),double(audio[i])})out[at++]=n;}for(uint32_t i=0;i<data.object_count();++i){DungeonControlledState v;CHECK(model.object_state(model.handle(data.object(i).id),v,error));for(double n:{double(v.on),double(v.animation),double(v.order),double(v.registrations)})out[at++]=n;}for(double n:{double(paused),double(cutscene),double(player_events),double(ui_events),double(audio_events)})out[at++]=n;for(auto n:camera_values)out[at++]=n;CHECK(at==out.size());return out;}
 std::vector<Projection>run(const std::vector<Row>&rows){std::vector<Projection>result;auto main=data.policy().main_switch,first=id("Bottom2/switch1"),second=id("Bottom2/switch2"),plate=id("Bottom2/Plate1"),other_plate=id("Bottom2/Plate4");CHECK(main&&first&&second&&plate&&other_plate);
  for(auto row:rows){begin_row();switch(row.index){
  case 0:prepare(false);break;case 1:operate(first);break;case 2:animations();timers(false);break;
  case 3:paused=true;operate(first);break;case 4:paused=false;operate(first,DungeonSwitchMethod::Interact,true,false);break;
  case 5:operate(first);break;case 6:animations();break;case 7:operate(first,DungeonSwitchMethod::Hit,false);animations();break;
  case 8:operate(plate,DungeonSwitchMethod::PlayerEntered,false);break;case 9:case 10:operate(plate,DungeonSwitchMethod::PlayerEntered);break;case 11:animations();break;
  case 12:operate(second);break;case 13:animations();break;case 14:camera_done(true);break;case 15:camera_done(false);break;
  case 16:operate(other_plate,DungeonSwitchMethod::PlayerEntered);animations();break;case 17:operate(other_plate,DungeonSwitchMethod::PlayerEntered);break;
  case 18:operate(main);break;case 19:animations();break;case 20:timers(true);break;case 21:camera_done(true);break;case 22:camera_done(false);break;case 23:operate(main);break;
  case 24:operate(first);animations();break;case 25:CHECK(model.leave_area(false,error));consume();break;case 26:prepare(true);break;case 27:animations();timers(false);break;default:CHECK(false);}
  auto actual=capture();if(actual!=row.value){std::fprintf(stderr,"Original switch row %u %s\n",row.index,row.action.c_str());for(size_t i=0;i<actual.size();++i)if(actual[i]!=row.value[i])std::fprintf(stderr," column%zu actual%.9g expected%.9g\n",i,actual[i],row.value[i]);}CHECK(actual==row.value);result.push_back(actual);}
  return result;
 }
};
}
int main(int argc,char**argv){
 CHECK(argc==3);DungeonSwitchesData data;CHECK(data.load_file(argv[1],error));std::ifstream input(argv[2]);CHECK(bool(input));std::string line;CHECK(bool(std::getline(input,line)));std::vector<Row>rows;
 while(std::getline(input,line)){if(line.empty())continue;std::istringstream stream(line);Row r;CHECK(bool(stream>>r.index>>r.action));for(auto&v:r.value)CHECK(bool(stream>>v));std::string extra;CHECK(!(stream>>extra));rows.push_back(r);}CHECK(rows.size()==28);
 Harness a(data.view()),b(data.view());CHECK(a.run(rows)==b.run(rows));
 Harness h(data.view());h.prepare(false);const auto old=h.model.handle(h.id("Bottom2/switch1"));const auto old_timer=h.ready.front();std::vector<DungeonSwitchFlag>flags;for(uint32_t i=0;i<data.view().switch_count();++i)flags.push_back({data.view().switch_node(i).id,false});auto previous=h.capture();auto generation=h.model.generation();
 for(unsigned mode=0;mode<5;++mode){auto bad=flags;if(mode==0)bad.pop_back();if(mode==1)bad[0].id=bad[1].id;if(mode==2)bad[0].id=0;CHECK(!h.model.prepare(mode==3?DungeonSwitchesView{}:data.view(),mode==4?3:4,"Catacombs",bad,error));CHECK(h.capture()==previous&&h.model.generation()==generation);}
 CHECK(!h.model.prepare(data.view(),4,"wrong",flags,error));CHECK(h.capture()==previous);
 CHECK(!h.model.object_animation_finished(old,error));CHECK(!h.model.operate(h.model.handle(data.view().object(0).id),DungeonSwitchMethod::Hit,true,false,true,error));CHECK(!h.model.operate(old,DungeonSwitchMethod(99),true,false,true,error));
 CHECK(!h.model.camera_finished(old_timer,error));CHECK(h.capture()==previous);h.prepare(false);CHECK(!h.model.timer_finished(old_timer,error));CHECK(!h.model.operate(old,DungeonSwitchMethod::Interact,true,false,true,error));
 h.begin_row();h.animations();h.timers(false);h.begin_row();auto first=h.id("Bottom2/switch1");
 // Source restarts the same animation player without discarding older
 // update_state continuations. One finish resumes both captured states.
 CHECK(h.model.operate(h.model.handle(first),DungeonSwitchMethod::Interact,true,false,true,error));CHECK(h.model.operate(h.model.handle(first),DungeonSwitchMethod::Hit,false,false,true,error));h.consume();h.begin_row();
 unsigned changed=0;for(uint32_t i=0;i<data.view().object_count();++i){CHECK(h.model.object_animation_finished(h.model.handle(data.view().object(i).id),error));std::vector<bool>states;for(auto event:h.model.events())if(event.kind==DungeonSwitchEventKind::ObjectStateChanged){++changed;CHECK(!event.silent);states.push_back(event.value);}if(!states.empty()){CHECK(states.size()==2&&states[0]!=states[1]);DungeonControlledState current;CHECK(h.model.object_state(h.model.handle(data.view().object(i).id),current,error));CHECK(states.back()==current.on);}h.model.clear_events();}CHECK(changed==8);
 h.prepare(false);h.begin_row();h.animations();h.timers(false);h.begin_row();
 for(unsigned i=0;i<128;++i)CHECK(h.model.operate(h.model.handle(first),DungeonSwitchMethod::Interact,true,false,true,error));
 const auto count=h.model.events().size();auto bounded=h.capture();CHECK(!h.model.operate(h.model.handle(first),DungeonSwitchMethod::Interact,true,false,true,error));CHECK(h.model.events().size()==count&&h.capture()==bounded);
 h.prepare(false);h.begin_row();h.animations();h.timers(false);h.begin_row();
 for(unsigned i=0;i<2047;++i)CHECK(h.model.leave_area(false,error));const auto pending=h.model.events().size();auto unchanged=h.capture();CHECK(!h.model.operate(h.model.handle(first),DungeonSwitchMethod::Interact,true,false,true,error));CHECK(h.model.events().size()==pending&&h.capture()==unchanged);h.model.clear_events();for(uint32_t i=0;i<data.view().object_count();++i)CHECK(h.model.object_animation_finished(h.model.handle(data.view().object(i).id),error));CHECK(h.model.events().empty());
 DungeonSwitchRuntime empty;CHECK(!empty.idle_frame(error)&&!empty.leave_area(false,error)&&!empty.timer_finished({},error));
 // A format-valid artificial nonzero update order has no admitted multi-tier
 // choreography in this method component. Reject before replacing its owner.
 {std::vector<uint8_t>bytes;CHECK(encore::read_file(argv[1],bytes,65536,error));auto u=[](const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;};auto put=[&](size_t at,uint32_t v){for(unsigned i=0;i<4;++i)bytes[at+i]=uint8_t(v>>(i*8));};auto offset=u(bytes.data()+116);put(offset+16,1);put(16,encore::crc32(bytes.data()+32,bytes.size()-32));DungeonSwitchesData tiers;CHECK(tiers.load(bytes.data(),bytes.size(),error));auto old_generation=h.model.generation();CHECK(!h.model.prepare(tiers.view(),4,"Catacombs",flags,error));CHECK(h.model.generation()==old_generation);}
 std::printf("Dungeon switch state: %u checks;28 frozen original projections twice, asynchronous camera/wall continuations, source reset, restart waiters, stale ownership and atomic bounds; detached component only\n",checks);
}
