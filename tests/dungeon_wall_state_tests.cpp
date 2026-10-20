#include "encore/dungeon_wall_state.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
using namespace encore::upstream;
namespace {unsigned checks=0;std::string error;
void check(bool v,const char*s){++checks;if(!v){std::fprintf(stderr,"Wall state %u: %s (%s)\n",checks,s,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
std::string events(const std::vector<DungeonWallEvent>&rows){std::ostringstream out;for(const auto&row:rows){CHECK(row.owner==3761819854u&&row.generation==42);if(out.tellp()>0)out<<',';out<<uint32_t(row.kind)<<':';if(row.kind==DungeonWallEventKind::AudioPlaying)out<<int(row.playing);else if(row.kind==DungeonWallEventKind::CameraShake)out<<row.duration;else if(row.kind==DungeonWallEventKind::Finished)out<<row.clip;else CHECK(false);}return out.str().empty()?"-":out.str();}
}
int main(int argc,char**argv){
 CHECK(argc==3);DungeonWallTimelineData data;CHECK(data.load_file(argv[1],error));std::vector<std::string>first;
 for(unsigned pass=0;pass<2;++pass){
  std::ifstream input(argv[2]);CHECK(bool(input));std::string line,name;unsigned rows=0,cases=0;DungeonWallState state;std::vector<std::string>projection;
  while(std::getline(input,line)){
   if(line.empty()||line[0]=='#')continue;std::istringstream s(line);std::string op;s>>op;
   if(op=="case"){s>>name;state=DungeonWallState{};CHECK(state.initialize(data,42,error));++cases;continue;}
   double value=0,position=0;uint32_t frame=0,body=0,audio=0,playing=0,assigned=0;std::string expected,extra;CHECK(bool(s>>value>>frame>>body>>audio>>playing>>assigned>>position>>expected)&&!(s>>extra));
   if(op=="play")CHECK(value==uint32_t(value)&&state.play(uint32_t(value),error));else if(op=="advance")CHECK(state.advance_manual(value,error));else CHECK(op=="flush"&&state.flush_deferred(error));
   const auto actual=events(state.take_events());const auto pose=state.pose();
   const bool match=pose.frame==frame&&pose.body==bool(body)&&pose.visible&&!pose.hitbox&&!pose.interaction&&state.audio_request()==bool(audio)&&state.playing()==bool(playing)&&state.assigned_clip()==assigned&&std::abs(state.position()-position)<.0000003&&actual==expected;
   if(!match)std::fprintf(stderr,"Original %s row%u %s %.17g actual frame%u body%u audio%u playing%u id%u pos%.17g events%s; expected frame%u body%u audio%u playing%u id%u pos%.17g events%s\n",name.c_str(),rows,op.c_str(),value,pose.frame,pose.body,state.audio_request(),state.playing(),state.assigned_clip(),state.position(),actual.c_str(),frame,body,audio,playing,assigned,position,expected.c_str());
   CHECK(match);std::ostringstream result;result<<std::setprecision(17)<<pose.frame<<' '<<pose.body<<' '<<state.audio_request()<<' '<<state.playing()<<' '<<state.assigned_clip()<<' '<<state.position()<<' '<<actual;projection.push_back(result.str());++rows;
  }
  CHECK(cases==6&&rows==309);if(!pass)first=projection;else CHECK(first==projection);
 }
 DungeonWallState state;const double nan=std::numeric_limits<double>::quiet_NaN();
 CHECK(!state.play(4,error)&&!state.process(0,error)&&!state.advance_manual(0,error)&&!state.flush_deferred(error));
 CHECK(!state.initialize(DungeonWallTimelineData{},42,error)&&!state.initialize(data,0,error));CHECK(state.initialize(data,42,error));
 CHECK(!state.play(0,error)&&!state.play_role(DungeonWallRole(4),error));CHECK(state.play_role(DungeonWallRole::On,error));CHECK(state.advance_manual(.6,error));CHECK(state.pending_count()==1);state.take_events();
 const auto before=state.position();const auto pose=state.pose();const auto pending=state.pending_count();
 for(double bad:{-1.,61.,nan,double(INFINITY)}){CHECK(!state.advance_manual(bad,error)&&!state.process(bad,error));CHECK(state.position()==before&&state.pose().frame==pose.frame&&state.pending_count()==pending&&state.take_events().empty());}
 CHECK(!state.initialize(data,100,error)&&state.position()==before&&state.pending_count()==1);CHECK(!state.play(65535,error)&&state.position()==before&&state.pending_count()==1);
 CHECK(state.play_role(DungeonWallRole::Reset,error)&&state.advance_manual(0,error));state.take_events();CHECK(state.flush_deferred(error)&&state.take_events().size()==1);
 DungeonWallState capacity;CHECK(capacity.initialize(data,42,error));
 for(unsigned i=0;i<32;++i){CHECK(capacity.play_role(DungeonWallRole::On,error)&&capacity.advance_manual(3,error));CHECK(capacity.take_events().size()==2);}
 CHECK(capacity.pending_count()==64&&capacity.play_role(DungeonWallRole::On,error));const auto retained=capacity.pose();CHECK(!capacity.advance_manual(3,error));CHECK(capacity.position()==0&&capacity.pose().frame==retained.frame&&capacity.pending_count()==64&&capacity.take_events().empty());
 CHECK(capacity.flush_deferred(error)&&capacity.take_events().size()==64&&capacity.advance_manual(3,error));capacity.take_events();CHECK(capacity.pending_count()==2);CHECK(capacity.play_role(DungeonWallRole::Reset,error));
 for(unsigned i=0;i<127;++i)CHECK(capacity.advance_manual(0,error));CHECK(!capacity.flush_deferred(error)&&capacity.pending_count()==2);CHECK(capacity.take_events().size()==127&&capacity.flush_deferred(error)&&capacity.take_events().size()==2);
 for(unsigned i=0;i<128;++i)CHECK(capacity.advance_manual(0,error));CHECK(!capacity.advance_manual(0,error)&&capacity.position()==0&&capacity.pending_count()==0);CHECK(capacity.take_events().size()==128&&capacity.advance_manual(0,error));
 DungeonWallState automatic;CHECK(automatic.initialize(data,42,error)&&automatic.play_role(DungeonWallRole::On,error)&&automatic.process(3,error));automatic.take_events();CHECK(automatic.flush_deferred(error));automatic.take_events();CHECK(!automatic.playing()&&automatic.process(.1,error)&&automatic.take_events().empty());CHECK(automatic.advance_manual(.1,error)&&automatic.take_events().size()==1);
 std::printf("Wall typed state: %u checks;309 original observations twice,immediate audio/deferred camera/finish order,atomic bounded queues; no live audio,camera,flags or scene\n",checks);
}
