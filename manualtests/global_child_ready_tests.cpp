#include "../platform/ctr/podunk_global_children.hpp"
#include "../tests/manual_require.hpp"
#include "encore/crc32.hpp"
#include "encore/global_child_ready.hpp"
#include <cmath>
#include <cstring>
#include <limits>
namespace encore::upstream {
namespace {
uint32_t word(const std::vector<uint8_t> &b, size_t at) {
  uint32_t x = 0;
  for (size_t i = 0; i < 4; ++i)
    x |= uint32_t(b.at(at + i)) << (8 * i);
  return x;
}
void put(std::vector<uint8_t> &b, size_t at, uint32_t x) {
  for (size_t i = 0; i < 4; ++i)
    b.at(at + i) = uint8_t(x >> (8 * i));
}
void seal(std::vector<uint8_t> &b) {
  put(b, 20, crc32(b.data() + 128, b.size() - 128));
}
} // namespace
// Explicit manual entry only; no constructors/Ready are substituted by
// fixtures.
void global_child_format_manual(const std::vector<uint8_t> &pack,
                                const FieldGlobalConstructorData &constructor) {
  std::string error;
  GlobalChildReadyData data;
  MANUAL_REQUIRE(data.load(pack.data(), pack.size(), constructor, error));
  MANUAL_REQUIRE(data.nodes().size() == 2);
  const auto ir = data.ir_sha256();
  auto reject = [&](const std::vector<uint8_t> &bad) {
    MANUAL_REQUIRE(!data.load(bad.data(), bad.size(), constructor, error));
    MANUAL_REQUIRE(data.valid() && data.ir_sha256() == ir && !error.empty());
  };
  for (size_t at : {size_t(8), size_t(12), size_t(24), size_t(28), size_t(32),
                    size_t(36)}) {
    auto bad = pack;
    put(bad, at, word(bad, at) + 1);
    reject(bad);
  }
  for (size_t at : {size_t(40), size_t(60), size_t(124), size_t(128)}) {
    auto bad = pack;
    bad.at(at) ^= 1;
    seal(bad);
    reject(bad);
  }
  auto bad = pack;
  bad.pop_back();
  reject(bad);
  bad = pack;
  bad.push_back(0);
  put(bad, 16, uint32_t(bad.size()));
  seal(bad);
  reject(bad);
  bad = pack;
  bad.at(196) = 0xff;
  seal(bad);
  reject(bad);
  bad = pack;
  put(bad, bad.size() - 16, word(bad, bad.size() - 12));
  seal(bad);
  reject(bad); // visible == hidden
  bad = pack;
  double nan = std::numeric_limits<double>::quiet_NaN();
  std::memcpy(bad.data() + bad.size() - 64, &nan, 8);
  seal(bad);
  reject(bad);
  FieldGlobalConstructorData absent;
  MANUAL_REQUIRE(!data.load(pack.data(), pack.size(), absent, error));
  MANUAL_REQUIRE(!data.load(nullptr, pack.size(), constructor, error));
}
// Caller constructs the real source tree and routes actual ReadyScript before
// invoking this explicit case. It never directly claims native Ready success.
void global_child_runtime_negative_manual(GlobalChildReadyRuntime &runtime,
                                          FieldObjectId actual_slowmo,
                                          FieldObjectId actual_mouse) {
  std::string error;
  const auto start = runtime.slowmo().start;
  MANUAL_REQUIRE(!runtime.start_slowmo(actual_mouse, 1, 1, false, error));
  MANUAL_REQUIRE(!runtime.start_slowmo(
      actual_slowmo, std::numeric_limits<double>::infinity(), 1, false, error));
  MANUAL_REQUIRE(!runtime.start_slowmo(actual_slowmo, 1,
                                       std::numeric_limits<double>::quiet_NaN(),
                                       false, error));
  MANUAL_REQUIRE(!runtime.input(actual_slowmo, error));
  MANUAL_REQUIRE(!runtime.set_active(actual_slowmo, true, error));
  MANUAL_REQUIRE(runtime.slowmo().start == start && !runtime.poisoned());
}
void global_child_native_uninitialized_manual() {
  encore::ctr::PodunkGlobalClockInput actual;
  std::string error;
  double scalar = 0;
  uint64_t now = 0;
  uint32_t mode = 0;
  Vec2 speed{};
  MANUAL_REQUIRE(!actual.time_scale(scalar, error));
  MANUAL_REQUIRE(!actual.set_time_scale(1, error));
  MANUAL_REQUIRE(!actual.ticks_msec(now, error));
  MANUAL_REQUIRE(!actual.idle_delta(scalar, error));
  MANUAL_REQUIRE(!actual.mouse_mode(mode, error));
  MANUAL_REQUIRE(!actual.mouse_speed(speed, error));
  MANUAL_REQUIRE(!actual.set_mouse_mode(0, error));
}
} // namespace encore::upstream
