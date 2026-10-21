#include "encore/field_steps.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Shadow clock %d: %s (%s)\n",__LINE__,#x,error.c_str());std::exit(1);}}while(0)
uint32_t word(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t n){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(n>>(i*8));}
void sign(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
}
int main(int argc,char**argv){
 CHECK(argc==2);const std::string root=argv[1];FieldStepsData data;CHECK(data.load_file((root+"/catacombs.encsteps").c_str(),error));const auto content=data.view();std::ifstream profiles(root+"/profiles.tsv");CHECK(profiles.good());std::string profile;unsigned total=0,profile_count=0;
 while(std::getline(profiles,profile)){
  if(profile.empty()||profile[0]=='#')continue;std::istringstream config(profile);std::string file;unsigned fps,no_draw;CHECK(bool(config>>file>>fps>>no_draw)&&fps>0&&no_draw<=1);std::ifstream native(root+"/"+file);CHECK(native.good());FieldStepShadowState state;CHECK(state.initialize(content,error));bool visible=true,player_pause=false,tree_pause=false;Vec2 position{128.25f,-40.5f};unsigned ticks=0;std::string line;
  while(std::getline(native,line)){
   std::istringstream row(line);unsigned tick,count;CHECK(bool(row>>tick>>count)&&tick==ticks&&count<=8);
   for(unsigned i=0;i<count;++i){unsigned op;double a,b;CHECK(bool(row>>op>>a>>b));switch(op){case 0:CHECK(a>=0&&a<content.shadow_count()&&a==std::floor(a));CHECK(state.set_animation(content.text(content.shadow(uint32_t(a)).name),error));break;case 1:CHECK(a>=0&&a<=UINT32_MAX&&a==std::floor(a));CHECK(state.set_frame(uint32_t(a),error));break;case 2:CHECK(a==0||a==1);visible=a!=0;break;case 3:CHECK(a==0||a==1);player_pause=a!=0;break;case 4:CHECK(a==0||a==1);tree_pause=a!=0;break;case 5:position={float(a),float(b)};break;default:CHECK(false);}}
   uint32_t bits,process,mode,frame,playing,behind,native_visible,parent_paused,tree_paused;Vec2 observed;CHECK(bool(row>>bits>>process>>mode>>frame>>playing>>behind>>native_visible>>parent_paused>>tree_paused>>observed.x>>observed.y));std::string extra;CHECK(!(row>>extra));float delta;std::memcpy(&delta,&bits,4);CHECK(std::isfinite(delta)&&delta>0&&delta<=1);CHECK(process==uint32_t(!tree_pause));CHECK(native_visible==uint32_t(visible)&&parent_paused==uint32_t(player_pause)&&tree_paused==uint32_t(tree_pause)&&observed.x==position.x&&observed.y==position.y);
   CHECK(state.advance_idle(delta,process,error));CHECK(state.animation()==mode&&state.frame()==frame&&state.playing()==bool(playing)&&state.behind()==bool(behind));CHECK(std::isfinite(state.timeout())&&state.timeout()>=0);++ticks;++total;
  }
  CHECK(ticks==128);++profile_count;
 }
 CHECK(profile_count>0);FieldStepShadowState state;CHECK(state.initialize(content,error));const auto frame=state.frame();const auto mode=state.animation();const auto timeout=state.timeout();
 for(double delta:{-1.0,1.001,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}){CHECK(!state.advance_idle(delta,true,error));CHECK(state.frame()==frame&&state.animation()==mode&&state.timeout()==timeout);}
 CHECK(state.advance_idle(0,true,error)&&state.timeout()==timeout);CHECK(state.advance_idle(1,false,error)&&state.timeout()==timeout);FieldStepShadowState empty;CHECK(!empty.advance_idle(0,true,error));
 std::vector<uint8_t>bytes;CHECK(encore::read_file((root+"/catacombs.encsteps").c_str(),bytes,1048576,error));const auto modes=word(bytes,164),policy=word(bytes,84);
 {auto bad=bytes;put(bad,modes+8,1);sign(bad);FieldStepsData metadata;CHECK(metadata.load(bad.data(),bad.size(),error));FieldStepShadowState rejected;CHECK(!rejected.initialize(metadata.view(),error));CHECK(!rejected.prepared());}
 {auto alternate=bytes;put(alternate,policy+20,0);sign(alternate);FieldStepsData metadata;CHECK(metadata.load(alternate.data(),alternate.size(),error));FieldStepShadowState stopped;CHECK(stopped.initialize(metadata.view(),error));CHECK(!stopped.playing()&&stopped.timeout()==0);CHECK(stopped.advance_idle(1,true,error)&&stopped.frame()==0&&stopped.timeout()==0);CHECK(stopped.set_animation(metadata.view().text(metadata.view().policy().shadow),error));CHECK(stopped.playing()&&stopped.timeout()>0);}
 std::printf("Shadow native clock: %u checks;%u fixed-rate native observations in%u profiles;binary32 phase,repeat mode/frame,hide/player pause/tree pause/render admission;no complete scene/GPU pass\n",checks,total,profile_count);
}
