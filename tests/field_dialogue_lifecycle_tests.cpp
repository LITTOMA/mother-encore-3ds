// Manual-only source parser/ownership cases. Never registered or auto-run.
#include "encore/field_dialogue_lifecycle.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
using namespace encore::upstream;
namespace {
std::vector<uint8_t> bytes(const char *p) {
  std::ifstream f(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}
uint32_t word(const std::vector<uint8_t> &b, size_t i) {
  return uint32_t(b[i]) | uint32_t(b[i + 1]) << 8 | uint32_t(b[i + 2]) << 16 |
         uint32_t(b[i + 3]) << 24;
}
void repair(std::vector<uint8_t> &b) {
  for (size_t i = 16; i < 20; ++i)
    b[i] = 0;
  uint32_t c = ~0u;
  for (auto x : b) {
    c ^= x;
    for (unsigned j = 0; j < 8; ++j)
      c = (c >> 1) ^ (0xedb88320u & uint32_t(-int32_t(c & 1)));
  }
  c = ~c;
  for (unsigned i = 0; i < 4; ++i)
    b[16 + i] = uint8_t(c >> (i * 8));
}
} // namespace
int main() {
  FieldDialogueLifecycleData d;
  std::string e;
  auto pack = bytes("romfs/data/podunk-dialogue-lifecycle.encdlife");
  assert(d.load(pack.data(), pack.size(), e));
  assert(!d.steps().empty());
  FieldProgrammeData p;
  assert(p.load_file("romfs/data/podunk-programmes.encprog", e));
  for (uint32_t i = 0; i < p.program_count(); ++i)
    assert(d.supports(p, i, e));
  for (auto index : {8u, 20u, 24u}) {
    auto bad = pack;
    bad[index] = 2;
    repair(bad);
    assert(!d.load(bad.data(), bad.size(), e) && d.valid());
  }
  auto bad = pack;
  bad[52] = 1;
  repair(bad);
  assert(!d.load(bad.data(), bad.size(), e));
  bad = pack;
  bad.push_back(0);
  assert(!d.load(bad.data(), bad.size(), e));
  assert(!d.load(nullptr, pack.size(), e));
  assert(!d.load(pack.data(), pack.size() - 1, e));
  size_t at = 64;
  at += 4 + word(pack, at);
  const auto factory = at + 32;
  for (auto offset : {factory, factory + 4}) {
    bad = pack;
    for (size_t i = 0; i < 4; ++i)
      bad[offset + i] = 0;
    repair(bad);
    assert(!d.load(bad.data(), bad.size(), e) && d.valid());
  }
  bad = pack;
  for (size_t i = 0; i < 32; ++i)
    bad[factory + 8 + i] = 0;
  repair(bad);
  assert(!d.load(bad.data(), bad.size(), e) && d.valid());
  at += 32 + 8 + 32 + 8 + 12;
  for (unsigned i = 0; i < 5; ++i)
    at += 4 + word(pack, at);
  auto count = word(pack, at);
  at += 4;
  const auto first_ref = at;
  bad = pack;
  for (size_t i = 0; i < 4; ++i)
    bad[first_ref + 4 + i] = 0;
  repair(bad);
  assert(!d.load(bad.data(), bad.size(), e) && d.valid());
  bad = pack;
  for (size_t i = 0; i < 4; ++i)
    bad[first_ref + 8 + i] = 255;
  repair(bad);
  assert(!d.load(bad.data(), bad.size(), e) && d.valid());
  for (uint32_t i = 0; i < count; ++i) {
    at += 12;
    at += 4 + word(pack, at);
    at += 4 + word(pack, at);
  }
  at += 4;
  bad = pack;
  bad[at + 4] = 255;
  repair(bad);
  assert(!d.load(bad.data(), bad.size(), e));
  bad = pack;
  bad[at] = 255;
  repair(bad);
  assert(!d.load(bad.data(), bad.size(), e));
  bad = pack;
  bad[at + 8] = 13;
  repair(bad);
  assert(!d.load(bad.data(), bad.size(), e));
  FieldDialogueLifecycleRuntime r;
  DialogueAction unknown;
  unknown.kind = DialogueActionKind::QueueBattle;
  assert(!r.admit(unknown, {}, e));
  assert(!r.ready(17, 2, e));
  assert(!r.done(17, 2, 0, e));
  assert(!r.animation_finished(17, 2, e));
  assert(!r.deleted(0, e));
  return 0;
}
