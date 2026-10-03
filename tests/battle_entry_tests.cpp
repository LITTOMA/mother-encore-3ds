#include "encore/battle_entry.hpp"
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
using namespace encore::upstream;
namespace {
unsigned checks=0;
void check(bool ok,const char* why){++checks;if(!ok){std::cerr<<"FAIL: "<<why<<"\n";std::exit(1);}}
void near(float actual,double expected,double tolerance,const char* why,unsigned frame){++checks;if(!std::isfinite(actual)||std::abs(double(actual)-expected)>tolerance){std::cerr<<"FAIL frame "<<frame<<" "<<why<<": "<<actual<<" vs "<<expected<<"\n";std::exit(1);}}
struct Golden {unsigned frame=0;double x=0,y=0,sx=1,sy=1,plate_y=20,cx=0,cy=0,csx=1,csy=1;bool visible=true,menu=false;};
double number(const std::string&line){const auto begin=line.find('"');return std::strtod(line.c_str()+(begin==std::string::npos?0:begin+1),nullptr);}
std::vector<Golden> read_reference(const char* path){
 std::ifstream in(path);check(bool(in),"native reference exists");std::string line;std::vector<Golden> out;Golden row;bool active=false;
 auto pair=[&](double&a,double&b){std::getline(in,line);a=number(line);std::getline(in,line);b=number(line);};
 while(std::getline(in,line)){
  if(line.find("\"frame\":")!=std::string::npos){row={};row.frame=unsigned(std::strtoul(line.c_str()+line.find(':')+1,nullptr,10));active=true;}
  else if(active&&line.find("\"player_pos\"")!=std::string::npos)pair(row.x,row.y);
  else if(active&&line.find("\"player_scale\"")!=std::string::npos)pair(row.sx,row.sy);
  else if(active&&line.find("\"player_visible\"")!=std::string::npos)row.visible=line.find("true")!=std::string::npos;
  else if(active&&line.find("\"plate_pos\"")!=std::string::npos){double x;pair(x,row.plate_y);}
  else if(active&&line.find("\"cursor_pos\"")!=std::string::npos)pair(row.cx,row.cy);
  else if(active&&line.find("\"cursor_scale\"")!=std::string::npos)pair(row.csx,row.csy);
  else if(active&&line.find("\"menu_active\"")!=std::string::npos){row.menu=line.find("true")!=std::string::npos;out.push_back(row);active=false;}
 }
 check(out.size()==150,"150 unchanged native frames");return out;
}
uint32_t role(const BattleView&v,BattleRole r){for(uint32_t i=0;i<v.count(BattleSection::Layouts);++i)if(v.layout(i).role==uint32_t(r))return i;check(false,"required source role");return 0;}
OpeningBattleRequest request(const RoomView&r){const auto b=r.battle(0);OpeningBattleRequest q;q.requested=true;q.queued=true;q.record_index=0;q.actor_index=b.actor_instance_index;q.advantage=b.advantage;q.can_run=false;q.enemy=r.string(b.enemy_string).data();q.win_flag_index=b.win_flag_index;return q;}
}
int main(int argc,char**argv){
 check(argc==4,"provide external battle pack, room pack, unchanged Godot timing JSON");
 std::string error;BattleData bd;RoomData rd;check(bd.load_file(argv[1],error),error.c_str());check(rd.load_file(argv[2],error),error.c_str());const auto v=bd.view();const auto r=rd.view();const auto q=request(r);
 const auto golden=read_reference(argv[3]);const auto pi=role(v,BattleRole::PartyTransition),platei=role(v,BattleRole::PartyPlate),cursori=role(v,BattleRole::MenuCursor),enemyi=role(v,BattleRole::EnemySprite);
 BattleEntry entry;const auto off=v.parameter(BattleParameter::PartyScreenOffset);BattleEntrySnapshot snapshot;snapshot.player_screen={126-off.x,94-off.y};snapshot.enemy_screen={142,78};snapshot.enemy_frame=0;
 check(entry.begin(v,r,q,snapshot),entry.error());check(entry.encounter_audio_pending(),"encounter queued once");check(entry.take_encounter_audio()==v.metadata().encounter_audio&&!entry.encounter_audio_pending(),"encounter consuming read");
 check(entry.input(1,true)&&entry.selection()==0&&entry.requested_action()==0,"entry input gated");
 const auto sprite=v.layout(pi),plate=v.layout(platei);
 const auto portraiti=role(v,BattleRole::PartySprite);
 // Independent native Control/VBox hierarchy proof in reports/battle-parent-layout-reference.
 const double portrait_parent_y[]={193,172.822723,155.174667,140.253479,128.339249,119.886627,116};
 for(const auto&f:golden){
  if(f.frame)check(entry.idle_frame(1.0/60.0),entry.error());
  const auto p=entry.pose(pi),pp=entry.pose(platei);
  if(f.frame<=18&&f.frame%3==0)near(entry.pose(portraiti).rect.y,portrait_parent_y[f.frame/3],0.0002,"portrait inherits source PlayerInfo entry translation",f.frame);
  near(p.rect.x,f.x,0.0002,"source jump x",f.frame);near(p.rect.y,f.y,0.0003,"source jump y",f.frame);
  // The timing probe intentionally begins after the separate initial squash;
  // compare its scale only once the native jump starts.
  if(f.frame>=30){near(p.rect.z/sprite.rect.z,f.sx,0.00002,"jump scale x",f.frame);near(p.rect.w/sprite.rect.w,f.sy,0.00002,"jump scale y",f.frame);}
  check(p.visible==f.visible,"source arrival deferred hide");
  if(f.frame>=65)near(pp.rect.y-plate.rect.y,f.plate_y-20,0.0001,"source quake chain/discontinuous from",f.frame);
  check((entry.phase()==BattleEntryPhase::Commands)==f.menu,"source native menu activation frame");
  if(f.frame==76)check(!entry.pose(enemyi).visible,"enemy not revealed before deferred callback");
  if(f.frame==77)check(entry.pose(enemyi).visible,"enemy shown on native callback frame");
 }
 check(entry.phase()==BattleEntryPhase::Commands,"first menu terminal scope");
 near(entry.pose(portraiti).rect.y,96,0.0001,"portrait later show tween overrides settled parent entry track",150);
 // Replay the source cursor's exact native tween after menu activation; the
 // reference moved its independent cursor at frame10, and first sampled11.
 check(entry.input(1,false)&&entry.selection()==1,"source compact visible action order");
 for(unsigned n=0;n<=12;++n){if(n)check(entry.idle_frame(1.0/60.0),entry.error());const auto p=entry.pose(cursori);const auto&g=golden[10+n];near(p.rect.x+p.rect.z/2,g.cx,0.0001,"cursor quartic-out center x",n);near(p.rect.y+p.rect.w/2,g.cy,0.0001,"cursor center y",n);near(p.rect.z/v.layout(cursori).rect.z,g.csx,0.00002,"cursor source squash x",n);near(p.rect.w/v.layout(cursori).rect.w,g.csy,0.00002,"cursor source squash y",n);}
 check(entry.input(1,false)&&entry.selection()==2,"next Defend");check(entry.input(1,false)&&entry.selection()==2,"repeat timer gates held input");for(int i=0;i<4;++i)entry.idle_frame(1.0/60.0);check(entry.input(1,false)&&entry.selection()==0,"wrap to Basic");
 check(entry.input(0,false,true)&&entry.selection()==0,"cancel resets first menu");check(entry.input(0,true)&&entry.phase()==BattleEntryPhase::ActionRequested,"confirm stops at typed action request");check(entry.requested_action()==v.menu(0).id,"selected source action ID");const auto hp=entry.participant(0).hp;entry.idle_frame(1.0/60.0);check(entry.participant(0).hp==hp,"no fabricated action damage");check(entry.input(-1,true)&&entry.selection()==0,"no repeated action execution past boundary");
 check(entry.resume_commands(),"new round resets command page");for(int i=0;i<4;++i)entry.idle_frame(1.0/60.0);
 check(entry.input(1,true)&&entry.selection()==1,"select Items command");check(entry.resume_commands(false)&&entry.selection()==1,"submenu Back preserves selected command");check(entry.resume_commands(true)&&entry.selection()==0,"new round still resets Basic");
 {BattleEntry e;auto s=snapshot;s.camera_shaking=true;check(e.begin(v,r,q,s)&&e.phase()==BattleEntryPhase::WaitingCamera,"wait camera shake");check(e.idle_frame(1.0/60.0,true)&&e.scene_time()==0,"shake does not advance entry");check(e.idle_frame(1.0/60.0,false)&&e.phase()==BattleEntryPhase::Entering,"shake release starts scene");}
 {BattleEntry e;auto s=snapshot;s.player_screen.x=v.parameter(BattleParameter::PartyNudge).x;check(!e.begin(v,r,q,s),"unknown required cosmetic choice fails closed");s.nudge_sign=1;BattleEntry good;check(good.begin(v,r,q,s),"explicit cosmetic sign accepted");}
 {BattleEntry e;check(e.begin(v,r,q,snapshot),"negative input fixture");check(!e.idle_frame(std::numeric_limits<double>::infinity()),"infinite dt rejected");}
 {BattleEntry e;check(e.begin(v,r,q,snapshot),"negative delta fixture");check(!e.idle_frame(-1),"negative dt rejected");}
 {BattleEntry e;auto bad=q;bad.advantage=-1;check(!e.begin(v,r,bad,snapshot),"unimplemented advantage rejected");}
 std::cout<<"Battle entry: "<<checks<<" scoped checks passed against unchanged Godot reference; no combat/GPU/hardware claim\n";
}
