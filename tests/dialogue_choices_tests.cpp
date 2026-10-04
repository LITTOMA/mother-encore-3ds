#include "encore/dialogue_choices.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <limits>
using namespace encore::upstream;
namespace {
unsigned checks=0;bool okay=true;
void check(bool value,const char*label){++checks;if(!value){okay=false;std::fprintf(stderr,"FAIL: %s\n",label);}}
bool near(float a,float b){return std::abs(a-b)<.001f;}
std::vector<DialogueChoicesEvent>drain(DialogueChoices&m){std::vector<DialogueChoicesEvent>out;DialogueChoicesEvent e;while(m.poll_event(e))out.push_back(e);return out;}
uint32_t crc(const uint8_t*p,size_t n){uint32_t c=~0u;while(n--){c^=*p++;for(unsigned i=0;i<8;++i)c=(c>>1)^(0xedb88320u&uint32_t(-int32_t(c&1)));}return ~c;}
void put(std::vector<uint8_t>&b,size_t offset,uint32_t value){for(unsigned i=0;i<4;++i)b[offset+i]=uint8_t(value>>(8*i));}
void repair(std::vector<uint8_t>&b){put(b,12,uint32_t(b.size()));put(b,16,crc(b.data()+24,b.size()-24));}
size_t find(const std::vector<uint8_t>&b,const std::string&s){return size_t(std::search(b.begin(),b.end(),s.begin(),s.end())-b.begin());}
}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 DialogueChoicesData d;std::string error;check(d.load_file(argv[1],error),error.c_str());if(!okay)return 1;
 check(d.columns()==3&&d.child_count()==6,"source grid includes hidden children");
 check(d.trailing_blank_lines()==1,"source adds blank line to reserve options row");
 check(d.minimum().w==60&&d.minimum().h==10&&d.grid().x==52&&d.grid().y==38,"source local options geometry");
 check(d.groups().size()==2&&d.groups()[0].options.size()==2,"cancel remains hidden");
 const auto&g=d.groups()[0];check(g.options[0].text=="Record"&&g.options[1].text=="Nothing, really","source translated labels");
 check(g.options[0].rect.w==64&&g.options[1].rect.x==76,"native translated GridContainer layout");
 check(d.sound(DialogueChoiceSound::Accept)==d.sound(DialogueChoiceSound::Cancel)&&d.sound(DialogueChoiceSound::Accept)=="Audio/Sound effects/Cursor 2.mp3","source confirm and cancel both InputSound");
 check(d.validate_program(0,g.program_identity,g.program_command_count,error),"bind matching program");
 check(!d.validate_program(2,g.program_identity,g.program_command_count,error),"unknown group rejected");
 check(!d.validate_program(0,"Reusable/other",g.program_command_count,error),"wrong program rejected");
 check(!d.validate_program(0,g.program_identity,g.program_command_count-1,error),"wrong program extent rejected");
 DialogueChoices m;check(!m.text_completed(error),"completion without prepare rejected");
 check(m.prepare(d,0,g.program_identity,g.program_command_count,error),"prepare source choices");
 check(!m.pose().visible&&m.phase()==DialogueChoicesPhase::WaitingText,"choices hidden while text prints");
 check(m.step(.5,{1,0,true,true},error)&&drain(m).empty()&&m.phase()==DialogueChoicesPhase::WaitingText,"early input cannot select");
 check(!m.prepare(d,0,g.program_identity,g.program_command_count,error),"double prepare rejected");
 check(m.text_completed(error)&&m.pose().visible&&m.pose().selected==0,"actual completion selects first source option");
 check(!m.text_completed(error),"duplicate completion rejected");
 check(near(m.pose().arrow_x,52-5-8.f/6)&&near(m.pose().arrow_y,44),"arrow source cursor offset and size adjustment");
 check(m.step(0,{-1,0,false,false},error)&&m.pose().selected==0&&drain(m).empty(),"left boundary does not wrap or sound");
 check(m.step(0,{0,1,false,false},error)&&m.pose().selected==0&&drain(m).empty(),"down cannot select hidden row");
 check(m.step(0,{1,0,false,false},error)&&m.pose().selected==1,"right selects Nothing");
 auto events=drain(m);check(events.size()==1&&events[0].kind==DialogueChoicesEventKind::SoundRequested&&events[0].sound==DialogueChoiceSound::Move,"navigation emits only source move sound");
 check(near(m.pose().arrow_x,52-5-8.f/6),"arrow starts movement at prior position");
 check(m.step(.05,{},error)&&near(m.pose().arrow_x,52-5-8.f/6+76*.9375f),"source quart-out midpoint");
 check(m.step(.05,{},error)&&near(m.pose().arrow_x,52-5-8.f/6+76),"source movement duration");
 check(m.step(.1,{},error)&&m.pose().arrow_frame==1,"source arrow frame timing");
 check(m.step(0,{1,0,false,false},error)&&m.pose().selected==1&&drain(m).empty(),"hidden third choice does not wrap");
 check(m.step(0,{0,0,true,false},error)&&!m.pose().visible,"Nothing confirms and hides options");
 events=drain(m);check(events.size()==1&&events[0].kind==DialogueChoicesEventKind::Selected&&events[0].target_pc==g.options[1].target_pc&&!events[0].cancelled&&events[0].clear_dialogue&&events[0].sound_after_target,"Nothing carries checked PC and ordered clear/confirm sound");
 check(m.step(0,{0,0,true,false},error)&&drain(m).empty(),"resolved input cannot duplicate result");
 check(m.prepare(d,0,g.program_identity,g.program_command_count,error)&&m.text_completed(error),"next visit resets choice");
 check(m.pose().selected==0&&m.step(0,{0,0,true,false},error),"Record is initial source selection");
 events=drain(m);check(events.size()==1&&events[0].target_pc==g.options[0].target_pc&&!events[0].cancelled,"Record emits own source target");
 for(int selected=0;selected<2;++selected){m.close();m.prepare(d,0,g.program_identity,g.program_command_count,error);m.text_completed(error);if(selected)m.step(0,{1,0,false,false},error);drain(m);
  check(m.step(0,{0,0,true,true},error),"simultaneous cancel and accept follows cancel");events=drain(m);check(events.size()==1&&events[0].target_pc==g.cancel_target_pc&&events[0].cancelled&&events[0].sound==DialogueChoiceSound::Cancel&&events[0].sound_after_target,"cancel target independent of highlighted option");}
 m.close();check(m.phase()==DialogueChoicesPhase::Closed&&!m.pose().visible&&!m.group(),"explicit teardown drops data and state");
 check(!m.step(-.1,{},error)&&!m.step(std::numeric_limits<double>::quiet_NaN(),{},error)&&!m.step(0,{2,0,false,false},error),"invalid numeric/input domain rejected");
 std::ifstream file(argv[1],std::ios::binary);const std::vector<uint8_t>bytes((std::istreambuf_iterator<char>(file)),{});DialogueChoicesData rejected;
 for(size_t n=0;n<bytes.size();++n)check(!rejected.load(bytes.data(),n,error),"all truncations reject");
 auto mutated=bytes;put(mutated,8,2);check(!rejected.load(mutated.data(),mutated.size(),error),"unknown schema rejected");
 mutated=bytes;put(mutated,20,3);check(!rejected.load(mutated.data(),mutated.size(),error),"unknown capability rejected");
 mutated=bytes;mutated.back()^=1;check(!rejected.load(mutated.data(),mutated.size(),error),"CRC damage rejected");
 mutated=bytes;put(mutated,24,0);repair(mutated);check(!rejected.load(mutated.data(),mutated.size(),error),"invalid source columns rejected");
 mutated=bytes;put(mutated,76,0x7ff80000);repair(mutated);check(!rejected.load(mutated.data(),mutated.size(),error),"nonfinite source time rejected");
 mutated=bytes;const auto record=find(mutated,"Record");check(record<mutated.size(),"locate fixture option");put(mutated,record+6,g.program_command_count);repair(mutated);check(!rejected.load(mutated.data(),mutated.size(),error),"CRC-valid option branch beyond extent rejected");
 mutated=bytes;const auto resource=find(mutated,"graphics/ui/house/cursor.t3x");check(resource<mutated.size(),"locate source resource");mutated[resource]='/';repair(mutated);check(!rejected.load(mutated.data(),mutated.size(),error),"absolute resource path rejected");
 mutated=bytes;mutated.push_back(0);repair(mutated);check(!rejected.load(mutated.data(),mutated.size(),error),"CRC-valid trailing payload rejected");
 check(!d.load(mutated.data(),mutated.size(),error)&&d.valid()&&d.groups()[0].options[0].text=="Record","failed reload preserves earlier checked data");
 std::printf("Dialogue choices: %u checks, %s\n",checks,okay?"passed":"FAILED");return okay?0:1;
}
