#include "encore/house_presentation.hpp"
#include "encore/mick_treats.hpp"
#include "encore/utf8.hpp"
#include <cstdlib>
#include <cmath>
#include <iostream>

using namespace encore::upstream;
namespace {
unsigned checks=0;
void check(bool value,const char* message){++checks;if(!value){std::cerr<<message<<'\n';std::exit(1);}}
// Layout adapter only: exercise English and Chinese control flow without
// making source-font metrics or pixel matching part of this timing test.
bool glyph(void*,uint32_t c,float& advance){advance=8;return c>=32&&c!=0x2063&&c!=0x2064;}
std::string visible(const HousePresentation& box){std::string result;for(const auto& line:box.dialogue_pose().lines)result+=line.text;return result;}
void tick(HousePresentation& box){check(box.advance_dialogue(1.0/60),box.error());check(box.advance_chrome(1.0/60),box.error());}
}
int main(int argc,char** argv){
 check(argc==4,"house, font and Mick resources required");
 HouseData house;BattleData font;MickData mick;std::string error;
 check(house.load_file(argv[1],error),error.c_str());check(font.load_file(argv[2],error),error.c_str());check(mick.load_file(argv[3],error),error.c_str());
 const auto view=mick.view();unsigned total_gates=0;
 for(const auto locale:{"en","zh_Hans_CN"})for(uint32_t i=0;i<view.count(MickSection::Texts);++i){
  const auto row=view.text(i);std::string text(view.string(std::string_view(locale)=="en"?row.en:row.zh));
  check(resolve_dialogue_tags(text,"Ninten","A","B","+",error,"Burgers",13),error.c_str());
  std::u32string decoded;check(encore::utf8_decode(text,decoded),"valid source UTF-8");
  unsigned expected=0;for(auto c:decoded)expected+=c==0x2063;
  SourceRandom rng(13);HousePresentation box;check(box.begin(house.view(),font.view(),rng),box.error());box.set_glyph_advance(glyph,nullptr);
  check(box.present_plain("Mick",text),box.error());check(box.advance_chrome(.4),box.error());
  unsigned stops=0;
  for(unsigned guard=0;guard<20000&&!box.dialogue_finished();++guard){
   // One B-down accelerates printing. A held after that supplies no new edge.
   if(!box.dialogue_stopped())box.input(false,true);
   tick(box);
   if(!box.dialogue_stopped())continue;
   ++stops;const auto count=box.visible_characters();const auto page=visible(box);
   check(!box.dialogue_finished()&&box.dialogue_pose().cursor_visible,"WAIT requires confirmation before final completion");
   for(unsigned hold=0;hold<600;++hold)tick(box);
   check(box.dialogue_stopped()&&box.visible_characters()==count&&visible(box)==page,"idle and held keys cannot scroll past a WAIT");
   check(!box.take_dialogue_advance(),"internal wait cannot advance the Mick programme");
   box.input(true,false);check(!box.dialogue_stopped()&&!box.dialogue_finished(),"fresh A resumes exactly one source segment");
   const auto period=house.view().interaction().text_seconds;
   check(box.advance_dialogue(period*.25),box.error());
   // Godot retains the unused portion of the previous physics tick at WAIT.
   // Bound normal-speed progress, including that remainder, rather than
   // inventing a new zeroed clock when the player resumes.
   check(box.visible_characters()-count<=unsigned(std::ceil((1.0/60+period*.25)/period)),"WAIT resets prior B acceleration");
  }
  check(box.dialogue_finished()&&stops==expected,"every original English/Chinese WAIT preserved");total_gates+=stops;
  check(visible(box).find("\xe2\x81\xa3")==std::string::npos,"control marker never rendered");
  const auto final=visible(box);for(unsigned hold=0;hold<120;++hold)tick(box);
  check(box.dialogue_active()&&visible(box)==final&&!box.take_dialogue_advance(),"completed phrase remains until fresh confirmation");
  box.input(true,false,true,true);check(box.take_dialogue_advance()&&!box.take_dialogue_advance(),"final confirmation is consumed once");
 }
 check(total_gates>0,"real Mick resources must contain source waits");
 SourceRandom rng(7);HousePresentation box;check(box.begin(house.view(),font.view(),rng),box.error());box.set_glyph_advance(glyph,nullptr);
 check(!box.present_plain("M\xe2\x81\xa3ick","Hi"),"wait control forbidden in speaker");
 check(!box.present_plain("Mick","Hi\xe2\x81\xa4"),"unknown invisible control rejected");
 check(!box.present_plain("Mick",std::string(1,char(0xff))),"malformed UTF-8 rejected");
 check(box.present_plain("Mick","First\xe2\x81\xa3\nSecond"),box.error());box.advance_chrome(.4);box.input(true,false);
 for(unsigned frame=0;frame<600;++frame)tick(box);
 check(box.dialogue_stopped()&&visible(box)=="First","one held A accelerates only to first WAIT");
 box.input(true,false);for(unsigned frame=0;frame<600;++frame)tick(box);
 check(box.dialogue_finished()&&visible(box)=="FirstSecond","next fresh A resumes without dismissing phrase");
 std::cout<<"Mick dialogue pacing: "<<checks<<" checks; "<<total_gates<<" source waits passed\n";
}
