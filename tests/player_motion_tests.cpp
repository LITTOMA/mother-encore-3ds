#include "encore/field_player_tree.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>
using namespace encore::upstream;
namespace {unsigned checks=0;std::string error;void check(bool ok,const char*s){++checks;if(!ok){std::fprintf(stderr,"Player motion %u: %s (%s)\n",checks,s,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
}
int main(int argc,char**argv){
 CHECK(argc==5);FieldSwingEntryData data;FieldSwingActionData action;FieldSwingData swing;CHECK(data.load_file(argv[1],error));CHECK(swing.load_file(argv[2],error));CHECK(action.load_file(argv[3],error));
 std::ifstream in(argv[4]);CHECK(in.good());std::string line;FieldPlayerTree player;SwingMotionFlags flags;uint32_t previous=UINT32_MAX,observations=0,cases=0;Vec2 direction{0,1};
 while(std::getline(in,line)){if(line.empty())continue;std::istringstream row(line);uint32_t id,initial,incap,op,state,main_frame,frame,main,special,hit,behind,attack,paused,paused_state,starts,selected;double time,pivot;
  CHECK(bool(row>>id>>initial>>incap>>op>>state>>time>>main_frame>>frame>>main>>special>>hit>>pivot>>behind>>attack>>paused>>paused_state>>starts>>selected));row>>std::ws;CHECK(row.eof());
  if(id!=previous){CHECK(id==cases&&op==0);++cases;direction={0,1};CHECK(player.initialize(data,action,swing,initial,direction,float(time),"",error));flags={true,true,true,true,true};previous=id;}
  if(op==1){SwingActionContext c;c.direction=direction;c.incapacitated=bool(incap);c.skill_available=true;bool accepted=false;CHECK(player.input(action.strings()[2],true,c,flags,accepted,error));}
  else if(op==2)CHECK(player.advance(1.0/60,error));else if(op==3)CHECK(player.flush(flags,error));else if(op==4)CHECK(player.pause(false,false,bool(incap),flags,error));else if(op==6)CHECK(player.unpause(error));
  else if(op==8||op==11||op==12){direction=op==8?Vec2{1,0}:op==11?Vec2{0,-1}:Vec2{-1,0};CHECK(player.set_direction(direction,error));}
  else if(op==9||op==13){direction=op==9?Vec2{1,0}:Vec2{0,-1};CHECK(player.set_idle_direction(direction,error));CHECK(player.travel(incap?data.incapacitated_idle():data.idle(),error));}
  else if(op==10)CHECK(player.travel(incap?7:data.motion_states()[1],error));else CHECK(op==0);
  const auto&p=player.pose();const bool match=player.state_id()==state&&std::abs(player.position()-time)<.000002&&player.main_frame()==main_frame&&p.frame==frame&&p.main==bool(main)&&p.special==bool(special)&&p.hitbox==bool(hit)&&std::abs(p.pivot-pivot)<.0001&&p.behind==bool(behind)&&player.attacking()==bool(attack)&&player.paused()==bool(paused)&&player.paused_state()==paused_state&&player.starts()==starts&&(player.selected_skill()==action.strings()[3])==bool(selected);
  if(!match){std::fprintf(stderr,"Original direction row%u case%u op%u node%u/%u time%.9g/%.9g main%u/%u frame%u/%u vis%u%u/%u%u hit%u/%u attack%u/%u starts%u/%u pivot%.9g/%.9g\n",observations,id,op,player.state_id(),state,player.position(),time,player.main_frame(),main_frame,p.frame,frame,unsigned(p.main),unsigned(p.special),main,special,unsigned(p.hitbox),hit,unsigned(player.attacking()),attack,player.starts(),starts,p.pivot,pivot);CHECK(false);}++observations;
 }
 CHECK(in.eof()&&cases==14&&observations==2058);const auto old=player.pose();const auto state=player.state_id();
 CHECK(!player.set_direction({},error)&&!player.set_direction({2,0},error)&&!player.set_direction({std::numeric_limits<float>::quiet_NaN(),0},error)&&!player.set_idle_direction({},error));CHECK(player.state_id()==state&&player.pose().frame==old.frame);
 FieldPlayerTree empty;CHECK(!empty.set_direction({0,1},error)&&!empty.set_idle_direction({0,1},error));
 std::printf("Player directional handoff: %u checks; %u original setter, pause, repeated-direction, motion and attack observations across %u cases; no original movement physics/GPU/audio/hardware claim\n",checks,observations,cases);
}
