#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

// Read-only emulator observation ABI. No input, save IO, RNG, or game mutation.
// CDB finds the writable mapping by magic and validates version/size/sequence.
// Do not expose C++ object layouts or pointers to the runner.
struct EncoreSemanticProbe {
    char magic[8]={'E','N','C','S','E','M','0','1'};
    uint32_t version=2,byte_size=692,sequence=0,frame=0,width=0,height=0;
    uint32_t title_phase=0,title_option=0,naming_phase=0,naming_field=0,naming_selected=0;
    uint32_t intro_phase=0,menu_phase=0,menu_command=0,menu_slot=0,menu_candidate=0;
    uint32_t field_scene=0,field_phase=0,house_phase=0,world_stage=0;
    uint32_t paused=0,crouch=0,session_ready=0,battle_active=0,level=0;
    uint32_t hp=0,pp=0,inventory_count=0,key_count=0;
    uint32_t audio_available=0,music_playing=0,music_asset=0;
    uint32_t dsp_music_playing=0,dsp_music_sample_pos=0,dsp_music_wave_seq=0,psi_learned=0;
    uint32_t intro_scene=0,intro_door=0,overflow=0;
    float player_x=0,player_y=0,direction_x=0,direction_y=0;
    char house_error[128]{},field_error[128]{},menu_error[128]{},audio_error[128]{};
};
static_assert(sizeof(EncoreSemanticProbe)==692,"Semantic probe ABI changed");
static_assert(offsetof(EncoreSemanticProbe,player_x)==164,"Semantic probe word order changed");
static_assert(offsetof(EncoreSemanticProbe,house_error)==180,"Semantic probe text order changed");

template<size_t N> inline void semantic_probe_text(char (&out)[N],std::string_view value,uint32_t&overflow){
    if(value.size()>=N){overflow=1;out[0]=0;return;}
    if(!value.empty())std::memcpy(out,value.data(),value.size());
    out[value.size()]=0;
}
