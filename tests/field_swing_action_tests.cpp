#include "encore/field_swing_action.hpp"
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
void check(bool ok,const char*s){++checks;if(!ok){std::fprintf(stderr,"Swing action %u: %s (%s)\n",checks,s,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
uint32_t get(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(v>>(8*i));}
void seal(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
uint32_t flags(const SwingMotionFlags&f){return uint32_t(f.crouch)|uint32_t(f.idle)<<1|uint32_t(f.tap_run)<<2|uint32_t(f.running)<<3|uint32_t(f.walking)<<4;}
}
int main(int argc,char**argv){
 CHECK(argc==4);std::vector<uint8_t>bytes;CHECK(encore::read_file(argv[1],bytes,32*1024,error));FieldSwingActionData data;FieldSwingData swing,empty;CHECK(swing.load_file(argv[2],error));CHECK(!data.bind(swing,error));CHECK(data.load(bytes.data(),bytes.size(),error));CHECK(data.valid()&&data.bind(swing,error)&&!data.bind(empty,error));CHECK(data.strings()[2]=="ui_cancel"&&data.strings()[3]=="swing"&&data.policy()[1]==63&&data.policy()[2]==31&&data.policy()[3]==4&&data.policy()[7]==200);
 auto unchanged=[&](){CHECK(data.valid()&&data.bind(swing,error)&&data.directions().size()==8&&data.policy()[1]==63&&data.strings()[0]=="Idle");};
 auto reject=[&](std::vector<uint8_t>b){seal(b);CHECK(!data.load(b.data(),b.size(),error));unchanged();};auto mutation=[&](size_t at,uint32_t value){auto b=bytes;put(b,at,value);reject(std::move(b));};
 for(size_t n=0;n<bytes.size();++n){CHECK(!data.load(bytes.data(),n,error));unchanged();}CHECK(!data.load(nullptr,bytes.size(),error));CHECK(!data.load(bytes.data(),32769,error));unchanged();
 for(auto r:std::vector<std::pair<size_t,uint32_t>>{{0,0},{8,0},{8,2},{12,0},{20,0},{20,2},{24,1},{28,1},{32,0},{52,4},{56,1},{60,1},{64,2},{68,0},{68,113},{72,0},{72,2},{76,1},{80,1},{84,0xffffffff},{88,7},{88,9},{92,1},{104,0},{104,257},{108,2}})mutation(r.first,r.second);
 {auto b=bytes;b[16]^=1;CHECK(!data.load(b.data(),b.size(),error));unchanged();}
 {auto b=bytes;b.push_back(0);put(b,12,uint32_t(b.size()));reject(b);}
 const auto meta=get(bytes,68),directions=get(bytes,84),strings=get(bytes,100);
 for(auto r:std::vector<std::pair<size_t,uint32_t>>{{0,0},{0,2},{4,0},{4,64},{8,32},{12,32},{16,1},{20,0},{20,3},{24,0},{24,2},{28,0},{28,65536},{32,1},{36,0},{36,65},{40,0},{60,0xffffffff}})mutation(meta+r.first,r.second);
 for(auto r:std::vector<std::pair<size_t,uint32_t>>{{0,0},{0,65536},{4,0x7fc00000},{8,0x40000000},{12,200},{16,4},{20,1},{24,1}})mutation(directions+r.first,r.second);
 {auto b=bytes;put(b,directions+4,0);put(b,directions+8,0);reject(b);}
 for(unsigned char c:std::array<unsigned char,4>{{0,':','/',127}}){auto b=bytes;b[strings]=c;reject(b);}
 {auto b=bytes;put(b,directions,99);seal(b);FieldSwingActionData other;CHECK(other.load(b.data(),b.size(),error));CHECK(!other.bind(swing,error));}
 {auto b=bytes;put(b,directions+4,0x3f000000);seal(b);FieldSwingActionData other;CHECK(other.load(b.data(),b.size(),error));CHECK(!other.bind(swing,error));}
 FieldSwingAction action;SwingMotionFlags motion{true,true,true,true,true};SwingActionContext context;context.direction={0,1};context.skill_available=true;bool accepted=true;
 CHECK(!action.bat_visible()&&!action.input("ui_cancel",true,context,motion,accepted,error)&&accepted&&flags(motion)==31);CHECK(!action.advance(0,error)&&!action.flush(motion,error));
 CHECK(action.initialize(data,swing,{0,1},"",error));
 std::ifstream in(argv[3]);CHECK(in.good());std::string line;uint32_t previous=UINT32_MAX,observations=0,cases=0;
 while(std::getline(in,line)){if(line.empty())continue;std::istringstream row(line);uint32_t id,incap,available,paused,door,climb,spin,moving,initial,op,pressed,frame,main_frame,main,special,hitbox,behind,bat_visible,bat_frame,attack,starts,node,selected,motion_flags,sound;float x,y;double value,pivot,time;
  CHECK(bool(row>>id>>x>>y>>incap>>available>>paused>>door>>climb>>spin>>moving>>initial>>op>>value>>pressed>>frame>>main_frame>>main>>special>>hitbox>>behind>>bat_visible>>bat_frame>>pivot>>attack>>starts>>node>>selected>>motion_flags>>time>>sound));row>>std::ws;CHECK(row.eof());
  if(id!=previous){CHECK(id==cases);++cases;CHECK(action.initialize(data,swing,{x,y},initial==0?"":initial==1?"swing":"unknown",error));motion={true,true,true,true,true};previous=id;}
  context={{x,y},bool(moving),bool(paused),bool(door),bool(climb),bool(spin),bool(available),bool(incap)};
  if(op==1){const auto before=action.starts();CHECK(action.input(value==1?data.strings()[2]:"fixture_unrelated",bool(pressed),context,motion,accepted,error));CHECK(accepted==(action.starts()!=before));}
  else if(op==2)CHECK(action.advance(value,error));else {CHECK(op==3);CHECK(action.flush(motion,error));}
  const auto&p=action.pose();const uint32_t skill=action.selected_skill().empty()?0:action.selected_skill()==data.strings()[3]?1:2;
  const bool match=p.frame==frame&&action.main_frame()==main_frame&&p.main==bool(main)&&p.special==bool(special)&&p.hitbox==bool(hitbox)&&p.behind==bool(behind)&&action.bat_visible()==bool(bat_visible)&&p.frame==bat_frame&&std::abs(p.pivot-pivot)<.0001&&action.attacking()==bool(attack)&&action.starts()==starts&&action.bat_node()==bool(node)&&skill==selected&&flags(motion)==motion_flags&&bool(action.starts())==bool(sound)&&(!node||std::abs(double(action.position())-time)<.000002);
  if(!match){std::fprintf(stderr,"Native source action row %u case%u op%u frame%u/%u main%u/%u special%u/%u hit%u/%u attack%u/%u node%u/%u starts%u/%u flags%u/%u t%.9g/%.9g\n",observations,id,op,p.frame,frame,unsigned(p.main),main,unsigned(p.special),special,unsigned(p.hitbox),hitbox,unsigned(action.attacking()),attack,unsigned(action.bat_node()),node,action.starts(),starts,flags(motion),motion_flags,action.position(),time);CHECK(false);}++observations;
 }
 CHECK(in.eof()&&cases==25&&observations==683);
 CHECK(action.initialize(data,swing,{0,-1},"",error));motion={true,true,true,true,true};context={};context.direction={0,-1};context.skill_available=true;
 const auto pose=action.pose();accepted=true;context.direction={0,0};CHECK(!action.input(data.strings()[2],true,context,motion,accepted,error));CHECK(accepted&&action.starts()==0&&flags(motion)==31&&action.pose().frame==pose.frame);
 context.direction={0,std::numeric_limits<float>::quiet_NaN()};CHECK(!action.input(data.strings()[2],true,context,motion,accepted,error));CHECK(action.starts()==0&&accepted);
 CHECK(!action.advance(-1,error)&&!action.advance(61,error)&&!action.advance(std::numeric_limits<double>::infinity(),error));CHECK(!action.initialize(data,empty,{0,1},"",error)&&!action.initialize(data,swing,{0,0},"",error)&&!action.initialize(data,swing,{0,1},std::string(65,'x'),error));CHECK(action.starts()==0&&action.pose().frame==pose.frame);
 context.direction={0,-1};CHECK(action.input(data.strings()[2],true,context,motion,accepted,error)&&accepted&&action.attacking());CHECK(action.advance(0,error)&&action.bat_node()&&action.pose().frame==pose.frame);CHECK(action.advance(0,error)&&action.pose().frame==30&&action.pose().special);CHECK(action.advance(1,error)&&!action.bat_node()&&action.attacking()&&action.pose().main&&!action.pose().special);motion.tap_run=true;CHECK(action.flush(motion,error)&&!motion.tap_run&&!action.attacking());
 std::printf("Swing action: %u checks; %u original logical-input/AnimationTree observations in %u cases; typed data/parser rollback and deferred control; no live game/physics/audio audibility claim\n",checks,observations,cases);
}
