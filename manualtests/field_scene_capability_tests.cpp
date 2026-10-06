#include "encore/field_scene_data.hpp"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <iostream>
using namespace encore::upstream;
// Explicit manual invocation only; reads the actual source resource, no fixture
// gameplay. Capability edits lie outside the payload CRC by format definition.
int main(int argc, char **argv) {
  if (argc != 2) return 2;
  std::ifstream f(argv[1], std::ios::binary);
  std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), {});
  if (b.size() < 128) return 2;
  FieldIdentity id;
  auto u32=[](const uint8_t*p){return uint32_t(p[0]) | uint32_t(p[1])<<8 |
      uint32_t(p[2])<<16 | uint32_t(p[3])<<24;};
  id.scene_id=u32(b.data()+36);
  std::copy_n(b.data()+40,20,id.upstream_commit.begin());
  std::copy_n(b.data()+60,32,id.source_sha256.begin());
  FieldSceneData d; std::string error;
  if (!d.load(b.data(),b.size(),id,error)) return 1;
  bool vending=false;
  for (uint32_t i=0;i<d.ready_count();++i) {
    const auto r=d.ready(i);
    if (d.string(r.node)=="Objects/VendingMachine/interact_dialog") return 1;
    if (r.role==FieldSceneRole::VendingMachine) vending=true;
  }
  if (!vending || d.scene_admitted()) return 1;
  for (uint8_t capability : {uint8_t(5),uint8_t(7)}) {
    auto bad=b;bad[32]=capability;bad[33]=bad[34]=bad[35]=0;
    FieldSceneData rejected;
    if (rejected.load(bad.data(),bad.size(),id,error)) return 1;
  }
  std::cout<<"Actual null script and independently gated Vending capability\n";
}
