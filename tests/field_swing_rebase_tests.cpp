#include "encore/field_swing_entry.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
void check(bool ok,const char*s){++checks;if(!ok){std::fprintf(stderr,"Swing rebase %u: %s (%s)\n",checks,s,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
}
int main(int argc,char**argv){
 CHECK(argc==5);FieldSwingData swing;FieldSwingActionData action;FieldSwingEntryData data;
 CHECK(swing.load_file(argv[2],error)&&action.load_file(argv[3],error)&&data.load_file(argv[1],error));
 std::ifstream input(argv[4]);CHECK(input.good());std::string line;FieldSwingEntry player;
 SwingMotionFlags motion;uint32_t prior=UINT32_MAX,sequence=UINT32_MAX,observations=0,cases=0;
 while(std::getline(input,line)){
  if(line.empty())continue;
  std::istringstream row(line);uint32_t id,seq,origin,incap,phase,node,main_frame,frame,main,special,hit,attack,behind,bat,starts,selected;
  float x,y,start;double time,pivot;
  CHECK(bool(row>>id>>seq>>origin>>x>>y>>incap>>start>>phase>>node>>time>>main_frame>>frame>>main>>special>>hit>>pivot>>attack>>behind>>bat>>starts>>selected));row>>std::ws;CHECK(row.eof());
  if(id!=prior){
   CHECK(id==cases&&phase==0&&seq==id/10);++cases;
   if(seq!=sequence){CHECK(player.initialize(data,action,swing,origin,{x,y},start,"",error));sequence=seq;}
   else{
    const auto old=player.pose();const auto count=player.action().starts();const auto skill=player.action().selected_skill();
    CHECK(player.rebase(origin,{x,y},start,error));CHECK(player.action().starts()==count&&player.action().selected_skill()==skill);
    CHECK(player.pose().frame==old.frame&&player.pose().pivot==old.pivot&&player.pose().behind==old.behind);
   }
   prior=id;motion={true,true,true,true,true};
  }
  if(phase==1){SwingActionContext context;context.direction={x,y};context.incapacitated=bool(incap);context.skill_available=true;bool accepted=false;CHECK(player.input(action.strings()[2],true,context,motion,accepted,error)&&accepted);}
  else if(phase==2)CHECK(player.advance(1.0/60,error));else if(phase==3)CHECK(player.flush(motion,error));else CHECK(phase==0);
  const auto p=player.pose();const bool match=player.state_id()==node&&player.main_frame()==main_frame&&p.frame==frame&&p.main==bool(main)&&p.special==bool(special)&&p.hitbox==bool(hit)&&std::abs(p.pivot-pivot)<.0001&&player.attacking()==bool(attack)&&p.behind==bool(behind)&&player.action().bat_visible()==bool(bat)&&player.action().starts()==starts&&(player.action().selected_skill()==action.strings()[3])==bool(selected)&&(node==data.idle()||std::abs(double(player.position())-time)<.000002);
  if(!match){std::fprintf(stderr,"Source row%u case%u phase%u state%u/%u main%u/%u sparse%u/%u behind%u/%u starts%u/%u\n",observations,id,phase,player.state_id(),node,player.main_frame(),main_frame,p.frame,frame,unsigned(p.behind),behind,player.action().starts(),starts);CHECK(false);}++observations;
 }
 CHECK(input.eof()&&cases==80&&observations==6560);
 const auto pose=player.pose();const auto count=player.action().starts();const auto old_state=player.state_id();
 for(float position:{-1.f,61.f,std::numeric_limits<float>::quiet_NaN()})CHECK(!player.rebase(data.idle(),{0,1},position,error));
 CHECK(!player.rebase(data.attack(),{0,1},0,error)&&!player.rebase(999,{0,1},0,error)&&!player.rebase(data.idle(),{0,0},0,error));
 CHECK(player.pose().frame==pose.frame&&player.pose().pivot==pose.pivot&&player.state_id()==old_state&&player.action().starts()==count);
 CHECK(player.rebase(data.motion_states()[1],{1,0},.02f,error));SwingActionContext context;context.direction={1,0};context.skill_available=true;bool accepted=false;
 CHECK(player.input(action.strings()[2],true,context,motion,accepted,error)&&accepted);CHECK(!player.rebase(data.idle(),{0,1},0,error)&&player.attacking());
 for(unsigned i=0;i<50&&player.attacking();++i){CHECK(player.advance(1.0/60,error));if(player.attacking()&&player.state_id()==data.idle())CHECK(!player.rebase(data.idle(),{0,1},0,error));CHECK(player.flush(motion,error));}
 CHECK(!player.attacking()&&player.rebase(data.idle(),{0,-1},0,error)&&player.action().starts()==count+1);
 FieldSwingEntry empty;FieldSwingAction base;CHECK(!empty.rebase(data.idle(),{0,1},0,error)&&!base.rebase({0,1},error));
 std::printf("Swing rebase: %u checks; %u source observations in 8 persistent-player sequences; atomic motion handoff and sparse pose/selection/counter retention\n",checks,observations);
}
