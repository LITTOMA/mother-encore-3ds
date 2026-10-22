#include "encore/coin_scene.hpp"
#include "encore/crc32.hpp"
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
using namespace encore::upstream;
namespace {
size_t checks=0;std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"Coin scene "<<__LINE__<<": "<<#x<<" ("<<error<<")\n";std::exit(1);}}while(0)
std::vector<uint8_t>read(const char*p){std::ifstream in(p,std::ios::binary);CHECK(bool(in));return {std::istreambuf_iterator<char>(in),{}};}
uint32_t word(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void set(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned n=0;n<4;++n)b[at+n]=uint8_t(v>>(8*n));}
void crc(std::vector<uint8_t>&b){set(b,16,encore::crc32(b.data()+32,b.size()-32));}
}
int main(int argc,char**argv){
 CHECK(argc==5);const auto raw=read(argv[1]);CoinSceneData d;CHECK(d.load(raw.data(),raw.size(),error)&&d.valid()&&d.generation()==1);CHECK(d.bindings().size()==49);const auto*first=&d.bindings().front();const auto*borrowed=first->path.data();CHECK(d.owner(first->owner)==first&&!d.owner(0));
 CoinsData coins;BreakablesData breakables;CHECK(coins.load_file(argv[2],error));CHECK(breakables.load_file(argv[3],error));CHECK(d.compatible(breakables,coins,error));CHECK(d.scene()==breakables.scene());
 for(const auto&v:d.bindings()){const auto*owner=breakables.owner(v.owner);CHECK(owner&&v.path==owner->spawner&&v.owner_path==owner->path&&v.default_count==coins.info().default_coins);CHECK(v.position.x==owner->position.x+v.local.x&&v.position.y==owner->position.y+v.local.y);}
 auto reject=[&](const uint8_t*p,size_t count){CHECK(!d.load(p,count,error));CHECK(d.valid()&&d.generation()==1&&&d.bindings().front()==first&&first->path.data()==borrowed);};reject(nullptr,raw.size());for(size_t n=0;n<raw.size();++n)reject(raw.data(),n);
 const auto meta=word(raw,68),records=word(raw,84),pool=word(raw,100);
 std::vector<size_t>changes{0,8,12,20,24,28,32,52,56,60};for(unsigned n=0;n<3;++n)for(unsigned k=0;k<4;++k)changes.push_back(64+16*n+4*k);for(unsigned n=0;n<8;++n)changes.push_back(meta+4*n);for(unsigned n=0;n<16;++n)changes.push_back(records+4*n);changes.push_back(pool);
 for(auto at:changes){auto bad=raw;set(bad,at,UINT32_MAX);crc(bad);reject(bad.data(),bad.size());}
 for(auto pair:std::vector<std::pair<size_t,uint32_t>>{{records+64,word(raw,records)},{records+68,word(raw,records+4)},{records+72,word(raw,records+8)},{records+12,0},{records+40,0},{records+16,0},{meta+12,2}}){auto bad=raw;set(bad,pair.first,pair.second);crc(bad);reject(bad.data(),bad.size());}
 auto bad=raw;bad.push_back(0);reject(bad.data(),bad.size());bad=raw;bad.back()^=1;reject(bad.data(),bad.size());CHECK(!d.load_file("missing-coin-scene-resource.enccf",error)&&d.generation()==1);
 CoinsData absent;BreakablesData missing;CHECK(!d.compatible(missing,coins,error));CHECK(!d.compatible(breakables,absent,error));CoinSceneData partial;CHECK(partial.load_file(argv[4],error));CHECK(!partial.compatible(breakables,coins,error));
 bad=raw;set(bad,records+4,breakables.owners().front().id);crc(bad);CoinSceneData foreign;CHECK(foreign.load(bad.data(),bad.size(),error));CHECK(!foreign.compatible(breakables,coins,error));
 CHECK(d.load(raw.data(),raw.size(),error)&&d.generation()==2&&d.compatible(breakables,coins,error));std::cout<<checks<<" Coin scene data checks passed\n";return 0;
}
