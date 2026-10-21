#include "encore/coins.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
using namespace encore::upstream;
namespace {
size_t checks=0;std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"Coins "<<__LINE__<<": "<<#x<<" ("<<error<<")\n";std::exit(1);}}while(0)
uint32_t word(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned n=0;n<4;++n)b[at+n]=uint8_t(v>>(8*n));}
void seal(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
}
int main(int argc,char**argv){
 CHECK(argc==2);std::vector<uint8_t>blob;CHECK(encore::read_file(argv[1],blob,128*1024,error));CoinsData data;CHECK(data.load(blob.data(),blob.size(),error));CHECK(data.valid());CHECK(data.directions().size()==data.info().direction_count);CHECK(data.atlas().size()==data.info().sparkle_count);CHECK(data.clips().size()==4);CHECK(data.clip(data.info().flipping)&&data.clip(data.info().grounded)&&data.clip(data.info().collect)&&data.clip(data.info().reset));CHECK(!data.clip(0)&&!data.clip(UINT32_MAX));
 const auto*retained=&data.clips()[0];const auto*directions=data.directions().data();const auto old_cash=data.info().cash;
 const auto rejected=[&](std::vector<uint8_t>b,bool checksum=true){if(checksum&&b.size()>=32)seal(b);CHECK(!data.load(b.data(),b.size(),error));CHECK(!error.empty()&&data.valid());CHECK(&data.clips()[0]==retained&&data.directions().data()==directions&&data.info().cash==old_cash);};
 for(size_t n=0;n<blob.size();++n)CHECK(!data.load(blob.data(),n,error));CHECK(!data.load(nullptr,blob.size(),error));
 for(size_t at:{size_t(0),size_t(8),size_t(12),size_t(16),size_t(20),size_t(24),size_t(28),size_t(32),size_t(52),size_t(56),size_t(60)}){auto b=blob;put(b,at,word(b,at)^0x80000000u);rejected(std::move(b),at!=16);}
 for(unsigned section=0;section<9;++section){const auto dir=64+section*16;for(unsigned field=0;field<4;++field){auto b=blob;put(b,dir+field*4,UINT32_MAX);rejected(std::move(b));}auto b=blob;put(b,dir+8,0);rejected(std::move(b));}
 auto tail=blob;tail.push_back(0);put(tail,12,uint32_t(tail.size()));rejected(std::move(tail));
 const auto section=[&](unsigned n){return word(blob,64+n*16+4);};
 for(auto offset:{size_t(0),size_t(16),size_t(24),size_t(40),size_t(64)}){auto b=blob;for(unsigned n=0;n<8;++n)b[section(0)+offset+n]=0;rejected(std::move(b));}
 for(unsigned n=0;n<9;++n){auto b=blob;put(b,section(0)+n*8+4,0x7ff80000u);rejected(std::move(b));}
 for(unsigned n=0;n<10;++n){auto b=blob;put(b,section(0)+72+n*4,0x7fc00000u);rejected(std::move(b));}
 for(unsigned n=0;n<24;++n){auto b=blob;put(b,section(1)+n*4,UINT32_MAX);rejected(std::move(b));}
 for(unsigned n:{0u,1u,2u,3u,4u,5u}){auto b=blob;put(b,section(2)+4*n,UINT32_MAX);rejected(std::move(b));}
 auto duplicate=blob;put(duplicate,section(2)+24,word(blob,section(2)));rejected(std::move(duplicate));
 for(unsigned n:{0u,1u,2u,3u,4u,5u}){auto b=blob;put(b,section(3)+4*n,UINT32_MAX);rejected(std::move(b));}
 auto wrong_value=blob;put(wrong_value,section(3)+8,0);put(wrong_value,section(3)+12,0x3fe00000u);rejected(std::move(wrong_value));
 auto wrong_time=blob;put(wrong_time,section(3)+24+4,word(blob,section(3)+4));rejected(std::move(wrong_time));
 for(unsigned n=0;n<2;++n){auto b=blob;put(b,section(4)+n*4,0x7fc00000u);rejected(std::move(b));}auto zero=blob;put(zero,section(4),0);put(zero,section(4)+4,0);rejected(std::move(zero));
 for(unsigned n=0;n<6;++n){auto b=blob;put(b,section(5)+n*4,UINT32_MAX);rejected(std::move(b));}
 for(unsigned n:{0u,1u,2u,3u,4u,5u,6u,7u,9u,11u}){auto b=blob;put(b,section(6)+n*4,UINT32_MAX);rejected(std::move(b));}
 for(unsigned n=0;n<6;++n){auto b=blob;put(b,section(7)+n*4,UINT32_MAX);rejected(std::move(b));}
 auto pool=blob;pool[section(8)]='a';rejected(std::move(pool));pool=blob;pool.back()='a';rejected(std::move(pool));pool=blob;pool[section(8)+1]='\n';rejected(std::move(pool));
 CHECK(!data.load_file("build/coins-intentionally-absent.encoin",error));CHECK(&data.clips()[0]==retained);CHECK(data.load_file(argv[1],error));std::cout<<checks<<" Coin resource checks passed\n";
}
