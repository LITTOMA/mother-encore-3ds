#include "encore/items_menu.hpp"
#include "encore/battle_action_presentation.hpp"
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
using namespace encore::upstream;
namespace {
unsigned checks=0;
#define check(value,message) do{++checks;if(!(value)){std::cerr<<"check "<<checks<<" line "<<__LINE__<<": "<<(message)<<'\n';std::exit(1);}}while(false)
struct Trace {Vec2 anchor;float glyph_x=0;ItemInstance item;std::string source,description;std::vector<ItemSoundEvent>sounds;uint64_t state=0,draws=0;std::vector<float>info_y;};
Trace run(const std::string&path,const char*battle_path,const char*round_path){
 ItemData data;BattleData battle;BattleRoundData round_data;std::string error;
 check(data.load_file(path.c_str(),error)&&battle.load_file(battle_path,error)&&round_data.load_file(round_path,error),error);
 auto view=data.view();InventoryState inventory;check(inventory.initialize(view),"Original inventory binds checked resource");check(inventory.size()==1&&inventory.instance(0).equipped,"Source bounded equipped item present");
 BattleItemsMenu menu;check(menu.initialize(inventory),menu.error());
 SourceRandom random(123);BattleActionPresentation presentation;check(presentation.begin(round_data.view(),battle.view(),random),presentation.error());BattleRound round;
 check(round.begin(round_data.view(),battle.view(),random,presentation),round.error());const auto state=random.state(),draws=random.raw_draw_count();
 check(round.request_menu(round_data.view().binding().items_menu)&&round.phase()==BattleRoundPhase::Items,"Actual original round enters Items command");check(menu.open(),menu.error());
 const auto info=view.layout_for(ItemLayoutRole::InfoPanel);uint32_t glyph=item_no_index;
 for(uint32_t i=0;i<view.count(ItemSection::Layouts);++i){auto l=view.layout(i);if(l.role==uint32_t(ItemLayoutRole::Hint)&&l.kind==uint32_t(ItemDrawKind::Sprite))glyph=i;}
 check(glyph!=item_no_index,"Checked hint glyph binding present");Trace result;result.anchor=menu.pose(info).source.anchor;result.glyph_x=menu.pose(glyph).rect.x;
 result.item=inventory.instance(0);auto def=view.definition(result.item.definition);result.source=view.string(def.source);result.description=view.string(def.description);
 for(unsigned pass=0;pass<3;++pass){
  for(unsigned i=0;i<6;++i){check(menu.idle_frame(1./60),menu.error());result.info_y.push_back(menu.pose(info).rect.y);}
  check(menu.input(0,0,true,false,false)&&menu.take_result()==ItemMenuResult::Restricted,"Original equipped item remains restricted");
  check(menu.input(0,0,false,false,true),menu.error());
 }
 check(menu.input(0,0,false,true,false)&&menu.take_result()==ItemMenuResult::Back,"Original cancel closes Items");
 check(round.return_from_items()&&round.phase()==BattleRoundPhase::Commands&&round.take_menu_return(),"Same core restores original command menu");
 check(menu.idle_frame(.1)&&!menu.visible(),"Original Close clip completes");
 check(random.state()==state&&random.raw_draw_count()==draws&&round.decisions().empty()&&round.number()==1,"Inspection spends no combat turn or shared RNG");
 check(inventory.instance(0).id==result.item.id&&inventory.instance(0).definition==result.item.definition&&inventory.instance(0).equipped==result.item.equipped&&inventory.instance(0).doses==result.item.doses,"Source item/instance identity survives inspection");
 result.sounds=menu.take_sounds();result.state=random.state();result.draws=random.raw_draw_count();return result;
}
}
int main(int argc,char**argv){
 check(argc==4,"Fixture directory, original entry and round paths required");std::string directory=argv[1],error;ItemData checked;
 check(checked.load_file((directory+"/baseline.encitems").c_str(),error),error);const auto owner=checked.view().metadata().owner;
 check(!checked.load_file((directory+"/bad-crc.encitems").c_str(),error),"Corrupt candidate fails checked load");check(checked.view().valid()&&checked.view().metadata().owner==owner,"Rejected candidate preserves prior owner");
 const auto a=run(directory+"/baseline.encitems",argv[2],argv[3]),b=run(directory+"/adapter.encitems",argv[2],argv[3]);
 check(a.anchor.x!=b.anchor.x&&b.anchor.x==.75f&&a.anchor.y==b.anchor.y,"Same executable menu pose consumes modified checked anchor");
 check(b.glyph_x==a.glyph_x-1,"Actual hint pose consumes independently changed adapter offset");
 check(a.info_y==b.info_y&&a.sounds==b.sounds&&a.state==b.state&&a.draws==b.draws,"Source tween/event sequence and shared RNG unchanged");
 check(a.item.id==b.item.id&&a.item.definition==b.item.definition&&a.item.equipped==b.item.equipped&&a.item.doses==b.item.doses&&a.source==b.source&&a.description==b.description,"Original stable inventory content unchanged");
 std::cout<<checks<<" Items binding consumer checks: real Items command, checked adapter poses, preserved inventory/events/RNG\n";
}
