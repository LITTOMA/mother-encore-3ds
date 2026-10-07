// Manual-only typed host/checked binary cases. Does not run in ordinary CI.
#include "encore/field_player_transitions.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
using namespace encore::upstream;
static uint32_t crc(const std::vector<uint8_t> &b) {
  uint32_t c = ~0u;
  for (size_t i = 0; i < b.size(); ++i) {
    c ^= i >= 16 && i < 20 ? 0 : b[i];
    for (unsigned j = 0; j < 8; ++j)
      c = (c >> 1) ^ (0xedb88320u & uint32_t(-int(c & 1)));
  }
  return ~c;
}
static void patch(std::vector<uint8_t> &b, size_t p, uint32_t v) {
  for (unsigned j = 0; j < 4; ++j)
    b[p + j] = uint8_t(v >> (j * 8));
  auto h = crc(b);
  for (unsigned j = 0; j < 4; ++j)
    b[16 + j] = uint8_t(h >> (j * 8));
}
int main(int argc, char **argv) {
  assert(argc == 2);
  FieldPlayerTransitionsData d;
  std::string e;
  assert(d.load_file(argv[1], e));
  assert(d.records().size() == 16 && d.areas().size() == 30);
  FieldTransitionActor a;
  a.id = 1;
  a.kind = 1;
  a.camera_id = 3;
  FieldGeometryActor body;
  body.kind = FieldGeometryKind::Rectangle;
  body.stable_id = a.id;
  body.layer = 1;
  body.mask = 257;
  body.extents = {3, 3};
  a.body_geometry = {body};
  auto area = body;
  area.stable_id = 2;
  area.area = true;
  area.layer = 256;
  a.area_geometry = {area};
  FieldTransitionContext c;
  c.player = 1;
  c.player_move = true;
  c.stack_empty = true;
  c.party = {a};
  c.prompt_mode = d.prompt_modes()[0];
  std::vector<FieldTransitionCommand> commands;
  FieldPlayerTransitionsHost h;
  h.bind = [](const auto &x, std::string &) { return x.valid(); };
  h.context = [&](auto &out, std::string &) {
    out = c;
    return true;
  };
  h.flag = [](const std::string &, bool &value, std::string &) {
    value = true;
    return true;
  };
  h.cached_ray = [](const auto &, const auto &, uint32_t &hit, std::string &) {
    hit = 0;
    return true;
  };
  h.apply = [&](const auto &command, std::string &) {
    commands.push_back(command);
    return true;
  };
  FieldPlayerTransitionsRuntime r;
  assert(r.initialize(d, h, e));
  for (const auto &record : d.records())
    assert(r.ready(record.id));
  const auto before = commands.size();
  assert(r.accept());
  assert(commands.size() == before);
  assert(r.physics_step(1.f / 60));
  assert(r.idle_frame(.1, false));
  assert(r.idle_frame(.1, true));
  bool had_idle_arrow = false;
  for (const auto &command : commands)
    had_idle_arrow |= command.kind == FieldTransitionCommandKind::ArrowPose;
  assert(had_idle_arrow);
  FieldPlayerTransitionsRuntime missing;
  auto incomplete = h;
  incomplete.cached_ray = {};
  assert(!missing.initialize(d, incomplete, e));
  incomplete = h;
  incomplete.apply = {};
  assert(!missing.initialize(d, incomplete, e));
  auto bad = c;
  bad.party[0].body_geometry[0].area = true;
  c = bad;
  assert(missing.initialize(d, h, e));
  assert(!missing.process_source(d.records().front().id, .01));
  c.party[0].body_geometry[0].area = false;
  c.player = 999;
  assert(missing.initialize(d, h, e));
  assert(!missing.process_source(d.records().front().id, .01));
  c.player = 1;
  assert(missing.initialize(d, h, e));
  assert(!missing.ready(999));
  assert(!missing.idle_frame(.1));
  std::ifstream f(argv[1], std::ios::binary);
  std::vector<uint8_t> raw((std::istreambuf_iterator<char>(f)), {});
  for (auto offset : {size_t(8), size_t(20), size_t(24), size_t(52), size_t(56),
                      size_t(60)}) {
    auto b = raw;
    patch(b, offset, 99999);
    assert(!d.load(b.data(), b.size(), e) && d.valid());
  }
  if (d.capability() == 2) {
    assert(d.bindings().state_arguments == 2 && !d.bindings().state_signal.empty());
    // Read the variable-length source suffix rather than assuming game strings.
    const auto &binding = d.bindings();
    size_t suffix = 4 + binding.state_signal.size() + 4 +
        4 + binding.ground_method.size() + 4 + binding.ground_path.size() + 4 +
        4 + binding.shadow_path.size() + 4 + binding.prompt_member.size();
    const size_t arity = raw.size() - suffix + 4 + binding.state_signal.size();
    auto invalid = raw;
    patch(invalid, arity, 99);
    assert(!d.load(invalid.data(), invalid.size(), e) && d.valid());
    const size_t factor = arity + 4 + 4 + binding.ground_method.size() +
        4 + binding.ground_path.size();
    invalid = raw;
    patch(invalid, factor, 0);
    assert(!d.load(invalid.data(), invalid.size(), e) && d.valid());
  }
  auto b = raw;
  patch(b, 64, 0);
  assert(!d.load(b.data(), b.size(), e) && d.valid());
  b = raw;
  b.push_back(0);
  assert(!d.load(b.data(), b.size(), e) && d.valid());
}
