#include "encore/localized_presentation.hpp"
#include "encore/content.hpp"
#include "encore/field_story.hpp"
#include "encore/new_game_setup.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
using namespace encore::upstream;
static unsigned checks=0;static std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Bridge locale %u: %s (%s)\n",checks,#x,error.c_str());std::exit(1);}}while(0)
int main(int argc,char**argv){CHECK(argc==2);const std::string root=argv[1];LocaleCatalog catalog;BattleRoundData round;
 CHECK(catalog.load_file((root+"/localization/localization.enclocale").c_str(),error));CHECK(round.load_file((root+"/battle/straydog/bridge.encround").c_str(),error));LocaleSelection locale;CHECK(locale.bind(catalog,error));LocalizedPresentation display(locale);std::string text;
 const auto view=round.view();const auto index=view.skill(view.binding().basic_skill).dialog;
 for(const auto code:{"en","zh_Hans_CN"}){CHECK(locale.select(code,error));for(uint32_t i=0;i<view.count(RoundSection::Texts);++i)CHECK(display.battle(view,i,"Alex",text,error));CHECK(display.battle(view,index,"Alex",text,error));CHECK(text.find("Alex")!=std::string::npos&&text.find("Ninten")==std::string::npos);}
 std::vector<uint8_t>bytes;CHECK(encore::read_file((root+"/battle/straydog/bridge.encround").c_str(),bytes,2*1024*1024,error));const auto original=view.string(view.text(index).text);auto found=std::search(bytes.begin(),bytes.end(),original.begin(),original.end());CHECK(found!=bytes.end());*found='X';std::fill(bytes.begin()+16,bytes.begin()+20,0);auto crc=encore::crc32(bytes.data(),bytes.size());for(unsigned i=0;i<4;++i)bytes[16+i]=uint8_t(crc>>(8*i));BattleRoundData changed;CHECK(changed.load(bytes.data(),bytes.size(),error));CHECK(!display.battle(changed.view(),index,"Alex",text,error));CHECK(catalog.valid());
 NewGameSetupData names;CHECK(names.load_file((root+"/ui/opening.encnewgame").c_str(),error));NativeSessionData session;CHECK(session.load_file((root+"/session/opening.encsession").c_str(),error));auto snapshot=session.defaults();
 for(const auto code:{"en","zh_Hans_CN"}){CHECK(locale.select(code,error));CHECK(format_field_story_text("[Pippi]",names,snapshot,locale,text,error));CHECK(!text.empty()&&text.find('[')==text.npos);}
 auto pippi=session.startup_characters()[3];CHECK(pippi.character_id=="pippi");pippi.nickname="Peep";snapshot.characters.push_back(pippi);CHECK(format_field_story_text("[Pippi] needs help.",names,snapshot,locale,text,error));CHECK(text=="Peep needs help.");
 for(const auto bad:{"[Unknown]","[Pippi","[Pippi:3]"}){const auto prior=text;CHECK(!format_field_story_text(bad,names,snapshot,locale,text,error));CHECK(text==prior);}
 std::printf("Bridge localization: %u checks; real catalog load, all admitted text/locale rows, custom name and stale-source rejection\n",checks);
}
