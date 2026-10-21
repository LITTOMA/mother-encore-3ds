#pragma once
#include "encore/geyser_timeline.hpp"
namespace encore::upstream {
class GeyserLifecycle;
struct GeyserGraphicsConfig {
 std::string texture;uint32_t width=0,height=0,texture_bytes=0,texture_crc=0,flags=0;
};
struct GeyserSpriteFrame {uint32_t index=0,x=0,y=0,width=0,height=0;};
struct GeyserSpriteDraw {GeyserSpriteFrame source;Vec2 position{};bool visible=false,flip_h=false,flip_v=false;};
// ENCGFX01 format1/capability1:strict independent image/source timeline
// ownership. Source root position is supplied by the actual factory;the
// prefab's authored root offset must not be added a second time here.
class GeyserGraphicsData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool valid()const{return bool(state_);}const GeyserGraphicsConfig&config()const;
 const GeyserTimelineData&timeline()const;const std::vector<GeyserSpriteFrame>&frames()const;
 bool matches(const GeyserTimelineData&)const;
 bool texture_matches(const uint8_t*,size_t,std::string&)const;
 bool compose(const GeyserTimeline&,uint64_t epoch,uint64_t instance,Vec2 root,Vec2 camera,bool visible,GeyserSpriteDraw&,std::string&)const;
 // Camera coordinates share the registry parent's audited translation space.
 bool compose_lifecycle(const GeyserLifecycle&,uint64_t epoch,uint64_t instance,Vec2 camera,bool visible,GeyserSpriteDraw&,std::string&)const;
private:struct State;std::shared_ptr<const State>state_;
 bool compose_pose(const GeyserSnapshot&,Vec2,Vec2,bool,GeyserSpriteDraw&,std::string&)const;
};
}
