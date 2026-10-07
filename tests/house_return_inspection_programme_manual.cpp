// Manual, deliberately unregistered and unrun. Link the existing shared core
// plus the new checked resource consumer, then run from the repository root.
#include "../include/encore/house_return_inspection_programme.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
#include <vector>
using namespace encore::upstream;
namespace {
std::vector<uint8_t>bytes(const char*path){std::ifstream f(path,std::ios::binary);assert(f);return {std::istreambuf_iterator<char>(f),{}};}
void put(std::vector<uint8_t>&b,size_t at,uint32_t n){for(unsigned i=0;i<4;++i)b.at(at+i)=uint8_t(n>>(i*8));}
void crc(std::vector<uint8_t>&b){put(b,52,0);uint32_t c=~0u;for(auto x:b){c^=x;for(unsigned i=0;i<8;++i)c=(c>>1)^((c&1)?0xedb88320u:0u);}put(b,52,~c);}
}
int main(){
  RoomData original;HouseData text;DrawerProgramData drawer;FieldInteractData interact;std::string e;
  assert(original.load_file("romfs/data/opening.encroom",e));
  assert(text.load_file("romfs/data/opening.enchouse",e));
  assert(drawer.load_file("romfs/data/opening.encdrawer",e));
  assert(interact.load_file("romfs/data/house-return-interact.encdialog",e));
  auto actual=bytes("romfs/data/house-return-room.encroom");
  encore::upstream::HouseReturnInspectionProgrammes checked;
  assert(checked.load(actual.data(),actual.size(),original.view(),text.view(),drawer.view(),interact,e));
  auto retained=checked.view().bytes();const auto room=checked.view();
  uint32_t space=~0u,grant=~0u;
  for(uint32_t i=0;i<room.command_count();++i){
    if(room.command(i).opcode==47)space=i;
    if(room.command(i).opcode==48)grant=i;
  }
  assert(space!=~0u&&grant!=~0u);
  auto reject=[&](std::vector<uint8_t>candidate){crc(candidate);
    assert(!checked.load(candidate.data(),candidate.size(),original.view(),text.view(),drawer.view(),interact,e));
    assert(checked.view().bytes()==retained);
  };
  auto candidate=actual;put(candidate,32,8);put(candidate,36,9);reject(candidate); // old capability cannot admit47/48
  candidate=actual;put(candidate,32,9);reject(candidate); // unsupported rule revision
  candidate=actual;candidate[room.section_offset(RoomSection::Command)+size_t(space)*48]=49;reject(candidate); // unknown opcode
  candidate=actual;put(candidate,room.section_offset(RoomSection::Command)+size_t(space)*48+40,0);reject(candidate); // backward/wrong phrase
  candidate=actual;put(candidate,room.section_offset(RoomSection::Command)+size_t(grant)*48+8,0xffffffffu);reject(candidate); // missing typed template
  candidate=actual;put(candidate,room.section_offset(RoomSection::Command)+size_t(grant)*48+8,100);reject(candidate); // schema-valid but absent actual Drawer template
  candidate=actual;put(candidate,room.section_offset(RoomSection::Program),0xabcdefu);reject(candidate); // changed original stable prefix
  candidate=actual;candidate.pop_back();reject(candidate); // truncated complete Room
}
