// Manual source only; not registered or executed by this resource slice.
#include "encore/house_inspection_restore.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cassert>
using namespace encore::upstream;
namespace {
void put(std::vector<uint8_t>&b,size_t at,uint32_t v){
  for(size_t i=0;i<4;++i)b[at+i]=uint8_t(v>>(8*i));
}
void crc(std::vector<uint8_t>&b){put(b,20,encore::crc32(b.data()+128,b.size()-128));}
}
void house_inspection_restore_parser_manual(
    const std::vector<uint8_t>&source,
    const HouseInspectionRestoreBindings&actual,
    RoomView actual_original_room,
    const std::vector<uint8_t>&actual_original_reentry){
  std::string e;HouseInspectionRestoreData positive;
  assert(positive.load(source.data(),source.size(),actual,e));
  assert(positive.valid()&&!positive.scene_admitted());
  assert(positive.restore().matches(actual.room,actual.house));
  auto rejects=[&](const std::vector<uint8_t>&b){
    HouseInspectionRestoreData d;
    assert(!d.load(b.data(),b.size(),actual,e));assert(!d.valid());
  };
  for(size_t n:{size_t(0),size_t(127),source.size()-1})
    rejects(std::vector<uint8_t>(source.begin(),source.begin()+n));
  for(size_t field:{size_t(8),size_t(12),size_t(24),size_t(28),size_t(32)}){
    auto b=source;put(b,field,0);rejects(b);
  }
  for(size_t field:{size_t(40),size_t(60)}){
    auto b=source;b[field]^=1;rejects(b);
  }
  constexpr uint8_t magic[]={'E','N','C','R','E','S','T','1'};
  const auto found=std::search(source.begin()+128,source.end(),magic,magic+8);
  assert(found!=source.end());const size_t inner=size_t(found-source.begin());
  // Unsupported embedded format/capability and a damaged inner CRC must fail
  // even after the outer CRC is repaired. The existing parser remains owner.
  for(size_t field:{size_t(8),size_t(20)}){
    auto b=source;put(b,inner+field,2);crc(b);rejects(b);
  }
  {auto b=source;b[inner+16]^=1;crc(b);rejects(b);}
  auto wrong=actual;wrong.room=actual_original_room;
  HouseInspectionRestoreData old_room;
  assert(!old_room.load(source.data(),source.size(),wrong,e));
  wrong=actual;wrong.room_ir_sha[0]^=1;HouseInspectionRestoreData room_ir;
  assert(!room_ir.load(source.data(),source.size(),wrong,e));
  wrong=actual;wrong.tree_ir_sha[0]^=1;HouseInspectionRestoreData tree_ir;
  assert(!tree_ir.load(source.data(),source.size(),wrong,e));
  wrong=actual;wrong.reentry_bytes=actual_original_reentry.data();
  wrong.reentry_size=actual_original_reentry.size();
  HouseInspectionRestoreData old_reentry;
  assert(!old_reentry.load(source.data(),source.size(),wrong,e));
  assert(!positive.load(source.data(),source.size(),wrong,e));
  // Failed admission leaves the previously published immutable Restore intact.
  assert(positive.valid()&&positive.matches(actual,e));
}
