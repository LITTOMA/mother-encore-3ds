#include "encore/new_game_setup.hpp"
#include "encore/content.hpp"
#include "encore/utf8.hpp"
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Preview line %d: %s: %s\n",__LINE__,#x,error.c_str());std::exit(1);}}while(0)
void tick(NewGameSetup&m,double dt,NamingInput in={}){CHECK(m.step(dt,in,error));}
void open_preview(NewGameSetup&m,const NewGameSetupData&names,const StartupSettingsData&settings,const LocaleSelection*locale=nullptr){
 m.set_locale(locale);CHECK(m.open(names,settings,error));
 // Traverse the actual naming workflow using source Don't Care and Next inputs.
 for(size_t field=0;field<names.fields.size();++field){CHECK(m.phase()==NamingPhase::Editing&&m.field_index()==field);NamingInput command;command.command=true;tick(m,0,command);NamingInput accept;accept.accept=true;tick(m,0,accept);CHECK(!m.name().empty());NamingInput next;next.next=true;tick(m,0,next);}
 CHECK(m.phase()==NamingPhase::Settings&&m.settings_row()==0);NamingInput accept;accept.accept=true;tick(m,0,accept);CHECK(m.phase()==NamingPhase::SettingOption);tick(m,0);
}
void u32(std::vector<uint8_t>&b,size_t at,uint32_t value){CHECK(at+4<=b.size());for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(value>>(8*i));}
void repair(std::vector<uint8_t>&b){u32(b,12,uint32_t(b.size()));u32(b,16,encore::crc32(b.data()+24,b.size()-24));}
StartupSettingsData load(const std::string&path){StartupSettingsData result;CHECK(result.load_file(path.c_str(),error));return result;}
}
int main(int argc,char**argv){
 CHECK(argc==3);const std::string root=argv[1],fixtures=argv[2];NewGameSetupData names;CHECK(names.load_file((root+"/opening.encnewgame").c_str(),error));
 auto source=load(root+"/opening.encsettings");CHECK(source.preview_minimum_characters==5);NewGameSetup menu;const auto source_defaults=source.defaults();
 auto five=load(fixtures+"/five.encsettings");open_preview(menu,names,five);CHECK(menu.preview_characters()==5);tick(menu,1);CHECK(menu.preview_characters()==5);
 auto six=load(fixtures+"/six.encsettings");open_preview(menu,names,six);CHECK(menu.preview_characters()==0);const double period=six.speeds[menu.option()];tick(menu,period);CHECK(menu.preview_characters()==0);tick(menu,period/2);CHECK(menu.preview_characters()==1);tick(menu,1);CHECK(menu.preview_characters()==2);
 // All-label predicate: a nonselected short label disables the selected preview.
 auto mixed=load(fixtures+"/mixed.encsettings");open_preview(menu,names,mixed);CHECK(menu.option()!=2&&menu.preview_characters()==6);
 // Same executable and labels, different independently loaded threshold.
 auto changed=load(fixtures+"/five-threshold4.encsettings");open_preview(menu,names,changed);CHECK(menu.preview_characters()==0);tick(menu,1);CHECK(menu.preview_characters()==1);NamingInput down;down.y=1;tick(menu,0,down);CHECK(menu.preview_characters()==0);tick(menu,1);CHECK(menu.preview_characters()==1);NamingInput cancel;cancel.cancel=true;tick(menu,0,cancel);CHECK(menu.phase()==NamingPhase::Settings&&menu.preview_characters()==0);
 LocaleCatalog catalog;CHECK(catalog.load_file((root+"/localization.enclocale").c_str(),error));LocaleSelection selected;CHECK(selected.bind(catalog,error));auto zero=load(fixtures+"/source-threshold0.encsettings");
 for(const char*code:{"en","zh_Hans_CN"}){
  CHECK(selected.select(code,error));open_preview(menu,names,source,&selected);bool worthwhile=true;size_t chosen=0;bool multibyte=false;
  for(size_t i=0;i<source.panels[0].labels.size();++i){const auto text=menu.localized("settings.panel/0/"+std::to_string(i),source.panels[0].labels[i].text);size_t count=0;CHECK(encore::utf8_count(text,count));CHECK(count>0);multibyte|=text.size()>count;worthwhile&=count>source.preview_minimum_characters;if(i==menu.option())chosen=count;}
  CHECK(menu.preview_characters()==(worthwhile?0:chosen));if(std::string(code)!="en")CHECK(multibyte);
  open_preview(menu,names,zero,&selected);CHECK(menu.preview_characters()==0);tick(menu,zero.speeds[menu.option()]*1.5);CHECK(menu.preview_characters()==1);
 }
 CHECK(source.defaults().text_speed==source_defaults.text_speed&&source.defaults().menu_flavor==source_defaults.menu_flavor&&source.defaults().button_prompts==source_defaults.button_prompts);
 std::vector<uint8_t>bytes;CHECK(encore::read_file((root+"/opening.encsettings").c_str(),bytes,1024*1024,error));
 const auto reject=[&](const std::vector<uint8_t>&bad){CHECK(!source.load(bad.data(),bad.size(),error));CHECK(source.valid()&&source.preview_minimum_characters==5&&source.defaults().text_speed==source_defaults.text_speed);};
 for(uint32_t threshold:{1025u,UINT32_MAX}){auto bad=bytes;u32(bad,bad.size()-4,threshold);repair(bad);reject(bad);}
 for(uint32_t version:{0u,1u,3u}){auto bad=bytes;u32(bad,8,version);reject(bad);}auto bad=bytes;u32(bad,20,2);reject(bad);
 bad=bytes;bad.back()^=1;reject(bad);bad=bytes;bad.resize(bad.size()-4);repair(bad);reject(bad);bad=bytes;bad.push_back(0);repair(bad);reject(bad);
 for(size_t cut:{size_t(0),size_t(23),bytes.size()-1}){CHECK(!source.load(bytes.data(),cut,error));CHECK(source.valid()&&source.preview_minimum_characters==5);}CHECK(!source.load_file((fixtures+"/absent.encsettings").c_str(),error));CHECK(source.valid());
 std::printf("Source settings preview: %u checks; all-label Unicode threshold, strict timing, resource changes, input workflow and parser rejection passed.\n",checks);
}
