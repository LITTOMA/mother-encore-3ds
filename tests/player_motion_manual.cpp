#include "encore/crc32.hpp"
#include "encore/player_motion.hpp"
#include <cassert>
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
  assert(live.load(pack.data(), pack.size(), init, ready, error));
  auto original = live.ir_sha256();
  assert(!live.load(nullptr, pack.size(), init, ready, error));
  assert(live.valid() && live.ir_sha256() == original);
  for (size_t at : {size_t(8), size_t(24), size_t(28), size_t(32), size_t(36),
                    size_t(40), size_t(60), size_t(124)}) {
    auto bad = pack;
    bad[at] ^= 0x40;
    assert(!live.load(bad.data(), bad.size(), init, ready, error));
    assert(live.ir_sha256() == original);
  }
  auto bad = pack;
  bad[128] ^= 1;
  auto crc = encore::crc32(bad.data() + 128, bad.size() - 128);
  for (unsigned i = 0; i < 4; ++i)
    bad[20 + i] = uint8_t(crc >> (8 * i));
  assert(!live.load(bad.data(), bad.size(), init, ready, error));
  for (size_t n : {size_t(0), size_t(127), pack.size() - 1})
    assert(!live.load(pack.data(), n, init, ready, error));
  encore::upstream::PlayerMotionRuntime runtime;
  assert(!runtime.healthy());
  assert(!runtime.native_callback("unknown", error));
}
