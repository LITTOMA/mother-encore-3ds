#pragma once
#include "encore/field_steps.hpp"
#include <memory>
namespace encore::upstream {
struct ShadowGraphicsConfig {
 uint32_t owner=0,width=0,height=0,texture_bytes=0,texture_crc=0,flags=0,initial=0;
 bool playing=false;std::string texture;Vec2 position{},offset{};
};
struct ShadowGraphicsMode {std::string name;float fps=0;uint32_t first=0,count=0;bool front=false,loop=false;};
struct ShadowGraphicsFrame {uint32_t mode=0,index=0,x=0,y=0,width=0,height=0;};
struct ShadowDraw {ShadowGraphicsFrame source;Vec2 position{};bool visible=false,behind=false,flip_h=false,flip_v=false;};
// ENCSHDW1 format1/capability1. Local Player child geometry and exact texture
// bytes are independent of stepping areas,save/catalog schemas and GPU APIs.
class ShadowGraphicsData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool valid()const{return bool(state_);}const ShadowGraphicsConfig&config()const;
 const std::vector<ShadowGraphicsMode>&modes()const;const std::vector<ShadowGraphicsFrame>&frames()const;
 const ShadowGraphicsFrame*frame(uint32_t mode,uint32_t frame)const;
 bool matches(const FieldStepShadowState&)const;bool texture_matches(const uint8_t*,size_t,std::string&)const;
 bool compose(const FieldStepShadowState&,uint32_t profile,Vec2 body,Vec2 camera,bool visible,ShadowDraw&,std::string&)const;
private:struct State;std::shared_ptr<const State>state_;
};
}
