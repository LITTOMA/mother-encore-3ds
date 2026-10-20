#include "encore/field_swing_entry.hpp"
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
void check(bool ok,const char*s){++checks;if(!ok){std::fprintf(stderr,"Swing entry %u: %s (%s)\n",checks,s,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
uint32_t get(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(v>>(8*i));}
void seal(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
}
int main(int argc,char**argv){
 CHECK(argc==5);FieldSwingData swing;FieldSwingActionData action;FieldSwingEntryData data,empty;CHECK(swing.load_file(argv[2],error));CHECK(action.load_file(argv[3],error));std::vector<uint8_t>bytes;CHECK(encore::read_file(argv[1],bytes,64*1024,error));CHECK(data.load(bytes.data(),bytes.size(),error));CHECK(data.valid()&&data.bind(action,swing,error)&&data.states().size()==8&&data.clips().size()==56&&data.frames()==200);CHECK(data.idle()==1&&data.attack()==8&&data.motion_states()[3]==4);
 auto unchanged=[&](){CHECK(data.valid()&&data.bind(action,swing,error)&&data.states().size()==8&&data.frames()==200);};auto reject=[&](std::vector<uint8_t>b){seal(b);CHECK(!data.load(b.data(),b.size(),error));unchanged();};auto mutation=[&](size_t at,uint32_t value){auto b=bytes;put(b,at,value);reject(std::move(b));};
 for(size_t n=0;n<bytes.size();++n){CHECK(!data.load(bytes.data(),n,error));unchanged();}CHECK(!data.load(nullptr,bytes.size(),error));CHECK(!data.load(bytes.data(),65537,error));unchanged();
 for(auto r:std::vector<std::pair<size_t,uint32_t>>{{0,0},{8,0},{8,2},{12,0},{20,0},{20,2},{24,1},{28,1},{32,0},{52,4},{56,1},{60,1},{64,2},{68,0},{68,145},{72,0},{72,2},{76,1},{84,0xffffffff},{88,1},{88,33},{92,1},{104,0},{104,257},{108,1},{120,4097},{124,4},{136,0},{140,2}})mutation(r.first,r.second);
 {auto b=bytes;b[16]^=1;CHECK(!data.load(b.data(),b.size(),error));unchanged();}
 {auto b=bytes;b.push_back(0);put(b,12,uint32_t(b.size()));reject(b);}
 const auto meta=get(bytes,68),states=get(bytes,84),clips=get(bytes,100),keys=get(bytes,116),strings=get(bytes,132);
 for(auto r:std::vector<std::pair<size_t,uint32_t>>{{0,0},{0,65536},{4,99},{8,1},{12,7},{16,55},{20,0},{24,9},{28,1},{32,8},{36,1}})mutation(meta+r.first,r.second);
 for(auto r:std::vector<std::pair<size_t,uint32_t>>{{0,0},{0,65536},{4,1},{8,0},{8,65},{12,99},{16,1},{20,1},{24,9},{28,1},{32,1},{32+12,99},{32+12,2}})mutation(states+r.first,r.second);
 for(auto r:std::vector<std::pair<size_t,uint32_t>>{{0,99},{4,1},{8,0x7fc00000},{12,0x40000000},{16,0},{16,0x7f800000},{20,2},{24,1},{28,0},{28,17},{32,2},{36,0xfffffffe}})mutation(clips+r.first,r.second);
 for(auto r:std::vector<std::pair<size_t,uint32_t>>{{0,0x7fc00000},{0,0x3f000000},{4,200},{8,0x7fc00000},{12,200}})mutation(keys+r.first,r.second);
 {auto b=bytes;b[strings]='/';reject(b);}
 {auto b=bytes;put(b,states+32+12,8);seal(b);FieldSwingEntryData other;CHECK(other.load(b.data(),b.size(),error));CHECK(!other.bind(action,swing,error));}
 std::ifstream in(argv[4]);CHECK(in.good());std::string line;FieldSwingEntry player;SwingMotionFlags flags{true,true,true,true,true};uint32_t previous=UINT32_MAX,observations=0,cases=0;
 while(std::getline(in,line)){if(line.empty())continue;std::istringstream row(line);uint32_t id,state,incap,phase,expected_state,main_frame,frame,main,special,hit,attack;float x,y,start;double delta,time,pivot;
  CHECK(bool(row>>id>>state>>x>>y>>incap>>start>>phase>>delta>>expected_state>>time>>main_frame>>frame>>main>>special>>hit>>pivot>>attack));row>>std::ws;CHECK(row.eof());
  if(id!=previous){CHECK(id==cases&&phase==0);++cases;CHECK(player.initialize(data,action,swing,state,{x,y},start,"",error));flags={true,true,true,true,true};previous=id;}
  if(phase==1){SwingActionContext context;context.direction={x,y};context.skill_available=true;context.incapacitated=bool(incap);bool accepted=false;CHECK(player.input(action.strings()[2],true,context,flags,accepted,error)&&accepted);}
  else if(phase==2)CHECK(player.advance(delta,error));else if(phase==3)CHECK(player.flush(flags,error));else CHECK(phase==0);
  const auto pose=player.pose();const bool match=player.state_id()==expected_state&&player.main_frame()==main_frame&&pose.frame==frame&&pose.main==bool(main)&&pose.special==bool(special)&&pose.hitbox==bool(hit)&&std::abs(pose.pivot-pivot)<.0001&&player.attacking()==bool(attack)&&(expected_state==data.idle()||std::abs(double(player.position())-time)<.000002);
  if(!match){std::fprintf(stderr,"Original entry row%u case%u phase%u origin%u state%u/%u mainframe%u/%u frame%u/%u hit%u/%u attack%u/%u t%.9g/%.9g\n",observations,id,phase,state,player.state_id(),expected_state,player.main_frame(),main_frame,pose.frame,frame,unsigned(pose.hitbox),hit,unsigned(player.attacking()),attack,player.position(),time);CHECK(false);}++observations;
 }
 CHECK(in.eof()&&observations==6560&&cases==80);
 CHECK(player.initialize(data,action,swing,4,{0,1},.04f,"",error));const auto before=player.pose();CHECK(!player.advance(-1,error)&&!player.advance(61,error)&&!player.advance(std::numeric_limits<double>::quiet_NaN(),error));CHECK(player.state_id()==4&&player.pose().frame==before.frame);CHECK(!player.initialize(empty,action,swing,4,{0,1},0,"",error)&&!player.initialize(data,action,swing,8,{0,1},0,"",error)&&!player.initialize(data,action,swing,4,{0,0},0,"",error)&&!player.initialize(data,action,swing,4,{0,1},-1,"",error)&&!player.initialize(data,action,swing,4,{0,1},1,"",error));CHECK(player.state_id()==4);
 SwingActionContext context;context.direction={1,0};context.skill_available=true;bool accepted=true;flags={true,true,true,true,true};CHECK(!player.input(action.strings()[2],true,context,flags,accepted,error));CHECK(accepted&&!player.attacking()&&flags.crouch&&flags.walking);context.direction={0,1};CHECK(player.input(action.strings()[2],true,context,flags,accepted,error)&&accepted);CHECK(!flags.crouch&&!flags.walking&&player.attacking());CHECK(player.advance(0,error)&&player.state_id()==4);CHECK(player.advance(1,error)&&player.state_id()==1&&player.attacking());CHECK(player.advance(0,error)&&player.state_id()==8);
 std::printf("Swing entry: %u checks; %u original motion-to-Bat observations across %u cases; topology/clip corruption and atomic snapshot admission; no game-loop or hardware claim\n",checks,observations,cases);
}
