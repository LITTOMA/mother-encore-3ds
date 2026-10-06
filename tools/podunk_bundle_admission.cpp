#include "encore/podunk_bundle.hpp"
#include <iostream>
#include <string>
using namespace encore::upstream;
namespace {
bool hex(const std::string &s, uint8_t *out, size_t n) {
  if (s.size() != n * 2)
    return false;
  for (size_t i = 0; i < n; ++i) {
    auto digit = [](char c) -> int {
      return c >= '0' && c <= '9'   ? c - '0'
             : c >= 'a' && c <= 'f' ? c - 'a' + 10
                                    : -1;
    };
    int a = digit(s[i * 2]), b = digit(s[i * 2 + 1]);
    if (a < 0 || b < 0)
      return false;
    out[i] = uint8_t(a * 16 + b);
  }
  return true;
}
} // namespace
// Format/source admission only. No constructor, lifecycle, behavior or tests.
int main(int argc, char **argv) {
  if (argc != 6)
    return 2;
  FieldIdentity id;
  std::string number = argv[3];
  uint64_t v = 0;
  if (number.empty())
    return 2;
  for (char c : number) {
    if (c < '0' || c > '9' || v > 0xffffffffu / 10)
      return 2;
    v = v * 10 + uint32_t(c - '0');
  }
  if (!v || v > 0xffffffffu)
    return 2;
  id.scene_id = uint32_t(v);
  if (!hex(argv[4], id.upstream_commit.data(), 20) ||
      !hex(argv[5], id.source_sha256.data(), 32))
    return 2;
  PodunkBundleData d;
  std::string e;
  if (!d.load_file(argv[1], id, e) || !d.verify_files(argv[2], e)) {
    std::cerr << e << '\n';
    return 1;
  }
  std::cout << d.packs().size() << " typed packs / " << d.assets().size()
            << " actual files admitted; no Ready execution\n";
  return 0;
}
