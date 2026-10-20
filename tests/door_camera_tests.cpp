#include "encore/cutscene_camera.hpp"
#include "room_fixture.hpp"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
using namespace encore::upstream;
int main(int argc,char**argv){
 if(argc!=2)return 2;
 unsigned checks=0,failures=0,rows=0,cases=0;
 std::ifstream input(argv[1]);if(!input)return 2;
 std::string line,name;
 auto check=[&](bool v,const char*s){++checks;if(!v){if(failures<15)std::fprintf(stderr,"Scaled door camera %s row%u:%s\n",name.c_str(),rows,s);++failures;}};
 CutsceneCamera camera,explicit_camera;
 while(std::getline(input,line)){
  if(line.empty()||line[0]=='#')continue;
  std::istringstream row(line);std::string op;row>>op;
  if(op=="case"){
   double magnitude,duration,interval,weight;Vec2 direction;unsigned diminish;row>>name>>magnitude>>duration>>direction.x>>direction.y>>interval>>weight>>diminish;
   if(row.fail()||diminish>1)return 2;
   check(camera.initialize(encore_test::room(),{432,397})&&explicit_camera.initialize(encore_test::room(),{432,397}),"init");
   check(camera.shake(magnitude,duration,direction),"actual admitted legacy nonunit request");check(explicit_camera.shake(magnitude,duration,direction,interval,weight,diminish!=0),"explicit parameters");++cases;
  }else if(op=="frame"){
   double delta;Vec2 expected;row>>delta>>expected.x>>expected.y;
   if(row.fail()||cases==0)return 2;
   check(camera.physics_frame(delta)&&explicit_camera.physics_frame(delta),"physics without RNG owner");
   const auto a=camera.display_offset(),b=explicit_camera.display_offset();check(std::abs(a.x-expected.x)<.00025f&&std::abs(a.y-expected.y)<.00025f,"original before-child offset");check(a.x==b.x&&a.y==b.y,"legacy and explicit source policy agree");++rows;
  }else return 2;
  if(row.fail())return 2;
  std::string extra;if(row>>extra)return 2;
 }
 check(cases==3&&rows==180,"complete source coverage");std::printf("Scaled door camera:%u checks,%u failures;180 original Shaker observations,real door direction(2,0);not whole door/hardware\n",checks,failures);return failures?1:0;
}
