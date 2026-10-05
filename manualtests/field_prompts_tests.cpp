#include "encore/field_prompts.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
#include <cstring>
using namespace encore::upstream;
static void put(std::vector<uint8_t>&b,size_t p,uint32_t v){for(unsigned i=0;i<4;++i)b[p+i]=uint8_t(v>>(8*i));}
static void repair(std::vector<uint8_t>&b){uint32_t c=~0u;for(size_t i=80;i<b.size();++i){c^=b[i];for(unsigned j=0;j<8;++j)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}put(b,16,~c);}
// Manual invocation only; never registered in routine CI or resource generation.
int main(){std::ifstream f("romfs/data/podunk-prompts.encfieldprompt",std::ios::binary);std::vector<uint8_t>b((std::istreambuf_iterator<char>(f)),{});assert(!b.empty());FieldPromptData d;std::string error;assert(d.load(b.data(),b.size(),error));assert(d.records().size()==146);auto original=d.records().front().id;
 for(size_t n:{size_t(0),size_t(79),size_t(80),b.size()-1})assert(!d.load(b.data(),n,error));assert(d.valid()&&d.records().front().id==original);
 for(size_t off:{size_t(8),size_t(20),size_t(76)}){auto bad=b;put(bad,off,0xffffffff);assert(!d.load(bad.data(),bad.size(),error));}
 {auto bad=b;bad.back()^=1;assert(!d.load(bad.data(),bad.size(),error));}
 {auto bad=b;bad.push_back(0);put(bad,12,uint32_t(bad.size()));repair(bad);assert(!d.load(bad.data(),bad.size(),error));}
 unsigned shows=0,hides=0,signals=0;FieldPromptRuntime runtime;FieldPromptObservation observation{0,false,{1,1}};FieldPromptHost host;host.observe=[&](uint32_t,auto&out,auto&){out=observation;return true;};host.key_name=[](auto,std::string&out,auto&){out="A";return true;};host.connect=[](auto,auto,auto,auto,auto&){return true;};host.publish=[](auto,const auto&,auto&){return true;};host.visibility=[&](auto,bool visible,auto&){if(visible)++shows;else ++hides;return true;};host.hide_signal=[&](auto,auto&){++signals;return true;};assert(runtime.initialize(d,host,error));assert(runtime.create(original));assert(runtime.ready(original));assert(!runtime.instance(original)->process&&!runtime.instance(original)->visible());assert(runtime.nearby(original,123,true));assert(runtime.idle_frame(original,1));assert(runtime.instance(original)->visible());assert(runtime.press(original));assert(runtime.idle_frame(original,1));assert(!runtime.instance(original)->pressing);assert(signals&&shows&&hides);assert(!runtime.ready(0));
}
