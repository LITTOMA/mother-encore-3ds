#include "encore/new_game_setup.hpp"
#include "encore/content.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
using namespace encore::upstream;
namespace {unsigned checks=0;std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Naming bindings line %d: %s: %s\n",__LINE__,#x,error.c_str());std::exit(1);}}while(0)
uint32_t get(const std::vector<uint8_t>&b,size_t p){return uint32_t(b[p])|uint32_t(b[p+1])<<8|uint32_t(b[p+2])<<16|uint32_t(b[p+3])<<24;}
void put(std::vector<uint8_t>&b,size_t p,uint32_t v){for(unsigned i=0;i<4;++i)b[p+i]=uint8_t(v>>(8*i));}
void fix(std::vector<uint8_t>&b){put(b,12,uint32_t(b.size()));put(b,16,encore::crc32(b.data()+24,b.size()-24));}
void run_names(NewGameSetup&m){for(unsigned i=0;i<6;++i){CHECK(m.field_index()==i);CHECK(m.step(.1,{0,0,false,false,false,true},error));CHECK(m.step(.1,{0,0,true},error));CHECK(!m.name().empty());CHECK(m.step(.1,{0,0,false,false,false,false,true},error));}CHECK(m.phase()==NamingPhase::Settings);}
}
int main(int argc,char**argv){
 CHECK(argc==3);std::vector<uint8_t>bytes;CHECK(encore::read_file(argv[1],bytes,1024*1024,error));CHECK(get(bytes,8)==3&&get(bytes,20)==2);CHECK(bytes.size()>116);const size_t tail=bytes.size()-116;
 NewGameSetupData data;StartupSettingsData settings;CHECK(data.load(bytes.data(),bytes.size(),error));CHECK(settings.load_file(argv[2],error));CHECK(data.presentation.source_width==320&&data.presentation.source_height==180);CHECK(data.presentation.box==0&&data.presentation.cursor==1&&data.presentation.actor==2&&data.presentation.shadow==3);CHECK(data.presentation.keyboard==std::vector<uint32_t>({4,5}));CHECK(data.error_duration==2);
 NewGameSetup menu;CHECK(menu.open(data,settings,error));CHECK(menu.step(.1,{0,0,true},error));const auto original=menu.take_sounds();CHECK(original.size()==1&&original[0]==data.sounds[1]);const auto name=menu.name();
 // One executable consumes a different valid role table; same input/name uses
 // the changed binary sound binding without C++ replacement or a fixture VM.
 auto changed=bytes;put(changed,tail+76,1);put(changed,tail+80,0);fix(changed);NewGameSetupData alternate;CHECK(alternate.load(changed.data(),changed.size(),error));CHECK(menu.open(alternate,settings,error));CHECK(menu.step(.1,{0,0,true},error));const auto other=menu.take_sounds();CHECK(menu.name()==name&&other.size()==1&&other[0]==data.sounds[0]&&other!=original);
 for(const auto*d:{&data,&alternate}){CHECK(menu.open(*d,settings,error));CHECK(menu.step(.1,{0,0,false,false,true},error));CHECK(menu.panel()==1);CHECK(menu.step(.1,{0,0,false,false,true},error));CHECK(menu.panel()==0);run_names(menu);}
 auto rejected=[&](std::vector<uint8_t> b){fix(b);const auto old=data.presentation.cursor;CHECK(!data.load(b.data(),b.size(),error));CHECK(data.valid()&&data.presentation.cursor==old&&data.fields.size()==6);};
 for(size_t offset:{size_t(0),size_t(20),size_t(92)}){auto bad=bytes;put(bad,tail+offset,99);rejected(std::move(bad));}
 for(const auto offsets:{std::array<size_t,2>{{28,32}},std::array<size_t,2>{{20,24}},std::array<size_t,2>{{48,52}},std::array<size_t,2>{{76,80}},std::array<size_t,2>{{92,96}}}){auto bad=bytes;put(bad,tail+offsets[1],get(bad,tail+offsets[0]));rejected(std::move(bad));}
 auto bad=bytes;put(bad,tail+4,0);rejected(bad);bad=bytes;put(bad,tail+4,1);rejected(bad);bad=bytes;put(bad,tail+88,99);rejected(bad);bad=bytes;put(bad,tail+100,0x7fc00000);rejected(bad);
 bad=bytes;put(bad,8,2);rejected(bad);bad=bytes;put(bad,20,99);rejected(bad);bad=bytes;bad.pop_back();rejected(bad);
 bad=bytes;const auto&path=data.resources[data.presentation.cursor].path;auto it=std::search(bad.begin()+24,bad.end(),path.begin(),path.end());CHECK(it!=bad.end());*it='/';rejected(bad);
 bad=bytes;bad.back()^=1;CHECK(!data.load(bad.data(),bad.size(),error));CHECK(data.valid());CHECK(!data.load(bytes.data(),23,error));
 std::printf("Checked naming presentation loader, rollback, same-executable role change and six-field settings entry: %u checks\n",checks);
}
