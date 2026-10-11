#include "encore/house_audio.hpp"
#include <algorithm>

namespace encore::upstream {
namespace {
std::string_view source_path(std::string_view path){
    if(path.substr(0,6)=="res://")path.remove_prefix(6);
    return path;
}
}
bool build_house_audio_plan(RoomView room,HouseView house,PhoneView phone,
                            const DialogueChoicesData& choices,const SaveMenuData& save,
                            const GameplayAudioData& cues,const AudioBank& bank,
                            std::vector<uint32_t>& output,std::string& error){
    if(!room.valid()||!house.valid()||!phone.valid()||!choices.valid()||!save.valid()||!bank.count()){
        error="House audio requires checked resource owners";return false;
    }
    std::vector<uint32_t> candidate;
    const auto add=[&](std::string_view path,const std::array<uint8_t,32>* digest=nullptr){
        AudioAsset found;uint32_t matches=0;
        for(uint32_t i=0;i<bank.count();++i){const auto asset=bank.asset(i);
            if(source_path(asset.source_path)==source_path(path)){found=asset;++matches;}
        }
        if(matches!=1){error="House audio source is absent or ambiguous: "+std::string(path);return false;}
        if(digest&&found.source_sha256!=*digest){error="House audio source digest disagrees: "+std::string(path);return false;}
        if(std::find(candidate.begin(),candidate.end(),found.stable_id)==candidate.end())candidate.push_back(found.stable_id);
        return true;
    };
    // Room source identity, not saved/gameplay identity: a source may use a
    // different stable ID in a room pack and the shared cutscene/audio bank.
    for(uint32_t i=0;i<room.resource_count();++i){const auto resource=room.resource(i);
        if(resource.kind==uint16_t(RoomResourceKind::AudioRequestOnly)&&
           !add(room.string(resource.path_string),&resource.sha256))return false;
    }
    for(uint32_t i=0;i<house.count(HouseSection::Segments);++i){const auto voice=house.string(house.segment(i).voice);
        // Empty is the pack's explicit narration/no-voice binding.
        if(!voice.empty()&&!add(voice))return false;
    }
    for(uint32_t i=0;i<phone.count(PhoneSection::Objects);++i){const auto object=phone.object(i);
        if(!add(phone.string(object.ring_sound))||!add(phone.string(object.hangup_sound)))return false;
    }
    for(uint32_t i=0;i<phone.count(PhoneSection::SoundKeys);++i)
        if(!add(phone.string(phone.sound_key(i).resource)))return false;
    if(!add(choices.sound(DialogueChoiceSound::Move))||!add(choices.sound(DialogueChoiceSound::Accept))||
       !add(choices.sound(DialogueChoiceSound::Cancel)))return false;
    for(uint32_t i=0;i<uint32_t(SaveMenuSound::Count);++i)if(!add(save.sound(SaveMenuSound(i))))return false;
    if(!add(cues.path(GameplayCue::MenuOpen))||!add(cues.path(GameplayCue::MenuClose))||
       !add(cues.path(GameplayCue::Confirm)))return false;
    output.swap(candidate);error.clear();return true;
}
}
