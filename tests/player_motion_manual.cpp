#include "encore/crc32.hpp"
#include "encore/player_motion.hpp"
#include "manual_require.hpp"
#include <cstring>
// Explicit manual entry only; caller supplies actual admitted dependencies.
// This source is compiled but never run by the automatic development path.
void player_motion_manual_formats(
    const std::vector<uint8_t> &pack,
    const encore::upstream::PlayerInitializationData &init,
    const encore::upstream::PlayerReadyData &ready) {
  using encore::upstream::PlayerMotionData;
  std::string error;
  PlayerMotionData live;
  MANUAL_REQUIRE(live.load(pack.data(), pack.size(), init, ready, error));
  auto original = live.ir_sha256();
  MANUAL_REQUIRE(!live.load(nullptr, pack.size(), init, ready, error));
  MANUAL_REQUIRE(live.valid() && live.ir_sha256() == original);
  for (size_t at : {size_t(8), size_t(24), size_t(28), size_t(32), size_t(36),
                    size_t(40), size_t(60), size_t(124)}) {
    auto bad = pack;
    bad[at] ^= 0x40;
    MANUAL_REQUIRE(!live.load(bad.data(), bad.size(), init, ready, error));
    MANUAL_REQUIRE(live.ir_sha256() == original);
  }
  auto bad = pack;
  bad[128] ^= 1;
  auto crc = encore::crc32(bad.data() + 128, bad.size() - 128);
  for (unsigned i = 0; i < 4; ++i)
    bad[20 + i] = uint8_t(crc >> (8 * i));
  MANUAL_REQUIRE(!live.load(bad.data(), bad.size(), init, ready, error));
  for (size_t n : {size_t(0), size_t(127), pack.size() - 1})
    MANUAL_REQUIRE(!live.load(pack.data(), n, init, ready, error));
  // Format/cap2's business symbol and source-signal sections are parsed,
  // never discarded. Malformed cases reach the real existing reader.
  if (live.business_bindings()) {
    auto word = [&](size_t at) { return uint32_t(pack[at]) |
      uint32_t(pack[at+1])<<8 | uint32_t(pack[at+2])<<16 | uint32_t(pack[at+3])<<24; };
    size_t at=192;
    auto text=[&]() { auto n=word(at); at+=4+n; };
    auto strings=[&]() { auto n=word(at);at+=4;while(n--)text(); };
    auto sources=word(at);at+=4;while(sources--){text();at+=32;}
    strings();strings();strings();
    auto states=word(at);at+=4+states*8;
    auto numbers=word(at);at+=4+numbers*8;
    at+=8;auto modes=word(at);at+=4+modes*40;
    for(unsigned n=0;n<3;++n)text();
    auto count_at=at;strings();auto signal_at=at;auto count=word(at);at+=4;
    MANUAL_REQUIRE(count && live.signals().size()==count);
    text();auto args_at=at;
    auto rejected=[&](size_t position,uint32_t value) {
      auto malformed=pack;
      for(unsigned n=0;n<4;++n)malformed[position+n]=uint8_t(value>>(8*n));
      auto checksum=encore::crc32(malformed.data()+128,malformed.size()-128);
      for(unsigned n=0;n<4;++n)malformed[20+n]=uint8_t(checksum>>(8*n));
      MANUAL_REQUIRE(!live.load(malformed.data(),malformed.size(),init,ready,error));
      MANUAL_REQUIRE(live.ir_sha256()==original);
    };
    rejected(count_at,0);rejected(count_at,0x1000);
    rejected(signal_at,0);rejected(signal_at,65);rejected(args_at,2);
    at=signal_at+4;for(uint32_t n=0;n<count;++n){text();at+=4;}
    for(unsigned n=0;n<8;++n)text();
    auto loop_count_at=at;auto loops=word(at);at+=4;
    for(uint32_t n=0;n<loops;++n){text();text();}
    auto flash_count_at=at;strings();auto mask_count_at=at;auto masks=word(at);at+=4;
    MANUAL_REQUIRE(masks && live.lifecycle().collision_masks.size()==masks);
    rejected(loop_count_at,0);rejected(loop_count_at,65);
    rejected(flash_count_at,0);rejected(flash_count_at,65);
    rejected(mask_count_at,0);rejected(mask_count_at,33);rejected(at,32);
    if(masks>1)rejected(at+4,word(at));
  }
  encore::upstream::PlayerMotionRuntime runtime;
  MANUAL_REQUIRE(!runtime.healthy());
  MANUAL_REQUIRE(!runtime.native_callback("unknown", error));
  MANUAL_REQUIRE(!runtime.pause(false,true,true,error));
  MANUAL_REQUIRE(!runtime.unpause(true,error));
  MANUAL_REQUIRE(!runtime.collisions(true,error));
  MANUAL_REQUIRE(!runtime.direction_and_input({},error));
  MANUAL_REQUIRE(!runtime.exit_camera(error));
}
