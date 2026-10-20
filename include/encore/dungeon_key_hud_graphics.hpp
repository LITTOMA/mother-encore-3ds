#pragma once
#include "encore/dungeon_key_indicator.hpp"
namespace encore::upstream {
// ENCKGFX1 format1/capability1: icon byte identity, borrowed-font identity,
// original local geometry and explicit reference/expanded viewport anchors.
struct KeyHudGraphicsConfig {
 std::string texture;uint32_t width=0,height=0,texture_bytes=0,texture_crc=0,font_bytes=0,font_crc=0,color=0;
 Vec2 reference{},expanded{},icon_offset{},text_offset{};
};
struct KeyHudDraw {Vec2 icon{},text{};std::string label;bool visible=false;};
class DungeonKeyHudGraphicsData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool valid()const{return bool(state_);}const KeyHudGraphicsConfig&config()const;
 const DungeonKeyIndicatorData&indicator()const;
 bool matches(const DungeonKeyIndicator&)const;
 bool texture_matches(const uint8_t*,size_t,std::string&)const;
 bool compose(const DungeonKeyIndicator&,float,float,KeyHudDraw&,std::string&)const;
private:struct State;std::shared_ptr<const State>state_;
};
}
