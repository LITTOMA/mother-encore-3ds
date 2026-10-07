#include "encore/podunk_bundle.hpp"
#include "encore/crc32.hpp"
#include "manual_require.hpp"
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
using namespace encore::upstream;
namespace {
uint32_t word(const std::vector<uint8_t> &b, size_t at) {
  MANUAL_REQUIRE(at + 4 <= b.size());
  return uint32_t(b[at]) | uint32_t(b[at + 1]) << 8 |
         uint32_t(b[at + 2]) << 16 | uint32_t(b[at + 3]) << 24;
}
void put(std::vector<uint8_t> &b, size_t at, uint32_t value) {
  MANUAL_REQUIRE(at + 4 <= b.size());
  for (unsigned i = 0; i < 4; ++i) b[at + i] = uint8_t(value >> (8 * i));
}
bool hex(const std::string &s, uint8_t *out, size_t n) {
  if (s.size() != 2 * n) return false;
  for (size_t i = 0; i < n; ++i) {
    unsigned value = 0;
    for (unsigned j = 0; j < 2; ++j) {
      const char c = s[2 * i + j];
      const unsigned digit = c >= '0' && c <= '9' ? unsigned(c - '0') :
                             c >= 'a' && c <= 'f' ? unsigned(c - 'a' + 10) : 99;
      if (digit > 15) return false;
      value = 16 * value + digit;
    }
    out[i] = uint8_t(value);
  }
  return true;
}
}
int main(int argc, char **argv) {
  if (argc != 5) {
    std::cerr << "bundle scene-id source-pin source-sha\n";
    return 2;
  }
  FieldIdentity identity;
  identity.scene_id = uint32_t(std::strtoul(argv[2], nullptr, 10));
  MANUAL_REQUIRE(hex(argv[3], identity.upstream_commit.data(), 20));
  MANUAL_REQUIRE(hex(argv[4], identity.source_sha256.data(), 32));
  std::ifstream in(argv[1], std::ios::binary);
  MANUAL_REQUIRE(bool(in));
  std::vector<uint8_t> original{std::istreambuf_iterator<char>(in), {}};
  PodunkBundleData owner;
  std::string error;
  MANUAL_REQUIRE(owner.load(original.data(), original.size(), identity, error));
  const auto *geometry = owner.entry(PodunkPackRole::HouseGeometry);
  const auto *reentry = owner.entry(PodunkPackRole::HouseReentry);
  const auto *tree = owner.entry(PodunkPackRole::HouseNodeTree);
  const auto *inspection = owner.entry(PodunkPackRole::HouseInspectionRoom);
  MANUAL_REQUIRE(geometry && reentry && tree && geometry != reentry && tree != geometry);
  MANUAL_REQUIRE(reentry->format == 2 && reentry->family == 0x454e0075 &&
                 reentry->capability == 1 && reentry->rules == 1);
  MANUAL_REQUIRE(geometry->format == 1 && geometry->family == 0x454e001b &&
                 geometry->capability == 1 && geometry->rules == 1);
  MANUAL_REQUIRE(geometry->original_header.size() == 128 &&
                 !std::memcmp(geometry->original_header.data(), "ENCFGEO1", 8));
  MANUAL_REQUIRE(tree->format == 1 && tree->family == 0x454e003c &&
                 tree->capability == 3 && tree->rules == 1);
  MANUAL_REQUIRE(tree->original_header.size() == 128 &&
                 !std::memcmp(tree->original_header.data(), "ENCFNTR1", 8));
  MANUAL_REQUIRE(inspection && inspection->kind == 3 && inspection->format == 1 &&
                 inspection->family == 0x454e0002 && inspection->rules == 8 &&
                 inspection->capability == 10 &&
                 inspection->original_header.size() == 128 &&
                 !std::memcmp(inspection->original_header.data(), "ENCRMD01", 8));
  const auto counts = size_t(132) + word(original, 128);
  const auto count = word(original, counts);
  MANUAL_REQUIRE(count == owner.packs().size());
  size_t at = counts + 12, geometry_at = 0, geometry_header = 0,
         tree_at = 0, tree_header = 0, reentry_at = 0,
         inspection_at = 0, inspection_header = 0;
  for (uint32_t i = 0; i < count; ++i) {
    const size_t start = at;
    MANUAL_REQUIRE(word(original, start) == i + 1);
    at += 36;
    at += 4 + word(original, at);
    at += 96;
    const auto header_size = word(original, at);
    const auto header_start = at + 4;
    at += 4 + header_size;
    MANUAL_REQUIRE(at <= original.size() && header_size == 128);
    if (i + 1 == uint32_t(PodunkPackRole::HouseGeometry)) {
      geometry_at = start;
      geometry_header = header_start;
    }
    if (i + 1 == uint32_t(PodunkPackRole::HouseNodeTree)) {
      tree_at = start;
      tree_header = header_start;
    }
    if (i + 1 == uint32_t(PodunkPackRole::HouseReentry)) reentry_at = start;
    if (i + 1 == uint32_t(PodunkPackRole::HouseInspectionRoom)) {
      inspection_at = start;
      inspection_header = header_start;
    }
  }
  MANUAL_REQUIRE(geometry_at && tree_at && reentry_at && inspection_at && inspection_header);
  size_t font_atlas_at = 0;
  for (uint32_t i = 0; i < word(original, counts + 4); ++i) {
    const size_t start = at;
    MANUAL_REQUIRE(word(original, start) == i + 1);
    at += 16;
    const uint32_t length = word(original, at);
    MANUAL_REQUIRE(at + 4 + length + 32 <= original.size());
    const std::string asset_path(original.begin() + at + 4,
                                 original.begin() + at + 4 + length);
    at += 4 + length + 32;
    if (asset_path.substr(0, 6) == "fonts/" &&
        asset_path.size() > 4 && asset_path.substr(asset_path.size() - 4) == ".t3x") {
      MANUAL_REQUIRE(word(original, start + 4) == uint32_t(PodunkAssetKind::Font));
      font_atlas_at = start;
    }
  }
  MANUAL_REQUIRE(font_atlas_at);
  const auto path = geometry->path;
  auto rejected = [&](std::vector<uint8_t> candidate) {
    put(candidate, 20, encore::crc32(candidate.data() + 128, candidate.size() - 128));
    PodunkBundleData empty;
    MANUAL_REQUIRE(!empty.load(candidate.data(), candidate.size(), identity, error));
    MANUAL_REQUIRE(!empty.valid());
    MANUAL_REQUIRE(!owner.load(candidate.data(), candidate.size(), identity, error));
    MANUAL_REQUIRE(owner.valid() && owner.entry(PodunkPackRole::HouseGeometry)->path == path);
  };
  auto candidate = original; put(candidate, counts, count - 1); rejected(candidate);
  candidate = original; put(candidate, font_atlas_at + 4, uint32_t(PodunkAssetKind::Texture)); rejected(candidate);
  candidate = original; put(candidate, font_atlas_at + 4, 99); rejected(candidate);
  candidate = original; put(candidate, geometry_at, uint32_t(PodunkPackRole::HouseReentry)); rejected(candidate);
  candidate = original; put(candidate, geometry_at, count + 1); rejected(candidate);
  candidate = original; candidate[geometry_header] = 'X'; rejected(candidate);
  candidate = original; candidate[tree_header] = 'X'; rejected(candidate);
  candidate = original; put(candidate, reentry_at + 16, 1); rejected(candidate);
  // The independent Room format is accepted only in its own role. These
  // mutations reseal the bundle CRC; each must still reject atomically.
  candidate = original; put(candidate, inspection_at + 4, 1); rejected(candidate);
  candidate = original; put(candidate, geometry_at + 4, 3); rejected(candidate);
  candidate = original; put(candidate, inspection_header + 40, inspection->identity.scene_id ^ 1u); rejected(candidate);
  candidate = original; candidate[inspection_header + 56] ^= 1u; rejected(candidate);
  for (size_t offset : {size_t(28), size_t(32), size_t(36)}) {
    candidate = original; put(candidate, inspection_header + offset, 99); rejected(candidate);
  }
  for (size_t offset : {size_t(16), size_t(20), size_t(24), size_t(28)}) {
    candidate = original; put(candidate, geometry_at + offset, 99); rejected(candidate);
    candidate = original; put(candidate, tree_at + offset, 99); rejected(candidate);
  }
  std::cout << "House bundle role schema checks completed; no resource lifecycle executed\n";
}
