// Explicit manual parser checks. Not registered in routine builds or CI.
#include "encore/field_scene_signal_callbacks.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <iostream>
using namespace encore::upstream;
namespace {
uint32_t read32(const std::vector<uint8_t>&b,size_t at) {
  return uint32_t(b.at(at))|uint32_t(b.at(at+1))<<8|
         uint32_t(b.at(at+2))<<16|uint32_t(b.at(at+3))<<24;
}
void write32(std::vector<uint8_t>&b,size_t at,uint32_t v) {
  for(size_t i=0;i<4;++i)b.at(at+i)=uint8_t(v>>(8*i));
}
void checksum(std::vector<uint8_t>&b) {
  write32(b,28,uint32_t(b.size()-88));
  write32(b,32,encore::crc32(b.data()+88,b.size()-88));
}
bool rejected(const std::vector<uint8_t>&b,const FieldIdentity&id) {
  FieldSceneSignalCallbacksData owner;std::string e;
  return !owner.load(b.data(),b.size(),id,e)&&!owner.valid()&&!e.empty();
}
}
int main(int argc,char**argv) {
  if(argc!=2)return 2;
  std::ifstream f(argv[1],std::ios::binary);
  std::vector<uint8_t> original((std::istreambuf_iterator<char>(f)),{});
  if(original.size()<128)return 2;
  FieldIdentity id;id.scene_id=read32(original,24);
  std::copy_n(original.data()+36,20,id.upstream_commit.begin());
  std::copy_n(original.data()+56,32,id.source_sha256.begin());
  FieldSceneSignalCallbacksData valid;std::string e;
  if(!valid.load(original.data(),original.size(),id,e)||
     !valid.party_member_declaration()||valid.party_member_name().empty())return 1;
  auto require=[&](bool ok,const char*name){if(!ok){std::cerr<<name<<'\n';return false;}return true;};
  for(auto offset:{size_t(8),size_t(12),size_t(16),size_t(20)}) {
    auto bad=original;write32(bad,offset,read32(bad,offset)+1);
    if(!require(rejected(bad,id),"Unknown format/capability/rules/family accepted"))return 1;
  }
  auto legacy=original;write32(legacy,8,1);write32(legacy,12,1);
  if(!require(rejected(legacy,id),"Old payload accepted as new party binding"))return 1;
  size_t at=88;at+=4+read32(original,at);at+=4+read32(original,at);at+=32+8;
  const auto member_size=read32(original,at);at+=4;
  auto zero=original;write32(zero,at+member_size,0);checksum(zero);
  if(!require(rejected(zero,id),"Zero party declaration accepted"))return 1;
  auto utf8=original;utf8.at(at)=0xff;checksum(utf8);
  if(!require(rejected(utf8,id),"Invalid member UTF-8 accepted"))return 1;
  auto proof=original;std::fill_n(proof.begin()+at+member_size+4,32,0);checksum(proof);
  if(!require(rejected(proof,id),"Missing constructor proof accepted"))return 1;
  auto crc=original;crc.back()^=1;
  if(!require(rejected(crc,id),"Corrupt source payload accepted"))return 1;
  auto short_pack=original;short_pack.pop_back();
  if(!require(rejected(short_pack,id),"Truncated source pack accepted"))return 1;
  auto trailing=original;trailing.push_back(0);checksum(trailing);
  if(!require(rejected(trailing,id),"Unknown trailing payload accepted"))return 1;
  auto foreign=id;foreign.source_sha256[0]^=1;
  if(!require(rejected(original,foreign),"Foreign scene identity accepted"))return 1;
  std::cout<<"Manual scene signal parser checks passed\n";
}
