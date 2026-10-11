#include "encore/house_audio.hpp"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>

using namespace encore::upstream;
namespace {
void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
uint32_t u32(const uint8_t* p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
void put32(std::vector<uint8_t>& b,size_t at,uint32_t n){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(n>>(i*8));}
void fix_crc(std::vector<uint8_t>& b){put32(b,16,0);put32(b,16,audio_crc32(b.data(),b.size()));}
std::string_view source_path(std::string_view s){if(s.substr(0,6)=="res://")s.remove_prefix(6);return s;}
}
int main(int argc,char** argv){
    check(argc==2,"RomFS root required");const std::string root=argv[1];std::string error;
    RoomData room;HouseData house;PhoneData phone;DialogueChoicesData choices;SaveMenuData save;GameplayAudioData cues;AudioBank bank;
    check(room.load_file((root+"/world/house/opening.encroom").c_str(),error),error.c_str());
    check(house.load_file((root+"/world/house/opening.enchouse").c_str(),error),error.c_str());
    check(phone.load_file((root+"/world/house/opening.encphone").c_str(),error),error.c_str());
    check(choices.load_file((root+"/ui/opening.encchoices").c_str(),error),error.c_str());
    check(save.load_file((root+"/ui/opening.encsavemenu").c_str(),error),error.c_str());
    check(cues.load_file((root+"/sound/banks/gameplay.enccues").c_str(),error),error.c_str());
    const std::string bank_path=root+"/sound/banks/opening.encaudio";
    check(bank.load_file(bank_path.c_str(),error),error.c_str());std::vector<uint32_t> plan;
    const auto build=[&](const AudioBank& audio){return build_house_audio_plan(room.view(),house.view(),phone.view(),choices,save,cues,audio,plan,error);};
    check(build(bank),error.c_str());const auto expected=plan;
    check(!plan.empty()&&plan.size()<bank.count(),"House plan does not prepare the entire game audio bank");
    auto unique=plan;std::sort(unique.begin(),unique.end());check(std::adjacent_find(unique.begin(),unique.end())==unique.end(),"Shared cues prepare once");
    const auto require=[&](std::string_view path){
        for(uint32_t i=0;i<bank.count();++i){const auto asset=bank.asset(i);
            if(source_path(asset.source_path)==source_path(path)){
                check(std::find(plan.begin(),plan.end(),asset.stable_id)!=plan.end(),"Phone interaction source missing from preparation plan");return i;
            }
        }
        check(false,"Required source absent from checked bank");return UINT32_MAX;
    };
    // Actual audited Dad voice and source Phone streams, not synthetic fixture cues.
    const auto adult=require("Audio/Sound effects/text/Adult.mp3");
    require("Audio/Sound effects/phonering.wav");require("Audio/Sound effects/phonehangup.wav");
    require(choices.sound(DialogueChoiceSound::Move));require(choices.sound(DialogueChoiceSound::Accept));require(choices.sound(DialogueChoiceSound::Cancel));
    for(uint32_t i=0;i<uint32_t(SaveMenuSound::Count);++i)require(save.sound(SaveMenuSound(i)));
    require(cues.path(GameplayCue::MenuOpen));require(cues.path(GameplayCue::MenuClose));require(cues.path(GameplayCue::Confirm));
    check(build(bank)&&plan==expected,"Repeated preparation has the same identities");
    std::ifstream file(bank_path,std::ios::binary);const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(file),{}};
    auto changed=bytes;const size_t voice_path=u32(changed.data()+64+adult*96+68);
    changed[voice_path+6]='X';fix_crc(changed);AudioBank missing;
    check(missing.load(changed.data(),changed.size(),error),error.c_str());
    check(!build(missing)&&error.find("Adult.mp3")!=error.npos&&plan==expected,"Missing Dad voice rejects before interaction and preserves plan");
    uint32_t room_audio=UINT32_MAX;
    for(uint32_t i=0;i<room.view().resource_count();++i){const auto r=room.view().resource(i);
        if(r.kind==uint16_t(RoomResourceKind::AudioRequestOnly)){room_audio=require(room.view().string(r.path_string));break;}
    }
    check(room_audio!=UINT32_MAX,"Real house has checked audio dependencies");changed=bytes;changed[64+room_audio*96+4]^=1;fix_crc(changed);AudioBank wrong_digest;
    check(wrong_digest.load(changed.data(),changed.size(),error),error.c_str());
    check(!build(wrong_digest)&&error.find("digest disagrees")!=error.npos&&plan==expected,"Mismatched source digest rejects without publishing a partial plan");
    AudioBank empty;check(!build(empty)&&plan==expected,"Unloaded bank rejected");
    check(!build_house_audio_plan({},house.view(),phone.view(),choices,save,cues,bank,plan,error)&&plan==expected,"Unloaded room rejected");
    check(!build_house_audio_plan(room.view(),{},phone.view(),choices,save,cues,bank,plan,error)&&plan==expected,"Unloaded house rejected");
    check(!build_house_audio_plan(room.view(),house.view(),{},choices,save,cues,bank,plan,error)&&plan==expected,"Unloaded phone rejected");
    DialogueChoicesData empty_choices;SaveMenuData empty_save;GameplayAudioData empty_cues;
    check(!build_house_audio_plan(room.view(),house.view(),phone.view(),empty_choices,save,cues,bank,plan,error)&&plan==expected,"Unloaded choices rejected");
    check(!build_house_audio_plan(room.view(),house.view(),phone.view(),choices,empty_save,cues,bank,plan,error)&&plan==expected,"Unloaded save menu rejected");
    check(!build_house_audio_plan(room.view(),house.view(),phone.view(),choices,save,empty_cues,bank,plan,error)&&plan==expected,"Unloaded dialogue cues rejected");
    std::cout<<"House audio admission: "<<expected.size()<<" checked streams; no hardware audibility claim\n";
}
