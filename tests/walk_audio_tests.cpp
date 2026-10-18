#include "room_fixture.hpp"
#include "fixtures/movement_v0410.hpp"
#include "encore/field_steps.hpp"
#include <fstream>
#include <sstream>
#include <limits>
using namespace encore::upstream;
namespace {unsigned checks=0;void check(bool v,const char*s){++checks;if(!v){std::fprintf(stderr,"Walk audio %u: %s\n",checks,s);std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
class Blocked final:public MotionSolver {public:bool slide(Vec2 p,Vec2,SlideResult&r)const override{r={p,{}};return true;}};
class Invalid final:public MotionSolver {public:bool slide(Vec2,Vec2,SlideResult&)const override{return false;}};
}
int main(int argc,char**argv){
 CHECK(argc==3);std::ifstream f(argv[1]);CHECK(bool(f));std::string line,error;std::getline(f,line);CHECK(line.substr(0,10)=="# sequence");
 FreeMotionSolver solver;unsigned sequence_index=0,frames=0;FieldStepsData data;CHECK(data.load_file(argv[2],error));
 for(const auto&sequence:movement_reference::sequences){WalkState s;s.position={sequence.x,sequence.y};s.direction=encore_test::room().scene().start_direction;s.speed=encore_test::room().rule_f32(RoomRuleKey::WalkSpeed);FieldRunLoop loop;CHECK(loop.initialize(data.view(),data.view().text(data.view().sound(data.view().policy().default_sound).name),error));
  for(unsigned i=0;i<sequence.count;++i){CHECK(bool(std::getline(f,line)));std::istringstream row(line);unsigned seq,frame;int x,y,toggle,paused,door;std::string operations;CHECK(bool(row>>seq>>frame>>x>>y>>toggle>>paused>>door>>operations));CHECK(seq==sequence_index&&frame==i);const auto&sample=sequence.frames[i];CHECK(x==sample.input.x&&y==sample.input.y&&toggle==sample.input.toggle&&paused==sample.input.paused&&door==sample.input.entering_door);std::string extra;CHECK(!(row>>extra));
   WalkAudioCalls calls;CHECK(advance_walk(s,sample.input,solver,encore_test::room(),&calls));CHECK(s.position.x==sample.expected.position.x&&s.position.y==sample.expected.position.y&&s.running==sample.expected.running&&s.animation==sample.expected.animation);
   std::ostringstream actual;for(unsigned j=0;j<calls.count;++j){if(j)actual<<',';actual<<unsigned(calls.calls[j]);}CHECK((calls.count?actual.str():"-")==operations);CHECK(loop.apply_walk_calls(calls,error));CHECK(loop.running()==s.running);loop.clear_events();++frames;
  }++sequence_index;
 }CHECK(!std::getline(f,line));CHECK(frames==227&&sequence_index==14);
 WalkState s;s.direction={0,1};s.speed=encore_test::room().rule_f32(RoomRuleKey::RunSpeed);s.running=s.substantial=s.tap_run=true;WalkAudioCalls calls;Blocked blocked;
 CHECK(advance_walk(s,{1,0,true},blocked,encore_test::room(),&calls));CHECK(calls.count==3&&calls.calls[0]==WalkAudioCall::SetRunningFalse&&calls.calls[1]==WalkAudioCall::SetRunningTrue&&calls.calls[2]==WalkAudioCall::SetRunningFalse);CHECK(!s.running&&!s.tap_run);
 const auto before=s;WalkAudioCalls original;original.count=1;original.calls[0]=WalkAudioCall::SetRunningTrue;calls=original;Invalid invalid;CHECK(!advance_walk(s,{1,0},invalid,encore_test::room(),&calls));CHECK(calls.count==original.count&&calls.calls==original.calls&&s.position.x==before.position.x&&s.position.y==before.position.y);
 CHECK(!advance_walk(s,{2,0},solver,encore_test::room(),&calls));CHECK(calls.count==original.count&&calls.calls==original.calls);
 FieldRunLoop loop;CHECK(loop.initialize(data.view(),data.view().text(data.view().sound(0).name),error));CHECK(loop.set_running(true,false,true,false,error));const auto event_count=loop.events().size();
 for(unsigned op:{0u,4u,255u}){WalkAudioCalls bad;bad.count=2;bad.calls[0]=WalkAudioCall::SetRunningFalse;bad.calls[1]=WalkAudioCall(op);CHECK(!loop.apply_walk_calls(bad,error));CHECK(loop.running()&&loop.playing()&&loop.events().size()==event_count);}
 WalkAudioCalls bad;bad.count=4;CHECK(!loop.apply_walk_calls(bad,error));CHECK(loop.running()&&loop.playing()&&loop.events().size()==event_count);FieldRunLoop unavailable;CHECK(!unavailable.apply_walk_calls({},error));
 std::printf("Walk audio: %u checks, %u source frames, 14 sequences; source call order and composed loop, rejected movement and unknown instruction rollback; host only\n",checks,frames);
}
