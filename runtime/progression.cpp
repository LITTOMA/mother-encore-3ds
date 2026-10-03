#include "encore/progression.hpp"
#include "encore/room_data.hpp"
namespace encore::upstream {
int32_t progression_level_cap(const RoomView& content){return content.valid()?int32_t(content.experience_count()):0;}
bool experience_for_level(const RoomView& content,int32_t level,int32_t& output){
    const int32_t cap=progression_level_cap(content);if(level<1||cap<1)return false;
    if(level>cap)level=cap;
    output=int32_t(content.experience(uint32_t(level-1)));return true;
}
int32_t level_for_experience(const RoomView& content,int32_t experience){
    const int32_t cap=progression_level_cap(content);if(cap<1)return 0;
    int32_t level=1;while(level<cap&&experience>=int32_t(content.experience(uint32_t(level))))++level;
    return level;
}
}
