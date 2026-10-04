#include "encore/battle_entry.hpp"
#include <cstdlib>
#include <iostream>
#include <string>
using namespace encore::upstream;
namespace {
unsigned checks=0;
void check(bool ok,const std::string&message){++checks;if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
bool equal(const BattleValue&a,const BattleValue&b){return a.x==b.x&&a.y==b.y&&a.z==b.z&&a.w==b.w;}
}
int main(int argc,char**argv){
 check(argc==3,"Entry fixture directory and actual room pack required");
 const std::string directory=argv[1];std::string error;BattleData baseline,changed;RoomData room;
 check(baseline.load_file((directory+"/baseline.encbattle").c_str(),error),error);
 check(changed.load_file((directory+"/changed.encbattle").c_str(),error),error);
 check(room.load_file(argv[2],error),error);
 const auto source=room.view().battle(0);OpeningBattleRequest request;
 request.requested=true;request.queued=true;request.record_index=0;request.actor_index=source.actor_instance_index;
 request.advantage=source.advantage;request.can_run=false;request.enemy=room.view().string(source.enemy_string).data();request.win_flag_index=source.win_flag_index;
 BattleEntrySnapshot snapshot;const auto offset=baseline.view().parameter(BattleParameter::PartyScreenOffset);
 snapshot.player_screen={126-offset.x,94-offset.y};snapshot.enemy_screen={142,78};
 BattleEntry a,b;check(a.begin(baseline.view(),room.view(),request,snapshot),a.error());check(b.begin(changed.view(),room.view(),request,snapshot),b.error());
 bool margin_observed=false;
 check(a.content().count(BattleSection::Layouts)==b.content().count(BattleSection::Layouts),"Source layout topology retained");
 for(uint32_t index=0;index<a.content().count(BattleSection::Layouts);++index){
  const auto x=a.content().layout(index),y=b.content().layout(index);
  if(x.kind==uint32_t(BattleDrawKind::Tiled)&&x.role==uint32_t(BattleRole::PartyPlate)){
   check(y.margins[0]==x.margins[0]+1,"Same actual entry consumer reads changed tiled border binding");margin_observed=true;
   for(unsigned side=1;side<4;++side)check(x.margins[side]==y.margins[side],"Other source borders unchanged");
  }
 }
 check(margin_observed,"Actual source party plate present");
 check(a.take_encounter_audio()==b.take_encounter_audio(),"Source encounter audio identity retained");
 for(unsigned frame=0;frame<150;++frame){
  if(frame){check(a.idle_frame(1./60.),a.error());check(b.idle_frame(1./60.),b.error());}
  check(a.phase()==b.phase()&&a.party_landed()==b.party_landed()&&a.mask_active()==b.mask_active(),"Source callback phases retained");
  for(uint32_t index=0;index<a.content().count(BattleSection::Layouts);++index){
   const auto x=a.pose(index),y=b.pose(index);
   check(equal(x.rect,y.rect)&&equal(x.color,y.color)&&equal(x.flash_color,y.flash_color)&&x.flash_modifier==y.flash_modifier&&x.frame==y.frame&&x.visible==y.visible,"Actual 150-frame source poses retained");
  }
 }
 check(a.phase()==BattleEntryPhase::Commands,"Source entry reaches commands");
 check(a.input(0,true)&&b.input(0,true)&&a.requested_action()==b.requested_action(),"Original selected action identity retained");
 std::cout<<checks<<" entry binding consumer checks; no GPU/emulator/hardware claim\n";
}
