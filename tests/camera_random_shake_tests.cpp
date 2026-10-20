#include "encore/cutscene_camera.hpp"
#include "encore/source_random.hpp"
#include "room_fixture.hpp"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
using namespace encore::upstream;
namespace {
unsigned checks=0,failures=0;
void check(bool v,const std::string& label){++checks;if(!v){if(failures<35)std::fprintf(stderr,"%s\n",label.c_str());++failures;}}
bool close(Vec2 a,Vec2 b){return std::abs(a.x-b.x)<.00025f&&std::abs(a.y-b.y)<.00025f;}
void replay(const char*file){
 std::ifstream in(file);check(bool(in),"reference file opens");std::string line,name;CutsceneCamera camera;SourceRandom random(0);unsigned rows=0,cases=0,tails=0;
 while(std::getline(in,line)){
  if(line.empty()||line[0]=='#')continue;std::istringstream s(line);std::string op;s>>op;
  if(op=="case"){
   uint64_t seed=0;s>>name>>seed;random.seed(seed);check(camera.initialize(encore_test::room(),{432,397}),name+" initialize");++cases;
  }else if(op=="shake"){
   double mag=0,duration=0,interval=0,weight=0;Vec2 direction{};int diminish=0;s>>mag>>duration>>direction.x>>direction.y>>interval>>weight>>diminish;
   check(camera.shake(mag,duration,direction,interval,weight,diminish!=0),name+" source shake admitted");
  }else if(op=="frame"){
   double delta=0;Vec2 before{};uint64_t count=0,state=0;s>>delta>>before.x>>before.y>>count>>state;
   check(camera.physics_frame(random,delta),name+" physics accepted");
   check(close(camera.display_offset(),before),name+" parent samples previous child property");
   check(random.raw_draw_count()==count,name+" raw draw count");check(random.state()==state,name+" exact RNG state");++rows;
  }else if(op=="tail"){
   for(unsigned i=0;i<3;++i){uint32_t expected=0;s>>expected;check(random.randi()==expected,name+" subsequent shared stream");++tails;}
  }else check(false,"unknown reference operation");
  check(!s.fail(),"complete reference record");std::string extra;check(!(s>>extra),"no trailing reference fields");
 }
 check(cases==10&&rows==845&&tails==30,"complete original observation coverage");
}
}
int main(int argc,char**argv){
 if(argc!=2)return 2;replay(argv[1]);replay(argv[1]);
 const auto room=encore_test::room();CutsceneCamera camera;SourceRandom random(123);const auto nan=std::numeric_limits<double>::quiet_NaN();
 check(!camera.physics_frame(random),"uninitialized random physics rejected");check(random.raw_draw_count()==0,"uninitialized rejection preserves RNG");
 check(camera.initialize(room,{432,397}),"negative setup");
 check(!camera.shake(5,.5,{}),"legacy random request rejects without owner");
 struct Bad{double magnitude,duration,interval,weight;Vec2 direction;};
 const Bad bad[]={{5,.5,0,1,{}},{5,.5,-1,1,{}},{5,.5,nan,1,{}},{5,.5,.02,nan,{}},{5,.5,.02,-1,{}},{5,.5,.02,2,{}},{5,.001,.02,1,{}},{5,61,.02,1,{}},{5,nan,.02,1,{}},{-1,.5,.02,1,{}},{nan,.5,.02,1,{}},{1000001,.5,.02,1,{}},{5,1,.00000001,1,{}},{5,.5,.02,1,{1000001,0}},{5,.5,.02,1,{0,float(nan)}}};
 for(const auto&v:bad){check(!camera.shake(v.magnitude,v.duration,v.direction,v.interval,v.weight,false),"invalid explicit parameters rejected");check(!camera.is_shaking(),"invalid request has no partial shake");}
 check(camera.shake(5,.5,{},.02,1,false),"explicit stochastic request accepted");const auto before=camera.center(),offset=camera.display_offset();
 check(!camera.physics_frame(),"random update without stream rejects");check(!camera.physics_frame({900,900}),"target update without stream rejects");
 check(close(before,camera.center())&&close(offset,camera.display_offset()),"missing stream preserves camera");
 const auto state=random.state();check(!camera.physics_frame(random,-1)&&!camera.physics_frame(random,nan)&&!camera.physics_frame(random,2),"invalid random delta rejects");
 check(random.state()==state&&random.raw_draw_count()==0,"invalid random delta preserves RNG");
 CutsceneCamera clean;SourceRandom expected(123);check(clean.initialize(room,{432,397})&&clean.shake(5,.5,{},.02,1,false),"atomic comparison setup");
 for(unsigned i=0;i<50;++i){check(camera.physics_frame(random)&&clean.physics_frame(expected),"continued random physics");check(close(camera.center(),clean.center())&&random.state()==expected.state(),"failed calls leave future behavior identical");}
 CutsceneCamera axis;check(axis.initialize(room,{432,397})&&axis.shake(4,.2,{1,0}),"legacy axis accepted");const auto old=random.state();
 for(unsigned i=0;i<30;++i)check(axis.physics_frame(random),"fixed axis with explicit stream");check(random.state()==old,"fixed axis consumes no random draws");
 CutsceneCamera capacity;check(capacity.initialize(room,{432,397}),"capacity setup");for(unsigned i=0;i<16;++i)check(capacity.shake(1,.2,{},.02,.5,true),"bounded concurrent admission");
 check(!capacity.shake(1,.2,{},.02,.5,true),"seventeenth concurrent shaker rejected");
 CutsceneCamera dormant;SourceRandom dormant_random(7);check(dormant.initialize(room,{432,397})&&dormant.shake(5,.5,{},.02,1,false)&&dormant.change_to_actor(42,{440,397}),"dormant random camera setup");
 check(!dormant.physics_frame(),"noncurrent random shaker also needs owner");for(unsigned i=0;i<20;++i)check(dormant.physics_frame(dormant_random),"noncurrent random camera advances");
 check(dormant_random.raw_draw_count()>0,"dormant camera uses shared stream");
 std::printf("Camera random shake: %u checks, %u failures;845 original rows replayed twice; subsequent RNG exact\n",checks,failures);return failures?1:0;
}
