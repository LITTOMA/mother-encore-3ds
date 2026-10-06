// Manual-only source parser and execution cases. No automatic registration.
#include "encore/field_door_npc.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
using namespace encore::upstream;
int main(int argc, char **argv) {
  assert(argc == 2);
  std::ifstream f(argv[1], std::ios::binary);
  std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), {});
  std::string e;
  FieldDoorNpcData data;
  assert(data.load(b.data(), b.size(), e));
  auto bad = b;
  bad[8] = 2;
  assert(!data.load(bad.data(), bad.size(), e));
  assert(data.valid());
  assert(!data.load(b.data(), b.size() - 1, e));
  bad = b;
  bad.back() ^= 1;
  assert(!data.load(bad.data(), bad.size(), e));
  FieldDoorNpcHost h;
  FieldDoorUi ui;
  std::vector<std::string> order;
  bool admitted = true, seen = false;
  uint64_t next = 1;
  h.admit_ready = [](const auto &, const auto &, std::string &) {
    return true;
  };
  h.body_is_current_player = [](uint32_t id, bool &o, std::string &) {
    o = id == 1;
    return true;
  };
  h.query_ui = [&](FieldDoorUi &o, std::string &) {
    o = ui;
    return true;
  };
  h.read_flag = [](std::string_view, bool &o, std::string &) {
    o = false;
    return true;
  };
  h.read_seen = [&](std::string_view, bool &o, std::string &) {
    o = seen;
    return true;
  };
  h.mark_seen = [&](std::string_view key, std::string &) {
    assert(!key.empty());
    seen = true;
    order.push_back("seen");
    return true;
  };
  h.node_path = [&](uint32_t id, std::string &o, std::string &) {
    assert(id == data.bindings()[0].id);
    o = "/root/manual/" + data.bindings()[0].node;
    return true;
  };
  h.admit_start = [&](const auto &, const auto &, std::string &) {
    return admitted;
  };
  h.turn_player = [&](uint32_t, const auto &turn, std::string &) {
    assert(turn == data.policy().turn);
    order.push_back("turn");
    return true;
  };
  h.pause_player = [&](const auto &pause, std::string &) {
    assert(pause == data.policy().pause);
    order.push_back("pause");
    return true;
  };
  h.set_cutscene = [&](bool value, std::string &) {
    ui.cutscene = value;
    order.push_back(value ? "cutscene_on" : "cutscene_off");
    return true;
  };
  h.black_bars = [&](bool value, std::string &) {
    assert(value);
    order.push_back("bars");
    return true;
  };
  h.play_knock = [&](const auto &binding, std::string &) {
    assert(binding.audio.path == data.bindings()[0].audio.path);
    order.push_back("knock");
    return true;
  };
  h.create_timer = [&](uint32_t, const auto &policy, uint64_t &out,
                       std::string &) {
    assert(policy.timer_seconds == data.policy().timer_seconds);
    assert(policy.process_pause && policy.strict_negative &&
           policy.global_fifo);
    out = next++;
    order.push_back("timer");
    return true;
  };
  h.admit_dialogue = [](const auto &, const auto &, std::string &) {
    return true;
  };
  h.open_room_and_unpause = [&](const auto &binding,
                                const FieldDoorSelection &s,
                                std::string_view completion, std::string &) {
    assert(binding.programme(s.dialog));
    assert(completion == data.policy().completion);
    order.push_back("room");
    return true;
  };
  FieldDoorNpcRuntime core;
  assert(core.initialize(data, h, e));
  const auto id = data.bindings()[0].id;
  assert(!core.ready(id + 1, e));
  assert(core.ready(id, e));
  assert(core.body_enter(id, 2, e));
  assert(!core.state(id)->processing);
  assert(core.body_enter(id, 1, e));
  ui.pause = true;
  assert(core.idle_process(id, true, e));
  assert(core.state(id)->processing && order.empty());
  ui.pause = false;
  admitted = false;
  assert(!core.idle_process(id, true, e));
  assert(order.empty() && !seen);
  admitted = true;
  assert(core.idle_process(id, true, e));
  assert((order == std::vector<std::string>{"turn", "pause", "cutscene_on",
                                            "bars", "knock", "timer"}));
  assert(!seen && !core.state(id)->processing && core.waiters().size() == 1);
  assert(!core.timer_timeout(2, e));
  assert(core.exit_tree(id, e));
  assert(core.timer_timeout(1, e));
  assert(seen && order[6] == "seen" && order[7] == "room" &&
         order[8] == "cutscene_off");
  assert(core.enter_tree(id, e));
  assert(core.body_enter(id, 1, e));
  assert(core.idle_process(id, true, e));
  assert(core.free_instance(id, e));
  auto size = order.size();
  assert(core.timer_timeout(2, e));
  assert(order.size() == size);
  assert(core.waiters().empty());
}
