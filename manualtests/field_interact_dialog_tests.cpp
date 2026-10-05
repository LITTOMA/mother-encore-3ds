#include "encore/field_interact_dialog.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
using namespace encore::upstream;
namespace {
void put(std::vector<uint8_t> &b, size_t at, uint32_t v) {
  for (unsigned i = 0; i < 4; ++i)
    b[at + i] = uint8_t(v >> (8 * i));
}
void seal(std::vector<uint8_t> &b) {
  put(b, 16, 0);
  uint32_t c = ~0u;
  for (uint8_t v : b) {
    c ^= v;
    for (unsigned i = 0; i < 8; ++i)
      c = (c >> 1) ^ (0xedb88320u & uint32_t(-int32_t(c & 1)));
  }
  put(b, 16, ~c);
}
} // namespace
int main(int argc, char **argv) {
  assert(argc == 2);
  std::ifstream f(argv[1], std::ios::binary);
  std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), {});
  assert(b.size() > 64);
  FieldInteractData d;
  std::string e;
  assert(d.load(b.data(), b.size(), e));
  const auto count = d.records().size();
  const auto first = d.records().front().id;
  for (const auto &field : std::vector<std::pair<size_t, uint32_t>>{
           {8, 2}, {20, 0}, {20, 2}, {24, 2}, {28, 0}, {52, 1}}) {
    auto bad = b;
    put(bad, field.first, field.second);
    seal(bad);
    assert(!d.load(bad.data(), bad.size(), e));
    assert(d.valid() && d.records().size() == count &&
           d.records().front().id == first);
  }
  auto short_blob = b;
  short_blob.pop_back();
  assert(!d.load(short_blob.data(), short_blob.size(), e));
  auto extra = b;
  extra.push_back(0);
  put(extra, 12, uint32_t(extra.size()));
  seal(extra);
  assert(!d.load(extra.data(), extra.size(), e));
  FieldInteractRuntime runtime;
  FieldInteractHost host;
  std::vector<std::string> calls;
  std::function<bool(std::string &)> flags;
  host.admit_ready = [](const auto &, auto &) { return true; };
  host.connect_flags = [&](auto, auto cb, auto &) {
    calls.push_back("connect");
    flags = cb;
    return true;
  };
  host.read_flag = [](auto, bool &v, auto &) {
    v = false;
    return true;
  };
  host.visible = [&](auto, bool, auto &) {
    calls.push_back("visible");
    return true;
  };
  host.queue_free = [&](auto, auto &) {
    calls.push_back("free");
    return true;
  };
  host.apply_serialized_offset = [](auto, auto, auto &) { return true; };
  host.admit_programme = [](auto, auto &) { return true; };
  host.open_programme = [&](auto, auto, auto &) {
    calls.push_back("dialogue");
    return true;
  };
  host.telepathy_effect = [&](bool, auto &) {
    calls.push_back("effect");
    return true;
  };
  assert(runtime.initialize(d, host, e));
  assert(runtime.instantiate(first));
  assert(runtime.ready(first));
  assert(calls.front() == "visible" && calls.back() == "connect");
  calls.clear();
  assert(runtime.telepathy(first));
  assert(calls == std::vector<std::string>({"effect", "dialogue"}));
  calls.clear();
  assert(runtime.interact_item(first, "not-the-source-item"));
  assert(calls.empty());
  // Failed source preflight never starts the Telepathy effect.
  FieldInteractRuntime rejected;
  host.admit_programme = [](auto, auto &e) {
    e = "unadmitted programme";
    return false;
  };
  assert(rejected.initialize(d, host, e));
  assert(rejected.instantiate(first));
  assert(rejected.ready(first));
  calls.clear();
  assert(!rejected.telepathy(first));
  assert(calls.empty());
}