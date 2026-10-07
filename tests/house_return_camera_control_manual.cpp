// Manual source cases only. Not run as part of this slice. Caller supplies the
// actual checked full House Tree/resource and independent fresh native fixtures.
#include "../platform/ctr/house_return_camera_control.hpp"
#include "encore/crc32.hpp"
#include <cassert>
namespace encore::ctr::manual {
namespace {
void put(std::vector<uint8_t>&b,size_t at,uint32_t x){for(size_t i=0;i<4;++i)b.at(at+i)=uint8_t(x>>(8*i));}
void checksum(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
}
void house_room_shaker_parser(const upstream::FieldNodeTreeData&actual_house,
 const std::array<uint8_t,32>&actual_tree_ir,const std::vector<uint8_t>&pack){
 HouseReturnCameraControlData data;std::string error;
 assert(data.load(pack.data(),pack.size(),actual_house,actual_tree_ir,error));
 const auto prior=data.ir_sha256();auto bad=pack;
 // Format, capability and rule versions are independent admission contracts.
 for(const auto offset:{8u,24u,28u}){bad=pack;put(bad,offset,0xffffffffu);
  assert(!data.load(bad.data(),bad.size(),actual_house,actual_tree_ir,error));assert(data.valid()&&data.ir_sha256()==prior);}
 bad=pack;bad.back()^=1;assert(!data.load(bad.data(),bad.size(),actual_house,actual_tree_ir,error));
 bad=pack;bad.push_back(0);put(bad,12,uint32_t(bad.size()));checksum(bad);
 assert(!data.load(bad.data(),bad.size(),actual_house,actual_tree_ir,error));
 // First source record follows pin/scene/source/tree/IR proofs. Recalculate CRC
 // so failure must come from the exact full Tree receiver/Ready check.
 bad=pack;put(bad,152,0);checksum(bad);assert(!data.load(bad.data(),bad.size(),actual_house,actual_tree_ir,error));
 bad=pack;put(bad,156,0);checksum(bad);assert(!data.load(bad.data(),bad.size(),actual_house,actual_tree_ir,error));
 auto wrong=actual_tree_ir;wrong[0]^=1;assert(!data.load(pack.data(),pack.size(),actual_house,wrong,error));
 assert(data.valid()&&data.ir_sha256()==prior);
}
void house_room_shaker_unmapped_receivers(HouseReturnCameraControl&fresh_actual,
 upstream::FieldObjectId actual_shaker,upstream::FieldObjectId another_actual_node){
 std::string error;
 assert(!fresh_actual.deferred({another_actual_node,upstream::FieldDeferredKind::Call,"start_shake",0,{}},error));
 assert(!fresh_actual.deferred({actual_shaker,upstream::FieldDeferredKind::Call,"delayed_start",0,{std::string("unsupported")}},error));
 assert(!fresh_actual.deferred({actual_shaker,upstream::FieldDeferredKind::Call,"_on_Timer_timeout",0,{}},error));
 // Remaining actual-native fixtures to run during integration:
 // - Ready replay or audio Resource from another stream/Tree rejects.
 // - game_over getter unavailable rejects; battle=true short circuits it.
 // - enabled rumble with missing Input rejects before interval RNG advances.
 // - CurrentCamera pointing to unknown native/legacy value camera rejects.
 // - stopped Timer start consumes one interval only after Camera/audio/joy;
 //   running Timer start is no-op; stop preserves all delayed-start waiters.
 // - repeating Timer rearms using old wait before callback sets next wait.
 // - delayed_start resumes only its actual Bus/SceneTreeTimer frame, including
 //   two concurrent waits and process_pause=true on the one retained list.
 // - callback/receiver deletion releases source wait states, never list timers.
}
}
