#include "encore/dungeon_mechanism_state.hpp"
#include "encore/dungeon_mechanism_presentation.hpp"
#include "encore/dungeon_mechanism_audio.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
using namespace encore::upstream;
namespace {unsigned checks=0;std::string error,label;
void check(bool v,const char*s){++checks;if(!v){std::fprintf(stderr,"Mechanism state %s check%u: %s (%s)\n",label.c_str(),checks,s,error.c_str());std::exit(1);}}
#define CHECK(v) check((v),#v)
std::string scalar(double v){std::ostringstream out;out<<std::setprecision(12)<<v;return out.str();}
std::string joined(const std::vector<std::string>&v){std::string s;for(const auto&r:v){if(!s.empty())s+=',';s+=r;}return s.empty()?"-":s;}
struct Projection {
 DungeonMechanismAudioRouter*router=nullptr;
 std::vector<std::string>player,ui,music,camera,timeline;
 std::deque<DungeonMechanismCallback>ready,wall,cameras;DungeonMechanismCallback used_focus;
 void collect(DungeonMechanismState&state){const auto events=state.take_events();std::vector<DungeonMechanismSoundCommand>sounds;size_t consumed=0;if(router)CHECK(router->plan(events,sounds,error));for(const auto&event:events){
  CHECK(event.epoch==state.epoch());
  if(event.kind==DungeonMechanismEventKind::AnimationFinished){timeline.push_back("finished:"+std::to_string(event.owner)+':'+std::to_string(event.clip));continue;}
  if(event.kind==DungeonMechanismEventKind::WallAudio){bool playing=event.playing;if(router){CHECK(consumed<sounds.size());const auto&s=sounds[consumed++];CHECK(s.owner==event.owner&&s.kind==DungeonMechanismSoundKind::Node&&s.playing==event.playing);playing=s.playing;}timeline.push_back("audio:"+std::to_string(playing));continue;}
  if(event.kind==DungeonMechanismEventKind::WallCamera){
   const DungeonMechanismBinding*b=nullptr;for(const auto&row:state.data().bindings())if(row.owner==event.owner)b=&row;CHECK(b);const auto*p=state.data().profile(b->profile);CHECK(p&&p->wall.valid());const auto&v=p->wall.camera();
   camera.push_back("shake:"+scalar(v.magnitude)+':'+scalar(event.duration)+':'+scalar(v.direction.x)+':'+scalar(v.direction.y)+':'+scalar(v.interval)+':'+scalar(v.weight)+':'+scalar(v.diminish));continue;
  }
  CHECK(event.kind==DungeonMechanismEventKind::Request);const auto&r=event.request;const DungeonMechanismCallback callback{event.epoch,event.callback_ticket};
  if(router&&(r.kind==DungeonSwitchEventKind::SwitchAudio||r.kind==DungeonSwitchEventKind::DeniedAudio)){CHECK(consumed<sounds.size());const auto&s=sounds[consumed++];CHECK(s.playing&&s.request_owner==event.owner&&s.owner==(r.kind==DungeonSwitchEventKind::SwitchAudio?event.owner:0));}
  switch(r.kind){
   case DungeonSwitchEventKind::ReadyTimer:CHECK(callback.ticket);ready.push_back(callback);break;
   case DungeonSwitchEventKind::WallTimer:CHECK(callback.ticket);wall.push_back(callback);break;
   case DungeonSwitchEventKind::CameraMove:CHECK(callback.ticket);cameras.push_back(callback);camera.push_back("move:"+scalar(r.x)+':'+scalar(r.y)+':'+scalar(r.duration));break;
   case DungeonSwitchEventKind::CameraReturn:CHECK(callback.ticket);cameras.push_back(callback);camera.push_back("return:"+scalar(r.duration));break;
   case DungeonSwitchEventKind::Pause:player.push_back("1");break;
   case DungeonSwitchEventKind::Unpause:player.push_back("0");break;
   case DungeonSwitchEventKind::Cutscene:ui.push_back(std::to_string(r.value));break;
   case DungeonSwitchEventKind::MusicFade:music.push_back("fade:"+scalar(r.duration));break;
   case DungeonSwitchEventKind::MusicResume:music.push_back("resume");break;
   // These explicit typed requests are covered by graph/source identity,
   // pose/state observations and the separate original-method reference.
   case DungeonSwitchEventKind::SwitchAnimation:case DungeonSwitchEventKind::ObjectAnimation:
   case DungeonSwitchEventKind::SwitchAudio:case DungeonSwitchEventKind::SwitchHit:
   case DungeonSwitchEventKind::FlagChanged:case DungeonSwitchEventKind::FlagsUpdated:
   case DungeonSwitchEventKind::ObjectStateChanged:CHECK(!event.callback_ticket);break;
   case DungeonSwitchEventKind::DeniedAudio:CHECK(false);break; // absent in this source scenario
   default:CHECK(false);
  }
 }if(router)CHECK(consumed==sounds.size());}
 void clear_projection(){player.clear();ui.clear();music.clear();camera.clear();timeline.clear();}
};
}
int main(int argc,char**argv){
 CHECK(argc==3||argc==4);DungeonMechanismBindingsData data;CHECK(data.load_file(argv[1],error));DungeonMechanismPresentationData presentation;
 if(argc==4){CHECK(presentation.load_file(argv[3],error));CHECK(presentation.bindings().graph().scene_id()==data.graph().scene_id()&&presentation.bindings().bindings().size()==data.bindings().size());data=presentation.bindings();}
 const auto graph=data.graph();std::vector<DungeonSwitchFlag>flags;for(uint32_t i=0;i<graph.switch_count();++i)flags.push_back({graph.switch_node(i).id,false});
 std::vector<std::string>first;uint32_t other_owner=0;
 for(unsigned pass=0;pass<2;++pass){
  DungeonMechanismState state;CHECK(state.prepare(data,42,flags,error));Projection actual;DungeonMechanismAudioRouter audio;if(presentation.media_bound()){CHECK(audio.prepare(presentation,42,error));actual.router=&audio;}std::ifstream input(argv[2]);CHECK(bool(input));std::string line;unsigned rows=0;
  std::vector<std::string>projection;
  while(std::getline(input,line)){
   if(line.empty()||line[0]=='#')continue;
   std::istringstream s(line);std::string prefix,op,p,u,m,c,t,extra;uint32_t argument=0,paused=0,cutscene=0;s>>prefix>>label>>op>>argument>>paused>>cutscene>>p>>u>>m>>c>>t;CHECK(!s.fail()&&prefix=="step"&&!(s>>extra));
   actual.clear_projection();
   if(label=="other_switch_on")other_owner=argument;
   if(op=="ready")CHECK(rows==0);
   else if(op=="frame"){CHECK(state.advance(1./60,error));actual.collect(state);CHECK(state.flush_deferred(error));}
   else if(op=="ready_timers"){
    CHECK(actual.ready.size()==5);auto ready=actual.ready;actual.ready.clear();for(const auto&callback:ready){CHECK(state.timer_finished(callback,error));actual.collect(state);}
   }else if(op=="interact")CHECK(state.operate(argument,DungeonSwitchMethod::Interact,true,false,true,error));
   else if(op=="camera"){
    CHECK(actual.cameras.size()==1);const auto callback=actual.cameras.front();actual.cameras.pop_front();CHECK(state.camera_finished(callback,error));if(label=="focus_complete")actual.used_focus=callback;
   }else if(op=="idle_wall"){
    CHECK(state.idle_frame(error));actual.collect(state);CHECK(actual.wall.size()==1&&actual.cameras.size()==1);
    // A repeated old focus callback cannot complete the distinct return even
    // though the lower graph operation token is the same across both phases.
    CHECK(!state.camera_finished(actual.used_focus,error));
    CHECK(!state.camera_finished({43,actual.cameras.front().ticket},error));
    CHECK(!state.timer_finished(actual.cameras.front(),error));
    const auto callback=actual.wall.front();actual.wall.pop_front();CHECK(state.timer_finished(callback,error));
   }else if(op=="leave")CHECK(state.leave_area(false,error));else CHECK(op=="noop");
   actual.collect(state);
   const auto ap=joined(actual.player),au=joined(actual.ui),am=joined(actual.music),ac=joined(actual.camera),at=joined(actual.timeline);
   if(ap!=p||au!=u||am!=m||ac!=c||at!=t)std::fprintf(stderr,"Service mismatch %s: actual P%s U%s M%s C%s T%s; expected P%s U%s M%s C%s T%s\n",label.c_str(),ap.c_str(),au.c_str(),am.c_str(),ac.c_str(),at.c_str(),p.c_str(),u.c_str(),m.c_str(),c.c_str(),t.c_str());
   CHECK(ap==p&&au==u&&am==m&&ac==c&&at==t);CHECK(state.paused()==bool(paused)&&state.cutscene()==bool(cutscene));
   projection.push_back(ap+' '+au+' '+am+' '+ac+' '+at+' '+std::to_string(state.paused())+' '+std::to_string(state.cutscene()));
   for(const auto&binding:data.bindings()){
    CHECK(bool(std::getline(input,line)));std::istringstream row(line);uint32_t owner=0,frame=0,body=0,clip=0,playing=0,on=0;double position=0;row>>prefix>>owner>>frame>>body>>clip>>playing>>position>>on;CHECK(!row.fail()&&prefix=="node"&&!(row>>extra)&&owner==binding.owner);
    DungeonMechanismPose pose;CHECK(state.inspect(owner,pose,error));const bool match=pose.owner==owner&&pose.pose.frame==frame&&pose.pose.body==bool(body)&&pose.clip==clip&&pose.playing==bool(playing)&&std::abs(pose.position-position)<.0000003&&pose.on==bool(on);
    if(!match)std::fprintf(stderr,"Pose mismatch %s owner%u: actual frame%u body%u clip%u playing%u pos%.17g on%u; expected frame%u body%u clip%u playing%u pos%.17g on%u\n",label.c_str(),owner,pose.pose.frame,pose.pose.body,pose.clip,pose.playing,pose.position,pose.on,frame,body,clip,playing,position,on);
    CHECK(match);projection.push_back(std::to_string(owner)+':'+std::to_string(pose.pose.frame)+':'+std::to_string(pose.pose.body)+':'+std::to_string(pose.clip)+':'+std::to_string(pose.playing)+':'+scalar(pose.position)+':'+std::to_string(pose.on));
    if(presentation.valid()){
     DungeonMechanismDraw draw;CHECK(presentation.draw(owner,pose.pose.frame,draw,error));const auto*sprite=presentation.sprite(binding.profile);CHECK(sprite&&draw.profile==binding.profile&&draw.owner==owner&&draw.x==frame%sprite->columns*sprite->cell_width&&draw.y==frame/sprite->columns*sprite->cell_height);
     for(size_t i=0;i<presentation.shapes().size();++i)if(presentation.shapes()[i].owner==owner){DungeonMechanismRectangle rectangle;CHECK(presentation.rectangle(i,rectangle,error));CHECK(rectangle.extents.x>0&&rectangle.extents.y>0);}
    }
   }
   ++rows;
  }
  CHECK(rows==232&&actual.ready.empty()&&actual.wall.empty()&&actual.cameras.empty());if(!pass)first=projection;else CHECK(first==projection);
  DungeonMechanismPose before,after;const auto owner=data.bindings()[0].owner;CHECK(state.inspect(owner,before,error));
  for(double invalid:{-1.,61.,std::numeric_limits<double>::quiet_NaN(),double(INFINITY)})CHECK(!state.advance(invalid,error));
  CHECK(!state.prepare(data,43,flags,error)&&!state.operate(0,DungeonSwitchMethod::Interact,true,false,true,error));
  CHECK(!state.timer_finished({42,0},error)&&!state.camera_finished({42,UINT64_MAX},error));
  CHECK(state.inspect(owner,after,error)&&before.pose.frame==after.pose.frame&&before.position==after.position&&before.on==after.on&&state.take_events().empty());
  if(audio.valid()){std::vector<DungeonMechanismSoundCommand>stops;CHECK(audio.retire(stops,error)&&stops.size()==6);for(const auto&s:stops)CHECK(s.kind==DungeonMechanismSoundKind::Node&&!s.playing&&s.owner);}
 }
 CHECK(other_owner);
 auto prepared=[&](){DungeonMechanismState state;CHECK(state.prepare(data,42,flags,error));const auto initial=state.take_events();for(const auto&e:initial)if(e.kind==DungeonMechanismEventKind::Request&&e.request.kind==DungeonSwitchEventKind::ReadyTimer)CHECK(state.timer_finished({e.epoch,e.callback_ticket},error));state.take_events();CHECK(state.advance(2,error)&&state.flush_deferred(error));state.take_events();return state;};
 auto snapshot=[&](const DungeonMechanismState&state){std::vector<DungeonMechanismPose>out;for(const auto&b:data.bindings()){DungeonMechanismPose p;CHECK(state.inspect(b.owner,p,error));out.push_back(p);}return out;};
 auto unchanged=[&](const DungeonMechanismState&state,const std::vector<DungeonMechanismPose>&before,size_t count,size_t callbacks){
  const auto after=snapshot(state);CHECK(state.event_count()==count&&state.pending_callbacks()==callbacks&&before.size()==after.size());
  for(size_t i=0;i<before.size();++i){const auto&a=before[i];const auto&b=after[i];CHECK(a.owner==b.owner&&a.clip==b.clip&&a.pose.frame==b.pose.frame&&a.pose.visible==b.pose.visible&&a.pose.body==b.pose.body&&a.pose.hitbox==b.pose.hitbox&&a.pose.interaction==b.pose.interaction&&a.position==b.position&&a.playing==b.playing&&a.on==b.on);}
 };
 {auto bounded=prepared();bool rejected=false;label="graph_waiter_capacity";
  for(unsigned i=0;i<300;++i){const auto before=snapshot(bounded);const auto count=bounded.event_count(),callbacks=bounded.pending_callbacks();
   if(!bounded.operate(other_owner,DungeonSwitchMethod::Interact,true,false,true,error)){CHECK(error=="Dungeon switch animation waiter capacity exceeded"||error=="Dungeon switch operation capacity exceeded");unchanged(bounded,before,count,callbacks);rejected=true;break;}
   bounded.take_events();
  }CHECK(rejected);
 }
 {auto bounded=prepared();bool rejected=false;label="event_queue_capacity";
  for(unsigned i=0;i<500;++i){auto before=snapshot(bounded);auto count=bounded.event_count(),callbacks=bounded.pending_callbacks();
   if(!bounded.operate(other_owner,DungeonSwitchMethod::Interact,true,false,true,error)){CHECK(error=="Mechanism event capacity exceeded");unchanged(bounded,before,count,callbacks);rejected=true;break;}
   before=snapshot(bounded);count=bounded.event_count();callbacks=bounded.pending_callbacks();
   if(!bounded.advance(1.1,error)){CHECK(error=="Mechanism event capacity exceeded");unchanged(bounded,before,count,callbacks);rejected=true;break;}
  }CHECK(rejected);
 }
 DungeonMechanismState empty;DungeonMechanismPose pose;CHECK(!empty.prepare(data,0,flags,error)&&!empty.prepare({},42,flags,error));CHECK(!empty.advance(0,error)&&!empty.flush_deferred(error)&&!empty.idle_frame(error)&&!empty.leave_area(false,error)&&!empty.inspect(1,pose,error));
 std::printf("Mechanism source integration: %u checks;232 original observations twice,14 source-ordered owners,source-clock completions,independent timer/camera continuations,epoch/ticket rejection,atomic capacity failures; no live Room/media/save\n",checks);
}
