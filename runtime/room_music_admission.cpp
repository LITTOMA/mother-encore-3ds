#include "encore/room_music_admission.hpp"
#include <algorithm>
#include <cmath>

namespace encore::upstream {
bool validate_room_music_region_call(std::string_view scene_uri,
                                    std::string_view node, bool play,
                                    double duration,
                                    const BasementProgressionData &progression,
                                    const MusicRegionData &music,
                                    std::string &error) {
  auto fail = [&](const char *message) { error = message; return false; };
  if (!progression.valid() || !music.valid() || !std::isfinite(duration))
    return fail("Music region source owners incomplete");
  const auto &bindings = progression.music_regions();
  const auto original = std::find_if(bindings.begin(), bindings.end(),
      [&](const auto &binding) { return binding.node == node; });
  if (original == bindings.end())
    return fail("Music region source node absent");
  if (scene_uri != "res://" + original->scene)
    return fail("Music region belongs to a different source scene");
  const auto &regions = music.regions();
  const auto region = std::find_if(regions.begin(), regions.end(),
      [&](const auto &binding) { return binding.source_path == node; });
  if (region == regions.end() || region->id != original->region_id ||
      region->track_id != original->track_id ||
      region->volume_db != original->volume_db ||
      region->fadein_seconds != original->fadein_seconds ||
      region->fadeout_seconds != original->fadeout_seconds)
    return fail("Music region source service binding differs");
  const auto &tracks = music.tracks();
  const auto track = std::find_if(tracks.begin(), tracks.end(),
      [&](const auto &binding) { return binding.id == original->track_id; });
  if (track == tracks.end() || track->source_path != original->music)
    return fail("Music region source track differs");
  if (play ? duration != 0 : (duration != 0 && duration != original->default_stop_seconds))
    return fail("Music region method source/default duration differs");
  error.clear();
  return true;
}

bool admit_room_music_bindings(RoomView room,
                              const BasementProgressionData &progression,
                              const MusicRegionData &music,
                              std::string &error) {
  if (!room.valid()) { error = "Music region Room owner absent"; return false; }
  std::string pin;
  const char hex[] = "0123456789abcdef";
  for (size_t i = 56; i < 76; ++i) {
    pin += hex[room.bytes()[i] >> 4];
    pin += hex[room.bytes()[i] & 15];
  }
  if (!progression.bind_reviewed_commit(pin, error)) return false;
  const auto scene = room.string(room.scene().source_scene_string);
  for (uint32_t i = 0; i < room.binding_count(); ++i) {
    const auto binding = room.binding(i);
    const bool play = binding.kind == uint16_t(RoomBindingKind::PlayMusicRegion);
    if (!play && binding.kind != uint16_t(RoomBindingKind::StopMusicRegion)) continue;
    if (!validate_room_music_region_call(scene, room.string(binding.target_index),
                                        play, binding.duration, progression, music, error))
      return false;
  }
  error.clear();
  return true;
}
}
