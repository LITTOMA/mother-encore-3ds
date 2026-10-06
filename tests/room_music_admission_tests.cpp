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
  BasementProgressionData progression;
  MusicRegionData music;
  check(room.load_file((root + "data/opening.encroom").c_str(), error));
  check(progression.load_file((root + "data/house.encbasement").c_str(), error));
  check(music.load_file((root + "sound/banks/house.encmusic").c_str(), error));
  check(admit_room_music_bindings(room.view(), progression, music, error));
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
