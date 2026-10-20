#include "encore/field_player_tree.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <limits>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
void check(bool ok,const char*s){++checks;if(!ok){std::fprintf(stderr,"Player tree %u: %s (%s)\n",checks,s,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
uint32_t get(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(v>>(8*i));}
void seal(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
}
int main(int argc,char**argv){
 CHECK(argc==5);FieldSwingEntryData data,legacy;FieldSwingActionData action;FieldSwingData swing;std::vector<uint8_t>bytes;
 CHECK(encore::read_file(argv[1],bytes,64*1024,error));CHECK(data.load(bytes.data(),bytes.size(),error));CHECK(data.capability()==2&&data.incapacitated_idle()==6);CHECK(swing.load_file(argv[2],error));CHECK(action.load_file(argv[3],error));CHECK(data.bind(action,swing,error));
 for(const auto&a:data.states())for(const auto&b:data.states())CHECK(data.route(a.id,b.id));CHECK(!data.route(0,1)&&!data.route(1,99));
 auto reject=[&](std::vector<uint8_t>b){seal(b);CHECK(!data.load(b.data(),b.size(),error));CHECK(data.valid()&&data.capability()==2&&data.incapacitated_idle()==6&&data.bind(action,swing,error));};
 for(size_t n=0;n<bytes.size();++n)CHECK(!data.load(bytes.data(),n,error));
 const auto meta=get(bytes,68),states=get(bytes,84),routes=get(bytes,148);
 for(auto r:std::vector<std::pair<size_t,uint32_t>>{{20,1},{20,3},{52,5},{148,0},{152,63},{156,4},{meta+28,0},{meta+28,1},{meta+28,8},{meta+28,99},{states+28,131072},{states+28,1},{states+28,99},{states+7*32+28,65536},{routes,0},{routes+4,99},{routes+8,2},{routes+16+8,1},{routes+16+12,1},{routes+16+8,99}}){auto b=bytes;put(b,r.first,r.second);reject(b);}
 {auto b=bytes;for(unsigned i=0;i<16;++i)b[routes+16+i]=b[routes+i];reject(b);}
 {auto b=bytes;put(b,states+7*32+28,0);seal(b);FieldSwingEntryData other;CHECK(other.load(b.data(),b.size(),error));CHECK(!other.bind(action,swing,error));}
 CHECK(legacy.load_file("tests/fixtures/field-swing-entry-v0410.encentry",error)&&legacy.capability()==1);
 FieldPlayerTree player;CHECK(!player.initialize(legacy,action,swing,1,{0,1},0,"",error));SwingMotionFlags flags;bool accepted=true;CHECK(!player.advance(0,error)&&!player.unpause(error)&&!player.pause(false,true,false,flags,error)&&!player.travel(1,error));
 std::ifstream in(argv[4]);CHECK(in.good());std::string line;uint32_t previous=UINT32_MAX,observations=0,cases=0;
 while(std::getline(in,line)){if(line.empty())continue;std::istringstream row(line);uint32_t id,initial,incap,idle,op,state,main_frame,frame,main,special,hit,behind,attack,paused,paused_state,starts,selected;float x,y;double time,pivot;
  CHECK(bool(row>>id>>initial>>x>>y>>incap>>idle>>op>>state>>time>>main_frame>>frame>>main>>special>>hit>>pivot>>behind>>attack>>paused>>paused_state>>starts>>selected));row>>std::ws;CHECK(row.eof());
  if(id!=previous){CHECK(id==cases&&op==0);++cases;CHECK(player.initialize(data,action,swing,initial,{x,y},float(time),"",error));flags={true,true,true,true,true};previous=id;}
  if(op==1||op==5||op==7){SwingActionContext c;c.direction={x,y};c.incapacitated=bool(incap);c.skill_available=true;CHECK(player.input(action.strings()[2],true,c,flags,accepted,error));CHECK(accepted==(op!=5));}
  else if(op==2)CHECK(player.advance(1.0/60,error));else if(op==3)CHECK(player.flush(flags,error));else if(op==4)CHECK(player.pause(false,bool(idle),bool(incap),flags,error));else if(op==6)CHECK(player.unpause(error));else CHECK(op==0);
  const auto&p=player.pose();const bool match=player.state_id()==state&&std::abs(player.position()-time)<.000002&&player.main_frame()==main_frame&&p.frame==frame&&p.main==bool(main)&&p.special==bool(special)&&p.hitbox==bool(hit)&&std::abs(p.pivot-pivot)<.0001&&p.behind==bool(behind)&&player.attacking()==bool(attack)&&player.paused()==bool(paused)&&player.paused_state()==paused_state&&player.starts()==starts&&(player.selected_skill()==action.strings()[3])==bool(selected);
  if(!match){std::fprintf(stderr,"Original pause row%u case%u op%u node%u/%u time%.9g/%.9g main%u/%u frame%u/%u vis%u%u/%u%u hit%u/%u attack%u/%u paused%u/%u freeze%u/%u starts%u/%u pivot%.9g/%.9g\n",observations,id,op,player.state_id(),state,player.position(),time,player.main_frame(),main_frame,p.frame,frame,unsigned(p.main),unsigned(p.special),main,special,unsigned(p.hitbox),hit,unsigned(player.attacking()),attack,unsigned(player.paused()),paused,player.paused_state(),paused_state,player.starts(),starts,p.pivot,pivot);CHECK(false);}++observations;
 }
 CHECK(in.eof()&&cases==28&&observations==3556);
 // Retain the earlier eight-direction incoming-path observations unchanged.
 std::ifstream incoming("reports/iggy-tutorial/swing-entry-reference-4/reference.tsv");CHECK(incoming.good());previous=UINT32_MAX;uint32_t entry_rows=0,entry_cases=0;
 while(std::getline(incoming,line)){if(line.empty())continue;std::istringstream row(line);uint32_t id,state,incap,op,expected,main_frame,frame,main,special,hit,attack;float x,y,start;double delta,time,pivot;
  CHECK(bool(row>>id>>state>>x>>y>>incap>>start>>op>>delta>>expected>>time>>main_frame>>frame>>main>>special>>hit>>pivot>>attack));row>>std::ws;CHECK(row.eof());
  if(id!=previous){CHECK(id==entry_cases&&op==0);++entry_cases;CHECK(player.initialize(data,action,swing,state,{x,y},start,"",error));flags={true,true,true,true,true};previous=id;}
  if(op==1){SwingActionContext c;c.direction={x,y};c.incapacitated=bool(incap);c.skill_available=true;CHECK(player.input(action.strings()[2],true,c,flags,accepted,error)&&accepted);}else if(op==2)CHECK(player.advance(delta,error));else if(op==3)CHECK(player.flush(flags,error));else CHECK(op==0);
  const auto&p=player.pose();CHECK(player.state_id()==expected&&player.main_frame()==main_frame&&p.frame==frame&&p.main==bool(main)&&p.special==bool(special)&&p.hitbox==bool(hit)&&std::abs(p.pivot-pivot)<.0001&&player.attacking()==bool(attack)&&(expected==data.idle()||std::abs(player.position()-time)<.000002));++entry_rows;
 }
 CHECK(incoming.eof()&&entry_rows==6560&&entry_cases==80);
 CHECK(!player.advance(-1,error)&&!player.advance(61,error)&&!player.advance(std::numeric_limits<double>::quiet_NaN(),error)&&!player.travel(99,error));
 FieldSwingPlayer unprepared;CHECK(!unprepared.set_incapacitated(true,error));
 std::printf("Player tree: %u checks; %u original pause/unpause/repeated-input observations across %u cases; plus %u unchanged incoming-path rows/%u cases; capability1 retained, capability2 negative admission; no world/GPU/audio or hardware claim\n",checks,observations,cases,entry_rows,entry_cases);
}
