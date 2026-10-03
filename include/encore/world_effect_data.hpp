#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace encore::upstream {
struct WorldEffectColor {float r=0,g=0,b=0,a=0;};
struct WorldEffectSettings {
 uint32_t texture_width=0,texture_height=0;
 float source_width=0,source_height=0,appear_duration=0,disappear_duration=0,cycle_duration=0,opacity=0;
 float move_x=0,move_y=0,amplitude_y=0,frequency_y=0,speed_y=0,translation_ping_pong_y=0,amplitude_ping_pong_y=0,move_divisor=0,pixel_snap_uv_epsilon=0;
 WorldEffectColor initial_modulate{};
};
struct WorldEffectKey {float time=0,ease=0;WorldEffectColor color{};};
class WorldEffectView {
public:
 bool valid()const{return bytes_!=nullptr;}
 WorldEffectSettings settings()const;
 uint32_t key_count()const;WorldEffectKey key(uint32_t)const;
 uint32_t palette_count()const;uint32_t palette(uint32_t)const;
 const uint8_t*texels()const;
private:
 friend class WorldEffectData;const uint8_t*bytes_=nullptr;size_t size_=0;
 const uint8_t*record(uint32_t section,uint32_t index)const;
 uint32_t count(uint32_t section)const;
};
// Validated owner with borrowed immutable views. Failed loads preserve old data.
class WorldEffectData {
public:
 WorldEffectData()=default;WorldEffectData(const WorldEffectData&)=delete;WorldEffectData&operator=(const WorldEffectData&)=delete;
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 WorldEffectView view()const{WorldEffectView v;if(!bytes_.empty()){v.bytes_=bytes_.data();v.size_=bytes_.size();}return v;}
private:std::vector<uint8_t>bytes_;
};
}
