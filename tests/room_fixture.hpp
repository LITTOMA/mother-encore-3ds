#pragma once
#include "encore/room_data.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

#ifndef ENCORE_TEST_ROOM_PATH
#error "Tests require an explicit ENCORE_TEST_ROOM_PATH external room pack"
#endif
namespace encore_test {
inline encore::upstream::RoomView room() {
    // Test-only lifetime owner. Production callers never get a default room.
    static encore::upstream::RoomData data;
    static const bool loaded=[] {
        std::string error;
        if(!data.load_file(ENCORE_TEST_ROOM_PATH,error)) {
            std::fprintf(stderr,"External test room failed: %s\n",error.c_str());std::abort();
        }
        return true;
    }();
    (void)loaded;
    return data.view();
}
inline uint32_t actor(std::string_view name) {
    const auto r=room();
    for(uint32_t i=0;i<r.actor_instance_count();++i)if(r.string(r.actor_instance(i).display_name_string)==name)return i;
    std::fprintf(stderr,"Missing external test actor: %.*s\n",int(name.size()),name.data());std::abort();
}
inline uint32_t profile(std::string_view name) { return room().actor_instance(actor(name)).profile_index; }
inline uint32_t resource(std::string_view path) {
    const auto r=room();for(uint32_t i=0;i<r.resource_count();++i)if(r.string(r.resource(i).path_string)==path)return i;
    std::fprintf(stderr,"Missing external test resource: %.*s\n",int(path.size()),path.data());std::abort();
}
inline uint32_t opening_program() {
    const auto r=room();if(!r.trigger_count()){std::fprintf(stderr,"External test room lacks opening trigger\n");std::abort();}
    return r.trigger(0).program_index;
}
inline uint32_t actor_clip(std::string_view actor_name,std::string_view animation) {
    using namespace encore::upstream;
    const auto r=room();const auto p=r.actor_profile(profile(actor_name));
    if(animation=="Idle")return p.idle_clip;
    if(animation=="surprise")return p.emote_clip;
    if(animation=="Open") {
        const auto texture=r.resource(p.primary_resource);
        for(uint32_t i=0;i<r.clip_count();++i) {const auto c=r.clip(i);if(c.channel==0&&(c.flags&2)&&c.frame_count==uint32_t(texture.columns)*texture.rows)return i;}
    }
    return kRoomNoIndex;
}
}
