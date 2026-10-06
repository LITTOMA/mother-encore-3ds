#include "encore/field_battle_bg_resources.hpp"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iterator>
using namespace encore::upstream;
static uint32_t word(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
static void store(std::vector<uint8_t>&b,size_t p,uint32_t v){for(unsigned i=0;i<4;++i)b[p+i]=uint8_t(v>>(8*i));}
static void crc(std::vector<uint8_t>&b){uint32_t c=~0u;for(size_t p=128;p<b.size();++p){c^=b[p];for(unsigned i=0;i<8;++i)c=(c>>1)^((c&1)?0xedb88320u:0u);}store(b,20,~c);}
int main(int argc,char**argv){assert(argc==2);std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t>b((std::istreambuf_iterator<char>(f)),{});assert(b.size()>128);FieldIdentity id;id.scene_id=word(b.data()+36);std::copy_n(b.data()+40,20,id.upstream_commit.begin());std::copy_n(b.data()+60,32,id.source_sha256.begin());FieldBattleBgData d;std::string e;assert(d.load(b.data(),b.size(),id,e));assert(d.resources().size()==50);for(size_t at:{size_t(8),size_t(24),size_t(28),size_t(32),size_t(124)}){auto bad=b;store(bad,at,999);assert(!d.load(bad.data(),bad.size(),id,e));}auto bad=b;bad.push_back(0);store(bad,16,uint32_t(bad.size()));crc(bad);assert(!d.load(bad.data(),bad.size(),id,e));bad=b;std::string classname="PanelContainer";auto p=std::search(bad.begin()+128,bad.end(),classname.begin(),classname.end());assert(p!=bad.end());*p='X';crc(bad);assert(!d.load(bad.data(),bad.size(),id,e));for(size_t n=0;n<128;++n)assert(!d.load(b.data(),n,id,e));assert(d.valid());}
