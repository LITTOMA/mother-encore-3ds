#pragma once
#include "encore/room_data.hpp"
#include "encore/basement_progression.hpp"
#include "encore/music_regions.hpp"
#include "encore/restore_data.hpp"

namespace encore::upstream {
// Room carries a res:// URI; reviewed scene bindings carry a canonical path.
// Compare their identity at the typed boundary, never strip arbitrary schemes.
bool validate_room_music_region_call(std::string_view scene_uri,
                                    std::string_view node, bool play,
                                    double duration,
                                    const BasementProgressionData &,
                                    const MusicRegionData &, std::string &);
bool admit_room_music_bindings(RoomView, const BasementProgressionData &,
                              const MusicRegionData &, std::string &);
// The exact immutable bindings used by HouseMusicHost::prepare, also admitted
// before resource publication. No scene execution, audio allocation or RNG.
bool admit_house_music_bindings(RoomView, const RestoreData &,
                               const BasementProgressionData &,
                               const MusicRegionData &, std::string &);
}
