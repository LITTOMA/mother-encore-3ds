#include "encore/field_swing.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
void check(bool ok,const char*s){++checks;if(!ok){std::fprintf(stderr,"Swing %u: %s (%s)\n",checks,s,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
uint32_t get(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(v>>(8*i));}
void seal(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
bool near(float a,double b,double eps=0.0001){return std::isfinite(a)&&std::abs(double(a)-b)<eps;}
bool same(SwingPose a,SwingPose b){return a.frame==b.frame&&a.pivot==b.pivot&&a.main==b.main&&a.special==b.special&&a.hitbox==b.hitbox&&a.behind==b.behind;}
}
int main(int argc,char**argv){
 CHECK(argc==3);std::vector<uint8_t>bytes;CHECK(encore::read_file(argv[1],bytes,64*1024,error));FieldSwingData data;CHECK(!data.valid()&&!data.clip(1)&&!data.direction({0,1}));CHECK(data.load(bytes.data(),bytes.size(),error));CHECK(data.valid()&&data.clips().size()==8);CHECK(data.strings()[3]=="bat"&&data.strings()[2]=="Audio/Sound effects/Ninten Bat.mp3");
 for(const auto&c:data.clips()){CHECK(data.direction(c.direction)&&data.direction(c.direction)->id==c.id);CHECK(data.clip(c.id)&&near(c.duration,0.333333));}
 for(Vec2 v:std::vector<Vec2>{{0,0},{2,0},{0,std::numeric_limits<float>::infinity()}})CHECK(!data.direction(v));
 const auto old=data.initial();auto reject=[&](std::vector<uint8_t>b){seal(b);CHECK(!data.load(b.data(),b.size(),error));CHECK(data.valid()&&same(data.initial(),old)&&data.clips().size()==8);};auto mutation=[&](size_t at,uint32_t value){auto b=bytes;put(b,at,value);reject(std::move(b));};
 for(size_t n=0;n<bytes.size();++n){CHECK(!data.load(bytes.data(),n,error));CHECK(data.valid()&&same(data.initial(),old));}
 CHECK(!data.load(nullptr,bytes.size(),error));
 for(auto row:std::vector<std::pair<size_t,uint32_t>>{{0,0},{8,0},{8,2},{12,0},{20,0},{20,2},{24,1},{28,1},{32,0},{52,3},{56,1},{60,1},{64,2},{68,0},{72,0},{76,1},{80,1},{84,0},{88,9},{92,1},{104,0},{108,1},{120,0},{124,2}})mutation(row.first,row.second);
 {auto b=bytes;b[16]^=1;CHECK(!data.load(b.data(),b.size(),error));}
 auto policy=get(bytes,68),clips=get(bytes,84),keys=get(bytes,100),strings=get(bytes,116);
 for(auto row:std::vector<std::pair<size_t,uint32_t>>{{0,0},{4,65535},{8,16},{12,1},{16,0},{20,0},{16,0x7f800000},{16+13*4,0},{16+14*4,0},{16+18*4,0x7fc00000},{92,1},{96,0xffffffff},{124,1}})mutation(policy+row.first,row.second);
 {auto b=bytes;b[strings]=0;reject(b);}
 for(auto row:std::vector<std::pair<size_t,uint32_t>>{{0,0},{24,get(bytes,clips)},{4,0x7fc00000},{8,0x40000000},{12,0},{16,1},{20,0xffffffff}})mutation(clips+row.first,row.second);
 for(auto row:std::vector<std::pair<size_t,uint32_t>>{{0,0x7fc00000},{4,0},{4,9},{8,0x7fc00000},{8,0x47800000},{12,8}})mutation(keys+row.first,row.second);
 for(uint32_t i=0;i<get(bytes,104);++i){auto at=keys+i*16,kind=get(bytes,at+4);if(kind==8){mutation(at,0);mutation(at+8,0x3f800000);break;}}
 FieldSwingPlayer player;CHECK(!player.advance(0,error)&&!player.play(1,false,error)&&!player.flush(error));CHECK(player.initialize(data,error));
 std::ifstream in(argv[2]);CHECK(in.good());std::string line;std::getline(in,line);uint32_t observations=0;
 while(std::getline(in,line)){if(line.empty())continue;std::istringstream row(line);std::string op;double value,pivot,position;uint32_t frame,bat;int main,special,hitbox,behind,playing,attack;CHECK(bool(row>>op>>value>>frame>>main>>special>>hitbox>>behind>>bat>>pivot>>playing>>attack>>position));
  if(op=="play")CHECK(player.play(uint32_t(value),false,error));else if(op=="advance")CHECK(player.advance(value,error));else {CHECK(op=="flush");CHECK(player.flush(error));}
  const auto&p=player.pose();if(!(p.frame==frame&&p.main==bool(main)&&p.special==bool(special)&&p.hitbox==bool(hitbox)&&p.behind==bool(behind)&&p.frame==bat&&near(p.pivot,pivot)&&player.playing()==bool(playing)&&player.attacking()==bool(attack)&&near(player.position(),position,0.000001))){std::fprintf(stderr,"Native swing observation %u %s: frame%u/%u main%d/%d special%d/%d hit%d/%d behind%d/%d pivot%.9g/%.9g play%d/%d attack%d/%d t%.9g/%.9g\n",observations,op.c_str(),p.frame,frame,p.main,main,p.special,special,p.hitbox,hitbox,p.behind,behind,p.pivot,pivot,player.playing(),playing,player.attacking(),attack,player.position(),position);CHECK(false);}++observations;
 }
 CHECK(observations==314);
 {const auto pose=player.pose();const auto time=player.position();CHECK(!player.play(0,false,error));CHECK(!player.advance(-1,error)&&!player.advance(61,error)&&!player.advance(std::numeric_limits<double>::quiet_NaN(),error));CHECK(same(pose,player.pose())&&time==player.position());FieldSwingData empty;CHECK(!player.initialize(empty,error));CHECK(same(pose,player.pose()));}
 CHECK(player.initialize(data,error));CHECK(player.play(1,true,error));CHECK(player.advance(.1,error));CHECK(near(player.position(),.07)&&player.attacking());CHECK(player.advance(1,error));CHECK(!player.playing()&&player.attacking()&&player.pending_callbacks()==1);CHECK(player.flush(error));CHECK(!player.attacking()&&!player.pose().hitbox);
 for(unsigned i=0;i<64;++i){CHECK(player.play(1,false,error));CHECK(player.advance(1,error));}CHECK(player.pending_callbacks()==64);CHECK(player.play(1,false,error));const auto pose=player.pose();CHECK(!player.advance(1,error));CHECK(player.position()==0&&same(pose,player.pose())&&player.pending_callbacks()==64);CHECK(player.flush(error));CHECK(player.advance(1,error));CHECK(player.flush(error));
 std::printf("Field swing: %u checks; %u native property/deferred-callback observations, eight directions, parser corruption/rollback and speed ownership; not input/overlap/audio/hardware\n",checks,observations);
}
