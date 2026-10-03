#include "encore/menu_navigation.hpp"
#include "encore/items_data.hpp"
#include "encore/battle_entry.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using namespace encore::upstream;
namespace {
unsigned checks=0;
void check(bool ok,const char* why){
 ++checks;
 if(!ok){std::cerr<<"FAIL: "<<why<<'\n';std::exit(1);}
}
void direction(MenuDirection got,int x,int y,const char* why){check(got.x==x&&got.y==y,why);}
struct Fixture {
 MenuNavigationRepeat repeat;
 double delay,interval;
 explicit Fixture(BattleValue timing):delay(timing.x),interval(timing.y){
  direction(sample(0,0,0),0,0,"neutral activates command context");
 }
 MenuDirection sample(int x,int y,double dt,uint32_t context=1,bool grid=false){
  return repeat.sample(context,x,y,dt,delay,interval,grid);
 }
};

void taps_and_hold(BattleValue timing){
 Fixture f(timing);
 for(unsigned tap=0;tap<8;++tap){
  direction(f.sample(1,0,.005),1,0,"each short press moves immediately");
  direction(f.sample(1,0,.005),0,0,"short hold has no second pulse");
  direction(f.sample(0,0,.005),0,0,"release clears the pending repeat");
 }
 direction(f.sample(-1,0,0),-1,0,"left press moves immediately");
 direction(f.sample(-1,0,f.delay-.001),0,0,"first repeat waits full delay");
 direction(f.sample(-1,0,.001001),-1,0,"first repeat fires after configured delay");
 direction(f.sample(-1,0,f.interval-.001),0,0,"subsequent repeat waits full interval");
 direction(f.sample(-1,0,.001001),-1,0,"held repeat fires after configured interval");
 direction(f.sample(0,0,0),0,0,"release after repeat is neutral");
 direction(f.sample(-1,0,.001),-1,0,"repress is immediate even inside repeat interval");
 direction(f.sample(-1,0,f.interval+.001),0,0,"repress starts the first delay again");
}

void hold_at_rate(BattleValue timing,unsigned hz){
 Fixture f(timing);const double dt=1.0/hz;
 direction(f.sample(1,0,0),1,0,"hold starts with exactly one immediate move");
 std::vector<double> pulses;
 for(unsigned frame=1;frame<=2*hz;++frame){
  const auto p=f.sample(1,0,dt);
  check(p.y==0&&(p.x==0||p.x==1),"hold pulse is a single unit on the requested axis");
  if(p.x)pulses.push_back(frame*dt);
 }
 check(pulses.size()>=10,"sustained hold keeps repeating at both sampling rates");
 // Timings are serialized as float; allow sub-microsecond pack roundoff.
 check(pulses.front()+1e-7>=f.delay&&pulses.front()<=f.delay+dt+1e-7,
       "first hold repeat is delayed 350ms within one input sample");
 for(size_t i=1;i<pulses.size();++i){
  const double gap=pulses[i]-pulses[i-1];
  check(gap+1e-7>=f.interval&&gap<=f.interval+dt+1e-7,
        "held repeats are bounded by 100ms plus one input sample");
 }
 direction(f.sample(0,0,dt),0,0,"release ends sustained hold");
 direction(f.sample(0,0,10),0,0,"released input never repeats later");
}

void directions_and_grid(BattleValue timing){
 Fixture f(timing);
 direction(f.sample(1,0,0),1,0,"right press starts direction-change fixture");
 direction(f.sample(1,0,f.delay-.001),0,0,"old direction nearly reaches repeat");
 direction(f.sample(-1,0,.01),-1,0,"direction reversal moves immediately");
 direction(f.sample(-1,0,f.delay-.001),0,0,"reversal restarts full first delay");
 direction(f.sample(-1,0,.001001),-1,0,"reversed direction repeats after fresh delay");
 // The platform combines opposed keys before this adapter: right-left, down-up.
 direction(f.sample(1-1,1-1,1),0,0,"opposed directions normalize to neutral");
 direction(f.sample(-1,0,0),-1,0,"leaving opposed neutral permits immediate press");
 direction(f.sample(0,1,0),0,0,"command row ignores vertical input");
 direction(f.sample(1,1,0),1,0,"command row keeps horizontal diagonal component");

 Fixture grid(timing);
 direction(grid.sample(1,1,0,2,true),0,0,"held diagonal is blocked on entering grid");
 direction(grid.sample(0,0,0,2,true),0,0,"neutral arms grid");
 direction(grid.sample(1,1,0,2,true),0,1,"grid diagonal gives vertical priority");
 direction(grid.sample(-1,1,grid.delay-.001,2,true),0,0,
           "changing ignored diagonal component does not create another move");
 direction(grid.sample(-1,1,.001001,2,true),0,1,"vertical diagonal hold repeats vertically");
 direction(grid.sample(-1,0,0,2,true),-1,0,"releasing vertical starts horizontal direction");
 direction(grid.sample(1-1,-1,0,2,true),0,-1,"opposed horizontal keys preserve vertical movement");
 direction(grid.sample(0,-1,grid.delay-.001,2,true),0,0,"axis change resets first delay");
}

void contexts_and_stalls(BattleValue timing){
 Fixture f(timing);
 direction(f.sample(1,0,0),1,0,"context fixture initially moves");
 direction(f.sample(1,0,f.delay,2,true),0,0,"submenu transition blocks held direction");
 direction(f.sample(-1,0,10,2,true),0,0,"changing direction does not bypass transition block");
 direction(f.sample(-1,0,10,1),0,0,"cancel and return still block held direction");
 direction(f.sample(-1,0,10,1),0,0,"blocked hold cannot expire into a repeat");
 direction(f.sample(0,0,0,1),0,0,"neutral clears transition block");
 direction(f.sample(-1,0,0,1),-1,0,"fresh press after return moves immediately");
 direction(f.sample(-1,0,10,0),0,0,"inactive context suppresses held navigation");
 direction(f.sample(-1,0,10,1),0,0,"return from inactive context also requires neutral");
 direction(f.sample(0,0,0,1),0,0,"release rearms active context");
 direction(f.sample(1,0,0,1),1,0,"press after inactive transition moves");
 direction(f.sample(1,0,20),1,0,"long stalled frame produces one move");
 for(unsigned catchup=0;catchup<8;++catchup)
  direction(f.sample(1,0,0),0,0,"stalled frame leaves no repeat debt for catch-up");
 direction(f.sample(1,0,f.interval-.001),0,0,"stall restarts repeat interval from current sample");
 direction(f.sample(1,0,.001001),1,0,"normal repeat resumes after stalled sample");
 f.repeat.reset();
 direction(f.sample(1,0,20),0,0,"explicit reset does not leak a held press into a new context");
 direction(f.sample(0,0,0),0,0,"reset adapter rearms on neutral");
 direction(f.sample(1,0,0),1,0,"reset adapter accepts a new press");
 for(double bad:{-1.0,std::numeric_limits<double>::infinity(),
                 std::numeric_limits<double>::quiet_NaN()})
  direction(f.sample(1,0,bad),0,0,"invalid elapsed time cannot advance a held repeat");
 direction(f.sample(1,0,f.delay),1,0,"invalid elapsed samples preserve the first delay");
}

void battle_integration(BattleValue timing,const BattleView& battle,const RoomView& room){
 const auto source=room.battle(0);OpeningBattleRequest request;
 request.requested=true;request.queued=true;request.record_index=0;
 request.actor_index=source.actor_instance_index;request.advantage=source.advantage;
 request.can_run=false;request.enemy=room.string(source.enemy_string).data();
 request.win_flag_index=source.win_flag_index;
 const auto off=battle.parameter(BattleParameter::PartyScreenOffset);
 BattleEntrySnapshot snapshot;snapshot.player_screen={126-off.x,94-off.y};
 snapshot.enemy_screen={142,78};BattleEntry entry;
 check(entry.begin(battle,room,request,snapshot),entry.error());
 for(unsigned frame=0;frame<240&&entry.phase()!=BattleEntryPhase::Commands;++frame)
  check(entry.idle_frame(1.0/60.0),entry.error());
 check(entry.phase()==BattleEntryPhase::Commands,"battle fixture reaches command menu");
 const auto count=battle.count(BattleSection::Menus);
 check(count>1&&(battle.metadata().flags&2),"checked battle fixture contains wrapping command choices");
 Fixture f(timing);
 auto apply=[&](int x,double dt,uint32_t context=1){
  const auto pulse=f.sample(x,0,dt,context);
  check(entry.input(pulse.x,false,false,true),entry.error());
 };
 for(unsigned tap=0;tap<count*2;++tap){
  apply(1,.005);
  check(entry.selection()==(tap+1)%count,"rapid separate taps each advance exactly one command");
  apply(1,.005);
  check(entry.selection()==(tap+1)%count,"holding during a rapid tap cannot skip a command");
  apply(0,.005);
 }
 // No idle frame has expired the native source gate: the adapter owns repeats.
 apply(1,0);check(entry.selection()==1,"new press bypasses native debounce only when pre-gated");
 apply(1,20);check(entry.selection()==2%count,"stalled input advances command selection only once");
 for(unsigned i=0;i<8;++i)apply(1,0);
 check(entry.selection()==2%count,"catch-up cannot skip commands after a stall");
 check(entry.input(0,true,false,true),"confirm selected command");
 check(entry.phase()==BattleEntryPhase::ActionRequested,"confirm closes command input context");
 apply(1,.01,2);
 check(entry.resume_commands(false),"cancel submenu restores selected command");
 const auto selected=entry.selection();apply(1,20,1);
 check(entry.selection()==selected,"submenu return preserves selection while direction remains held");
 apply(0,0);apply(-1,.005);
 check(entry.selection()==(selected+count-1)%count,"fresh press after cancel navigates immediately");
}
}

int main(int argc,char** argv){
 check(argc==4,"provide checked Items, battle, and room packs");
 ItemData items;BattleData battle;RoomData room;std::string error;
 check(items.load_file(argv[1],error),error.c_str());
 check(battle.load_file(argv[2],error),error.c_str());
 check(room.load_file(argv[3],error),error.c_str());
 const auto timing=items.view().parameter(ItemParameter::InputRepeat);
 check(std::abs(timing.x-.35)<1e-6&&std::abs(timing.y-.1)<1e-6,
       "checked external resource defines 350ms delay and 100ms repeat interval");
 taps_and_hold(timing);hold_at_rate(timing,15);hold_at_rate(timing,60);
 directions_and_grid(timing);contexts_and_stalls(timing);
 battle_integration(timing,battle.view(),room.view());
 std::cout<<"Menu navigation: "<<checks<<" adapter and command integration checks passed\n";
}
