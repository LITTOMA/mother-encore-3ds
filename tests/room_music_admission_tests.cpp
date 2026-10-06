#include "encore/room_music_admission.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace encore::upstream;
int main(int argc, char **argv) {
  if (argc != 2) return 2;
  const std::string root = std::string(argv[1]) + "/";
  std::string error;
  auto check = [&](bool value) {
    if (!value) { std::cerr << error << '\n'; std::exit(1); }
  };
  RoomData room;
  HouseData house;
  RestoreData restore;
  BasementProgressionData progression;
  MusicRegionData music;
  check(room.load_file((root + "data/opening.encroom").c_str(), error));
  check(progression.load_file((root + "data/house.encbasement").c_str(), error));
  check(music.load_file((root + "sound/banks/house.encmusic").c_str(), error));
  check(admit_room_music_bindings(room.view(), progression, music, error));
  check(house.load_file((root + "data/house.enchouse").c_str(), error));
  check(restore.load_file((root + "data/opening.encrestore").c_str(), room.view(), house.view(), error));
  check(admit_house_music_bindings(room.view(), restore, progression, music, error));
  RestoreData absent_restore;
  check(!admit_house_music_bindings(room.view(), absent_restore, progression, music, error));
  check(!admit_house_music_bindings(RoomView{}, restore, progression, music, error));
  // Mutate detached test owners, never generated resource files. Each candidate
  // reaches the same immutable binding routine used by the device scene host.
  auto &geometry = const_cast<std::vector<BasementMusicBinding>&>(progression.music_regions());
  check(geometry.size() > 1);
  const auto original = geometry.front();
  geometry.front().scene = "res://" + original.scene;
  check(!admit_house_music_bindings(room.view(), restore, progression, music, error));
  geometry.front() = original;
  geometry.front().parent_disappear_flag = "unknown_source_flag";
  check(!admit_house_music_bindings(room.view(), restore, progression, music, error));
  geometry.front() = original;
  geometry.front().source_ordinal = geometry.back().source_ordinal;
  check(!admit_house_music_bindings(room.view(), restore, progression, music, error));
  geometry.front() = original;
  geometry.front().geometry.x += 1;
  check(!admit_house_music_bindings(room.view(), restore, progression, music, error));
  geometry.front() = original;
  auto &areas = const_cast<std::vector<RestoreMusicArea>&>(restore.music_areas());
  const auto saved_area = areas.front();
  areas.front().source_sha256[0] ^= 1;
  check(!admit_house_music_bindings(room.view(), restore, progression, music, error));
  areas.front() = saved_area;
  areas.front().conditions.clear();
  check(!admit_house_music_bindings(room.view(), restore, progression, music, error));
  areas.front() = saved_area;
  check(admit_house_music_bindings(room.view(), restore, progression, music, error));

  for (const auto &binding : progression.music_regions()) {
    const auto uri = "res://" + binding.scene;
    auto call = [&](std::string_view scene, std::string_view node, bool play,
                    double duration) {
      return validate_room_music_region_call(scene, node, play, duration,
                                              progression, music, error);
    };
    check(call(uri, binding.node, true, 0));
    check(call(uri, binding.node, false, 0));
    check(call(uri, binding.node, false, binding.default_stop_seconds));
    check(!call(binding.scene, binding.node, true, 0));
    check(!call("res://" + uri, binding.node, true, 0));
    check(!call(uri + ".foreign", binding.node, true, 0));
    check(!call(uri, binding.node + "/unknown", true, 0));
    check(!call(uri, binding.node, true, binding.default_stop_seconds));
    check(!call(uri, binding.node, false, -1));
    check(!call(uri, binding.node, false, std::numeric_limits<double>::quiet_NaN()));
    check(!call(uri, binding.node, false, std::numeric_limits<double>::infinity()));
  }
  MusicRegionData missing;
  check(!admit_room_music_bindings(room.view(), progression, missing, error));
  check(admit_room_music_bindings(room.view(), progression, music, error));
  std::cout << "Original Room music URI and source bindings admitted\n";
}
