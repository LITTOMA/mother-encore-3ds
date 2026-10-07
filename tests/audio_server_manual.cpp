// Manual-only resource parser cases. Not registered or executed automatically.
#include "encore/audio_data.hpp"
#include "encore/audio_server.hpp"
#include <fstream>
#include <iostream>
namespace {
void put(std::vector<uint8_t> &b, size_t at, uint32_t n) {
  for (size_t i = 0; i < 4; ++i)
    b.at(at + i) = uint8_t(n >> (i * 8));
}
} // namespace
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  std::ifstream f(argv[1], std::ios::binary);
  f.seekg(0, std::ios::end);
  auto size = f.tellg();
  if (size < 128)
    return 2;
  f.seekg(0);
  std::vector<uint8_t> bytes(static_cast<size_t>(size));
  if (!f.read(reinterpret_cast<char *>(bytes.data()), size))
    return 2;
  encore::upstream::AudioServerData valid;
  std::string e;
  if (!valid.load(bytes.data(), bytes.size(), e)) {
    std::cerr << e;
    return 1;
  }
  const auto original = valid.ir_sha();
  const auto buses = valid.buses().size();
  for (size_t at : {size_t(0), size_t(8), size_t(20), size_t(24), size_t(28),
                    size_t(32), size_t(124)}) {
    auto bad = bytes;
    bad[at] ^= 0x80;
    if (valid.load(bad.data(), bad.size(), e) || valid.ir_sha() != original ||
        valid.buses().size() != buses)
      return 1;
  }
  // A valid CRC does not permit an unknown/trailing body record. Failure must
  // leave the previously loaded data object intact.
  auto bad = bytes;
  bad.push_back(0);
  put(bad, 16, static_cast<uint32_t>(bad.size()));
  put(bad, 20,
      encore::upstream::audio_crc32(bad.data() + 128, bad.size() - 128));
  if (valid.load(bad.data(), bad.size(), e) || valid.ir_sha() != original ||
      valid.buses().size() != buses)
    return 1;
  return 0;
}
