// Manual source-bank projection cases; never run during normal builds.
#include "encore/music_regions.hpp"
#include <cassert>
#include <cmath>
#include <limits>
void music_region_asset_projection_manual(
    const encore::upstream::MusicRegionData &regions,
    const encore::upstream::AudioBank &complete_bank,
    const encore::upstream::AudioBank &missing_track,
    const encore::upstream::AudioBank &wrong_source,
    const encore::upstream::AudioBank &wrong_path,
    const encore::upstream::AudioBank &nonlooping,
    const encore::upstream::AudioBank &wrong_silence) {
  using namespace encore::upstream;
  std::string error;
  assert(regions.valid());
  // Caller uses genuine complete source bank, including unrelated sfx. Its
  // total count must not become the region player's bounded track count.
  assert(complete_bank.count() > regions.tracks().size());
  std::vector<AudioAsset> selected;
  assert(regions.select_assets(complete_bank, complete_bank.master_db(),
                               MusicRegionController::maximum_voices, selected,
                               error));
  assert(selected.size() == regions.tracks().size());
  for (size_t i = 0; i < selected.size(); ++i) {
    assert(selected[i].stable_id == regions.tracks()[i].id);
    assert(selected[i].source_sha256 == regions.tracks()[i].source_sha);
    assert(selected[i].source_path == regions.tracks()[i].source_path);
    assert(selected[i].loops());
  }
  const auto original = selected;
  auto unchanged = [&]() {
    assert(selected.size() == original.size());
    for (size_t i = 0; i < selected.size(); ++i)
      assert(selected[i].stable_id == original[i].stable_id &&
             selected[i].source_path.data() == original[i].source_path.data());
  };
  for (float master :
       {complete_bank.master_db() + 1, std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::quiet_NaN()}) {
    assert(!regions.select_assets(complete_bank, master,
                                  MusicRegionController::maximum_voices,
                                  selected, error));
    unchanged();
  }
  for (uint32_t capacity :
       std::array<uint32_t, 2>{0, uint32_t(regions.tracks().size() - 1)}) {
    assert(!regions.select_assets(complete_bank, complete_bank.master_db(),
                                  capacity, selected, error));
    unchanged();
  }
  MusicRegionData absent;
  assert(!absent.select_assets(complete_bank, complete_bank.master_db(),
                               MusicRegionController::maximum_voices, selected,
                               error));
  unchanged();
  for (const auto *bank : {&missing_track, &wrong_source, &wrong_path,
                           &nonlooping, &wrong_silence}) {
    assert(!regions.select_assets(*bank, bank->master_db(),
                                  MusicRegionController::maximum_voices,
                                  selected, error));
    unchanged();
  }
}
