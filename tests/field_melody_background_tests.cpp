// Manual-only source execution/parser cases; no automatic registration.
#include "encore/field_melody_background.hpp"
#include <cassert>
#include <cmath>
#include <fstream>
#include <iterator>
using namespace encore::upstream;
int main(int argc, char **argv) {
  assert(argc == 2);
  std::ifstream f(argv[1], std::ios::binary);
  std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), {});
  std::string e;
  FieldMelodyBackgroundData data;
  assert(data.load(b.data(), b.size(), e));
  auto bad = b;
  bad[8] = 2;
  assert(!data.load(bad.data(), bad.size(), e));
  assert(data.valid());
  bad = b;
  bad.back() ^= 1;
  assert(!data.load(bad.data(), bad.size(), e));
  assert(!data.load(b.data(), b.size() - 1, e));
  const auto id = data.bindings()[0].id;
  FieldMelodyActor actor{true, true, true, 20, {40, 80}};
  Vec2 local{10, 20};
  uint64_t token = 1;
  int prints = 0, active_calls = 0, noops = 0;
  bool admitted = true;
  FieldMelodyBackgroundHost h;
  h.admit_ready = [](const auto &, const auto &, const auto &, std::string &) {
    return true;
  };
  h.admit_call = [&](const auto &, bool, std::string &) { return admitted; };
  h.camera_screen_center = [&](Vec2 &o, std::string &) {
    o = local;
    return true;
  };
  h.root_object = [](uint32_t, FieldMelodyObject &o, std::string &) {
    o = 10;
    return true;
  };
  h.set_global_position = [&](uint32_t, Vec2 p, std::string &) {
    assert(p.x == local.x && p.y == local.y);
    return true;
  };
  h.local_position = [&](uint32_t, Vec2 &o, std::string &) {
    o = local;
    return true;
  };
  h.dialogue_actors = [](auto &o, std::string &) {
    o = {{"first", 1}};
    return true;
  };
  h.talker = [](FieldMelodyObject &o, std::string &) {
    o = 1;
    return true;
  };
  h.print_actor_key = [&](std::string_view, std::string &) {
    ++prints;
    return true;
  };
  h.describe_actor = [&](FieldMelodyObject o, FieldMelodyActor &a,
                         std::string &) {
    assert(o == 1);
    a = actor;
    return true;
  };
  h.set_actor_position = [&](FieldMelodyObject, Vec2 p, std::string &) {
    actor.position = p;
    return true;
  };
  h.set_actor_active = [&](FieldMelodyObject, bool value, std::string &) {
    assert(value);
    ++active_calls;
    return true;
  };
  h.remove_child = [&](FieldMelodyObject parent, FieldMelodyObject,
                       std::string &) {
    if (actor.parent == parent)
      actor.parent = 0;
    else
      ++noops;
    return true;
  };
  h.add_child = [&](FieldMelodyObject parent, FieldMelodyObject,
                    std::string &) {
    if (!actor.parent)
      actor.parent = parent;
    else
      ++noops;
    return true;
  };
  h.set_visible = [](uint32_t, bool value, std::string &) {
    assert(value);
    return true;
  };
  h.write_color = [](uint32_t, FieldMelodyColorRole, BattleValue,
                     std::string &) { return true; };
  h.animation_started = [&](uint32_t, std::string_view name, std::string &) {
    assert(name == data.policy().clip);
    return true;
  };
  h.animation_stopped = [](uint32_t, std::string &) { return true; };
  h.create_tween = [&](const FieldMelodyTweenSpec &s, uint64_t &o,
                       std::string &) {
    assert(s.owner == id);
    o = token++;
    return true;
  };
  h.tween_signal = [](uint64_t, FieldMelodyTweenSignal, std::string &) {
    return true;
  };
  FieldMelodyBackgroundRuntime core;
  assert(core.initialize(data, h, e));
  assert(!core.ready(id + 1, e));
  assert(core.ready(id, e));
  assert(core.state(id)->visible && core.state(id)->self_modulate.w == 0);
  admitted = false;
  assert(!core.appear(id, e));
  assert(core.state(id)->moved.empty() && prints == 0);
  admitted = true;
  assert(core.appear(id, e));
  assert(prints == 1 && core.state(id)->moved.size() == 2 && active_calls == 0);
  assert(actor.parent == 10 && actor.position.x == 20 &&
         actor.position.y == 40);
  assert(core.animation_idle(id, .25f, true, e));
  const auto pos = core.state(id)->animation_time;
  assert(core.tween_step(1, .25f, 1, true, e));
  assert(core.state(id)->self_modulate.w == .5f);
  assert(!core.tween_step(1, .25f, 1, true, e));
  assert(core.disappear(id, e));
  assert(core.state(id)->playing && core.state(id)->moved.size() == 2);
  assert(core.tween_step(1, .25f, 2, true, e));
  assert(core.state(id)->self_modulate.w == 1);
  assert(core.tween_step(2, .25f, 2, true, e));
  assert(core.state(id)->self_modulate.w == .5f);
  assert(core.tween_step(2, .25f, 3, true, e));
  assert(core.state(id)->self_modulate.w == 0 && !core.state(id)->playing &&
         core.state(id)->animation_time == 0);
  assert(actor.parent == 20 && actor.position.x == 40 &&
         actor.position.y == 80 && active_calls == 2 && noops == 2);
  assert(core.state(id)->moved.empty() && pos > 0);
  assert(core.appear(id, e));
  assert(core.animation_idle(id, .1f, true, e));
  const auto saved = core.state(id)->animation_time;
  assert(core.appear(id, e));
  assert(core.state(id)->animation_time == saved);
  assert(core.exit_tree(id, e));
  assert(core.tween_step(3, .1f, 4, true, e));
  assert(!core.tweens()[0].started);
  assert(core.enter_tree(id, e));
  assert(core.tween_step(3, .1f, 5, true, e));
  assert(core.free_instance(id, e));
  assert(core.tween_step(3, .1f, 6, true, e));
}
