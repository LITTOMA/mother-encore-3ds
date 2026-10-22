#include "encore/dropped_items.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cstdlib>
#include <iostream>

using namespace encore::upstream;
namespace {
size_t checks=0;
void check(bool ok,int line){++checks;if(!ok){std::cerr<<"Dropped data failure at "<<line<<"\n";std::exit(1);}}
#define CHECK(value) check(bool(value),__LINE__)
uint32_t word(const std::vector<uint8_t>&b,size_t at){return uint32_t(b.at(at))|uint32_t(b.at(at+1))<<8|uint32_t(b.at(at+2))<<16|uint32_t(b.at(at+3))<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t value){for(unsigned i=0;i<4;++i)b.at(at+i)=uint8_t(value>>(8*i));}
void checksum(std::vector<uint8_t>&b){put(b,12,uint32_t(b.size()));put(b,16,encore::crc32(b.data()+32,b.size()-32));}
}
int main(int argc,char**argv){
 CHECK(argc==4);std::string error;std::vector<uint8_t>base;
 CHECK(encore::read_file(argv[1],base,128*1024,error));DroppedItemsData data;
 CHECK(!data.valid());CHECK(!data.owner(1));CHECK(!data.item(1));
 CHECK(data.load(base.data(),base.size(),error));CHECK(error.empty());
 CHECK(data.valid()&&data.generation()==1&&data.owners().size()==7&&data.templates().size()==3);
 CHECK(data.info().capacity==16&&data.info().scene==4);
 CHECK(data.value(DroppedScalar::SparkleMaximum)==47&&data.value(DroppedScalar::SparkleFrames)==40);
 CHECK(data.shapes()[1][0]<0&&data.shapes()[1][1]>0);
 CHECK(data.clip(DroppedClipRole::Blink)&&data.clip(DroppedClipRole::Blink)->loop);
 BreakablesData breaks;CHECK(breaks.load_file(argv[3],error));CHECK(data.compatible(breaks,error));
 DroppedItemsData partial;CHECK(partial.load_file(argv[2],error));CHECK(!partial.compatible(breaks,error));
 CHECK(!data.compatible(BreakablesData{},error));
 const auto*owner=&data.owners()[0];const auto id=owner->id;const auto*item=data.item(owner->template_id);
 CHECK(item&&data.owner(id)==owner);
 const auto rejected=[&](const std::vector<uint8_t>&bytes){
  CHECK(!data.load(bytes.data(),bytes.size(),error));CHECK(!error.empty());
  CHECK(data.valid()&&data.generation()==1&&data.owner(id)==owner&&data.item(item->id)==item);
 };
 CHECK(!data.load(nullptr,base.size(),error));CHECK(data.owner(id)==owner);
 for(size_t size=0;size<base.size();++size){auto copy=base;copy.resize(size);rejected(copy);}
 const auto mutate=[&](size_t at,uint32_t value){auto copy=base;put(copy,at,value);checksum(copy);rejected(copy);};
 for(size_t at:{size_t(8),size_t(20)})for(uint32_t version:{0u,2u,UINT32_MAX})mutate(at,version);
 for(size_t at:{size_t(28),size_t(56),size_t(60)})mutate(at,1);
 mutate(52,7);mutate(24,0);mutate(24,65536);
 {auto copy=base;copy[0]^=1;rejected(copy);}
 {auto copy=base;copy[32]^=1;checksum(copy);rejected(copy);}
 {auto copy=base;copy[16]^=1;rejected(copy);}
 for(unsigned section=0;section<8;++section){
  const size_t at=64+section*16;mutate(at,0);mutate(at+4,0);mutate(at+4,UINT32_MAX);
  mutate(at+8,0);mutate(at+8,UINT32_MAX);mutate(at+12,word(base,at+12)+1);
 }
 const auto offset=[&](unsigned section){return word(base,64+16*section+4);};
 const auto meta=offset(0),policy=offset(1),templates=offset(2),owners=offset(3),shapes=offset(4),clips=offset(5),keys=offset(6),pool=offset(7);
 mutate(meta+36,0);mutate(meta+36,4097);mutate(meta+56,2);mutate(meta+72,4);mutate(meta+76,1);
 mutate(meta+4,2);mutate(meta+40,1u<<20);mutate(policy,0);mutate(policy,22);
 mutate(policy+4+4,0x7ff80000u); // binary64 NaN
 mutate(policy+4+4,0xbff00000u); // negative duration
 mutate(policy+12*uint32_t(DroppedScalar::CollectTransition)+8,0x3ff00000u); // unsupported transition 1
 mutate(templates+12,2);mutate(templates+8,0);mutate(templates+20,0);mutate(templates+28,1);
 mutate(templates+32,word(base,templates)); // duplicate stable template
 mutate(owners+40,word(base,owners));mutate(owners+20,0);mutate(owners+28,8);
 mutate(owners+28,1);mutate(owners+32,0x7fc00000u);mutate(owners+4,0);
 mutate(shapes,0);mutate(shapes+12,0);mutate(shapes+20,0);
 mutate(clips+4,3);mutate(clips+12,1);mutate(clips+16,0);mutate(clips+20,2);mutate(clips+24,0);
 mutate(keys,0x7fc00000u);mutate(keys+4,2);mutate(keys+8,2);
 {auto copy=base;copy[pool]=1;checksum(copy);rejected(copy);}
 {auto copy=base;copy.back()='x';checksum(copy);rejected(copy);}
 {auto copy=base;copy[pool+1]=':';checksum(copy);rejected(copy);}
 {auto copy=base;copy.push_back(0);checksum(copy);rejected(copy);}
 CHECK(data.load(base.data(),base.size(),error)&&data.generation()==2);
 CHECK(data.compatible(breaks,error));
 std::cout<<checks<<" Dropped data checks passed\n";
}
