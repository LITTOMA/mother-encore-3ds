#include "encore/room_music_admission.hpp"
#include <algorithm>
#include <cmath>
#include <set>

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

bool admit_house_music_bindings(RoomView room, const RestoreData &restore,
                               const BasementProgressionData &progression,
                               const MusicRegionData &music,
                               std::string &error) {
  auto fail = [&](const char *message) { error = message; return false; };
  if (!room.valid() || !restore.valid() || !progression.valid() ||
      !music.valid() || music.regions().size() != progression.music_regions().size() ||
      restore.music_areas().size() != music.regions().size())
    return fail("House music candidate resources rejected");
  if (room.scene().actor_hull_count < 3)
    return fail("House music player hull unavailable");
  if (!admit_room_music_bindings(room, progression, music, error)) return false;
  auto flag_exists = [&](std::string_view name) {
    if (name.empty()) return true;
    for (uint32_t i = 0; i < room.flag_count(); ++i)
      if (room.string(room.flag(i).name_string) == name) return true;
    return false;
  };
  std::set<uint32_t> ordinals;
  for (const auto &g : progression.music_regions()) {
    // Reuse the same URI/path boundary as explicit Room music calls. The
    // source resource stores a relative path; Room stores a res:// URI.
    if (!validate_room_music_region_call(room.string(room.scene().source_scene_string),
                                         g.node, true, 0, progression, music, error))
      return false;
    if (!flag_exists(g.parent_disappear_flag))
      return fail("House music parent disappearance flag absent");
    if (!ordinals.insert(g.source_ordinal).second)
      return fail("House music source lifecycle ordinal duplicated");
    const auto binding = std::find_if(music.regions().begin(), music.regions().end(),
        [&](const auto &r) { return r.source_path == g.node; });
    // validate_room_music_region_call already admitted this exact binding.
    if (binding == music.regions().end() || !flag_exists(binding->appear_flag) ||
        !flag_exists(binding->disappear_flag))
      return fail("House music region flag absent");
    const auto track = std::find_if(music.tracks().begin(), music.tracks().end(),
        [&](const auto &t) { return t.id == g.track_id; });
    const auto area = std::find_if(restore.music_areas().begin(), restore.music_areas().end(),
        [&](const auto &a) { return a.source_path == g.node; });
    if (track == music.tracks().end() || area == restore.music_areas().end() ||
        area->resource_path != g.music || area->source_sha256 != track->source_sha ||
        float(area->center.x) != g.geometry.x || float(area->center.y) != g.geometry.y ||
        float(area->extents.x) != g.geometry.z || float(area->extents.y) != g.geometry.w ||
        float(area->volume_db) != g.volume_db ||
        float(area->fadein_seconds) != g.fadein_seconds ||
        float(area->fadeout_seconds) != g.fadeout_seconds)
      return fail("House restore/music source geometry rejected");
    std::vector<std::pair<std::string, bool>> expected;
    if (!g.parent_disappear_flag.empty()) expected.emplace_back(g.parent_disappear_flag, false);
    if (!binding->appear_flag.empty()) expected.emplace_back(binding->appear_flag, true);
    if (!binding->disappear_flag.empty()) expected.emplace_back(binding->disappear_flag, false);
    if (expected.size() != area->conditions.size())
      return fail("House restore music flag coverage rejected");
    for (size_t i = 0; i < expected.size(); ++i)
      if (expected[i].first != area->conditions[i].flag_name ||
          expected[i].second != area->conditions[i].expected_value)
        return fail("House restore music flag binding rejected");
  }
  error.clear();
  return true;
}
}
