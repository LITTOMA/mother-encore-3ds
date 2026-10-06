#include "encore/player_ready.hpp"
#include <cassert>
using namespace encore::upstream;
// Manual full-test runner may pass actual admitted 0056/005a data. Not
// registered as an automatic test and never executed during development.
void player_ready_format_manual(const PlayerInitializationData &init,
                                const std::vector<uint8_t> &pack) {
  std::string error;
  PlayerReadyData data;
  assert(data.load(pack.data(), pack.size(), init, error));
  const auto old = data.ir_sha256();
  auto damaged = pack;
  damaged[8] ^= 1;
  assert(!data.load(damaged.data(), damaged.size(), init, error));
  damaged = pack;
  damaged[32] ^= 1;
  assert(!data.load(damaged.data(), damaged.size(), init, error));
  damaged = pack;
  damaged[28] ^= 1;
  assert(!data.load(damaged.data(), damaged.size(), init, error));
  assert(data.valid() && data.ir_sha256() == old);
  damaged = pack;
  damaged.push_back(0);
  assert(!data.load(damaged.data(), damaged.size(), init, error));
  PlayerAnimationGraph graph;
  assert(!graph.initialize(data, {}, error));
  assert(!graph.travel("unknown source graph state", error));
  assert(!graph.process(-1, error));
  assert(!graph.set_blend("unknown source parameter", {}, error));
  PlayerInitializationBody absent;
  PlayerInitializationMember wrong;
  wrong.kind = 7;
  assert(!absent.assign_member("unbound source member", wrong, error));
  assert(!absent.bind_onready("unbound onready", 1, error));
}
void player_ready_graph_manual(const PlayerReadyData &data,
                               PlayerGraphHost actual_tracks) {
  std::string error;
  PlayerAnimationGraph graph;
  assert(graph.initialize(data, std::move(actual_tracks), error));
  assert(!graph.start("unknown source graph state", error));
  assert(graph.start(data.start(), error));
  assert(graph.set_active(true, error));
  assert(graph.process(0, error));
  assert(graph.playing() && graph.current() == data.start());
  for (const auto &parameter : data.blend_parameters())
    assert(graph.set_blend(parameter, data.direction(), error));
  assert(graph.process(1.0f / 60, error));
  assert(graph.stop(error));
  assert(graph.process(1.0f / 60, error));
  assert(!graph.playing());
}
