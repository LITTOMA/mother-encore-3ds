#include "encore/field_global_flags.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
using namespace encore::upstream;
// Manual source projection cases. Compile-only is not a behaviour run and
// does not stand in for the actual global signal owner or complete save UI.
int main(int argc, char **argv) {
  assert(argc == 2);
  std::ifstream f(argv[1], std::ios::binary);
  std::vector<uint8_t> raw{std::istreambuf_iterator<char>(f), {}};
  if (raw.size() < 96)
    return 2;
  std::string e;
  FieldGlobalFlagsData d;
  assert(d.load(raw.data(), raw.size(), e));
  assert(d.constructor().size() == 178 && d.profiles().size() == 2);
  for (auto at : {8, 20, 24, 28}) {
    auto bad = raw;
    bad[size_t(at)] ^= 1;
    assert(!d.load(bad.data(), bad.size(), e) && d.valid());
  }
  assert(!d.load(raw.data(), raw.size() - 1, e));
  auto crc_bad = raw;
  crc_bad.back() ^= 1;
  assert(!d.load(crc_bad.data(), crc_bad.size(), e));
  FieldGlobalExternalSpec owner;
  owner.stable_id = 1;
  owner.identity.upstream_commit = d.pin();
  owner.source = d.owner_source();
  owner.script = owner.source;
  assert(d.source_hash(owner.source, owner.source_sha));
  owner.script_sha = owner.source_sha;
  FieldGlobalFlagsRuntime r, missing, bad_owner;
  assert(!missing.initialize(d, owner, {}, e));
  auto wrong = owner;
  wrong.script_sha[0] ^= 1;
  assert(!bad_owner.initialize(d, wrong, [](auto &) { return true; }, e));
  unsigned emits = 0;
  bool present = false, value = false;
  auto flag = d.constructor().front().first;
  assert(r.initialize(
      d, owner,
      [&](std::string &err) {
        ++emits;
        return r.read(false, flag, present, value, err) && present && value;
      },
      e));
  assert(r.read(false, flag, present, value, e) && present && !value);
  assert(r.set_normal(flag, true, true, e) && emits == 1);
  assert(r.set_normal(flag, true, true, e) && emits == 2);
  assert(r.set_normal("manual_unknown_registered_name", true, true, e) &&
         emits == 2);
  assert(r.read(false, "manual_unknown_registered_name", present, value, e) &&
         !present && !value);
  assert(r.set_object("scene/object", true, false, e));
  assert(r.mark_seen("/root/scene/npc::1:dialogue", e));
  std::vector<uint8_t> saved;
  assert(r.encode_save(saved, e));
  FieldFlagProjection source;
  source.normal = {{"manual_unknown_saved_name", true}};
  assert(r.load_source(source, e));
  assert(r.read(false, flag, present, value, e) && present && !value);
  assert(r.read(true, "scene/object", present, value, e) && !present);
  assert(r.restore_save(saved.data(), saved.size(), e));
  assert(r.read(true, "scene/object", present, value, e) && present && value);
  assert(r.seen("/root/scene/npc::1:dialogue", value, e) && value);
  auto revision = r.revision();
  auto bad = saved;
  bad.back() ^= 1;
  assert(!r.restore_save(bad.data(), bad.size(), e) &&
         revision == r.revision());
  for (auto at : {8, 20, 24, 28, 32, 52}) {
    bad = saved;
    bad[size_t(at)] ^= 1;
    assert(!r.restore_save(bad.data(), bad.size(), e) &&
           revision == r.revision());
  }
  source.objects = {{"duplicate", true}, {"duplicate", false}};
  assert(!r.load_source(source, e) && revision == r.revision());
  assert(!r.load_profile(2, e));
  FieldGlobalFlagsRuntime partial;
  assert(partial.initialize(
      d, owner,
      [](auto &err) {
        err = "actual emit rejected";
        return false;
      },
      e));
  assert(!partial.set_normal(flag, true, true, e));
  assert(partial.state().normal.front().second);
  assert(!partial.read(false, flag, present, value, e));
}
