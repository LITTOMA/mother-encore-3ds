#include "encore/battle_data.hpp"
#include "encore/content.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace encore::upstream;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static uint32_t get(const std::vector<uint8_t>&b,size_t o){return uint32_t(b[o])|(uint32_t(b[o+1])<<8)|(uint32_t(b[o+2])<<16)|(uint32_t(b[o+3])<<24);}
static void set(std::vector<uint8_t>&b,size_t o,uint32_t n){for(unsigned i=0;i<4;++i)b[o+i]=uint8_t(n>>(i*8));}
static void fix(std::vector<uint8_t>&b){set(b,16,0);uint32_t c=~0u;for(auto v:b){c^=v;for(unsigned i=0;i<8;++i)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}set(b,16,~c);}
int main(int argc,char**argv){if(argc!=2)return 2;std::vector<uint8_t>b;std::string e;CHECK(encore::read_file(argv[1],b,1024*1024,e));BattleData data;CHECK(data.load(b.data(),b.size(),e));auto original=data.view();CHECK(original.participant(0).hp==62&&original.participant(0).pp==26&&original.participant(0).defense==12);CHECK(original.participant(1).hp==30);CHECK(original.count(BattleSection::Menus)==3);CHECK(original.parameter(BattleParameter::SceneDuration).x==1.9f);
 for(size_t n=0;n<b.size();++n)CHECK(!data.load(b.data(),n,e));
 CHECK(data.view().participant(0).hp==62);
 for(auto edit:std::vector<std::pair<size_t,uint32_t>>{{8,99},{20,13},{24,0},{28,7},{52,1},{64+4,0},{64+16+8,10000},{64+3*16+12,999}}){auto bad=b;set(bad,edit.first,edit.second);fix(bad);CHECK(!data.load(bad.data(),bad.size(),e));}
 const auto layout=get(b,64+2*16+4),key=get(b,64+4*16+4),parameter=get(b,64+5*16+4),meta=get(b,64+8*16+4),glyph=get(b,64+11*16+4);
 for(auto edit:std::vector<std::pair<size_t,uint32_t>>{{layout+4,99},{layout+8,99},{layout+12,999},{layout+40,0x7fc00000},{key,0x7fc00000},{parameter,999},{meta+12,999},{glyph+4,999},{glyph+16,99999}}){auto bad=b;set(bad,edit.first,edit.second);fix(bad);CHECK(!data.load(bad.data(),bad.size(),e));}
 auto copy=b;CHECK(data.load(copy.data(),copy.size(),e));std::fill(copy.begin(),copy.end(),0);CHECK(data.view().participant(1).hp==30);
 CHECK(!BattleView{}.valid());CHECK(BattleView{}.count(BattleSection::Layouts)==0);CHECK(BattleView{}.string(0).empty());
 std::printf("BattleData: %u checks; immutable external content, bad spans/CRC/schema/indices and rollback\n",checks);
}
