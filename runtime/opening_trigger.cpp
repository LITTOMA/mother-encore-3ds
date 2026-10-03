#include "encore/opening_trigger.hpp"
#include <algorithm>
#include <cmath>
namespace encore::upstream {
bool first_trigger_overlap(const RoomView& content,uint32_t index,Vec2 position) {
    if(!content.valid()||index>=content.trigger_count()||!std::isfinite(position.x)||!std::isfinite(position.y))return false;
    const auto scene=content.scene();const auto trigger=content.trigger(index);
    auto separated=[&](Vec2 axis){
        float amin=INFINITY,amax=-INFINITY,bmin=INFINITY,bmax=-INFINITY;
        for(uint32_t i=0;i<scene.actor_hull_count;++i){const auto p=content.vertex(scene.actor_hull_first+i);const float d=(p.x+position.x)*axis.x+(p.y+position.y)*axis.y;amin=std::min(amin,d);amax=std::max(amax,d);}
        for(uint32_t i=0;i<trigger.vertex_count;++i){const auto p=content.vertex(trigger.first_vertex+i);const float d=p.x*axis.x+p.y*axis.y;bmin=std::min(bmin,d);bmax=std::max(bmax,d);}
        return amax<=bmin||bmax<=amin;
    };
    for(uint32_t i=0;i<scene.actor_hull_count;++i){const auto a=content.vertex(scene.actor_hull_first+i),b=content.vertex(scene.actor_hull_first+(i+1)%scene.actor_hull_count);if(separated({a.y-b.y,b.x-a.x}))return false;}
    for(uint32_t i=0;i<trigger.vertex_count;++i){const auto a=content.vertex(trigger.first_vertex+i),b=content.vertex(trigger.first_vertex+(i+1)%trigger.vertex_count);if(separated({a.y-b.y,b.x-a.x}))return false;}
    return true;
}
}
