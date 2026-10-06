// Manual-only cases. Not registered or executed by automatic builds.
#include "encore/field_programme.hpp"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iterator>
using namespace encore::upstream;
namespace {
std::vector<uint8_t> bytes(const char *p) {
  std::ifstream f(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}
void repair(std::vector<uint8_t> &b) {
  for (size_t i = 16; i < 20; ++i)
    b[i] = 0;
  uint32_t crc = ~0u;
  for (auto x : b) {
    crc ^= x;
    for (unsigned j = 0; j < 8; ++j)
      crc = (crc >> 1) ^ (0xedb88320u & uint32_t(-int32_t(crc & 1)));
  }
  crc = ~crc;
  for (unsigned i = 0; i < 4; ++i)
    b[16 + i] = uint8_t(crc >> (8 * i));
}
} // namespace
int main() {
  FieldProgrammeData data;
  BasementProgressionData basement;
  std::string error;
  auto pack = bytes("romfs/data/podunk-programmes.encprog");
  assert(data.load(pack.data(), pack.size(), error));
  assert(basement.load_file("romfs/data/house.encbasement", error));
  auto before = data.program_count();
  assert(before > 0);
  auto bad = pack;
  bad[8] = 2;
  repair(bad);
  assert(!data.load(bad.data(), bad.size(), error) &&
         data.program_count() == before);
  bad = pack;
  bad[20] = 2;
  repair(bad);
  assert(!data.load(bad.data(), bad.size(), error));
  bad = pack;
  bad[24] = 2;
  repair(bad);
  assert(!data.load(bad.data(), bad.size(), error));
  bad = pack;
  bad.push_back(0);
  assert(!data.load(bad.data(), bad.size(), error));
  assert(!data.load(nullptr, pack.size(), error));
  auto read_u32 = [](const std::vector<uint8_t> &v, size_t at) {
    return uint32_t(v[at]) | uint32_t(v[at + 1]) << 8 |
           uint32_t(v[at + 2]) << 16 | uint32_t(v[at + 3]) << 24;
  };
  size_t flag_emit_at = 64;
  for (unsigned i = 0; i < 2; ++i)
    flag_emit_at += 4 + read_u32(pack, flag_emit_at);
  bad = pack;
  bad[flag_emit_at] = 2;
  repair(bad);
  assert(!data.load(bad.data(), bad.size(), error) && data.valid());
  FieldProgrammeHost host;
  std::map<std::string, bool> flags, seen;
  uint32_t grants = 0, mutations = 0;
  uint64_t object = 73;
  bool deny = false;
  std::vector<DialogueActionKind> trace;
  host.flag = [&](std::string_view flag, bool &out, std::string &) {
    out = flags[std::string(flag)];
    return true;
  };
  host.actor = [&](uint32_t id, uint64_t &out, std::string &path,
                   std::string &) {
    if (id != data.npc().id)
      return false;
    out = object;
    path = "/root/ActualScene/" + data.npc().node;
    return true;
  };
  host.seen = [&](std::string_view key, bool &out, std::string &) {
    out = seen[std::string(key)];
    return true;
  };
  host.mark_seen = [&](std::string_view key, std::string &) {
    seen[std::string(key)] = true;
    ++mutations;
    return true;
  };
  host.admit = [&](const FieldProgrammeData &, uint32_t,
                   const FieldProgrammeContext &, const DialogueAction &,
                   std::string &e) {
    if (deny) {
      e = "manual unimplemented endpoint";
      return false;
    }
    return true;
  };
  host.key_effects.validate_key_item = [&](const BasementKeyItem &key,
                                           std::string &) {
    return basement.key_item(key.id) == &key;
  };
  host.key_effects.grant_key_item = [&](const BasementKeyItem &,
                                        std::string &) {
    ++grants;
    trace.push_back(DialogueActionKind::GrantKeyItem);
    return true;
  };
  host.key_effects.validate_skill = [](const BasementSkill &, std::string &) {
    return true;
  };
  host.key_effects.learn_skill = [](const BasementSkill &, std::string &) {
    return false;
  };
  host.apply = [&](const FieldProgrammeData &d, uint32_t,
                   const FieldProgrammeContext &, const DialogueAction &a,
                   std::string &) {
    trace.push_back(a.kind);
    if (a.kind == DialogueActionKind::SetFlag) {
      flags[*d.flag(a.target_index)] = a.value != 0;
      ++mutations;
    }
    return true;
  };
  host.open_dialogue = [&](const FieldProgrammeData &, uint32_t,
                           const FieldProgrammeContext &, uint32_t,
                           uint64_t &out, std::string &) {
    out = 91;
    return true;
  };
  host.admit_dialogue_ready = [](uint64_t id, uint32_t generation,
                                 std::string &) {
    return id == 91 && generation == 17;
  };
  FieldProgrammeRuntime runtime;
  assert(runtime.initialize(&data, &basement, host, error));
  deny = true;
  assert(!runtime.admit_npc(data.npc().id, true, error));
  assert(!runtime.start_npc(data.npc().id, true, 17, error));
  assert(mutations == 0 && grants == 0);
  deny = false;
  // Derive required scratch branch from real key-bearing programme, not fixture
  // text.
  uint32_t key_program = 0;
  bool found = false;
  for (uint32_t i = 0; i < data.program_count(); ++i) {
    auto p = data.program(i);
    for (uint32_t j = 0; j < p.command_count; ++j)
      if (data.command(p.first_command + j).opcode ==
          uint16_t(DialogueActionKind::GrantKeyItem)) {
        key_program = i;
        found = true;
      }
  }
  assert(found);
  for (const auto &r : data.npc().rows)
    if (r.thoughts && r.program == data.record(key_program)->path)
      flags[r.flag] = true;
  uint32_t selected = 0;
  std::string seen_key;
  assert(runtime.select_npc(data.npc().id, true, selected, seen_key, error) &&
         selected == key_program);
  assert(runtime.admit_npc(data.npc().id, true, error) && grants == 0);
  assert(runtime.start_npc(data.npc().id, true, 17, error));
  assert(runtime.scheduler().status() == DialogueStatus::Idle && trace.empty());
  assert(!runtime.dialogue_ready(92, 17, error));
  assert(runtime.dialogue_ready(91, 17, error));
  while (runtime.scheduler().status() == DialogueStatus::AwaitDialogue)
    assert(runtime.finish_dialogue());
  assert(runtime.scheduler().status() == DialogueStatus::Completed &&
         grants == 1);
  auto grant =
      std::find(trace.begin(), trace.end(), DialogueActionKind::GrantKeyItem);
  assert(grant != trace.end() && grant + 2 < trace.end() &&
         grant[1] == DialogueActionKind::ShowDialogue &&
         grant[2] == DialogueActionKind::PlaySound);
  assert(runtime.select_npc(data.npc().id, true, selected, seen_key, error) &&
         selected != key_program);
  // FieldNpc owns its source mark_seen before open_program. This bridge must
  // neither bypass that boundary nor publish a second seen write.
  FieldProgrammeRuntime from_npc;
  assert(from_npc.initialize(&data, &basement, host, error));
  assert(from_npc.select_npc(data.npc().id, true, selected, seen_key, error));
  const auto selected_path = data.record(selected)->path;
  const auto seen_before = mutations;
  assert(!from_npc.start_selected_npc(data.npc().id, selected_path, true, 17,
                                      error));
  assert(mutations == seen_before);
  assert(host.mark_seen(seen_key, error));
  const auto marked = mutations;
  assert(!from_npc.start_selected_npc(
      data.npc().id, data.record(key_program)->path, true, 17, error));
  assert(from_npc.start_selected_npc(data.npc().id, selected_path, true, 17,
                                     error));
  assert(mutations == marked && grants == 1);
  assert(!from_npc.dialogue_ready(91, 18, error));
  assert(from_npc.dialogue_ready(91, 17, error));
  while (from_npc.scheduler().status() == DialogueStatus::AwaitDialogue)
    assert(from_npc.finish_dialogue());
  assert(from_npc.scheduler().status() == DialogueStatus::Completed &&
         grants == 1);
  for (const auto &r : data.npc().rows)
    if (!r.thoughts && !r.supported)
      flags[r.flag] = true;
  auto prior = mutations;
  assert(!runtime.admit_npc(data.npc().id, false, error));
  assert(!runtime.start_npc(data.npc().id, false, 17, error) &&
         mutations == prior);
  return 0;
}
