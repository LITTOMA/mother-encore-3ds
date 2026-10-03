#include "encore/items_menu.hpp"
#include "encore/battle_round.hpp"
#include "encore/battle_entry.hpp"
#include <cmath>
#include <cstdio>
#include <string>
using namespace encore::upstream;
namespace {
unsigned checks=0;bool ok=true;
void check(bool x,const char*s){++checks;if(!x){ok=false;std::fprintf(stderr,"FAIL: %s\n",s);}}
struct Host:BattleRoundHost {int32_t hp=0;bool emit(const BattleRoundCue&,SourceRandom&)override{return true;}bool ready(BattleRoundGate,uint32_t)const override{return true;}int32_t current_hp(uint32_t)const override{return hp;}};
}
int main(int argc,char**argv){
 if(argc!=4)return 2;std::string error;ItemData data;BattleData battle;BattleRoundData round_data;
 check(data.load_file(argv[1],error),error.c_str());check(battle.load_file(argv[2],error),error.c_str());check(round_data.load_file(argv[3],error),error.c_str());if(!ok)return 1;
 InventoryState inventory;check(inventory.initialize(data.view()),"session initial inventory");
 check(inventory.size()==1&&inventory.instance(0).equipped==1,"source equipped cap only");
 const auto item=inventory.instance(0);const auto def=data.view().definition(item.definition);
 check(data.view().string(def.source)=="BaseballCap"&&!inventory.can_use(0),"original cap cannot be selected while equipped");
 check(data.view().string(def.description)=="A hat for baseball players.\nOther @ Defense +5.","original translated description resolved by native font export");
 BattleItemsMenu menu;check(menu.initialize(inventory),"menu binds persistent owner");
 SourceRandom random(123);Host host;host.hp=battle.view().participant(round_data.view().binding().player_participant).hp;BattleRound round;
 check(round.begin(round_data.view(),battle.view(),random,host),"round initializes");const auto state=random.state(),draws=random.raw_draw_count();const auto hp=round.battler(0).target_hp;
 for(unsigned run=0;run<4;++run){
  check(round.request_menu(round_data.view().binding().items_menu)&&round.phase()==BattleRoundPhase::Items,"Items command opens real menu phase");check(menu.open(),"menu opens");
  check(menu.selection()==0&&menu.row_offset()==0,"new command resets selection");
  const auto panel=data.view().layout_for(ItemLayoutRole::Panel),cursor=data.view().layout_for(ItemLayoutRole::Cursor),info=data.view().layout_for(ItemLayoutRole::InfoPanel);
  check(menu.pose(panel).rect.y==-100,"source Open first key");
  const auto native_cursor=data.view().layout(cursor);check(menu.pose(cursor).rect.x==native_cursor.rect.x&&menu.pose(cursor).rect.y==native_cursor.rect.y,"preserve native cursor global-to-local rounding");
  check(menu.idle_frame(.1)&&menu.pose(panel).rect.y==8,"source Open end key");
  for(auto dir:{Vec2{-1,0},Vec2{1,0},Vec2{0,-1},Vec2{0,1}})check(menu.input(int(dir.x),int(dir.y),false,false,false)&&menu.selection()==0,"single item wraps without selecting empty labels");
  check(menu.input(0,0,true,false,false)&&menu.take_result()==ItemMenuResult::Restricted,"disabled confirm stays in menu");
  check(menu.active()&&inventory.size()==1&&inventory.instance(0).id==item.id&&inventory.instance(0).equipped==item.equipped&&inventory.instance(0).doses==item.doses,"inspection preserves instance and equipment");
  const bool prior=menu.info_visible();check(menu.input(0,0,false,false,true)&&menu.info_visible()!=prior,"source scope toggles description visibility");
  check(menu.idle_frame(.1),"info tween advances");
  const auto expected=data.view().layout(info).rect.y-(menu.info_visible()?data.view().parameter(ItemParameter::InfoMotion).y:0);
  check(menu.pose(info).rect.y==expected,"source info panel target");
  check(menu.input(0,0,false,true,false)&&menu.take_result()==ItemMenuResult::Back&&!menu.active(),"B closes item page");
  check(round.return_from_items()&&round.phase()==BattleRoundPhase::Commands&&round.take_menu_return(),"return to selected command without queue");
  check(menu.idle_frame(.1)&&!menu.visible(),"source Close completes");
  check(random.state()==state&&random.raw_draw_count()==draws&&round.number()==1&&round.battler(0).target_hp==hp&&round.decisions().empty(),"menu inspection consumes no turn HP or gameplay RNG");
 }
 check(!menu.idle_frame(-1)&&!menu.input(2,0,false,false,false),"invalid input rejected");
 check(inventory.instance(0).id==item.id,"persistent inventory survives repeated menu ownership");
 std::printf("Items menu/source inventory: %u checks\n",checks);return ok?0:1;
}
