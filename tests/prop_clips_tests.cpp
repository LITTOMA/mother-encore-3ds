#include "encore/prop_clips.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
void check(bool ok,const char*why){++checks;if(!ok){std::fprintf(stderr,"Prop clips %u: %s (%s)\n",checks,why,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
uint32_t get(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t value){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(value>>(8*i));}
void sign(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
bool same(const PropPose&a,const PropPose&b){return a.frame==b.frame&&a.visible==b.visible&&a.body==b.body&&a.hitbox==b.hitbox&&a.interaction==b.interaction;}
}
int main(int argc,char**argv){
 CHECK(argc==3);std::vector<uint8_t>bytes;CHECK(encore::read_file(argv[1],bytes,64*1024,error));PropClipsData data;
 CHECK(data.load(bytes.data(),bytes.size(),error));CHECK(data.valid()&&data.frame_count()==21&&data.clips().size()==5);CHECK(data.initial().frame==12&&data.initial().visible&&data.initial().body&&data.initial().hitbox&&data.initial().interaction);
 PropClipPlayer player;CHECK(player.initialize(data,error));CHECK(!player.playing()&&same(player.pose(),data.initial())&&player.advance(0,error));
 std::ifstream input(argv[2]);CHECK(bool(input));std::string line;CHECK(bool(std::getline(input,line)));unsigned rows=0;
 while(std::getline(input,line)){
  std::istringstream row(line);std::string op,completed,extra;double value;uint32_t frame,visible,body,hitbox,interaction,playing,current;
  CHECK(bool(row>>op>>value>>frame>>visible>>body>>hitbox>>interaction>>playing>>current>>completed)&&!(row>>extra));
  if(op=="play")CHECK(value==uint32_t(value)&&player.play(uint32_t(value),error));else{CHECK(op=="advance");CHECK(player.advance(value,error));}
  const auto pose=player.pose();std::ostringstream emitted;for(auto id:player.take_completed()){if(emitted.tellp()>0)emitted<<',';emitted<<id;}const std::string actual=emitted.str().empty()?"-":emitted.str();
  const bool matches=pose.frame==frame&&pose.visible==bool(visible)&&pose.body==bool(body)&&pose.hitbox==bool(hitbox)&&pose.interaction==bool(interaction)&&player.playing()==bool(playing)&&(player.playing()?player.assigned_clip():0)==current&&actual==completed;
  if(!matches)std::fprintf(stderr,"Native row%u %s; actual frame=%u flags=%u%u%u%u playing=%u id=%u time=%.17g completed=%s\n",rows,line.c_str(),pose.frame,pose.visible,pose.body,pose.hitbox,pose.interaction,player.playing(),player.assigned_clip(),player.position(),actual.c_str());
  CHECK(matches);++rows;
 }
 CHECK(rows>50);
 const auto policy=get(bytes,68),clips=get(bytes,84),keys=get(bytes,100);
 auto reject=[&](std::vector<uint8_t>b){sign(b);CHECK(!data.load(b.data(),b.size(),error));CHECK(data.valid()&&data.clips().size()==5&&data.initial().frame==12);};
 for(auto change:std::vector<std::pair<size_t,uint32_t>>{{8,2},{20,2},{24,1},{28,1},{32,0},{52,4},{56,1},{60,1},{64,2},{68,111},{72,0},{76,12},{84,UINT32_MAX},{92,20},{policy,0},{policy+4,21},{policy+8,16},{policy+12,1},{clips,0},{clips+24,1},{clips+8,1},{clips+12,0},{clips+16,1},{clips+20,1},{keys+4,6},{keys+8,21},{keys+16,1},{keys+20,22}}){auto b=bytes;put(b,change.first,change.second);reject(b);}
 for(float bad:{0.f,-1.f,61.f,INFINITY,NAN}){auto b=bytes;uint32_t bits;std::memcpy(&bits,&bad,4);put(b,clips+4,bits);reject(b);}
 for(float bad:{-1.f,61.f,INFINITY,NAN}){auto b=bytes;uint32_t bits;std::memcpy(&bits,&bad,4);put(b,keys,bits);reject(b);}
 {auto b=bytes;put(b,keys+16,get(b,keys+4));put(b,keys+20,get(b,keys+8));reject(b);}
 {auto b=bytes;b.push_back(0);put(b,12,uint32_t(b.size()));reject(b);}
 for(size_t n=0;n<bytes.size();++n){CHECK(!data.load(bytes.data(),n,error));CHECK(data.valid());}
 CHECK(!data.load(nullptr,bytes.size(),error));
 for(size_t i=0;i<bytes.size();++i){auto b=bytes;b[i]^=0x80;CHECK(!data.load(b.data(),b.size(),error));}
 PropClipPlayer independent;{PropClipsData transient;CHECK(transient.load(bytes.data(),bytes.size(),error));CHECK(independent.initialize(transient,error));}CHECK(independent.play(3,error)&&independent.advance(.2,error));
 const auto before=independent.pose();const auto time=independent.position();CHECK(!independent.play(0,error)&&same(before,independent.pose())&&independent.position()==time);
 PropClipsData invalid;CHECK(!independent.initialize(invalid,error)&&same(before,independent.pose()));
 for(double bad:{-1.,61.,double(INFINITY),double(NAN)})CHECK(!independent.advance(bad,error)&&same(before,independent.pose())&&independent.position()==time);
 CHECK(independent.take_completed().empty());CHECK(independent.play(4,error));CHECK(same(before,independent.pose()));CHECK(independent.advance(0,error)&&independent.pose().frame==13&&!independent.pose().body&&!independent.pose().hitbox&&!independent.pose().interaction);
 CHECK(independent.advance(1,error)&&independent.take_completed()==std::vector<uint32_t>{4});CHECK(independent.advance(1,error)&&independent.take_completed().empty());
 for(unsigned i=0;i<64;++i)CHECK(independent.play(2,error)&&independent.advance(1,error));CHECK(independent.play(2,error));const auto full=independent.pose();CHECK(!independent.advance(1,error)&&same(full,independent.pose())&&independent.position()==0);CHECK(independent.take_completed().size()==64);CHECK(independent.advance(1,error)&&independent.take_completed()==std::vector<uint32_t>{2});
 std::printf("Prop clips: %u checks; %u official-engine source animation observations, typed properties, interruption/restart, bounds and atomic malformed loading; not scene/gameplay/3DS verification\n",checks,rows);
}
