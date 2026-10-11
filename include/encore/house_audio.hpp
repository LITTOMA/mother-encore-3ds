#pragma once
#include "encore/audio_data.hpp"
#include "encore/dialogue_choices_data.hpp"
#include "encore/gameplay_audio.hpp"
#include "encore/house_data.hpp"
#include "encore/phone_data.hpp"
#include "encore/room_data.hpp"
#include "encore/save_menu_data.hpp"

namespace encore::upstream {
// Collect checked bank identities before the house becomes interactive. Paths
// and cue bindings come from resources; PCM admission remains the backend's job.
// Failed collection preserves the caller's previous plan.
bool build_house_audio_plan(RoomView,HouseView,PhoneView,const DialogueChoicesData&,
                            const SaveMenuData&,const GameplayAudioData&,const AudioBank&,
                            std::vector<uint32_t>&,std::string&);
}
