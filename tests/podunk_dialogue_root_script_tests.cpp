#include "encore/crc32.hpp"
#include "podunk_dialogue_root_script.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
using namespace encore::ctr;
namespace {
void check(bool value, const char *name) {
  if (!value) {
    std::fprintf(stderr, "%s\n", name);
    std::exit(1);
  }
}
void put(std::vector<uint8_t> &b, size_t at, uint32_t v) {
  check(at <= b.size() && b.size() - at >= 4, "manual mutation bounds");
  for (unsigned i = 0; i < 4; ++i)
    b[at + i] = uint8_t(v >> (8 * i));
}
uint32_t get(const std::vector<uint8_t> &b, size_t at) {
  check(at <= b.size() && b.size() - at >= 4, "manual read bounds");
  return uint32_t(b[at]) | uint32_t(b[at + 1]) << 8 |
         uint32_t(b[at + 2]) << 16 | uint32_t(b[at + 3]) << 24;
}
size_t skip_text(const std::vector<uint8_t> &b, size_t at) {
  auto count = get(b, at);
  check(at + 4 <= b.size() && count <= b.size() - at - 4,
        "manual source string bounds");
  return at + 4 + count;
}
void reseal(std::vector<uint8_t> &b) {
  put(b, 16, uint32_t(b.size()));
  put(b, 20, encore::crc32(b.data() + 128, b.size() - 128));
}
} // namespace
// Manual source cases; compiled only during routine development, never run.
int main(int argc, char **argv) {
  check(argc == 2, "source resource required");
  std::ifstream in(argv[1], std::ios::binary);
  std::vector<uint8_t> b{std::istreambuf_iterator<char>(in), {}};
  std::string e;
  PodunkDialogueRootData d;
  check(d.load(b.data(), b.size(), e), "actual root source pack");
  auto identity = d.identity();
  auto actions = d.actions();
  auto hash = d.recipe_sha();
  auto bad = b;
  put(bad, 8, 2);
  check(!d.load(bad.data(), bad.size(), e), "future version rejected");
  bad = b;
  put(bad, 28, 2);
  check(!d.load(bad.data(), bad.size(), e), "future capability rejected");
  bad = b;
  put(bad, 32, 46);
  check(!d.load(bad.data(), bad.size(), e), "incomplete real recipe rejected");
  bad = b;
  bad.back() ^= 1;
  check(!d.load(bad.data(), bad.size(), e), "corrupt payload rejected");
  for (size_t n = 0; n < b.size(); ++n)
    check(!d.load(b.data(), n, e), "every truncation rejected");
  bad = b;
  bad.push_back(0);
  reseal(bad);
  check(!d.load(bad.data(), bad.size(), e), "trailing source bytes rejected");
  bad = b;
  for (size_t i = 128; i < 160; ++i)
    bad[i] = 0;
  reseal(bad);
  check(!d.load(bad.data(), bad.size(), e), "zero recipe provenance rejected");
  bad = b;
  put(bad, 228, 0xffffffffu);
  reseal(bad);
  check(!d.load(bad.data(), bad.size(), e), "out of range source Cursor index");
  size_t first = skip_text(b, 232), second = skip_text(b, first);
  check(get(b, first) == get(b, second),
        "source actions equal mutation length");
  bad = b;
  for (size_t j = 0; j < get(b, first); ++j)
    bad[second + 4 + j] = b[first + 4 + j];
  reseal(bad);
  check(!d.load(bad.data(), bad.size(), e), "duplicate source action rejected");
  size_t at = skip_text(b, 232);
  for (unsigned j = 0; j < 10; ++j)
    at = skip_text(b, at);
  bad = b;
  put(bad, at + 32, 2);
  reseal(bad);
  check(!d.load(bad.data(), bad.size(), e), "unknown source boolean rejected");
  check(d.valid() && d.identity().scene_id == identity.scene_id &&
            d.actions() == actions && d.recipe_sha() == hash,
        "failed read preserves source owner");
  PodunkDialogueRootOwner owner;
  PodunkDialogueScriptState unused;
  check(!owner.state(1, unused, e), "no fake ObjectDB instance");
  check(!owner.begin(1, 1, e), "no fake ready/generation");
  check(!owner.text_completed(1, e), "no fake printer finish");
  check(!owner.wait_timeout(1, e), "no fake timeout");
  encore::upstream::FieldDeferredMessage message;
  message.object = 1;
  message.member = "unknown";
  check(!owner.deferred(message, e), "unknown deferred source fails");
  check(!owner.release(1, e), "no fake release");
  return 0;
}
