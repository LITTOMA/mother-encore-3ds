// Manual-only checked parser/real-source execution cases, never
// auto-registered.
#include "encore/field_stepping_sounds.hpp"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iterator>
using namespace encore::upstream;
int main(int argc, char **argv) {
  assert(argc == 2);
  std::ifstream f(argv[1], std::ios::binary);
  std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), {});
  FieldSteppingSoundsData data;
  std::string e;
  assert(data.load(b.data(), b.size(), e));
  auto bad = b;
  bad[8] = 2;
  assert(!data.load(bad.data(), bad.size(), e));
  assert(data.valid());
  assert(!data.load(b.data(), b.size() - 1, e));
  bad = b;
  bad.back() ^= 1;
  assert(!data.load(bad.data(), bad.size(), e));
  FieldSteppingSoundsRuntime core;
  FieldSteppingSoundsHost h;
  int sounds = 0, shadows = 0;
  bool admit = true;
  h.admit = [](const auto &, std::string &) { return true; };
  h.admit_ready = [](const auto &, const auto &, std::string &) {
    return true;
  };
  h.describe_body = [](uint32_t id, FieldSteppingBody &o, std::string &) {
    o = {id == 1, id == 1 || id == 2};
    return true;
  };
  h.admit_dispatch = [&](const auto &, std::string &) { return admit; };
  h.set_player_run_sound = [&](std::string_view, std::string &) {
    ++sounds;
    return true;
  };
  h.set_party_shadow = [&](uint32_t, std::string_view, std::string &) {
    ++shadows;
    return true;
  };
  h.geometry = [](uint32_t, const FieldSteppingShape &s,
                  FieldSteppingGeometry &o, std::string &) {
    o = {true, s.disabled, s.parts};
    return true;
  };
  assert(core.initialize(data, h, e));
  const auto id = data.bindings()[0].id;
  assert(!core.ready(data.bindings()[1].id, e));
  for (const auto &v : data.bindings())
    assert(core.ready(v.id, e));
  assert(core.disable(id, e));
  assert(core.body_enter(id, 1, e));
  assert(sounds == 0 && shadows == 1);
  assert(core.enable(id, e));
  assert(core.body_enter(id, 2, e));
  assert(sounds == 0 && shadows == 2);
  admit = false;
  assert(!core.body_enter(id, 1, e));
  assert(sounds == 0 && shadows == 2);
  admit = true;
  assert(core.body_enter(id, 1, e));
  assert(sounds == 1 && shadows == 3);
  const auto grass =
      std::find_if(data.bindings().begin(), data.bindings().end(),
                   [](const auto &v) { return v.exiting_sound.empty(); });
  assert(grass != data.bindings().end());
  assert(core.body_exit(grass->id, 1, e));
  assert(sounds == 1 && shadows == 3);
  assert(core.exit_tree(id, e));
  assert(!core.body_enter(id, 1, e));
}
